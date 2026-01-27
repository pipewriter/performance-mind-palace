# Marching Cubes for Obstacle Course Game

## What Is Marching Cubes?

Marching cubes is an algorithm that generates a **smooth triangle mesh** from a **3D volume** (scalar field).

**The Basic Idea:**
```
Volume Data (Grid)          →  Marching Cubes  →  Smooth Mesh
┌─┬─┬─┬─┐                                         ╱╲
│1│1│0│0│  Density values                        ╱  ╲
├─┼─┼─┼─┤  at grid points    Algorithm          │    │
│1│1│0│0│  (0 = air,          extracts          │    │
├─┼─┼─┼─┤   1 = solid)        surface            ╲  ╱
│0│0│0│0│                                          ╲╱
└─┴─┴─┴─┘
```

**Key Properties:**
- Input: 3D grid of scalar values (density, signed distance, etc.)
- Output: Triangle mesh with smooth surfaces
- No blocky voxel look
- Procedurally generate smooth caves, terrain, organic shapes

## Yes, It Could Work Great!

### Why Marching Cubes is Good for Obstacle Courses

**Advantages:**
1. **Smooth geometry** - No blocky look, perfect for parkour
2. **Procedural generation** - Easy to generate interesting shapes with noise
3. **Memory efficient** - Volume data is more compact than mesh
4. **Deformable** - Can modify terrain dynamically (explosions, digging)
5. **Natural LOD** - Can run at different resolutions per chunk
6. **Organic shapes** - Great for caves, tunnels, flowing forms

**Disadvantages:**
1. **Runtime cost** - Must generate mesh from volume
2. **More complex** - More moving parts than direct geometry
3. **Collision complexity** - Need to derive collision from volume
4. **Seam artifacts** - Chunk boundaries need careful handling

### Games That Use This

- **Astroneer** - Deformable terrain, smooth surfaces
- **Teardown** - Voxel destruction with smooth rendering
- **Minecraft with shaders** - Smooth terrain generation
- **No Man's Sky** - Procedural planet generation

## Volume Data Structure

### Scalar Field Options

You have several choices for what values to store:

#### Option 1: Density Field (Simplest)
```cpp
struct VolumeChunk {
    static constexpr int RESOLUTION = 32;  // 32x32x32 grid
    float density[RESOLUTION][RESOLUTION][RESOLUTION];

    // density > 0.5 = solid
    // density < 0.5 = air
    // Marching cubes extracts surface at 0.5 threshold
};
```

#### Option 2: Signed Distance Field (Best for Collision)
```cpp
struct VolumeChunk {
    static constexpr int RESOLUTION = 32;
    float sdf[RESOLUTION][RESOLUTION][RESOLUTION];

    // sdf > 0 = inside solid
    // sdf < 0 = outside (air)
    // sdf = 0 = surface
    // Value magnitude = distance to nearest surface
};
```

**Signed Distance Field (SDF) is recommended** because:
- Easy collision detection (just sample the field)
- Better for smooth surfaces
- Can compute normals analytically
- Industry standard for this approach

### Memory Calculation

```cpp
// Per chunk:
const int RESOLUTION = 32;  // 32x32x32 grid
const int VOXELS = RESOLUTION * RESOLUTION * RESOLUTION;  // 32,768 voxels
const int BYTES_PER_VALUE = 4;  // float32

// Memory per chunk:
// 32,768 voxels × 4 bytes = 131KB per chunk

// For 16-bit half precision:
// 32,768 voxels × 2 bytes = 65KB per chunk

// This is MUCH smaller than storing triangle mesh directly
```

### Chunk Structure with Volume Data

```cpp
struct VolumeChunk {
    ChunkCoord coord;

    // Volume data (the source of truth)
    static constexpr int RESOLUTION = 32;
    float sdf[RESOLUTION][RESOLUTION][RESOLUTION];  // Signed distance field

    // Generated mesh (cached, can be regenerated)
    Mesh* renderMesh = nullptr;
    bool meshDirty = true;

    // Collision (derived from SDF or mesh)
    std::vector<Triangle> collisionMesh;
    BVHNode* collisionBVH = nullptr;
    bool collisionDirty = true;

    // Metadata
    bool isLoaded = false;
    AABB bounds;
};
```

## Marching Cubes Algorithm

### Basic Implementation

```cpp
struct MarchingCubes {
    static constexpr float ISO_LEVEL = 0.0f;  // Surface at SDF = 0

    // Lookup tables (standard marching cubes tables)
    static const int edgeTable[256];
    static const int triTable[256][16];

    std::vector<Vertex> generateMesh(const VolumeChunk& chunk) {
        std::vector<Vertex> vertices;

        // For each cube in the volume
        for (int x = 0; x < chunk.RESOLUTION - 1; x++) {
            for (int y = 0; y < chunk.RESOLUTION - 1; y++) {
                for (int z = 0; z < chunk.RESOLUTION - 1; z++) {
                    processCube(chunk, x, y, z, vertices);
                }
            }
        }

        return vertices;
    }

    void processCube(const VolumeChunk& chunk, int x, int y, int z,
                    std::vector<Vertex>& vertices) {
        // Get 8 corner values
        float corners[8];
        corners[0] = chunk.sdf[x][y][z];
        corners[1] = chunk.sdf[x+1][y][z];
        corners[2] = chunk.sdf[x+1][y][z+1];
        corners[3] = chunk.sdf[x][y][z+1];
        corners[4] = chunk.sdf[x][y+1][z];
        corners[5] = chunk.sdf[x+1][y+1][z];
        corners[6] = chunk.sdf[x+1][y+1][z+1];
        corners[7] = chunk.sdf[x][y+1][z+1];

        // Calculate cube index (which corners are inside surface)
        int cubeIndex = 0;
        for (int i = 0; i < 8; i++) {
            if (corners[i] > ISO_LEVEL) {
                cubeIndex |= (1 << i);
            }
        }

        // Cube is entirely inside or outside surface
        if (edgeTable[cubeIndex] == 0) {
            return;
        }

        // Find intersection points on edges
        glm::vec3 edgePoints[12];
        if (edgeTable[cubeIndex] & 1)    edgePoints[0]  = interpolate(x, y, z, 0, corners);
        if (edgeTable[cubeIndex] & 2)    edgePoints[1]  = interpolate(x, y, z, 1, corners);
        // ... for all 12 edges

        // Create triangles using lookup table
        for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
            Vertex v1, v2, v3;
            v1.pos = edgePoints[triTable[cubeIndex][i]];
            v2.pos = edgePoints[triTable[cubeIndex][i+1]];
            v3.pos = edgePoints[triTable[cubeIndex][i+2]];

            // Calculate normals from SDF gradient
            v1.normal = calculateNormal(chunk, v1.pos);
            v2.normal = calculateNormal(chunk, v2.pos);
            v3.normal = calculateNormal(chunk, v3.pos);

            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);
        }
    }

    glm::vec3 interpolate(int x, int y, int z, int edge, const float corners[8]) {
        // Linear interpolation between two corners along edge
        // This is where the smoothness comes from!

        // Edge defines which two corners
        static const int edgeVertices[12][2] = {
            {0,1}, {1,2}, {2,3}, {3,0},  // Bottom face
            {4,5}, {5,6}, {6,7}, {7,4},  // Top face
            {0,4}, {1,5}, {2,6}, {3,7}   // Vertical edges
        };

        int v0 = edgeVertices[edge][0];
        int v1 = edgeVertices[edge][1];

        float val0 = corners[v0];
        float val1 = corners[v1];

        // Linear interpolation factor
        float t = (ISO_LEVEL - val0) / (val1 - val0);
        t = glm::clamp(t, 0.0f, 1.0f);

        // Get positions of the two corners
        glm::vec3 p0 = getCornerPosition(v0, x, y, z);
        glm::vec3 p1 = getCornerPosition(v1, x, y, z);

        // Interpolate position
        return glm::mix(p0, p1, t);
    }

    glm::vec3 calculateNormal(const VolumeChunk& chunk, glm::vec3 pos) {
        // Calculate gradient of SDF = surface normal
        const float h = 0.01f;

        float dx = sampleSDF(chunk, pos + glm::vec3(h, 0, 0)) -
                   sampleSDF(chunk, pos - glm::vec3(h, 0, 0));
        float dy = sampleSDF(chunk, pos + glm::vec3(0, h, 0)) -
                   sampleSDF(chunk, pos - glm::vec3(0, h, 0));
        float dz = sampleSDF(chunk, pos + glm::vec3(0, 0, h)) -
                   sampleSDF(chunk, pos - glm::vec3(0, 0, h));

        return glm::normalize(glm::vec3(dx, dy, dz));
    }
};
```

## Procedural Generation with Noise

### Creating Terrain with Noise Functions

```cpp
#include <FastNoiseLite.h>  // Popular noise library

void generateChunkVolume(VolumeChunk& chunk, glm::vec3 worldOffset) {
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.02f);

    for (int x = 0; x < chunk.RESOLUTION; x++) {
        for (int y = 0; y < chunk.RESOLUTION; y++) {
            for (int z = 0; z < chunk.RESOLUTION; z++) {
                // World position of this voxel
                glm::vec3 worldPos = worldOffset + glm::vec3(x, y, z);

                // Generate SDF value using noise
                float density = generateDensity(noise, worldPos);

                chunk.sdf[x][y][z] = density;
            }
        }
    }

    chunk.meshDirty = true;
    chunk.collisionDirty = true;
}

float generateDensity(FastNoiseLite& noise, glm::vec3 worldPos) {
    // Example: Hilly terrain with caves

    // Base terrain height
    float terrainHeight = noise.GetNoise(worldPos.x, worldPos.z) * 20.0f;

    // Distance from terrain surface (negative = above ground, positive = below)
    float sdf = worldPos.y - terrainHeight;

    // Add caves using 3D noise
    float caveNoise = noise.GetNoise(worldPos.x * 0.05f,
                                      worldPos.y * 0.05f,
                                      worldPos.z * 0.05f);
    if (caveNoise > 0.5f) {
        sdf = -1.0f;  // Carve out cave
    }

    return sdf;
}
```

### Creating Obstacles Procedurally

```cpp
void addPlatform(VolumeChunk& chunk, glm::vec3 center, glm::vec3 size) {
    for (int x = 0; x < chunk.RESOLUTION; x++) {
        for (int y = 0; y < chunk.RESOLUTION; y++) {
            for (int z = 0; z < chunk.RESOLUTION; z++) {
                glm::vec3 pos = glm::vec3(x, y, z);

                // Signed distance to box (platform)
                glm::vec3 diff = glm::abs(pos - center) - size * 0.5f;
                float dist = glm::length(glm::max(diff, glm::vec3(0.0f))) +
                            glm::min(glm::max(diff.x, glm::max(diff.y, diff.z)), 0.0f);

                // Combine with existing SDF (CSG union = min)
                chunk.sdf[x][y][z] = glm::min(chunk.sdf[x][y][z], dist);
            }
        }
    }
}

void addSphere(VolumeChunk& chunk, glm::vec3 center, float radius) {
    for (int x = 0; x < chunk.RESOLUTION; x++) {
        for (int y = 0; y < chunk.RESOLUTION; y++) {
            for (int z = 0; z < chunk.RESOLUTION; z++) {
                glm::vec3 pos = glm::vec3(x, y, z);
                float dist = glm::distance(pos, center) - radius;

                // CSG union
                chunk.sdf[x][y][z] = glm::min(chunk.sdf[x][y][z], dist);
            }
        }
    }
}

// You can build complex shapes with CSG operations:
// - Union: min(sdf1, sdf2)
// - Subtraction: max(sdf1, -sdf2)
// - Intersection: max(sdf1, sdf2)
```

## Collision Detection with Marching Cubes

You have several options:

### Option 1: Collide with Generated Mesh

Use the triangle mesh output from marching cubes:

```cpp
struct VolumeChunk {
    std::vector<Triangle> collisionMesh;
    BVHNode* collisionBVH = nullptr;

    void generateCollision() {
        // Run marching cubes at lower resolution for collision
        MarchingCubes mc;
        auto vertices = mc.generateMesh(*this);

        // Convert to triangles
        collisionMesh.clear();
        for (size_t i = 0; i < vertices.size(); i += 3) {
            Triangle tri;
            tri.v0 = vertices[i].pos;
            tri.v1 = vertices[i+1].pos;
            tri.v2 = vertices[i+2].pos;
            collisionMesh.push_back(tri);
        }

        // Build BVH for fast queries
        collisionBVH = buildBVH(collisionMesh);
        collisionDirty = false;
    }
};

bool checkCollision(const Capsule& capsule, const VolumeChunk& chunk) {
    // Query BVH for nearby triangles
    return queryBVH(chunk.collisionBVH, capsule);
}
```

**Pros:**
- Standard collision detection (capsule vs triangles)
- Can use existing algorithms
- Fast with BVH

**Cons:**
- Mesh can be complex (thousands of triangles)
- Must regenerate when volume changes

### Option 2: Direct SDF Collision (Recommended)

Sample the SDF directly for collision detection:

```cpp
float sampleSDF(const VolumeChunk& chunk, glm::vec3 worldPos) {
    // Convert world position to chunk-local grid coordinates
    glm::vec3 localPos = worldPosToChunkLocal(worldPos, chunk.coord);

    // Trilinear interpolation of SDF values
    return trilinearInterpolate(chunk.sdf, localPos);
}

bool checkCapsuleCollision(const Capsule& capsule, const VolumeChunk& chunk,
                          CollisionInfo& info) {
    // Sample SDF at capsule position
    float distToSurface = sampleSDF(chunk, capsule.center);

    if (distToSurface < capsule.radius) {
        // Collision! Calculate response

        // Normal = gradient of SDF
        info.normal = calculateNormalFromSDF(chunk, capsule.center);
        info.penetration = capsule.radius - distToSurface;
        info.collided = true;
        return true;
    }

    return false;
}

glm::vec3 calculateNormalFromSDF(const VolumeChunk& chunk, glm::vec3 pos) {
    const float h = 0.01f;

    float dx = sampleSDF(chunk, pos + glm::vec3(h, 0, 0)) -
               sampleSDF(chunk, pos - glm::vec3(h, 0, 0));
    float dy = sampleSDF(chunk, pos + glm::vec3(0, h, 0)) -
               sampleSDF(chunk, pos - glm::vec3(0, h, 0));
    float dz = sampleSDF(chunk, pos + glm::vec3(0, 0, h)) -
               sampleSDF(chunk, pos - glm::vec3(0, 0, h));

    return glm::normalize(glm::vec3(dx, dy, dz));
}
```

**Pros:**
- Super fast (just sampling a 3D array)
- Works directly with source data
- Normals come for free (gradient)
- Perfect accuracy

**Cons:**
- Need good interpolation
- Must sample SDF multiple times for capsule/sphere

**This is the best approach for marching cubes collision!**

### Option 3: Simplified Collision Mesh

Generate coarse collision mesh at lower resolution:

```cpp
void generateSimplifiedCollision(VolumeChunk& chunk) {
    // Run marching cubes at 1/4 resolution for collision
    const int COLLISION_RES = chunk.RESOLUTION / 4;

    VolumeChunk lowRes;
    lowRes.RESOLUTION = COLLISION_RES;

    // Downsample SDF
    for (int x = 0; x < COLLISION_RES; x++) {
        for (int y = 0; y < COLLISION_RES; y++) {
            for (int z = 0; z < COLLISION_RES; z++) {
                // Sample from higher resolution
                int srcX = x * 4;
                int srcY = y * 4;
                int srcZ = z * 4;
                lowRes.sdf[x][y][z] = chunk.sdf[srcX][srcY][srcZ];
            }
        }
    }

    // Generate low-poly mesh for collision
    MarchingCubes mc;
    auto vertices = mc.generateMesh(lowRes);

    // Build collision BVH from low-poly mesh
    // ...
}
```

## Handling Chunk Boundaries

### The Seam Problem

When chunks generate independently, you get cracks at boundaries:

```
Chunk A    Boundary     Chunk B
┌──────┐     │      ┌──────┐
│  ╱╲  │     │      │  ╱╲  │
│ ╱  ╲ │     ╳      │ ╱  ╲ │  <- GAP!
│╱    ╲│     │      │╱    ╲│
└──────┘     │      └──────┘
```

### Solution: Share Boundary Voxels

```cpp
struct VolumeChunk {
    // Add padding for neighbors
    static constexpr int RESOLUTION = 32;
    static constexpr int PADDED_RESOLUTION = RESOLUTION + 1;  // +1 for overlap

    float sdf[PADDED_RESOLUTION][PADDED_RESOLUTION][PADDED_RESOLUTION];
};

void generateChunkWithBoundaries(VolumeChunk& chunk, World& world) {
    // Generate this chunk's interior
    for (int x = 0; x < chunk.RESOLUTION; x++) {
        for (int y = 0; y < chunk.RESOLUTION; y++) {
            for (int z = 0; z < chunk.RESOLUTION; z++) {
                chunk.sdf[x][y][z] = generateDensity(worldPos(x, y, z));
            }
        }
    }

    // Copy boundary values from neighbors
    ChunkCoord right = {chunk.coord.x + 1, chunk.coord.y, chunk.coord.z};
    if (auto* neighbor = world.getChunk(right)) {
        for (int y = 0; y < chunk.RESOLUTION; y++) {
            for (int z = 0; z < chunk.RESOLUTION; z++) {
                chunk.sdf[chunk.RESOLUTION][y][z] = neighbor->sdf[0][y][z];
            }
        }
    }

    // Repeat for all 6 neighbors...
}
```

## Performance Considerations

### Asynchronous Mesh Generation

```cpp
class ChunkManager {
    std::thread workerThread;
    std::queue<VolumeChunk*> meshGenQueue;
    std::mutex queueMutex;

    void workerFunction() {
        while (running) {
            VolumeChunk* chunk = nullptr;

            {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (!meshGenQueue.empty()) {
                    chunk = meshGenQueue.front();
                    meshGenQueue.pop();
                }
            }

            if (chunk) {
                // Generate mesh on worker thread
                MarchingCubes mc;
                auto vertices = mc.generateMesh(*chunk);

                // Upload to GPU on main thread
                chunk->pendingVertices = vertices;
                chunk->readyForUpload = true;
            }
        }
    }

    void updateOnMainThread() {
        for (auto& [coord, chunk] : chunks) {
            if (chunk->readyForUpload) {
                // Upload mesh to GPU
                chunk->renderMesh = createMesh(chunk->pendingVertices);
                chunk->meshDirty = false;
                chunk->readyForUpload = false;
            }
        }
    }
};
```

### LOD (Level of Detail)

```cpp
void generateChunkAtLOD(VolumeChunk& chunk, int lodLevel) {
    // LOD 0 = full resolution (32x32x32)
    // LOD 1 = half resolution (16x16x16)
    // LOD 2 = quarter resolution (8x8x8)

    int resolution = chunk.RESOLUTION >> lodLevel;  // Divide by 2^lodLevel

    MarchingCubes mc;
    auto vertices = mc.generateMeshAtResolution(chunk, resolution);

    chunk->renderMesh = createMesh(vertices);
}

int chooseLOD(float distanceToPlayer) {
    if (distanceToPlayer < 50.0f) return 0;  // Full detail
    if (distanceToPlayer < 100.0f) return 1; // Half detail
    return 2;                                 // Quarter detail
}
```

### Caching

```cpp
struct VolumeChunk {
    // Cache the mesh to avoid regenerating every frame
    Mesh* renderMesh = nullptr;
    bool meshDirty = true;

    void modifyVoxel(int x, int y, int z, float value) {
        sdf[x][y][z] = value;
        meshDirty = true;  // Need to regenerate mesh
        collisionDirty = true;

        // Also mark neighbors dirty if on boundary
        if (x == 0 || x == RESOLUTION - 1 ||
            y == 0 || y == RESOLUTION - 1 ||
            z == 0 || z == RESOLUTION - 1) {
            markNeighborsDirty();
        }
    }
};
```

## When to Use Marching Cubes

### ✅ Use Marching Cubes If:

1. **You want smooth organic shapes** (caves, flowing terrain)
2. **You want deformable terrain** (mining, explosions)
3. **You're doing procedural generation** (noise-based worlds)
4. **You want efficient LOD**
5. **Memory is a concern** (volume data is compact)
6. **You're okay with added complexity**

### ❌ Don't Use Marching Cubes If:

1. **You want simple box/plane obstacles** (direct geometry is easier)
2. **You need pixel-perfect precision** (voxel is better)
3. **You want hand-crafted geometry** (use modeling tools + direct mesh)
4. **Performance is critical** (mesh generation has cost)
5. **Your game is simple** (added complexity not worth it)

## Hybrid Approach (Best of Both Worlds)

You can combine both approaches:

```cpp
struct Chunk {
    // Terrain uses marching cubes
    VolumeChunk terrain;
    Mesh* terrainMesh = nullptr;

    // Hand-placed obstacles use direct geometry
    std::vector<AABB> obstacles;
    std::vector<Ramp> ramps;

    void render() {
        // Render terrain from marching cubes
        renderMesh(terrainMesh);

        // Render obstacles directly
        for (auto& obstacle : obstacles) {
            renderBox(obstacle);
        }
    }

    void checkCollision(const Capsule& capsule) {
        // Check terrain using SDF
        if (checkSDFCollision(capsule, terrain)) {
            // ...
        }

        // Check obstacles using AABB tests
        for (auto& obstacle : obstacles) {
            if (checkCapsuleAABB(capsule, obstacle)) {
                // ...
            }
        }
    }
};
```

**This is often the best approach:**
- Terrain/background = marching cubes (smooth, organic)
- Platforms/obstacles = direct geometry (precise, hand-crafted)

## Practical Recommendation

For your first implementation:

**Phase 1: Start Simple**
- Direct geometry (AABB platforms, planes for floors)
- Get movement and collision working
- Build a few test levels

**Phase 2: Add Marching Cubes Later**
- Add for terrain/background only
- Keep obstacles as direct geometry
- Use SDF for terrain collision

**Phase 3: Full Integration**
- Procedural generation with noise
- Deformable terrain
- Full marching cubes pipeline

## Performance Targets

- **Mesh generation**: 5-20ms per chunk (do async!)
- **SDF sample**: <0.001ms (very fast)
- **Volume memory**: 65-130KB per chunk
- **Generated mesh**: 1K-10K triangles per chunk
- **Collision mesh**: 100-1K triangles per chunk (simplified)

## Example: Complete Pipeline

```cpp
class ChunkManager {
    void loadChunk(ChunkCoord coord) {
        VolumeChunk* chunk = new VolumeChunk();
        chunk->coord = coord;

        // 1. Generate volume data (fast, on main thread)
        generateChunkVolume(*chunk, chunkToWorldPos(coord));

        // 2. Queue mesh generation (slow, on worker thread)
        queueMeshGeneration(chunk);

        // 3. Collision is ready immediately (sample SDF directly)
        chunk->collisionReady = true;

        chunks[coord] = chunk;
    }

    void update(Player& player) {
        // Find chunks near player
        ChunkCoord playerChunk = worldPosToChunk(player.position);

        // Get collision candidates
        std::vector<VolumeChunk*> nearbyChunks;
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    ChunkCoord coord = {
                        playerChunk.x + dx,
                        playerChunk.y + dy,
                        playerChunk.z + dz
                    };
                    if (auto* chunk = getChunk(coord)) {
                        nearbyChunks.push_back(chunk);
                    }
                }
            }
        }

        // Check collision using SDF
        for (auto* chunk : nearbyChunks) {
            checkCapsuleSDFCollision(player.capsule, *chunk);
        }
    }
};
```

## Summary

**Yes, marching cubes can work great for an obstacle course game!**

**The approach:**
1. Store **SDF** (signed distance field) in chunks - 65-130KB per chunk
2. Generate **smooth mesh** with marching cubes - run async
3. Collide directly with **SDF** - super fast, accurate
4. Handle boundaries by **sharing edge voxels**

**Advantages:**
- Smooth, flowing geometry (perfect for parkour)
- Memory efficient
- Natural LOD
- Can do procedural generation easily
- Optional: deformable terrain

**When to use it:**
- Organic/flowing obstacle courses
- Procedural generation
- You want the option for deformation
- You're okay with added complexity

**When not to use it:**
- Simple box/plane obstacles (direct geometry is easier)
- Hand-crafted precise levels (use modeling tools)
- You're starting out (add later once basics work)

The hybrid approach (marching cubes for terrain + direct geometry for obstacles) is often the sweet spot!
