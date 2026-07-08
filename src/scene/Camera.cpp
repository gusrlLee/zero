#include "scene/Camera.h"
#include <algorithm>

Camera::Camera(float fov, float aspect, float nearClip, float farClip)
    : m_fov(fov), m_aspect(aspect), m_nearClip(nearClip), m_farClip(farClip) {
    updateCameraVectors();
}

void Camera::setAspectRatio(float aspect) {
    m_aspect = aspect;
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    glm::mat4 proj = glm::perspective(glm::radians(m_fov), m_aspect, m_nearClip, m_farClip);
    proj[1][1] *= -1.0f;

    return proj;
}

void Camera::processKeyboard(int key, bool isPressed) {
    // SDL 키 코드가 1024 이하라고 가정 (간단한 구현)
    if (key >= 0 && key < 1024) {
        m_keys[key] = isPressed;
    }
}

void Camera::processMouse(float xoffset, float yoffset) {
    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;

    m_yaw += xoffset;
    m_pitch -= yoffset; // Vulkan 화면 좌표계에 맞춰 직관적인 방향으로 설정

    // 화면이 180도 뒤집히는 것을 방지
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);

    updateCameraVectors();
}

void Camera::update(float deltaTime) {
    float velocity = m_movementSpeed * deltaTime;

    // 임시: SDL_SCANCODE_W(26), S(22), A(4), D(7) (추후 SDL 스캔코드에 맞게 매핑 필요)
    // 지금은 형태만 잡아두고, 이후 ZeroApp의 Event Loop와 연결할 때 정확히 매핑하겠습니다.
}

void Camera::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));

    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}