#pragma once
#include <rw_math.h>

struct Shader {
  int id;
  Shader(const char *vs_path, const char *fs_path);
  void use();
  int get_unif_loc(const char *unif_name);
  void set_unif_1f(const char *unif_name, float f);
  void set_unif_1i(const char *unif_name, int f);
  void set_unif_1u(const char *unif_name, unsigned int f);
  void set_unif_3fv(const char *unif_name, Vec3 *v);
  void set_unif_4fv(const char *unif_name, Vec4 *v);
  void set_unif_mat4(const char *unif_name, Mat4 *m);
};

