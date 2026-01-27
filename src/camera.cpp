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
            // Apply horizontal velocity (instant acceleration)
            velocity.x = moveDir.x * movementSpeed;
            velocity.z = moveDir.z * movementSpeed;
        } else if (onGround) {
            // Ground friction - decelerate to stop
            float friction = 12.0f * deltaTime;  // Friction coefficient
            glm::vec2 horizontalVel(velocity.x, velocity.z);
            float speed = glm::length(horizontalVel);

            if (speed > 0.001f) {
                float newSpeed = std::max(0.0f, speed - friction * speed);
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
    if (onGround && !noclip) {
        velocity.y = jumpStrength;
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

    // Collision detection: check multiple points on capsule
    glm::vec3 feetPos = newPosition - glm::vec3(0, playerHeight * 0.5f, 0);
    glm::vec3 headPos = newPosition + glm::vec3(0, playerHeight * 0.5f, 0);

    float sdfAtFeet = sampleSDF(chunkManager, feetPos);
    float sdfAtHead = sampleSDF(chunkManager, headPos);
    float sdfAtCenter = sampleSDF(chunkManager, newPosition);

    // Ground detection - check if feet are in solid ground
    if (sdfAtFeet > 0.2f) {  // Small threshold to prevent jitter
        // Push player up onto surface
        newPosition.y = position.y;
        velocity.y = 0.0f;
        onGround = true;
    } else if (sdfAtFeet > -0.5f && velocity.y <= 0.0f) {
        // Near ground and falling - snap to ground
        onGround = true;
        velocity.y = 0.0f;
    } else {
        onGround = false;
    }

    // Head collision
    if (sdfAtHead > 0.0f) {
        newPosition.y = position.y;
        velocity.y = std::min(velocity.y, 0.0f);
    }

    // Horizontal collision - check center
    if (sdfAtCenter > 0.0f) {
        // Inside solid - try to push out
        // For now, just prevent horizontal movement
        newPosition.x = position.x;
        newPosition.z = position.z;
        velocity.x = 0.0f;
        velocity.z = 0.0f;
    }

    position = newPosition;

    // CAMERA CLIPPING FIX: Push camera away from walls
    // Check if camera (eye position, which is ~at head height) is too close to solid
    float minClearance = 0.3f;  // Minimum distance from solid surfaces
    float sdfAtEye = sampleSDF(chunkManager, position);

    if (sdfAtEye > -minClearance) {
        // Too close to or inside solid - push back
        // Sample SDF in multiple directions to find the push-out direction
        glm::vec3 forward = getForward();
        glm::vec3 right = getRight();
        glm::vec3 up = getUp();

        // Calculate gradient (normal of nearest surface)
        float step = 0.1f;
        float dx = sampleSDF(chunkManager, position + right * step) -
                   sampleSDF(chunkManager, position - right * step);
        float dy = sampleSDF(chunkManager, position + up * step) -
                   sampleSDF(chunkManager, position - up * step);
        float dz = sampleSDF(chunkManager, position + forward * step) -
                   sampleSDF(chunkManager, position - forward * step);

        glm::vec3 gradient(dx, dy, dz);
        float gradLen = glm::length(gradient);

        if (gradLen > 0.001f) {
            // Push away from surface along gradient
            glm::vec3 pushDirection = -gradient / gradLen;  // Point away from solid
            float pushAmount = minClearance - sdfAtEye;
            position += pushDirection * pushAmount * 2.0f;  // Push out with some extra margin
        }
    }
}
