#include "camera.h"
#include "chunk_manager.h"
#include <algorithm>
#include <iostream>

Camera::Camera(glm::vec3 startPosition)
    : position(startPosition)
    , velocity(0.0f)
    , pitch(0.0f)
    , yaw(-90.0f * 3.14159f / 180.0f)  // Start facing forward (negative Z)
    , movementSpeed(8.0f)  // Quake-style speed
    , mouseSensitivity(0.002f)
    , gravity(-20.0f)  // Strong gravity for responsive feel
    , jumpStrength(8.0f)  // Jump velocity
    , playerHeight(1.8f)  // ~6 feet tall
    , playerRadius(0.4f)  // Capsule radius
    , onGround(false)
    , noclip(false)  // Start with physics enabled (press N to toggle noclip)
    , maxWalkableSlope(50.0f)  // 50 degrees max slope (Source engine uses ~45-50)
    , groundNormal(0, 1, 0)  // Default up
    , jumpsRemaining(2)  // Start with double jump available
{
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(position, position + getForward(), getUp());
}

glm::vec3 Camera::getForward() const {
    glm::vec3 forward;
    forward.x = cos(pitch) * cos(yaw);
    forward.y = sin(pitch);
    forward.z = cos(pitch) * sin(yaw);
    return glm::normalize(forward);
}

glm::vec3 Camera::getRight() const {
    return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Camera::getUp() const {
    return glm::normalize(glm::cross(getRight(), getForward()));
}

void Camera::processKeyboard(GLFWwindow* window, float deltaTime) {
    // Toggle noclip mode with N key
    static bool nWasPressed = false;
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS && !nWasPressed) {
        noclip = !noclip;
        std::cout << "Noclip " << (noclip ? "enabled" : "disabled") << std::endl;
        nWasPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_RELEASE) {
        nWasPressed = false;
    }

    if (noclip) {
        // Free-fly mode (old behavior)
        float speed = movementSpeed * deltaTime;
        glm::vec3 forward = getForward();
        glm::vec3 right = getRight();

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += forward * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= forward * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= right * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += right * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position.y += speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) position.y -= speed;
    } else {
        // Physics mode - horizontal movement only
        glm::vec3 forward = getForward();
        glm::vec3 right = getRight();

        // Project forward/right onto horizontal plane (ignore Y component)
        forward.y = 0.0f;
        right.y = 0.0f;
        if (glm::length(forward) > 0.001f) forward = glm::normalize(forward);
        if (glm::length(right) > 0.001f) right = glm::normalize(right);

        glm::vec3 moveDir(0.0f);
        bool isMoving = false;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { moveDir += forward; isMoving = true; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { moveDir -= forward; isMoving = true; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { moveDir -= right; isMoving = true; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { moveDir += right; isMoving = true; }

        if (isMoving && glm::length(moveDir) > 0.001f) {
            moveDir = glm::normalize(moveDir);

            if (onGround) {
                // On ground - instant acceleration (responsive)
                velocity.x = moveDir.x * movementSpeed;
                velocity.z = moveDir.z * movementSpeed;
            } else {
                // In air - limited air control
                float airControl = 0.3f;  // 30% control in air
                velocity.x += moveDir.x * movementSpeed * airControl * deltaTime;
                velocity.z += moveDir.z * movementSpeed * airControl * deltaTime;

                // Cap air speed
                float airSpeed = glm::length(glm::vec2(velocity.x, velocity.z));
                if (airSpeed > movementSpeed * 1.2f) {
                    float scale = (movementSpeed * 1.2f) / airSpeed;
                    velocity.x *= scale;
                    velocity.z *= scale;
                }
            }
        } else if (onGround) {
            // Ground friction - decelerate to stop
            float friction = 15.0f;  // Higher = faster stopping
            glm::vec2 horizontalVel(velocity.x, velocity.z);
            float speed = glm::length(horizontalVel);

            if (speed > 0.001f) {
                float drop = speed * friction * deltaTime;
                float newSpeed = std::max(0.0f, speed - drop);
                horizontalVel = glm::normalize(horizontalVel) * newSpeed;
                velocity.x = horizontalVel.x;
                velocity.z = horizontalVel.y;
            } else {
                velocity.x = 0.0f;
                velocity.z = 0.0f;
            }
        }

        // Jump with space
        static bool spaceWasPressed = false;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !spaceWasPressed) {
            jump();
            spaceWasPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
            spaceWasPressed = false;
        }
    }

    // Escape to close window
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw += xoffset;
    pitch += yoffset;

    // Constrain pitch to avoid flipping
    const float maxPitch = 89.0f * 3.14159f / 180.0f;
    pitch = std::clamp(pitch, -maxPitch, maxPitch);
}

void Camera::jump() {
    if (noclip) return;

    // Can jump if we have jumps remaining (includes double jump)
    if (jumpsRemaining > 0) {
        velocity.y = jumpStrength;
        jumpsRemaining--;
        onGround = false;
    }
}

// Sample SDF at a world position
float sampleSDF(const ChunkManager& chunkManager, glm::vec3 worldPos) {
    ChunkCoord chunkCoord = worldToChunkCoord(worldPos);

    // Find the chunk
    const auto& chunks = chunkManager.getChunks();
    auto it = chunks.find(chunkCoord);
    if (it == chunks.end()) {
        return -10.0f;  // No chunk = air
    }

    const VolumeChunk* chunk = it->second.get();
    glm::vec3 chunkWorldPos = chunkToWorldPos(chunkCoord);
    glm::vec3 localPos = worldPos - chunkWorldPos;

    // Convert to voxel coordinates
    glm::vec3 voxelPos = localPos / VOXEL_SIZE;

    // Bounds check
    if (voxelPos.x < 0 || voxelPos.x >= CHUNK_SIZE - 1 ||
        voxelPos.y < 0 || voxelPos.y >= CHUNK_SIZE - 1 ||
        voxelPos.z < 0 || voxelPos.z >= CHUNK_SIZE - 1) {
        return -10.0f;
    }

    // Trilinear interpolation
    int x0 = (int)voxelPos.x;
    int y0 = (int)voxelPos.y;
    int z0 = (int)voxelPos.z;
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    int z1 = z0 + 1;

    float fx = voxelPos.x - x0;
    float fy = voxelPos.y - y0;
    float fz = voxelPos.z - z0;

    // Interpolate
    float c00 = chunk->sdf[x0][y0][z0] * (1 - fx) + chunk->sdf[x1][y0][z0] * fx;
    float c01 = chunk->sdf[x0][y0][z1] * (1 - fx) + chunk->sdf[x1][y0][z1] * fx;
    float c10 = chunk->sdf[x0][y1][z0] * (1 - fx) + chunk->sdf[x1][y1][z0] * fx;
    float c11 = chunk->sdf[x0][y1][z1] * (1 - fx) + chunk->sdf[x1][y1][z1] * fx;

    float c0 = c00 * (1 - fy) + c10 * fy;
    float c1 = c01 * (1 - fy) + c11 * fy;

    return c0 * (1 - fz) + c1 * fz;
}

// Helper: Calculate SDF gradient (surface normal)
glm::vec3 calculateSDFNormal(const ChunkManager& chunkManager, glm::vec3 pos) {
    float step = 0.1f;
    float dx = sampleSDF(chunkManager, pos + glm::vec3(step, 0, 0)) -
               sampleSDF(chunkManager, pos - glm::vec3(step, 0, 0));
    float dy = sampleSDF(chunkManager, pos + glm::vec3(0, step, 0)) -
               sampleSDF(chunkManager, pos - glm::vec3(0, step, 0));
    float dz = sampleSDF(chunkManager, pos + glm::vec3(0, 0, step)) -
               sampleSDF(chunkManager, pos - glm::vec3(0, 0, step));

    glm::vec3 gradient(dx, dy, dz);
    float len = glm::length(gradient);
    return len > 0.001f ? gradient / len : glm::vec3(0, 1, 0);
}

void Camera::updatePhysics(float deltaTime, ChunkManager& chunkManager) {
    if (noclip) {
        onGround = false;
        return;  // Skip physics in noclip mode
    }

    // Apply gravity
    velocity.y += gravity * deltaTime;

    // Cap fall speed
    velocity.y = std::max(velocity.y, -50.0f);

    // Try to move
    glm::vec3 desiredMove = velocity * deltaTime;
    glm::vec3 newPosition = position + desiredMove;

    // --- GROUND DETECTION ---
    glm::vec3 feetPos = newPosition - glm::vec3(0, playerHeight * 0.5f, 0);
    float sdfAtFeet = sampleSDF(chunkManager, feetPos);

    onGround = false;

    // Generous ground detection range
    if (sdfAtFeet > -0.5f && velocity.y <= 0.5f) {
        // Near or in ground - calculate ground normal
        groundNormal = -calculateSDFNormal(chunkManager, feetPos);  // Invert (points away from solid)

        // Check slope angle
        float slopeAngle = glm::degrees(std::acos(glm::clamp(glm::dot(groundNormal, glm::vec3(0, 1, 0)), -1.0f, 1.0f)));

        if (slopeAngle <= maxWalkableSlope) {
            // Walkable slope
            onGround = true;
            jumpsRemaining = 2;  // Reset double jump when on ground
            velocity.y = 0.0f;

            // Gentle push out of ground if embedded (smooth correction)
            if (sdfAtFeet > 0.05f) {
                // Only correct if significantly embedded
                float correction = sdfAtFeet * 0.5f;  // Gentler correction (50% instead of 150%)
                newPosition.y += correction;
            } else if (sdfAtFeet > -0.1f) {
                // Very close to surface - snap gently to prevent jitter
                newPosition.y -= sdfAtFeet * 0.3f;
            }
        } else {
            // Too steep - slide down
            onGround = false;
            // Add downslope gravity component
            glm::vec3 downSlope = groundNormal - glm::vec3(0, 1, 0) * glm::dot(groundNormal, glm::vec3(0, 1, 0));
            if (glm::length(downSlope) > 0.001f) {
                downSlope = glm::normalize(downSlope);
                velocity += downSlope * gravity * deltaTime * 0.3f;  // Slide down slope
            }
        }
    }

    // --- SLOPE MOVEMENT MODULATION ---
    if (onGround && glm::length(glm::vec2(velocity.x, velocity.z)) > 0.1f) {
        // Project horizontal velocity onto slope
        glm::vec3 horizontalVel(velocity.x, 0, velocity.z);
        glm::vec3 right = glm::cross(groundNormal, glm::vec3(0, 1, 0));
        if (glm::length(right) < 0.001f) {
            right = glm::vec3(1, 0, 0);  // Fallback
        } else {
            right = glm::normalize(right);
        }
        glm::vec3 slopeForward = glm::cross(right, groundNormal);
        slopeForward = glm::normalize(slopeForward);

        // Check if moving uphill or downhill
        float uphillDot = glm::dot(glm::normalize(horizontalVel), slopeForward);
        if (uphillDot > 0.1f) {
            // Moving uphill - slow down based on slope
            float slopeAngle = glm::degrees(std::acos(glm::dot(groundNormal, glm::vec3(0, 1, 0))));
            float slopeFactor = 1.0f - (slopeAngle / maxWalkableSlope) * 0.5f;  // Up to 50% slower
            velocity.x *= slopeFactor;
            velocity.z *= slopeFactor;
        }
    }

    // --- HEAD COLLISION ---
    glm::vec3 headPos = newPosition + glm::vec3(0, playerHeight * 0.5f, 0);
    float sdfAtHead = sampleSDF(chunkManager, headPos);
    if (sdfAtHead > 0.0f) {
        newPosition.y = position.y;
        velocity.y = std::min(velocity.y, 0.0f);
    }

    // --- WALL COLLISION WITH SLIDING ---
    float sdfAtCenter = sampleSDF(chunkManager, newPosition);
    if (sdfAtCenter > 0.05f) {  // Only correct if meaningfully inside wall
        // Inside wall - calculate wall normal and slide along it
        glm::vec3 wallNormal = -calculateSDFNormal(chunkManager, newPosition);

        // Gentle push out of wall (smooth correction)
        float pushAmount = sdfAtCenter * 0.7f;  // Gentler than before
        newPosition += wallNormal * pushAmount;

        // Project velocity along wall (slide)
        glm::vec3 slideVel = velocity - wallNormal * glm::dot(velocity, wallNormal);
        velocity.x = slideVel.x;
        velocity.z = slideVel.z;
    }

    position = newPosition;

    // --- CAMERA CLIPPING FIX ---
    // Push camera away from walls if too close (very gentle)
    float minClearance = 0.2f;
    float sdfAtEye = sampleSDF(chunkManager, position);

    if (sdfAtEye > -minClearance && sdfAtEye < minClearance * 0.5f) {
        glm::vec3 eyeNormal = -calculateSDFNormal(chunkManager, position);
        float pushAmount = (minClearance - sdfAtEye) * 0.3f;  // Very gentle push
        position += eyeNormal * pushAmount;
    }

    // --- UNSTUCK MECHANISM ---
    // If deeply embedded in geometry, do gradual push-out (not instant)
    if (sdfAtCenter > 0.8f) {
        glm::vec3 escapeNormal = -calculateSDFNormal(chunkManager, position);
        float escapeAmount = (sdfAtCenter - 0.8f) * 2.0f;  // Gradual, not instant
        position += escapeNormal * escapeAmount;

        // Reduce velocity when unsticking, but don't kill it entirely
        velocity *= 0.5f;
    }
}
