#include "camera.h"
#include <algorithm>

Camera::Camera(glm::vec3 startPosition)
    : position(startPosition)
    , pitch(0.0f)
    , yaw(-90.0f * 3.14159f / 180.0f)  // Start facing forward (negative Z)
    , movementSpeed(5.0f)
    , mouseSensitivity(0.002f)
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
    float velocity = movementSpeed * deltaTime;

    glm::vec3 forward = getForward();
    glm::vec3 right = getRight();

    // WASD movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        position += forward * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        position -= forward * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        position -= right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        position += right * velocity;
    }

    // Q/E for up/down
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        position.y += velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        position.y -= velocity;
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
