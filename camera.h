#pragma once

#include <rw_math.h>
#include "global.h"

struct Camera {
  float fov_y;
  float near_z;
  float far_z;
  float pitch;
  float yaw;
  float roll;
  Vec2 mouse_pos;
  Vec3 pos;
  Vec3 target;
  Vec3 up;
  Vec3 right;
  Mat4 view_mat;
  Mat4 persp_mat;
  Mat4 ortho_mat;
  Camera() : Camera(rwm_v3_init(0, 0, 0), rwm_v3_init(0, 0, -1.0f), rwm_v3_init(0, 1, 0), 43.0f, 0.1f, 1000.0f, ASPECT_RATIO) {}
  Camera(Vec3 pos, Vec3 target, Vec3 up, float fov_y, float near_z, float far_z, float aspect);
  void update(float dt_ms);
};

Mat4 get_view_mat(Vec3 *position, Vec3 *target, Vec3 *up);

Mat4 orthographic(float left, float right, float bottom, float top, float near, float far);
Mat4 perspective(float fov_y, float near_z, float far_z, float aspect);

