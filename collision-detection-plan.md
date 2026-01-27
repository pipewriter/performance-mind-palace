# Collision Detection & Movement Plan

## Overview

For a fast-paced obstacle course game with Quake-style movement, you need:
- **Fast collision detection** (characters move FAST)
- **Smooth sliding** along walls and surfaces
- **Predictable physics** (no weird bouncing or sticking)
- **Simple primitives** (complex collision is slow and buggy)

## Core Primitives

### What You Actually Need

For a movement-focused game, stick to these primitives:

**1. Capsule (Player)**
```cpp
struct Capsule {
    glm::vec3 start;     // Bottom of cylinder
    glm::vec3 end;       // Top of cylinder
    float radius;
};
```
- **Best for characters** - smooth sliding, no getting stuck on edges
- Defined by line segment + radius
- Much better than boxes for player collision
- Used in: Quake, Half-Life, most FPS games

**2. AABB (Axis-Aligned Bounding Box)**
```cpp
struct AABB {
    glm::vec3 min;       // Bottom-left-back corner
    glm::vec3 max;       // Top-right-front corner
};
```
- **Best for static geometry** - walls, floors, obstacles
- Fast to test against
- Easy to build spatial partitioning with
- Can't rotate (that's the trade-off for speed)

**3. Plane**
```cpp
struct Plane {
    glm::vec3 normal;    // Direction plane faces
    float distance;      // Distance from origin
};
```
- **Best for large flat surfaces** - floors, walls, ramps
- Super fast collision tests
- Good for level geometry

**4. Sphere** (optional, for round objects)
```cpp
struct Sphere {
    glm::vec3 center;
    float radius;
};
```
- Fastest primitive to test
- Good for pickups, projectiles, triggers
- Not ideal for player (less smooth than capsule)

### What You DON'T Need (Yet)

- **OBB (Oriented Bounding Box)** - Too slow for fast movement, use AABB instead
- **Triangle soup** - Only for static world geometry, never for dynamic objects
- **Convex hulls** - Overkill for obstacle course, stick to primitives
- **Mesh collision** - Reserve for level geometry only

## Collision Detection Algorithms

### Capsule vs AABB (Your Main Case)

This is your bread and butter - player vs world collision.

```cpp
bool intersectCapsuleAABB(const Capsule& capsule, const AABB& box, CollisionInfo& info) {
    // 1. Find closest point on capsule line segment to box
    glm::vec3 closestOnLine = closestPointOnLineSegment(
        capsule.start, capsule.end, box
    );

    // 2. Find closest point on box to that point
    glm::vec3 closestOnBox = glm::clamp(closestOnLine, box.min, box.max);

    // 3. Check if distance is less than capsule radius
    glm::vec3 diff = closestOnLine - closestOnBox;
    float distSq = glm::dot(diff, diff);

    if (distSq < capsule.radius * capsule.radius) {
        info.penetration = capsule.radius - sqrt(distSq);
        info.normal = glm::normalize(diff);
        info.point = closestOnBox;
        return true;
    }
    return false;
}
```

### Sphere vs Plane (Simplest)

```cpp
bool intersectSpherePlane(const Sphere& sphere, const Plane& plane, CollisionInfo& info) {
    float distance = glm::dot(sphere.center, plane.normal) - plane.distance;

    if (distance < sphere.radius) {
        info.penetration = sphere.radius - distance;
        info.normal = plane.normal;
        info.point = sphere.center - plane.normal * distance;
        return true;
    }
    return false;
}
```

### AABB vs AABB (Broadphase)

```cpp
bool intersectAABB(const AABB& a, const AABB& b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}
```

## Collision Response - The Quake Way

### The Key Insight: Velocity Projection

When you hit a surface, you don't stop - you **slide** along it.

```cpp
struct CollisionInfo {
    glm::vec3 normal;        // Surface normal
    glm::vec3 point;         // Contact point
    float penetration;       // How far we're penetrating
    bool collided;
};

glm::vec3 slideVelocity(glm::vec3 velocity, glm::vec3 surfaceNormal) {
    // Remove component of velocity pointing into surface
    float backoff = glm::dot(velocity, surfaceNormal);

    // Add slight overbounce to prevent floating point errors
    const float OVERBOUNCE = 1.001f;

    return velocity - surfaceNormal * (backoff * OVERBOUNCE);
}
```

### Iterative Collision Resolution

The Quake engine does **multiple iterations** per frame to handle complex collisions:

```cpp
void moveWithCollision(Player& player, glm::vec3 wishVelocity, float deltaTime) {
    const int MAX_CLIP_PLANES = 5;  // Maximum surfaces to slide against

    glm::vec3 position = player.position;
    glm::vec3 velocity = wishVelocity * deltaTime;

    // Try to move for up to MAX_CLIP_PLANES bounces
    for (int bumpCount = 0; bumpCount < MAX_CLIP_PLANES; bumpCount++) {
        if (glm::length(velocity) < 0.001f) {
            break;  // Stopped moving
        }

        // Try to move
        glm::vec3 targetPos = position + velocity;
        CollisionInfo collision;

        if (!checkCollision(player, position, targetPos, collision)) {
            // No collision, move freely
            position = targetPos;
            break;
        }

        // Hit something, resolve penetration
        position = collision.point + collision.normal * (player.radius + 0.01f);

        // Slide along surface
        velocity = slideVelocity(velocity, collision.normal);
    }

    player.position = position;
}
```

## Quake-Style Movement System

### The Core Components

```cpp
struct PlayerMovement {
    glm::vec3 position;
    glm::vec3 velocity;
    bool onGround;

    // Movement constants (Quake 3 values)
    static constexpr float GRAVITY = 20.0f;           // Units per second squared
    static constexpr float WALK_SPEED = 6.0f;         // Base walk speed
    static constexpr float RUN_SPEED = 12.0f;         // Sprint speed
    static constexpr float JUMP_VELOCITY = 8.0f;      // Initial jump speed
    static constexpr float AIR_ACCEL = 0.7f;          // Air control multiplier
    static constexpr float GROUND_ACCEL = 10.0f;      // Ground acceleration
    static constexpr float FRICTION = 6.0f;           // Ground friction
    static constexpr float STOP_SPEED = 1.0f;         // Speed below which we apply extra friction
};
```

### Ground Movement

```cpp
void applyGroundMovement(PlayerMovement& player, glm::vec3 wishDir, float deltaTime) {
    // Apply friction
    float speed = glm::length(player.velocity);
    if (speed > 0.1f) {
        float control = (speed < STOP_SPEED) ? STOP_SPEED : speed;
        float drop = control * FRICTION * deltaTime;
        float newSpeed = std::max(0.0f, speed - drop);
        player.velocity *= (newSpeed / speed);
    }

    // Apply acceleration
    float wishSpeed = glm::length(wishDir) * RUN_SPEED;
    wishDir = glm::normalize(wishDir);

    accelerate(player, wishDir, wishSpeed, GROUND_ACCEL, deltaTime);
}

void accelerate(PlayerMovement& player, glm::vec3 wishDir, float wishSpeed,
                float accel, float deltaTime) {
    float currentSpeed = glm::dot(player.velocity, wishDir);
    float addSpeed = wishSpeed - currentSpeed;

    if (addSpeed <= 0) {
        return;  // Already at or above desired speed
    }

    float accelSpeed = accel * deltaTime * wishSpeed;
    accelSpeed = std::min(accelSpeed, addSpeed);

    player.velocity += wishDir * accelSpeed;
}
```

### Air Movement (The Secret Sauce)

This is what enables **strafe jumping** and **bunny hopping**:

```cpp
void applyAirMovement(PlayerMovement& player, glm::vec3 wishDir, float deltaTime) {
    // No friction in air

    // Air control - allows changing direction mid-air
    float wishSpeed = glm::length(wishDir) * RUN_SPEED;
    wishDir = glm::normalize(wishDir);

    // Lower acceleration in air (this is the key!)
    accelerate(player, wishDir, wishSpeed, AIR_ACCEL, deltaTime);

    // Apply gravity
    player.velocity.y -= GRAVITY * deltaTime;
}
```

### Jump

```cpp
void jump(PlayerMovement& player) {
    if (!player.onGround) {
        return;  // Can't jump in air
    }

    player.velocity.y = JUMP_VELOCITY;
    player.onGround = false;
}
```

### Ground Detection

```cpp
bool checkGrounded(const Player& player, const World& world) {
    // Cast a ray slightly below player
    glm::vec3 rayStart = player.position;
    glm::vec3 rayEnd = player.position - glm::vec3(0, player.capsule.radius + 0.1f, 0);

    RaycastHit hit;
    if (world.raycast(rayStart, rayEnd, hit)) {
        // Check if surface is flat enough to stand on
        const float MAX_SLOPE = 0.7f;  // cos(45 degrees)
        return hit.normal.y > MAX_SLOPE;
    }

    return false;
}
```

## Spatial Partitioning for Performance

When you have lots of obstacles, you can't test against everything every frame.

### Grid-Based Partitioning (Simplest)

Divide world into uniform grid cells:

```cpp
class SpatialGrid {
    static constexpr float CELL_SIZE = 10.0f;
    std::unordered_map<glm::ivec3, std::vector<Obstacle*>> grid;

    glm::ivec3 getCellCoords(glm::vec3 position) {
        return glm::ivec3(
            floor(position.x / CELL_SIZE),
            floor(position.y / CELL_SIZE),
            floor(position.z / CELL_SIZE)
        );
    }

    void insert(Obstacle* obstacle) {
        // Insert into all cells the obstacle overlaps
        AABB bounds = obstacle->getBounds();
        glm::ivec3 minCell = getCellCoords(bounds.min);
        glm::ivec3 maxCell = getCellCoords(bounds.max);

        for (int x = minCell.x; x <= maxCell.x; x++) {
            for (int y = minCell.y; y <= maxCell.y; y++) {
                for (int z = minCell.z; z <= maxCell.z; z++) {
                    grid[{x, y, z}].push_back(obstacle);
                }
            }
        }
    }

    std::vector<Obstacle*> query(glm::vec3 position, float radius) {
        std::vector<Obstacle*> results;
        glm::ivec3 minCell = getCellCoords(position - glm::vec3(radius));
        glm::ivec3 maxCell = getCellCoords(position + glm::vec3(radius));

        for (int x = minCell.x; x <= maxCell.x; x++) {
            for (int y = minCell.y; y <= maxCell.y; y++) {
                for (int z = minCell.z; z <= maxCell.z; z++) {
                    auto it = grid.find({x, y, z});
                    if (it != grid.end()) {
                        results.insert(results.end(), it->second.begin(), it->second.end());
                    }
                }
            }
        }

        return results;
    }
};
```

### BVH (Bounding Volume Hierarchy) - For Static Geometry

Better for complex static levels:

```cpp
struct BVHNode {
    AABB bounds;
    BVHNode* left = nullptr;
    BVHNode* right = nullptr;
    std::vector<Triangle*> triangles;  // Only in leaf nodes

    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

bool raycastBVH(const BVHNode* node, Ray ray, RaycastHit& hit) {
    if (!intersectRayAABB(ray, node->bounds)) {
        return false;  // Early out
    }

    if (node->isLeaf()) {
        // Test against triangles
        bool hitAny = false;
        for (auto tri : node->triangles) {
            if (intersectRayTriangle(ray, tri, hit)) {
                hitAny = true;
            }
        }
        return hitAny;
    }

    // Recurse to children
    bool hitLeft = raycastBVH(node->left, ray, hit);
    bool hitRight = raycastBVH(node->right, ray, hit);
    return hitLeft || hitRight;
}
```

## Continuous vs Discrete Collision

### Discrete (Most Games)

Check collision at current position each frame:
- **Pros**: Fast, simple
- **Cons**: Can tunnel through thin walls at high speed

```cpp
void discreteCollision(Player& player, float deltaTime) {
    glm::vec3 newPos = player.position + player.velocity * deltaTime;

    if (checkCollision(newPos)) {
        resolveCollision(player);
    } else {
        player.position = newPos;
    }
}
```

### Continuous (Safer for Fast Movement)

Check along the movement path:
- **Pros**: No tunneling, more accurate
- **Cons**: Slower, more complex

```cpp
void continuousCollision(Player& player, float deltaTime) {
    glm::vec3 start = player.position;
    glm::vec3 end = start + player.velocity * deltaTime;

    RaycastHit hit;
    if (sweepCapsule(player.capsule, start, end, hit)) {
        // Hit something during movement
        player.position = start + (end - start) * hit.time;
        resolveCollision(player, hit);
    } else {
        player.position = end;
    }
}
```

**Recommendation**: Use **discrete** collision with a **high fixed timestep** (120Hz or higher). This is what most modern games do - it's fast enough that tunneling rarely happens.

## Integration Loop

### Fixed Timestep Physics

**Critical for predictable movement:**

```cpp
void gameLoop() {
    const float FIXED_DT = 1.0f / 120.0f;  // 120 Hz physics
    float accumulator = 0.0f;

    auto lastTime = std::chrono::steady_clock::now();

    while (running) {
        auto currentTime = std::chrono::steady_clock::now();
        float frameTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Clamp frame time to prevent spiral of death
        frameTime = std::min(frameTime, 0.1f);
        accumulator += frameTime;

        // Fixed timestep physics updates
        while (accumulator >= FIXED_DT) {
            updatePhysics(FIXED_DT);  // Movement + collision
            accumulator -= FIXED_DT;
        }

        // Variable timestep rendering
        render();
    }
}
```

### Complete Movement Update

```cpp
void updatePhysics(float deltaTime) {
    // 1. Get player input
    glm::vec3 wishDir = getInputDirection();

    // 2. Check if grounded
    player.onGround = checkGrounded(player);

    // 3. Apply movement
    if (player.onGround) {
        applyGroundMovement(player, wishDir, deltaTime);
    } else {
        applyAirMovement(player, wishDir, deltaTime);
    }

    // 4. Handle jump input
    if (input.jumpPressed) {
        jump(player);
    }

    // 5. Move with collision detection
    moveWithCollision(player, player.velocity, deltaTime);

    // 6. Update camera
    updateCamera(player);
}
```

## Data Structures

### Practical Layout

```cpp
struct World {
    // Static geometry (never moves)
    std::vector<AABB> staticBoxes;
    BVHNode* staticGeometryBVH;

    // Dynamic objects (can move)
    std::vector<DynamicObject> dynamicObjects;
    SpatialGrid dynamicGrid;

    // Trigger volumes (no collision, just events)
    std::vector<Trigger> triggers;
};

struct Player {
    // Transform
    glm::vec3 position;
    glm::quat rotation;

    // Physics
    glm::vec3 velocity;
    bool onGround;

    // Collision shape
    Capsule collider;
    float radius = 0.5f;
    float height = 1.8f;

    // Camera
    glm::vec3 cameraOffset = glm::vec3(0, 1.6f, 0);  // Eye level
    float pitch = 0.0f;
    float yaw = 0.0f;
};

struct DynamicObject {
    glm::vec3 position;
    glm::vec3 velocity;
    AABB bounds;
    bool isStatic;  // If true, skip physics
};
```

## Tips for Obstacle Course Games

### 1. Predictable Movement is King

- Use **deterministic** physics (fixed timestep)
- No random bouncing or sliding
- Players need to trust the movement

### 2. Smooth Collision Response

- **Never** hard-stop the player
- Always slide along surfaces
- Small chamfers/bevels on corners help
- Capsule collider is your friend

### 3. Level Design Helpers

```cpp
// Snap to grid for consistent collision
glm::vec3 snapToGrid(glm::vec3 pos, float gridSize = 0.25f) {
    return glm::round(pos / gridSize) * gridSize;
}

// Validate obstacle placement
bool isValidObstacle(const AABB& box) {
    // Check if box is too thin (will cause problems)
    glm::vec3 size = box.max - box.min;
    const float MIN_SIZE = 0.1f;
    return size.x > MIN_SIZE && size.y > MIN_SIZE && size.z > MIN_SIZE;
}
```

### 4. Debug Visualization

Always render your collision shapes during development:

```cpp
void debugDrawCollision() {
    // Draw player capsule
    drawCapsule(player.collider, player.position, COLOR_GREEN);

    // Draw nearby obstacles
    auto nearby = spatialGrid.query(player.position, 10.0f);
    for (auto obstacle : nearby) {
        drawAABB(obstacle->bounds, COLOR_BLUE);
    }

    // Draw collision contacts
    for (auto contact : recentCollisions) {
        drawSphere(contact.point, 0.1f, COLOR_RED);
        drawLine(contact.point, contact.point + contact.normal, COLOR_YELLOW);
    }
}
```

## Performance Targets

For smooth gameplay:
- **Physics rate**: 120 Hz (8.3ms budget per frame)
- **Collision checks per frame**: <1000 for 60fps, <500 for 120fps
- **Spatial query time**: <0.1ms
- **Player movement + collision**: <1ms

## Next Steps for This Project

1. **Immediate**: Implement capsule collider for the cube
2. **Short-term**: Add basic WASD + mouse camera controls
3. **Medium-term**: Add ground detection and jumping
4. **Long-term**: Add strafe jumping mechanics

## References

- **Quake 3 source code**: Study `bg_pmove.c` for movement
- **Valve Developer Wiki**: Source engine movement
- **"Swept AABB Collision Detection"**: Good article on continuous collision
- **"Game Physics Engine Development"**: Ian Millington's book
- **Real-Time Collision Detection**: Christer Ericson's definitive guide
