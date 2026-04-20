#include "Camera.hpp"
#include "Core/Event.hpp"
#include "Core/Input.hpp"
#include "Platform/Platform.hpp"
// Vendor
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace hlx {
static constexpr u16 s_left_button = SDL_SCANCODE_A;
static constexpr u16 s_right_button = SDL_SCANCODE_D;
static constexpr u16 s_forward_button = SDL_SCANCODE_W;
static constexpr u16 s_backward_button = SDL_SCANCODE_S;
static constexpr u16 s_up_button = SDL_SCANCODE_SPACE;
static constexpr u16 s_down_button = SDL_SCANCODE_LCTRL;

// Move these into Camera.cpp
static bool camera_move_event(u16 code, void *sender, void *listener,
                              EventContext context) {
  Camera *cam = (Camera *)listener;
  cam->on_key_event(code == SDL_EVENT_KEY_DOWN, context.data.u16[0]);
  return false;
}

static bool camera_mouse_event(u16 code, void *sender, void *listener,
                               EventContext context) {
  Camera *cam = (Camera *)listener;
  cam->on_mouse_event(context.data.i16[0], context.data.i16[1]);
  return true;
}

static bool camera_mouse_button_event(u16 code, void *sender, void *listener,
                                      EventContext context) {
  Camera *cam = (Camera *)listener;
  cam->on_mouse_button_event(code == SDL_EVENT_MOUSE_BUTTON_DOWN,
                             context.data.u16[0]);
  return true;
}

static bool camera_scroll_event(u16 code, void *sender, void *listener,
                                EventContext context) {
  Camera *cam = (Camera *)listener;
  cam->on_mouse_scroll_event(context.data.i8[0]);
  return true;
}

void Camera::init() {
  velocity = glm::vec3(0.f);
  EventSys::register_event(SDL_EVENT_KEY_DOWN, this, camera_move_event);
  EventSys::register_event(SDL_EVENT_KEY_UP, this, camera_move_event);
  EventSys::register_event(SDL_EVENT_MOUSE_MOTION, this, camera_mouse_event);
  EventSys::register_event(SDL_EVENT_MOUSE_BUTTON_DOWN, this,
                           camera_mouse_button_event);
  EventSys::register_event(SDL_EVENT_MOUSE_BUTTON_UP, this,
                           camera_mouse_button_event);
  EventSys::register_event(SDL_EVENT_MOUSE_WHEEL, this, camera_scroll_event);
}

void Camera::shutdown() {
  EventSys::unregister_event(SDL_EVENT_KEY_DOWN, this, camera_move_event);
  EventSys::unregister_event(SDL_EVENT_KEY_UP, this, camera_move_event);
  EventSys::unregister_event(SDL_EVENT_MOUSE_MOTION, this, camera_mouse_event);
  EventSys::unregister_event(SDL_EVENT_MOUSE_BUTTON_DOWN, this,
                             camera_mouse_button_event);
  EventSys::unregister_event(SDL_EVENT_MOUSE_BUTTON_UP, this,
                             camera_mouse_button_event);
  EventSys::unregister_event(SDL_EVENT_MOUSE_WHEEL, this, camera_scroll_event);
}

void Camera::update(f32 delta_time) {
  if (glm::length(velocity) == 0)
    return;

  // 1. perspectiveGet the forward vector (you already calculate this in
  // on_mouse_event) If you don't store it, recalculate it here or use look_at -
  // position
  glm::vec3 forward = glm::normalize(look_at - position);

  // 2. Calculate local axes
  glm::vec3 world_up = glm::vec3(0, 1, 0);
  glm::vec3 right = glm::normalize(glm::cross(forward, world_up));
  glm::vec3 up = glm::cross(right, forward);

  // 3. Apply movement
  glm::vec3 move_dir =
      (forward * velocity.z) + (right * velocity.x) + (up * velocity.y);
  position += move_dir * speed * delta_time;

  // 4. Keep look_at in front of the camera
  look_at = position + forward;
  changed = true;
}

glm::mat4 Camera::get_rotation() {
  glm::quat pitch_rotation = glm::angleAxis(pitch, glm::vec3{1.f, 0.f, 0.f});
  glm::quat yaw_rotation = glm::angleAxis(yaw, glm::vec3{0.f, -1.f, 0.f});

  return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}

glm::mat4 Camera::get_view() {
  glm::mat4 camera_translation = glm::translate(glm::mat4(1.f), position);
  glm::mat4 camera_rotation = get_rotation();
  return glm::inverse(camera_translation * camera_rotation);
}

void Camera::on_key_event(bool key_down, u16 key_code) {
  if (!is_active)
    return;
  if (key_down) {
    switch (key_code) {
    case s_left_button: {
      velocity.x = -1;
      break;
    }
    case s_right_button: {
      velocity.x = 1;
      break;
    }
    case s_forward_button: {
      velocity.z = 1;
      break;
    }
    case s_backward_button: {
      velocity.z = -1;
      break;
    }
    case s_up_button: {
      velocity.y = 1;
      break;
    }
    case s_down_button: {
      velocity.y = -1;
      break;
    }
    default:
      break;
    }
  } else {
    switch (key_code) {
    case s_left_button: {
      velocity.x = 0;
      break;
    }
    case s_right_button: {
      velocity.x = 0;
      break;
    }
    case s_forward_button: {
      velocity.z = 0;
      break;
    }
    case s_backward_button: {
      velocity.z = 0;
      break;
    }
    case s_up_button: {
      velocity.y = 0;
      break;
    }
    case s_down_button: {
      velocity.y = 0;
      break;
    }
    default:
      break;
    }
  }
  changed = true;
}

void Camera::on_mouse_event(i16 x, i16 y) {
  if (!is_active) {
    return;
  }

  yaw += (f32)x / 10.f;
  pitch -= (f32)y / 10.f;
  pitch = glm::clamp(pitch, -89.0f, 89.0f);

  // Convert to direction
  glm::vec3 direction;
  direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  direction.y = sin(glm::radians(pitch));
  direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

  glm::vec3 forward = glm::normalize(direction);

  look_at = position + forward;
  changed = true;
}

void Camera::on_mouse_button_event(bool key_down, u16 key_code) {
  if (key_down) {
    if (key_code == BUTTON_RIGHT) {
      Platform::set_window_relative_mouse_mode(true);
      is_active = true;
    }
  } else {
    if (key_code == BUTTON_RIGHT) {
      Platform::set_window_relative_mouse_mode(false);
      is_active = false;
      velocity = glm::vec3(0.f);
    }
  }
}

void Camera::on_mouse_scroll_event(i8 direction) {
  if (InputSys::is_key_down(SDL_SCANCODE_LSHIFT)) {
    speed = direction > 0 ? (speed + 0.5f) : (speed - 0.5f);
    speed = glm::clamp(speed, 0.5f, 100.f);
  } else {
    f32 scroll_factor = 2.f;
    fov += (direction * -scroll_factor);
    fov = glm::clamp(fov, 2.f, 90.f);
  }
  changed = true;
}
} // namespace hlx
