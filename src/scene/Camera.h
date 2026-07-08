#ifndef __ZERO_CAMERA_HEADER__
#define __ZERO_CAMERA_HEADER__

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
	Camera(float fov, float aspect, float nearClip, float farClip);

	void update(float deltaTime);
	void processKeyboard(int key, bool isPressed);
	void processMouse(float xoffset, float yoffset);

    void setAspectRatio(float aspect);

	glm::mat4 getViewMatrix() const;
	glm::mat4 getProjectionMatrix() const;
	glm::vec3 getPosition() const { return m_position; }
private:
    void updateCameraVectors();

private:
    glm::vec3 m_position{ 0.0f, 0.0f, 3.0f }; 
    glm::vec3 m_front{ 0.0f, 0.0f, -1.0f };
    glm::vec3 m_up{ 0.0f, 1.0f, 0.0f };
    glm::vec3 m_right{ 1.0f, 0.0f, 0.0f };
    glm::vec3 m_worldUp{ 0.0f, 1.0f, 0.0f };

    float m_yaw = -90.0f; // 처음엔 -Z축을 바라봄
    float m_pitch = 0.0f;

    float m_movementSpeed = 5.0f;
    float m_mouseSensitivity = 0.1f;
    float m_fov;
    float m_aspect;
    float m_nearClip;
    float m_farClip;

    bool m_keys[1024]{ false };
};

#endif
