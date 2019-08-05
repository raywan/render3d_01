#include "camera.h"
#include <rw_math.h>
#include <math.h>
#include <stdio.h>
#include <SDL.h>
#include "input.h"
#include "global.h"

Camera::Camera(Vec3 pos, Vec3 target, Vec3 up, float fov_y, float near_z, float far_z, float aspect) {
  this->pos = pos;
  this->target = target;
  this->up = up;
  this->fov_y = fov_y;
  this->near_z = near_z;
  this->far_z = far_z;
  this->mouse_pos = rwm_v2_init(SCREEN_WIDTH/2.0, SCREEN_HEIGHT/2.0);
  this->pitch = 0.0;
  this->yaw = 0.0;
  rwm_v3_printf("pos", &this->pos);
  rwm_v3_printf("target", &this->target);
  rwm_v3_printf("up", &this->up);
  this->view_mat = get_view_mat(&this->pos, &this->target, &this->up);
  Vec3 camera_dir = rwm_v3_normalize(rwm_v3_subtract(this->target, this->pos));
  this->right = rwm_v3_normalize(rwm_v3_cross(camera_dir, this->up));
  puts("constructor");
  rwm_m4_puts(&view_mat);
  this->persp_mat = perspective(fov_y, near_z, far_z, aspect);
  this->ortho_mat = rwm_m4_identity();
}

Mat4 get_view_mat(Vec3 *position, Vec3 *target, Vec3 *up) {
  Vec3 v, u, r;

  v = rwm_v3_normalize(*position - *target);
  r = rwm_v3_normalize(rwm_v3_cross(*up, v));
  u = rwm_v3_cross(v, r);

  return rwm_m4_init_f(
    r.x, r.y, r.z, rwm_v3_dot(-(*position), r),
    u.x, u.y, u.z, rwm_v3_dot(-(*position), u),
    v.x, v.y, v.z, rwm_v3_dot(-(*position), v),
    0, 0, 0, 1.0f
  );
}


Mat4 perspective(float fov_y, float near, float far, float aspect) {
  float top = tan(rwm_to_radians(fov_y)/2.0f) * near;
  float bottom = -top;
  float right = top * aspect;
  float left = -right;
  Mat4 result = rwm_m4_init_f(
    (2.0f*near)/(right-left), 0, (right+left)/(right-left), 0,
    0, (2.0f*near)/(top-bottom), (top+bottom)/(top-bottom), 0,
    0, 0, -(far+near)/(far-near), -(2.0*far*near)/(far-near),
    0, 0, -1.0f, 0
  );
  return result;
}

void Camera::update(float dt) {
  // pos->z = sin(dt*0.05);
  bool has_moved = false;
  Vec3 camera_dir = rwm_v3_normalize(rwm_v3_subtract(this->target, this->pos));

  if (is_mouse_down(SDL_BUTTON_LEFT)) {
    int x, y;
    SDL_GetMouseState(&x, &y);
    //SDL_GetGlobalMouseState(&x, &y);
    Vec2 mouse_delta = rwm_v2_init((float) x - this->mouse_pos.x, (float) y - this->mouse_pos.y);
    // rwm_v2_puts(&mouse_delta);
    this->yaw = 0.002 * mouse_delta.x;
    this->pitch = 0.002 * mouse_delta.y;
    this->mouse_pos = rwm_v2_init((float)x, (float)y);
    // printf("x:  %d, y: %d,  x: %f, y: %f, dx: %f, dy: %f, yaw: %f, pitch: %f\n",  x,  y, mouse_pos.x, mouse_pos.y, mouse_delta.x, mouse_delta.y, yaw, pitch);
    Quaternion pitch_q = rwm_q_init_rotation(rwm_v3_cross(camera_dir, this->up), this->pitch);
    Quaternion yaw_q = rwm_q_init_rotation(this->up, this->yaw);
    Quaternion dir_q = rwm_q_normalize(rwm_q_mult(pitch_q, yaw_q));

    camera_dir = rwm_q_v3_apply_rotation(dir_q, camera_dir);
    this->target = this->pos + camera_dir;
    this->right = rwm_v3_normalize(rwm_v3_cross(camera_dir, this->up));
    has_moved = true;
  }

  const float velocity = 0.01;
  if (is_down(SDL_SCANCODE_LEFT) || is_down(SDL_SCANCODE_A)) {
    //this->pos.x -= velocity * dt;
    this->pos = rwm_v3_add(this->pos, -dt * velocity * this->right);
    this->target = this->pos + camera_dir;
    has_moved = true;
  }
  if (is_down(SDL_SCANCODE_RIGHT) || is_down(SDL_SCANCODE_D)) {
    //this->pos.x += velocity * dt;
    this->pos = rwm_v3_add(this->pos, dt * velocity * this->right);
    this->target = this->pos + camera_dir;
    has_moved = true;
  }
  if (is_down(SDL_SCANCODE_UP) || is_down(SDL_SCANCODE_W)) {
    this->pos = rwm_v3_add(this->pos, dt * velocity * camera_dir);
    this->target = this->pos + camera_dir;
    has_moved = true;
  }
  if (is_down(SDL_SCANCODE_DOWN) || is_down(SDL_SCANCODE_S)) {
    this->pos = rwm_v3_add(this->pos, -dt * velocity * camera_dir);
    this->target = this->pos + camera_dir;
    has_moved = true;
  }
  if (is_down(SDL_SCANCODE_SPACE) && is_down(SDL_SCANCODE_LSHIFT)) {
    this->pos = rwm_v3_add(this->pos, -dt*velocity * up);
    this->target = this->pos + camera_dir;
    has_moved = true;
  } else if (is_down(SDL_SCANCODE_SPACE)) {
    this->pos = rwm_v3_add(this->pos, dt * velocity * up);
    this->target = this->pos + camera_dir;
    has_moved = true;
  }

    // if (is_mouse_pressed(SDL_BUTTON_LEFT)) {
    //int x, y;
    //if (is_mouse_down(SDL_BUTTON_LEFT)) {
    //  SDL_ShowCursor(false);
      //SDL_GetMouseState(&x, &y);
      //printf("x dir: %d, y dir: %d\n",  (SCREEN_WIDTH/2 - x), (SCREEN_HEIGHT/2 - y));
    //  SDL_WarpMouseInWindow(win, SCREEN_WIDTH/2, SCREEN_HEIGHT/2);
    //} else {
    //  SDL_ShowCursor(true);
    //}


  if (has_moved) {
    this->view_mat = get_view_mat(&this->pos, &this->target, &this->up);
    // rwm_m4_puts(&this->view_mat);
  }
}