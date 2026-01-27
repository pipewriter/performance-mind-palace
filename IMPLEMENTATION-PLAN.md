# Marching Cubes Implementation Plan

## Current State
- ✅ Vulkan rendering pipeline working
- ✅ 3D cube with depth testing
- ✅ Camera transformation (model-view-projection)
- ✅ Dynamic viewport
- ✅ Vertex and index buffers
- ✅ Frame timing and FPS counter

## Goal
Build a marching cubes terrain renderer with collision detection and chunking system.

## Architecture Overview

```
World
├─ ChunkManager
│  ├─ Chunk[x,y,z] - Contains SDF volume data
│  ├─ Generate volumes from noise
│  └─ Stream chunks based on camera position
│
├─ MarchingCubes
│  ├─ Take SDF volume → Generate mesh
│  ├─ Lookup tables (edge table, tri table)
│  └─ Vertex interpolation
│
├─ Renderer
│  ├─ Upload generated meshes to GPU
│  ├─ Draw all visible chunks
│  └─ Dynamic mesh updates
│
└─ Camera/Player
   ├─ Free-flying camera (WASD + mouse)
   ├─ Collision detection against SDF
   └─ Movement with sliding
```

## Implementation Steps

### Phase 1: Foundation (Camera & Noise)
**Goal:** Get basic camera controls and noise generation working

#### Step 1.1: Add Camera Controls
- [ ] Add GLFW input callbacks for keyboard and mouse
- [ ] Implement free-flying camera (WASD movement, mouse look)
- [ ] Update camera view matrix from input
- [ ] Remove spinning cube, use static camera-controlled view

**Files to modify:**
- `src/main.cpp` - Add input handling
- `src/camera.h/cpp` - New files for camera class

**What this gives us:** Ability to move around the world

---

#### Step 1.2: Add Noise Library
- [ ] Add FastNoiseLite as dependency (single header)
- [ ] Create simple noise sampling function
- [ ] Test by printing noise values

**Files:**
- `src/noise.h` - Wrapper for noise generation
- Update `CMakeLists.txt` if needed

**What this gives us:** Procedural data generation

---

### Phase 2: Volume Data & Chunks
**Goal:** Create chunk system with SDF volume data

#### Step 2.1: Define Data Structures
- [ ] Create `ChunkCoord` struct (x, y, z)
- [ ] Create `VolumeChunk` struct with SDF array
- [ ] Define chunk size constants (32x32x32 voxels, 16m cube)

**Files:**
- `src/chunk.h` - Chunk data structures
- `src/types.h` - Common types (ChunkCoord, etc.)

**What this gives us:** Container for volume data

---

#### Step 2.2: Generate SDF from Noise
- [ ] Implement SDF generation function
- [ ] Use 3D Perlin noise for terrain
- [ ] Simple ground plane at y=0 for testing
- [ ] Function: `generateChunkVolume(VolumeChunk& chunk, ChunkCoord coord)`

**Files:**
- `src/volume_generator.cpp/h` - Volume generation logic

**What this gives us:** Procedural terrain data

---

#### Step 2.3: Basic Chunk Manager
- [ ] Create `ChunkManager` class
- [ ] Store chunks in `unordered_map<ChunkCoord, VolumeChunk>`
- [ ] Implement `getChunk(ChunkCoord)` and `createChunk(ChunkCoord)`
- [ ] Generate single chunk at origin for testing

**Files:**
- `src/chunk_manager.cpp/h` - Chunk management

**What this gives us:** Container for multiple chunks

---

### Phase 3: Marching Cubes Algorithm
**Goal:** Generate triangle mesh from SDF volume

#### Step 3.1: Marching Cubes Tables
- [ ] Add edge table (256 entries)
- [ ] Add triangle table (256 × 16 entries)
- [ ] These are standard lookup tables (can copy from references)

**Files:**
- `src/marching_cubes_tables.h` - Lookup tables

**What this gives us:** Core data for marching cubes

---

#### Step 3.2: Implement Algorithm
- [ ] Create `MarchingCubes` class
- [ ] Implement `generateMesh(VolumeChunk&)` function
- [ ] For each 2×2×2 cube of voxels:
  - Calculate cube index (which corners are solid)
  - Interpolate edge intersections
  - Generate triangles from lookup table
- [ ] Output: `std::vector<Vertex>` with positions and normals

**Files:**
- `src/marching_cubes.cpp/h` - Algorithm implementation

**Functions needed:**
```cpp
std::vector<Vertex> generateMesh(const VolumeChunk& chunk);
glm::vec3 interpolateEdge(float v1, float v2, glm::vec3 p1, glm::vec3 p2);
glm::vec3 calculateNormal(const VolumeChunk& chunk, glm::vec3 pos);
```

**What this gives us:** Mesh generation from volume

---

#### Step 3.3: Test Mesh Generation
- [ ] Generate mesh for single chunk
- [ ] Print vertex count
- [ ] Verify mesh has reasonable triangle count
- [ ] Debug visualization: print some vertex positions

**What this gives us:** Validation that algorithm works

---

### Phase 4: Rendering Integration
**Goal:** Render marching cubes mesh with Vulkan

#### Step 4.1: Dynamic Mesh Uploads
- [ ] Modify `VolumeChunk` to store `VkBuffer` handles
- [ ] Create function to upload mesh to GPU
- [ ] Handle dynamic buffer creation/destruction
- [ ] Store vertex count for draw calls

**Files to modify:**
- `src/chunk.h` - Add Vulkan buffer members
- `src/main.cpp` - Add buffer management functions

**What this gives us:** Ability to upload generated meshes

---

#### Step 4.2: Render Single Chunk
- [ ] Generate mesh for chunk at origin
- [ ] Upload to GPU
- [ ] Draw in render loop
- [ ] Remove old cube rendering code

**Files to modify:**
- `src/main.cpp` - Update render commands

**What this gives us:** First marching cubes terrain visible!

---

#### Step 4.3: Render Multiple Chunks
- [ ] Generate 3×3×3 chunks around origin
- [ ] Upload all meshes
- [ ] Draw all chunks in render loop
- [ ] Handle chunk boundaries (share edge voxels)

**What this gives us:** Larger terrain area

---

### Phase 5: Chunk Streaming
**Goal:** Load/unload chunks based on camera position

#### Step 5.1: Distance-Based Loading
- [ ] Calculate camera chunk position
- [ ] Load chunks within radius (e.g., 5 chunks)
- [ ] Store loaded chunks in manager
- [ ] Update every frame

**Files to modify:**
- `src/chunk_manager.cpp/h` - Add streaming logic

**What this gives us:** Infinite terrain streaming

---

#### Step 5.2: Unload Distant Chunks
- [ ] Check chunk distance from camera
- [ ] Unload chunks beyond render distance
- [ ] Free GPU buffers for unloaded chunks
- [ ] Prevent memory leaks

**What this gives us:** Bounded memory usage

---

#### Step 5.3: Asynchronous Generation (Optional)
- [ ] Move mesh generation to worker thread
- [ ] Queue chunks for generation
- [ ] Upload on main thread when ready
- [ ] Prevent frame hitches

**What this gives us:** Smooth performance while generating

---

### Phase 6: Collision Detection
**Goal:** Prevent camera from going through terrain

#### Step 6.1: SDF Sampling
- [ ] Implement `sampleSDF(VolumeChunk, worldPos)` function
- [ ] Trilinear interpolation between voxels
- [ ] Handle chunk boundaries (query neighbor chunks)

**Files:**
- `src/collision.cpp/h` - Collision detection

**What this gives us:** Distance to terrain at any point

---

#### Step 6.2: Sphere Collision
- [ ] Treat camera as sphere (radius 0.5m)
- [ ] Sample SDF at camera position
- [ ] If distance < radius: collision detected
- [ ] Calculate surface normal from SDF gradient

**What this gives us:** Basic collision detection

---

#### Step 6.3: Collision Response
- [ ] When collision detected, push camera out
- [ ] Direction: along surface normal
- [ ] Amount: penetration depth
- [ ] Implement velocity sliding (like in collision plan)

**Files to modify:**
- `src/camera.cpp` - Add collision to movement

**What this gives us:** Solid terrain you can't pass through

---

### Phase 7: Polish & Optimization
**Goal:** Make it look good and run fast

#### Step 7.1: Lighting
- [ ] Calculate proper normals from SDF gradient
- [ ] Add simple directional light in shader
- [ ] Pass light direction as uniform
- [ ] Basic diffuse shading

**Files to modify:**
- `shaders/shader.vert` - Pass normals
- `shaders/shader.frag` - Simple lighting

**What this gives us:** 3D depth perception

---

#### Step 7.2: Texturing/Colors
- [ ] Add height-based coloring (grass, rock, snow)
- [ ] Or simple ambient occlusion from SDF
- [ ] Make terrain visually interesting

**What this gives us:** Better visual appeal

---

#### Step 7.3: Performance Optimization
- [ ] Profile mesh generation time
- [ ] Add per-chunk BVH if needed
- [ ] Frustum culling for chunks
- [ ] LOD if necessary

**What this gives us:** Smooth 60fps

---

## Detailed First Steps (Phase 1.1)

Let's start with camera controls since that's the foundation:

### Camera Control Implementation

```cpp
// src/camera.h
class Camera {
public:
    glm::vec3 position;
    float pitch;  // Up/down rotation
    float yaw;    // Left/right rotation

    glm::mat4 getViewMatrix() const;
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;

    void processInput(GLFWwindow* window, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset);
};
```

### Input Callbacks

```cpp
// In main.cpp
static void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    // Update camera rotation
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Handle key presses
}

// In initWindow():
glfwSetCursorPosCallback(window, mouseCallback);
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
```

## File Structure After Implementation

```
vulkan-cpp-mind-palace/
├── src/
│   ├── main.cpp              (Modified - integrate everything)
│   ├── camera.h/cpp          (New - camera controls)
│   ├── chunk.h               (New - chunk data structures)
│   ├── chunk_manager.h/cpp   (New - chunk loading/streaming)
│   ├── volume_generator.h/cpp (New - SDF generation)
│   ├── marching_cubes.h/cpp  (New - mesh generation algorithm)
│   ├── marching_cubes_tables.h (New - lookup tables)
│   ├── collision.h/cpp       (New - collision detection)
│   └── noise.h               (New - noise wrapper)
├── shaders/
│   ├── shader.vert           (Modified - lighting)
│   └── shader.frag           (Modified - lighting)
└── build/
```

## Testing Strategy

After each phase:
1. **Visual test** - Does it look right?
2. **Print debug info** - Vertex counts, chunk positions, etc.
3. **Performance check** - FPS still good?
4. **Incremental commits** - One feature at a time

## Success Criteria

After all phases complete:
- [ ] Can fly around infinite terrain with WASD + mouse
- [ ] Terrain generates procedurally from noise
- [ ] Chunks load/unload as you move
- [ ] Collision prevents going through terrain
- [ ] Runs at 60fps
- [ ] Terrain looks smooth (not blocky)

## Estimated Timeline

- **Phase 1** (Camera): 30-60 minutes
- **Phase 2** (Chunks): 1-2 hours
- **Phase 3** (Marching Cubes): 2-3 hours
- **Phase 4** (Rendering): 1-2 hours
- **Phase 5** (Streaming): 1 hour
- **Phase 6** (Collision): 1-2 hours
- **Phase 7** (Polish): 1-2 hours

**Total: 8-15 hours** of implementation time

## Dependencies to Add

```cmake
# CMakeLists.txt additions:

# FastNoiseLite (header-only)
FetchContent_Declare(
    FastNoiseLite
    GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
    GIT_TAG v1.1.1
)
FetchContent_MakeAvailable(FastNoiseLite)

target_include_directories(vulkan_app PRIVATE
    ${FastNoiseLite_SOURCE_DIR}/Cpp
)
```

## Development Tips

1. **Start simple** - Single chunk at origin first
2. **Visualize SDF** - Print values to verify generation
3. **Debug normals** - Render as colors to verify they're correct
4. **Test edge cases** - Chunk boundaries are tricky
5. **Profile early** - Marching cubes can be slow
6. **Commit often** - Each phase is a good commit point

## Common Pitfalls to Avoid

1. **Chunk boundary seams** - Share edge voxels between chunks
2. **Inverted normals** - Marching cubes can flip normals
3. **Memory leaks** - Remember to destroy Vulkan buffers
4. **Slow generation** - Generate meshes asynchronously
5. **Incorrect SDF signs** - Positive = inside, negative = outside
6. **Z-fighting** - Ensure chunks don't overlap in rendering

## Next Step

Let's start with **Phase 1.1: Camera Controls**

I'll help you:
1. Create `src/camera.h/cpp` files
2. Add input callbacks to `main.cpp`
3. Replace spinning cube with camera-controlled view
4. Test flying around with WASD + mouse

Ready to begin?
