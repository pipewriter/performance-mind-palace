# Game Engine Architecture Plan

## Core Abstractions for Efficient Rendering

### 1. Mesh/Geometry Abstraction

The fundamental unit is typically a **Mesh** that wraps vertex and index buffers:

```cpp
class Mesh {
    VkBuffer vertexBuffer;
    VkBuffer indexBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;
    bool isDynamic;  // Can vertices change?
};
```

**Key Distinction: Static vs Dynamic Geometry**
- **Static meshes**: Buildings, rocks, unchanging terrain - uploaded once, never modified
- **Dynamic meshes**: Animated characters, particles, deformable terrain - updated frequently

### 2. Partial Vertex Updates

Most engines don't update ALL vertices. Instead:

**Approach A: Dirty Regions**
```cpp
class DynamicMesh {
    std::vector<Vertex> vertices;
    std::vector<DirtyRegion> dirtyRegions;  // Track what changed

    void markDirty(uint32_t startVertex, uint32_t count) {
        dirtyRegions.push_back({startVertex, count});
    }

    void flushToGPU() {
        // Only update the dirty regions
        for (auto& region : dirtyRegions) {
            vkMapMemory(...);
            memcpy(mapped + region.offset, vertices.data() + region.offset, region.size);
            vkUnmapMemory(...);
        }
        dirtyRegions.clear();
    }
};
```

**Approach B: Double/Triple Buffering**
- CPU writes to one buffer while GPU reads from another
- Prevents stalls and allows async updates
- Used for streaming data like particles or animated meshes

**Approach C: Separate Dynamic Parts**
- Static terrain base (never changes)
- Dynamic overlay layer (modificable - like Minecraft blocks)
- Only allocate dynamic buffers for things that actually change

### 3. Entity Component System (ECS)

Modern approach for organizing game objects:

```cpp
struct Transform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
};

struct MeshRenderer {
    Mesh* mesh;
    Material* material;
    bool visible;
};

struct Entity {
    Transform transform;
    MeshRenderer* renderer;  // Optional component
    // ... other components
};
```

**Benefits:**
- Data-oriented design (cache friendly)
- Easy to update only what changed
- Can process all entities with same component type in batch

### 4. Large World Management - Chunking

For big maps, divide the world into **chunks** (like Minecraft):

```cpp
struct Chunk {
    glm::ivec2 chunkCoords;  // e.g., (0,0), (1,0), (0,1)
    Mesh* mesh;
    bool isLoaded;
    bool isDirty;  // Need to regenerate mesh?
};

class World {
    std::unordered_map<glm::ivec2, Chunk*> chunks;
    glm::vec3 playerPosition;

    void update() {
        // Load chunks near player
        // Unload chunks far from player
        // Regenerate dirty chunks
    }
};
```

**Typical chunk size:** 16x16 to 64x64 units
- Small enough to load/unload quickly
- Large enough to not have too many chunks

### 5. Streaming System

**The core loop:**
```
1. Determine visible chunks (frustum culling)
2. Priority queue: Load closest chunks first
3. Background thread generates chunk meshes
4. Upload to GPU when ready
5. Unload chunks beyond render distance
```

**Async Loading:**
- Main thread: Rendering
- Worker threads: Terrain generation, mesh building
- Upload happens on main thread (Vulkan requirement)

### 6. Level of Detail (LOD)

Render distant objects with less detail:

```cpp
struct LODMesh {
    Mesh* lod0;  // Full detail
    Mesh* lod1;  // Half detail
    Mesh* lod2;  // Quarter detail
    float lod1Distance;  // Switch distances
    float lod2Distance;
};

Mesh* selectLOD(float distanceToCamera) {
    if (distance < lod1Distance) return lod0;
    if (distance < lod2Distance) return lod1;
    return lod2;
}
```

**For terrain specifically:**
- Near chunks: Full resolution (e.g., 1 vertex per meter)
- Medium distance: Half resolution (1 vertex per 2 meters)
- Far distance: Quarter resolution (1 vertex per 4 meters)

### 7. Spatial Partitioning

**Quadtree (2D) / Octree (3D)**
Hierarchical structure to quickly find nearby objects:

```
Root (entire world)
├─ NW quadrant
│  ├─ NW sub-quadrant
│  ├─ NE sub-quadrant
│  └─ ...
├─ NE quadrant
└─ ...
```

**Benefits:**
- Fast spatial queries: "What's near this position?"
- Efficient frustum culling: Skip entire branches
- Better than testing every object

### 8. Frustum Culling

Don't render what the camera can't see:

```cpp
bool isInFrustum(BoundingBox box, Frustum frustum) {
    // Test if box intersects view frustum
    return frustum.intersects(box);
}

void render() {
    for (auto& entity : entities) {
        if (isInFrustum(entity.boundingBox, camera.frustum)) {
            renderEntity(entity);
        }
    }
}
```

### 9. Procedural Terrain Generation

**Typical pipeline:**
```
1. Generate heightmap (Perlin/Simplex noise)
2. Create mesh from heightmap
3. Apply texturing/materials
4. Upload to GPU
```

**Example noise-based terrain:**
```cpp
float getHeight(int x, int z) {
    float height = 0;
    height += perlin(x * 0.01, z * 0.01) * 100;      // Large features
    height += perlin(x * 0.05, z * 0.05) * 20;       // Medium features
    height += perlin(x * 0.1, z * 0.1) * 5;          // Small details
    return height;
}
```

### 10. Practical Architecture

**Minimal viable structure:**

```
World
├─ ChunkManager
│  ├─ Loaded chunks (near player)
│  ├─ LoadQueue (chunks to load)
│  └─ UnloadQueue (chunks to unload)
│
├─ EntityManager
│  ├─ Entities (characters, items, etc.)
│  └─ Components (Transform, Renderer, Physics, etc.)
│
└─ Renderer
   ├─ Camera (view/projection matrices)
   ├─ RenderQueue (sorted by material/distance)
   └─ CommandBuffers (Vulkan commands)
```

**Update loop:**
```cpp
void gameLoop() {
    // 1. Update game logic
    entityManager.update(deltaTime);
    chunkManager.updateStreaming(player.position);

    // 2. Build render queue
    renderQueue.clear();
    for (auto& entity : entityManager.getVisible()) {
        if (frustumCull(entity)) {
            renderQueue.add(entity);
        }
    }

    // 3. Sort by material (reduces state changes)
    renderQueue.sortByMaterial();

    // 4. Render
    for (auto& item : renderQueue) {
        drawMesh(item.mesh, item.transform);
    }
}
```

## Performance Targets

**For a playable game:**
- **60 FPS minimum** (16.6ms per frame budget)
- **Chunk load time**: <50ms per chunk (ideally <10ms)
- **Render distance**: 10-20 chunks in each direction
- **Max visible chunks**: ~400-1600 chunks
- **Vertices per frame**: 1M - 10M (modern GPUs can handle this easily)

## Memory Considerations

**Typical memory layout:**
- **Static geometry**: 10-100 MB (buildings, props)
- **Dynamic geometry**: 1-10 MB (animated characters)
- **Terrain chunks**: 50-500 MB (depends on render distance)
- **Textures**: 100MB - 2GB (usually the largest)

**Streaming strategy:**
- Keep frequently used assets in memory
- Stream in/out based on player proximity
- Use compressed formats on disk, decompress to GPU

## Vulkan-Specific Optimizations

1. **Descriptor Sets**: Group resources by update frequency
   - Set 0: Per-frame (camera matrices)
   - Set 1: Per-material (textures, properties)
   - Set 2: Per-object (model matrix)

2. **Command Buffer Management**:
   - Pre-record static geometry commands
   - Re-record only when scene changes
   - Use secondary command buffers for parallel recording

3. **Buffer Management**:
   - Ring buffer for dynamic data
   - Staging buffers for uploads
   - Device-local memory for static geometry

4. **Synchronization**:
   - Fences for CPU-GPU sync
   - Semaphores for GPU-GPU sync
   - Minimize pipeline stalls

## Next Steps for This Project

1. **Immediate**: Abstract Mesh class (wrap vertex/index buffers)
2. **Short-term**: Add camera controls (WASD movement)
3. **Medium-term**: Implement chunking system for terrain
4. **Long-term**: Add procedural generation and LOD

## References & Learning

- **Unity/Unreal**: Study their component systems
- **Minecraft**: Excellent chunk streaming example
- **Doom (2016)**: idTech 6 engine talks (great Vulkan info)
- **Our Machinery blog**: Data-oriented design patterns
- **Arseny Kapoulkine's blog**: Performance optimization
