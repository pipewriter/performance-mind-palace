#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Forward declaration
class ChunkManager;

class Camera {
public:
    glm::vec3 position;
    glm::vec3 velocity;  // Physics velocity
    float pitch;  // Up/down rotation (radians)
    float yaw;    // Left/right rotation (radians)

    float movementSpeed;
    float mouseSensitivity;

    // Physics parameters
    float gravity;
    float jumpStrength;
    float playerHeight;
    float playerRadius;
    bool onGround;
    bool noclip;  // Toggle for free-fly vs physics mode
    float maxWalkableSlope;  // Maximum slope angle in degrees
    glm::vec3 groundNormal;  // Surface normal of ground we're standing on
    int jumpsRemaining;  // For double jump (2 = can double jump, 1 = used first jump, 0 = can't jump)

    Camera(glm::vec3 startPosition = glm::vec3(0.0f, 20.0f, 10.0f));

    // Get the view matrix for rendering
    glm::mat4 getViewMatrix() const;

    // Get direction vectors
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;

    // Process keyboard input (WASD movement)
    void processKeyboard(GLFWwindow* window, float deltaTime);

    // Process mouse movement (look around)
    void processMouseMovement(float xoffset, float yoffset);

    // Physics update with collision detection
    void updatePhysics(float deltaTime, ChunkManager& chunkManager);

    // Jump if on ground
    void jump();

private:
    void updateVectors();
};
