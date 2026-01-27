#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 position;
    float pitch;  // Up/down rotation (radians)
    float yaw;    // Left/right rotation (radians)

    float movementSpeed;
    float mouseSensitivity;

    Camera(glm::vec3 startPosition = glm::vec3(0.0f, 5.0f, 10.0f));

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

private:
    void updateVectors();
};
