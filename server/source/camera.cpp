#include "camera.hpp"

Camera::Camera()
{
    this->view_matrix.fill(glm::mat4(1.0f));
}

void Camera::update(GLFWwindow* window, bool focused)
{
    std::chrono::high_resolution_clock::time_point current_time = std::chrono::high_resolution_clock::now();
    float delta_time = std::chrono::duration_cast<std::chrono::duration<float, std::chrono::seconds::period>>(current_time - this->last_update).count();
    this->last_update = current_time;

    if(!focused)
    {
        return;
    }

    glm::ivec2 window_size = glm::ivec2(0);
    glfwGetWindowSize(window, &window_size.x, &window_size.y);

    glm::dvec2 mouse_position = glm::dvec2(0.0);
    glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);

    glm::vec2 mouse_delta = glm::vec2(mouse_position) - glm::vec2(window_size) / 2.0f;
    mouse_delta *= CAMERA_ROTATION_SPEED * delta_time;

    glfwSetCursorPos(window, window_size.x / 2, window_size.y / 2);

    this->horizontal_angle -= mouse_delta.x;
    this->vertical_angle += mouse_delta.y;

    this->vertical_angle = glm::clamp(this->vertical_angle, -glm::half_pi<float>(), glm::half_pi<float>());

    while (this->horizontal_angle < 0.0f)
    {
        this->horizontal_angle += glm::two_pi<float>();
    }

    while (this->horizontal_angle > glm::two_pi<float>())
    {
        this->horizontal_angle -= glm::two_pi<float>();
    }

    this->forward.x = glm::cos(this->vertical_angle) * glm::sin(this->horizontal_angle);
    this->forward.y = glm::sin(this->vertical_angle);
    this->forward.z = glm::cos(this->vertical_angle) * glm::cos(this->horizontal_angle);

    this->side.x = glm::sin(this->horizontal_angle + glm::half_pi<float>());
    this->side.y = 0.0f;
    this->side.z = glm::cos(this->horizontal_angle + glm::half_pi<float>());

    this->up = glm::cross(this->forward, this->side);

    glm::vec3 position_change = glm::vec3(0.0f);

    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        position_change.x -= delta_time * CAMERA_MOVEMENT_SPEED * glm::sin(this->horizontal_angle);
        position_change.z -= delta_time * CAMERA_MOVEMENT_SPEED * glm::cos(this->horizontal_angle);
    }

    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        position_change.x += delta_time * CAMERA_MOVEMENT_SPEED * glm::sin(this->horizontal_angle);
        position_change.z += delta_time * CAMERA_MOVEMENT_SPEED * glm::cos(this->horizontal_angle);
    }

    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        position_change.x -= delta_time * CAMERA_MOVEMENT_SPEED * glm::sin(this->horizontal_angle + glm::half_pi<float>());
        position_change.z -= delta_time * CAMERA_MOVEMENT_SPEED * glm::cos(this->horizontal_angle + glm::half_pi<float>());
    }

    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        position_change.x += delta_time * CAMERA_MOVEMENT_SPEED * glm::sin(this->horizontal_angle + glm::half_pi<float>());
        position_change.z += delta_time * CAMERA_MOVEMENT_SPEED * glm::cos(this->horizontal_angle + glm::half_pi<float>());
    }

    if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        position_change.y += delta_time * CAMERA_MOVEMENT_SPEED;
    }

    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        position_change.y -= delta_time * CAMERA_MOVEMENT_SPEED;
    }

    this->position += position_change;

    glm::mat4 view_rotation_matrix = glm::mat4(1.0f);
    view_rotation_matrix[0] = glm::vec4(this->side, 0.0f);
    view_rotation_matrix[1] = glm::vec4(this->up, 0.0f);
    view_rotation_matrix[2] = glm::vec4(this->forward, 0.0f);

    this->view_matrix[0] = glm::transpose(view_rotation_matrix) * glm::translate(-this->position);
    this->projection_matrix = glm::perspective(glm::radians(CAMERA_FIELD_OF_VIEW), (float)window_size.x / (float)window_size.y, CAMERA_NEAR_DISTANCE, CAMERA_FAR_DISTANCE);
}

void Camera::set_position(const glm::vec3& position)
{
    this->position = position;
}

void Camera::set_view_matrix(uint32_t view, const glm::mat4& view_matrix)
{
    this->view_matrix[view] = view_matrix;
}

void Camera::set_projection_matrix(const glm::mat4& projection_matrix)
{
    this->projection_matrix = projection_matrix;
}

const glm::vec3& Camera::get_position() const
{
    return this->position;
}

const glm::vec3& Camera::get_forward() const
{
    return this->forward;
}

const glm::vec3& Camera::get_side() const
{
    return this->side;
}

const glm::vec3& Camera::get_up() const
{
    return this->up;
}

const glm::mat4& Camera::get_projection_matrix() const
{
    return this->projection_matrix;
}

const glm::mat4& Camera::get_view_matrix(uint32_t view) const
{
    return this->view_matrix[view];
}