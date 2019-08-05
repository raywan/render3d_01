#pragma once

#include <vector>
#include <rw_math.h>

#define NUM_PACKED_ELEMENTS 8

enum class FaceFormat {
  UNDEFINED,
  V,
  VVT,
  VVN,
  VVTVN
};

typedef struct Mesh {
  std::vector<int> f;
  std::vector<int> v_idx;
  std::vector<int> uv_idx;
  std::vector<int> n_idx;
  // Raw data from the OBJ file
  std::vector<Vec3> v;
  std::vector<Vec2> uv;
  std::vector<Vec3> n;
  // Processed face data, index aligned and for GL_ARRAY_BUFFER rendering
  // NOTE(ray): This is useful for debugging, but we'll be using the packed data
  // most of the time
  std::vector<Vec3> buf_v;
  std::vector<Vec2> buf_uv;
  std::vector<Vec3> buf_n;

  // For EBO
  float *packed;

  FaceFormat format;
} Mesh;

uint8_t load_obj(Mesh *out_obj, const char *obj_path);
