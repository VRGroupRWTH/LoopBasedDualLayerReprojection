#ifndef HEADER_CAMERA
#define HEADER_CAMERA

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <types.hpp>
#include <chrono>
#include <array>

#define CAMERA_MOVEMENT_SPEED 1.5f
#define CAMERA_ROTATION_SPEED 0.05f
#define CAMERA_FIELD_OF_VIEW 80.0f
#define CAMERA_NEAR_DISTANCE 0.1f
#define CAMERA_FAR_DISTANCE 200.0f

class Camera
{
private:
    std::chrono::high_resolution_clock::time_point last_update = std::chrono::high_resolution_clock::now();

    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 side = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float vertical_angle = 0.0f;
    float horizontal_angle = -glm::pi<float>();

    glm::mat4 projection_matrix = glm::mat4(1.0f);
    std::array<glm::mat4, SHARED_VIEW_COUNT_MAX> view_matrix;

public:
    Camera();

    void update(GLFWwindow* window, bool focused);

    void set_position(const glm::vec3& position);
    void set_view_matrix(uint32_t view, const glm::mat4& view_matrix);
    void set_projection_matrix(const glm::mat4& projection_matrix);

    const glm::vec3& get_position() const;
    const glm::vec3& get_forward() const;
    const glm::vec3& get_side() const;
    const glm::vec3& get_up() const;

    const glm::mat4& get_projection_matrix() const;
    const glm::mat4& get_view_matrix(uint32_t view = 0) const;
};

#endif
