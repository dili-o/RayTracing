#pragma once

// Vendor
#include <glm/vec3.hpp>

namespace hlx {
struct Camera {
public:
  void init();
  void shutdown();

  void update(f32 delta_time);

  void on_key_event(bool key_down, u16 key_code);
  void on_mouse_event(i16 x, i16 y);
  void on_mouse_button_event(bool key_down, u16 key_code);
  void on_mouse_scroll_event(i8 direction);

public:
  glm::vec3 position;
  glm::vec3 look_at;
  glm::vec3 v_up;
  glm::vec3 u, v, w;
  f32 fov;
  bool changed{false};
  bool is_active{false};
  f32 yaw = -90.0f; // looking down -Z
  f32 pitch = 0.0f;

  glm::vec3 velocity;
  f32 speed = 5.0f;
  f32 sensitivity = 0.1f;
};
} // namespace hlx
