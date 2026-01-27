# Chunks and Collision Detection Integration

## The Core Question: Voxel or Not?

### Non-Voxel (Smooth Geometry) - RECOMMENDED for Obstacle Course

**Pros:**
- Smooth curved surfaces (ramps, pipes, half-pipes)
- Natural flow for parkour movement
- Artistic freedom (not locked to grid)
- Better for speed-based gameplay
- Can still use chunks for streaming

**Cons:**
- Geometry can span chunk boundaries
- Slightly more complex collision queries
- Need to handle cross-chunk objects

**Best for:** Parkour, racing, obstacle courses, Mirror's Edge style

### Voxel (Grid-Based)

**Pros:**
- Trivial chunk boundaries (everything snaps to grid)
- Easy to ensure nothing crosses chunks
- Simple collision detection
- Easy to modify terrain (destructible)

**Cons:**
- Everything looks blocky (unless you do marching cubes)
- Awkward for smooth movement
- Harder to make flowing obstacle courses
- More restrictive art style

**Best for:** Minecraft-style, building games, destructible terrain

## Recommendation: **Non-Voxel with Smart Chunking**

You want smooth geometry for good parkour feel, but you can still use chunks effectively.

## Chunk Structure with Collision

### Horizontal + Vertical Chunking

```cpp
const int CHUNK_SIZE = 32;          // 32x32 meters horizontal
const int CHUNK_HEIGHT = 16;        // 16 meters vertical
const int WORLD_HEIGHT_CHUNKS = 16; // 256 meters total height

struct ChunkCoord {
    int x, y, z;  // y is vertical

    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

// Hash function for unordered_map
namespace std {
    template<>
    struct hash<ChunkCoord> {
        size_t operator()(const ChunkCoord& c) const {
            return hash<int>()(c.x) ^ (hash<int>()(c.y) << 1) ^ (hash<int>()(c.z) << 2);
        }
    };
}
```

### Chunk with Collision Data

```cpp
struct Chunk {
    ChunkCoord coord;

    // Rendering data
    Mesh* mesh = nullptr;
    bool meshDirty = false;

    // Collision data
    std::vector<AABB> obstacles;        // Static geometry in this chunk
    std::vector<Plane> floors;          // Floor surfaces
    std::vector<CollisionPrimitive*> primitives;  // Mixed types

    // Spatial partitioning within chunk (optional)
    SpatialGrid* localGrid = nullptr;

    // Bounding volume for entire chunk
    AABB bounds;

    // State
    bool isLoaded = false;
    bool collisionReady = false;
};
```

## Handling Cross-Chunk Geometry

### Problem: What if a wall spans multiple chunks?

```
Chunk A          Chunk B
┌───────┬────────┐
│       │        │
│   ┌───┼────┐   │  <- This wall crosses boundary!
│   │   │    │   │
│   └───┼────┘   │
│       │        │
└───────┴────────┘
```

### Solution 1: Duplicate Boundary Geometry (Simplest)

Store geometry in **all chunks it touches**:

```cpp
void addObstacle(World& world, AABB obstacle) {
    // Find all chunks this obstacle overlaps
    ChunkCoord minChunk = worldPosToChunk(obstacle.min);
    ChunkCoord maxChunk = worldPosToChunk(obstacle.max);

    for (int x = minChunk.x; x <= maxChunk.x; x++) {
        for (int y = minChunk.y; y <= maxChunk.y; y++) {
            for (int z = minChunk.z; z <= maxChunk.z; z++) {
                ChunkCoord coord = {x, y, z};
                Chunk* chunk = world.getChunk(coord);
                if (chunk) {
                    chunk->obstacles.push_back(obstacle);
                }
            }
        }
    }
}
```

**Pros:**
- Simple collision queries (just check current chunk + neighbors)
- No special cases
- Works with chunk loading/unloading

**Cons:**
- Uses more memory (geometry duplicated)
- Must update all copies if obstacle moves

**Best for:** Static geometry (which is 99% of obstacle courses)

### Solution 2: Boundary Object List

Keep a separate list of objects that span chunks:

```cpp
struct World {
    std::unordered_map<ChunkCoord, Chunk> chunks;
    std::vector<AABB> crossChunkObstacles;  // Objects that span chunks
};

std::vector<AABB> getCollisionGeometry(World& world, glm::vec3 position, float radius) {
    std::vector<AABB> results;

    // Get local chunk geometry
    ChunkCoord chunk = worldPosToChunk(position);
    if (auto* c = world.getChunk(chunk)) {
        results.insert(results.end(), c->obstacles.begin(), c->obstacles.end());
    }

    // Always check cross-chunk obstacles
    for (const auto& obstacle : world.crossChunkObstacles) {
        if (sphereIntersectsAABB(position, radius, obstacle)) {
            results.push_back(obstacle);
        }
    }

    return results;
}
```

**Pros:**
- No duplication
- Easy to modify

**Cons:**
- Must always check global list
- Harder to stream in/out

### Solution 3: Chunk Overlap Zones

Expand chunk bounds slightly for collision queries:

```cpp
const float CHUNK_OVERLAP = 2.0f;  // 2 meters of overlap

AABB getChunkCollisionBounds(ChunkCoord coord) {
    AABB bounds = getChunkBounds(coord);

    // Expand by overlap amount
    bounds.min -= glm::vec3(CHUNK_OVERLAP);
    bounds.max += glm::vec3(CHUNK_OVERLAP);

    return bounds;
}

std::vector<AABB> queryCollision(World& world, glm::vec3 position, float radius) {
    std::vector<AABB> results;

    // Query current chunk + all neighbors
    ChunkCoord center = worldPosToChunk(position);

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                ChunkCoord coord = {center.x + dx, center.y + dy, center.z + dz};
                Chunk* chunk = world.getChunk(coord);

                if (chunk && chunk->collisionReady) {
                    results.insert(results.end(),
                                 chunk->obstacles.begin(),
                                 chunk->obstacles.end());
                }
            }
        }
    }

    return results;
}
```

**Recommendation:** Use **Solution 1** (duplicate boundary geometry) for simplicity. Memory is cheap, correctness is priceless.

## Collision Query Strategy

### Always Query Neighboring Chunks

```cpp
std::vector<CollisionPrimitive*> getCollisionCandidates(
    const World& world,
    const glm::vec3& position,
    float radius)
{
    std::vector<CollisionPrimitive*> candidates;

    // Calculate which chunks the sphere overlaps
    ChunkCoord minChunk = worldPosToChunk(position - glm::vec3(radius));
    ChunkCoord maxChunk = worldPosToChunk(position + glm::vec3(radius));

    // Query all overlapping chunks
    for (int x = minChunk.x; x <= maxChunk.x; x++) {
        for (int y = minChunk.y; y <= maxChunk.y; y++) {
            for (int z = minChunk.z; z <= maxChunk.z; z++) {
                ChunkCoord coord = {x, y, z};
                const Chunk* chunk = world.getChunk(coord);

                if (!chunk || !chunk->collisionReady) {
                    continue;  // Chunk not loaded
                }

                // Add all collision geometry from this chunk
                for (auto* primitive : chunk->primitives) {
                    candidates.push_back(primitive);
                }
            }
        }
    }

    return candidates;
}
```

### Fast-Moving Objects Need Wider Search

```cpp
std::vector<CollisionPrimitive*> getCollisionCandidatesForMovement(
    const World& world,
    const glm::vec3& start,
    const glm::vec3& end,
    float radius)
{
    // Create bounding box for entire movement
    AABB movementBounds;
    movementBounds.min = glm::min(start, end) - glm::vec3(radius);
    movementBounds.max = glm::max(start, end) + glm::vec3(radius);

    // Find all chunks the movement could touch
    ChunkCoord minChunk = worldPosToChunk(movementBounds.min);
    ChunkCoord maxChunk = worldPosToChunk(movementBounds.max);

    std::vector<CollisionPrimitive*> candidates;

    for (int x = minChunk.x; x <= maxChunk.x; x++) {
        for (int y = minChunk.y; y <= maxChunk.y; y++) {
            for (int z = minChunk.z; z <= maxChunk.z; z++) {
                ChunkCoord coord = {x, y, z};
                const Chunk* chunk = world.getChunk(coord);

                if (chunk && chunk->collisionReady) {
                    for (auto* primitive : chunk->primitives) {
                        candidates.push_back(primitive);
                    }
                }
            }
        }
    }

    return candidates;
}
```

## Vertical Chunking Considerations

### Player Height vs Chunk Height

```cpp
// Typical character dimensions
const float PLAYER_HEIGHT = 1.8f;
const float PLAYER_RADIUS = 0.4f;

// Chunk height should be larger than player
const float CHUNK_HEIGHT = 16.0f;  // Much larger than player

// Player can span at most 2 vertical chunks
int maxVerticalChunksSpanned = (int)ceil(PLAYER_HEIGHT / CHUNK_HEIGHT) + 1;  // = 2
```

### Vertical Queries Are Important

Players jump and fall - always check vertical neighbors:

```cpp
bool needsVerticalQuery(const Player& player) {
    // Check if player is near chunk boundary vertically
    float yInChunk = fmod(player.position.y, CHUNK_HEIGHT);

    return yInChunk < PLAYER_HEIGHT ||          // Near bottom
           yInChunk > CHUNK_HEIGHT - PLAYER_HEIGHT;  // Near top
}
```

### Jump Prediction

When player jumps, prefetch upper chunks:

```cpp
void handleJump(Player& player, World& world) {
    player.velocity.y = JUMP_VELOCITY;

    // Predict highest point of jump
    float maxJumpHeight = (JUMP_VELOCITY * JUMP_VELOCITY) / (2.0f * GRAVITY);
    glm::vec3 peakPos = player.position + glm::vec3(0, maxJumpHeight, 0);

    // Ensure chunks along jump path are loaded
    ChunkCoord startChunk = worldPosToChunk(player.position);
    ChunkCoord peakChunk = worldPosToChunk(peakPos);

    for (int y = startChunk.y; y <= peakChunk.y; y++) {
        ChunkCoord coord = {startChunk.x, y, startChunk.z};
        world.ensureChunkLoaded(coord);
    }
}
```

## Chunk Loading/Unloading Safety

### Critical Rule: Never Unload Chunks Near Player

```cpp
class ChunkManager {
    const float COLLISION_SAFETY_RADIUS = 50.0f;  // Never unload within this
    const float RENDER_DISTANCE = 200.0f;

    void updateChunks(const glm::vec3& playerPos) {
        ChunkCoord playerChunk = worldPosToChunk(playerPos);

        // Load chunks near player
        int loadRadius = (int)ceil(COLLISION_SAFETY_RADIUS / CHUNK_SIZE);

        for (int dx = -loadRadius; dx <= loadRadius; dx++) {
            for (int dy = -2; dy <= 2; dy++) {  // Always ±2 vertical
                for (int dz = -loadRadius; dz <= loadRadius; dz++) {
                    ChunkCoord coord = {
                        playerChunk.x + dx,
                        playerChunk.y + dy,
                        playerChunk.z + dz
                    };

                    ensureChunkLoaded(coord);
                }
            }
        }

        // Unload distant chunks
        for (auto it = chunks.begin(); it != chunks.end(); ) {
            float dist = chunkDistance(it->first, playerChunk);

            if (dist > RENDER_DISTANCE) {
                unloadChunk(it->first);
                it = chunks.erase(it);
            } else {
                ++it;
            }
        }
    }

    float chunkDistance(ChunkCoord a, ChunkCoord b) {
        glm::vec3 aPos = chunkToWorldPos(a);
        glm::vec3 bPos = chunkToWorldPos(b);
        return glm::distance(aPos, bPos);
    }
};
```

### Handle Missing Chunks Gracefully

```cpp
bool moveWithCollision(Player& player, glm::vec3 velocity, float deltaTime) {
    std::vector<CollisionPrimitive*> candidates =
        getCollisionCandidates(world, player.position, player.radius);

    if (candidates.empty()) {
        // No collision geometry available!
        // This could mean chunks aren't loaded

        // Option 1: Don't move (safest)
        std::cerr << "Warning: No collision geometry at player position!" << std::endl;
        return false;

        // Option 2: Assume invisible floor at y=0
        if (player.position.y < 0) {
            player.position.y = 0;
            player.velocity.y = 0;
            player.onGround = true;
        }
    }

    // Normal collision resolution
    // ...
}
```

## Procedural Generation + Chunks + Collision

### Generate Collision with Mesh

```cpp
struct ChunkGenerator {
    void generateChunk(Chunk& chunk) {
        // 1. Generate mesh geometry
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        generateChunkMesh(chunk.coord, vertices, indices);

        // 2. Generate collision geometry
        generateChunkCollision(chunk);

        // 3. Upload mesh to GPU
        chunk.mesh = createMesh(vertices, indices);
        chunk.meshDirty = false;
        chunk.collisionReady = true;
    }

    void generateChunkCollision(Chunk& chunk) {
        glm::vec3 chunkWorldPos = chunkToWorldPos(chunk.coord);

        // Example: Procedural obstacle placement
        // Use chunk coordinates as seed for deterministic generation
        uint32_t seed = hashChunkCoord(chunk.coord);
        Random rng(seed);

        // Place some platforms
        int numPlatforms = rng.range(3, 8);
        for (int i = 0; i < numPlatforms; i++) {
            glm::vec3 localPos = glm::vec3(
                rng.range(0.0f, CHUNK_SIZE),
                rng.range(0.0f, CHUNK_HEIGHT),
                rng.range(0.0f, CHUNK_SIZE)
            );

            glm::vec3 size = glm::vec3(
                rng.range(2.0f, 8.0f),
                rng.range(0.2f, 0.5f),  // Thin platform
                rng.range(2.0f, 8.0f)
            );

            AABB platform;
            platform.min = chunkWorldPos + localPos;
            platform.max = platform.min + size;

            chunk.obstacles.push_back(platform);
        }

        // Add floor if bottom chunk
        if (chunk.coord.y == 0) {
            Plane floor;
            floor.normal = glm::vec3(0, 1, 0);
            floor.distance = chunkWorldPos.y;
            chunk.floors.push_back(floor);
        }
    }
};
```

## Optimization: Per-Chunk BVH

For complex chunks with lots of geometry:

```cpp
struct Chunk {
    // ... other fields ...

    BVHNode* collisionBVH = nullptr;  // Acceleration structure

    void buildCollisionBVH() {
        if (obstacles.empty()) return;

        // Build BVH for all obstacles in this chunk
        collisionBVH = buildBVH(obstacles);
    }

    bool raycast(Ray ray, RaycastHit& hit) {
        if (!collisionBVH) return false;
        return raycastBVH(collisionBVH, ray, hit);
    }
};

// Query only uses BVH for nearby chunks
std::vector<CollisionPrimitive*> getCollisionCandidates(
    const World& world,
    const glm::vec3& position,
    float radius)
{
    std::vector<CollisionPrimitive*> candidates;

    // Query nearby chunks
    ChunkCoord playerChunk = worldPosToChunk(position);

    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                ChunkCoord coord = {playerChunk.x + dx, playerChunk.y + dy, playerChunk.z + dz};
                const Chunk* chunk = world.getChunk(coord);

                if (!chunk) continue;

                // Use BVH for fast broad-phase culling
                if (chunk->collisionBVH) {
                    queryBVH(chunk->collisionBVH, position, radius, candidates);
                } else {
                    // Fallback: add all obstacles
                    candidates.insert(candidates.end(),
                                    chunk->primitives.begin(),
                                    chunk->primitives.end());
                }
            }
        }
    }

    return candidates;
}
```

## Practical Architecture

### Complete World + Collision Integration

```cpp
class GameWorld {
public:
    void update(float deltaTime) {
        // 1. Update chunk streaming
        chunkManager.updateChunks(player.position);

        // 2. Update player physics
        updatePlayerPhysics(player, deltaTime);

        // 3. Update dynamic objects
        for (auto& obj : dynamicObjects) {
            updateObjectPhysics(obj, deltaTime);
        }
    }

private:
    Player player;
    ChunkManager chunkManager;
    std::vector<DynamicObject> dynamicObjects;

    void updatePlayerPhysics(Player& player, float deltaTime) {
        // Apply movement input
        glm::vec3 wishDir = getPlayerInput();

        if (player.onGround) {
            applyGroundMovement(player, wishDir, deltaTime);
        } else {
            applyAirMovement(player, wishDir, deltaTime);
        }

        // Get collision geometry from nearby chunks
        std::vector<CollisionPrimitive*> candidates =
            getCollisionCandidates(player.position, player.radius * 2.0f);

        if (candidates.empty()) {
            // No chunks loaded - don't move!
            std::cerr << "Warning: Player in unloaded area" << std::endl;
            return;
        }

        // Move with collision detection
        moveWithCollision(player, candidates, deltaTime);

        // Check grounded state
        player.onGround = checkGrounded(player, candidates);
    }

    void moveWithCollision(Player& player,
                          const std::vector<CollisionPrimitive*>& candidates,
                          float deltaTime) {
        const int MAX_ITERATIONS = 5;
        glm::vec3 velocity = player.velocity * deltaTime;

        for (int i = 0; i < MAX_ITERATIONS; i++) {
            if (glm::length(velocity) < 0.001f) break;

            // Find closest collision along movement path
            CollisionInfo closestHit;
            closestHit.time = 1.0f;
            bool hitSomething = false;

            for (auto* primitive : candidates) {
                CollisionInfo hit;
                if (sweepCapsuleVsPrimitive(player.capsule, player.position,
                                           velocity, primitive, hit)) {
                    if (hit.time < closestHit.time) {
                        closestHit = hit;
                        hitSomething = true;
                    }
                }
            }

            if (!hitSomething) {
                // No collision, move full distance
                player.position += velocity;
                break;
            }

            // Move to collision point
            player.position += velocity * closestHit.time;

            // Slide along surface
            velocity = slideVelocity(velocity * (1.0f - closestHit.time),
                                    closestHit.normal);
        }
    }
};
```

## Chunk Coordinate Utilities

```cpp
ChunkCoord worldPosToChunk(glm::vec3 worldPos) {
    return ChunkCoord{
        (int)floor(worldPos.x / CHUNK_SIZE),
        (int)floor(worldPos.y / CHUNK_HEIGHT),
        (int)floor(worldPos.z / CHUNK_SIZE)
    };
}

glm::vec3 chunkToWorldPos(ChunkCoord coord) {
    return glm::vec3(
        coord.x * CHUNK_SIZE,
        coord.y * CHUNK_HEIGHT,
        coord.z * CHUNK_SIZE
    );
}

AABB getChunkBounds(ChunkCoord coord) {
    glm::vec3 min = chunkToWorldPos(coord);
    glm::vec3 max = min + glm::vec3(CHUNK_SIZE, CHUNK_HEIGHT, CHUNK_SIZE);
    return {min, max};
}

uint32_t hashChunkCoord(ChunkCoord coord) {
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    hash = (hash ^ coord.x) * 16777619u;
    hash = (hash ^ coord.y) * 16777619u;
    hash = (hash ^ coord.z) * 16777619u;
    return hash;
}
```

## Design Guidelines

### 1. Chunk Size Matters

For obstacle course game:

```cpp
// Good sizes:
const float CHUNK_SIZE = 32.0f;      // 32x32 meter horizontal
const float CHUNK_HEIGHT = 16.0f;    // 16 meter vertical

// Why?
// - Large enough that most obstacles fit in one chunk
// - Small enough for fast loading
// - Player typically spans 1-2 chunks max
// - Not too many chunk boundaries to cross
```

### 2. Always Check 3x3x3 Chunks

```cpp
// A moving player can hit geometry in any of 27 chunks
// (3x3x3 cube centered on player)

const int CHUNK_SEARCH_RADIUS = 1;  // Check ±1 in each direction
```

### 3. Collision Before Rendering

```cpp
void loadChunk(ChunkCoord coord) {
    Chunk* chunk = new Chunk();
    chunk->coord = coord;

    // IMPORTANT: Generate collision FIRST
    generateChunkCollision(*chunk);
    chunk->collisionReady = true;

    // Then generate render mesh (can be async)
    generateChunkMesh(*chunk);

    chunks[coord] = chunk;
}
```

### 4. Handle Chunk Boundaries Seamlessly

```cpp
// Bad: Player feels "bump" at chunk boundary
// Good: Player slides smoothly across

// Achieve by:
// 1. Always querying neighbor chunks
// 2. Using duplicate geometry at boundaries
// 3. No special-case code for boundaries
```

## Performance Targets

- **Chunk generation**: <50ms (can be async)
- **Collision query**: <0.5ms for 27 chunks
- **Chunk streaming**: 10-20 chunks per frame max
- **Memory per chunk**: <1MB (100KB geometry + 900KB collision)

## Summary: The Best Approach

For a **Quake-style obstacle course** game with **chunked world**:

✅ **Use smooth (non-voxel) geometry**
- Better for parkour/flow
- Can still use chunks effectively

✅ **Duplicate boundary geometry**
- Simple, robust
- Memory is cheap

✅ **Always query 3x3x3 chunk neighborhood**
- Handles all cross-chunk cases
- No special logic needed

✅ **Generate collision with mesh**
- Keep them in sync
- Load collision first, render second

✅ **Vertical chunks matter**
- Players jump and fall
- Always check ±1 vertical chunks

✅ **Never unload chunks near player**
- 50+ meter safety radius
- Prevent falling through world

## Next Steps

1. Implement basic chunk system (without collision)
2. Add collision geometry to chunks
3. Test cross-chunk movement
4. Add chunk streaming based on player position
5. Profile and optimize collision queries
