#include <stdio.h>
#include <string>
#include <stdint.h>
#include <SDL.h>
#include <SDL_main.h>
#include <glad/glad.h>
#include <math.h>
#include <unordered_map>

#define RWM_IMPLEMENTATION
#include <rw_math.h>
#define RWTR_IMPLEMENTATION
#include <rw_transform.h>
#define RWTM_IMPLEMENTATION
#include <rw_time.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <TaskScheduler_c.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "shader.h"
#include "camera.h"
#include "input.h"
#include "global.h"
#include "mesh.h"

constexpr int TICKS_PER_SECOND = 60;
constexpr int SKIP_TICKS = 1000 / TICKS_PER_SECOND;
constexpr int MAX_FRAMESKIP = 10;
constexpr uint32_t SHADOW_WIDTH = 1024;
constexpr uint32_t SHADOW_HEIGHT = 1024;

enkiTaskScheduler *g_pTS;
enkiTaskSet *pTask;

bool quit = false;
bool is_next_state_wire = true;
SDL_Window* win;
SDL_GLContext gl_context;
uint32_t quad_vao = 0;
uint32_t cube_vao = 0;
uint32_t skybox_vao = 0;

uint32_t plane_vao = 0;
uint32_t plane_vbo = 0;

uint32_t sphere_vao = 0;
uint32_t sphere_vbo = 0;
uint32_t sphere_ebo = 0;

uint32_t mesh_vao = 0;
uint32_t mesh_vbo = 0;
uint32_t mesh_ebo = 0;

uint32_t to_mesh_vao = 0;
uint32_t to_mesh_v_vbo = 0;
uint32_t to_mesh_n_vbo = 0;
uint32_t to_mesh_t_vbo = 0;
uint32_t to_mesh_ebo = 0;

uint32_t idx_count = 0;

// These are globals for cubemap capturing
Mat4 capture_persp_mat = perspective(90.0f, 0.1f, 10.0f, 1.0f);
Vec3 capture_pos = rwm_v3_zero();
Vec3 capture_targets[6] = {
  rwm_v3_init(1.0, 0.0, 0.0),
  rwm_v3_init(-1.0, 0.0, 0.0),
  rwm_v3_init(0.0, 1.0, 0.0),
  rwm_v3_init(0.0, -1.0, 0.0),
  rwm_v3_init(0.0, 0.0, 1.0),
  rwm_v3_init(0.0, 0.0, -1.0),
};
Vec3 capture_ups[6] = {
  rwm_v3_init(0.0, -1.0, 0.0),
  rwm_v3_init(0.0, -1.0, 0.0),
  rwm_v3_init(0.0, 0.0, 1.0),
  rwm_v3_init(0.0, 0.0, -1.0),
  rwm_v3_init(0.0, -1.0, 0.0),
  rwm_v3_init(0.0, -1.0, 0.0),
};

struct PBRTextures {
  uint32_t albedo_tid;
  uint32_t metallic_tid;
  uint32_t roughness_tid;
  uint32_t normal_tid;
  uint32_t ao_tid;
};

struct TinyObjMesh {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  //float *packed;
  std::vector<float> packed;
  uint32_t total_vertices;
};

struct MeshFull {
  Mesh mesh;
  TinyObjMesh to_mesh;
  PBRTextures textures;
};

struct GBuffer {
  uint32_t fbo;
  uint32_t rbo;
  uint32_t g_pos;
  uint32_t g_normal;
  uint32_t g_albedo;
  uint32_t g_metallic;
  uint32_t g_roughness;
  uint32_t g_ao;
};

PBRTextures aluminium;
PBRTextures concrete;

MeshFull cerberus;
MeshFull bunny;

void render_plane();
void render_sphere();
void render_cube();
void render_quad();
void render_to_quad(Shader &quad_shader, uint32_t tex_color_buffer);
void render_skybox(Shader &skybox_shader, Camera &camera, uint32_t skybox_tid);
void render_mesh(Mesh &m);
void render_mesh_full(MeshFull &m);

TinyObjMesh tinyobj_load(std::string path) {
  TinyObjMesh result;
  std::string warn;
  std::string err;

  printf("tinyobj: Loading obj: %s\n", path.c_str());
  bool ret = tinyobj::LoadObj(&result.attrib, &result.shapes, &result.materials, &warn, &err, path.c_str());

  if (!warn.empty()) {
    printf("%s\n", warn.c_str());
  }

  if (!err.empty()) {
    printf("%s\n", err.c_str());
  }

  if (!ret) {
    exit(1);
  }

  // Don't deal with multiple things in an obj file for now
  assert(result.shapes.size() == 1);

  result.total_vertices = 0;
  for (size_t f = 0; f < result.shapes[0].mesh.num_face_vertices.size(); f++) {
    int fv = result.shapes[0].mesh.num_face_vertices[f];
    result.total_vertices += fv;
  }

  // Pack the data
  result.packed.reserve(NUM_PACKED_ELEMENTS * result.total_vertices);
  size_t index_offset = 0;
  for (size_t f = 0; f < result.shapes[0].mesh.num_face_vertices.size(); f++) {
    int fv = result.shapes[0].mesh.num_face_vertices[f];
    for (size_t v = 0; v < fv; v++) {
      tinyobj::index_t idx = result.shapes[0].mesh.indices[index_offset + v];
      result.packed.push_back(result.attrib.vertices[3*idx.vertex_index+0]);
      result.packed.push_back(result.attrib.vertices[3*idx.vertex_index+1]);
      result.packed.push_back(result.attrib.vertices[3*idx.vertex_index+2]);
      result.packed.push_back(result.attrib.texcoords[2*idx.texcoord_index+0]);
      result.packed.push_back(result.attrib.texcoords[2*idx.texcoord_index+1]);
      result.packed.push_back(result.attrib.normals[3*idx.normal_index+0]);
      result.packed.push_back(result.attrib.normals[3*idx.normal_index+1]);
      result.packed.push_back(result.attrib.normals[3*idx.normal_index+2]);
    }
    index_offset += fv;
  }

  return result;
}

void set_clear_color(int state) {
  switch (state) {
    case 0:
    glClearColor(0.0, 0.0, 0.0, 1.0);
    break;
    case 1:
    glClearColor(1.0, 0.0, 0.0, 1.0);
    break;
    case 2:
    glClearColor(0.0, 1.0, 0.0, 1.0);
    break;
    case 3:
    glClearColor(0.0, 0.0, 1.0, 1.0);
    break;
    default:
    glClearColor(1.0, 1.0, 1.0, 1.0);
    break;
  }
}

uint32_t load_texture(const char *path) {
  stbi_set_flip_vertically_on_load(true);
  uint32_t tid = 0;
  int w, h, num_components;
  unsigned char *data = stbi_load(path, &w, &h, &num_components, 0);
  if (data) {
    glGenTextures(1, &tid);
    GLenum format = GL_RED;
    if (num_components == 1) {
      format = GL_RED;
    } else if (num_components == 3) {
      format = GL_RGB;
    } else if (num_components == 4) {
      format = GL_RGBA;
    }
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    printf("Loaded texture: %s as tid %d\n", path, tid);
  } else {
    printf("ERROR: Texture could not be not loaded %s\n", path);
  }
  stbi_image_free(data);
  return tid;
}

void bind_pbr_textures(PBRTextures &t) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, t.albedo_tid);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, t.normal_tid);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, t.metallic_tid);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, t.roughness_tid);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, t.ao_tid);
}

void deferred_bind_pbr_textures(PBRTextures &t) {
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, t.metallic_tid);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, t.roughness_tid);
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_2D, t.ao_tid);
}

uint32_t load_cubemap(const std::string &dir) {
  uint32_t tid;
  glGenTextures(1, &tid);
  glBindTexture(GL_TEXTURE_CUBE_MAP, tid);
  std::string names[6] = {
    "right.jpg",
    "left.jpg",
    "top.jpg",
    "bottom.jpg",
    "front.jpg",
    "back.jpg"
  };

  int w, h, nrc;
  for (int i = 0; i < 6; i++) {
    std::string path = dir + "/" + names[i];
    unsigned char *data = stbi_load(path.c_str(), &w, &h, &nrc, 0);
    if (data) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      printf("Loaded cube map face: %s\n", path.c_str());
      stbi_image_free(data);
    } else {
      printf("ERROR: Texture could not be not loaded %s\n", path.c_str());
      stbi_image_free(data);
      return 0;
    }
  }
  printf("Loaded cube map texture: %d\n", tid);
  return tid;
}

uint32_t load_hdr_texture(const std::string &path) {
  stbi_set_flip_vertically_on_load(true);
  uint32_t tid = 0;
  int w, h, num_components;
  float *data = stbi_loadf(path.c_str(), &w, &h, &num_components, 0);
  if (data) {
    glGenTextures(1, &tid);
    glBindTexture(GL_TEXTURE_2D, tid);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    printf("Loaded HDR texture: %s as tid %d\n", path.c_str(), tid);
  } else {
    printf("ERROR: HDR texture could not be not loaded %s\n", path.c_str());
  }
  stbi_image_free(data);
  return tid;
}

uint32_t create_env_map(Shader &env_shader, const std::string &hdr_path) {
  uint32_t capture_fb;
  uint32_t capture_rb;
  glGenFramebuffers(1, &capture_fb);
  glGenRenderbuffers(1, &capture_rb);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fb);
  glBindRenderbuffer(GL_RENDERBUFFER, capture_rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 512, 512);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rb);

  uint32_t hdr_tid = load_hdr_texture(hdr_path);
  uint32_t cube_map_tid;
  glGenTextures(1, &cube_map_tid);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cube_map_tid);
  for (int i = 0; i < 6; i++) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, NULL);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  env_shader.use();
  env_shader.set_unif_1i("equirectangular_map", 0);
  env_shader.set_unif_mat4("u_projection", &capture_persp_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, hdr_tid);
  glViewport(0, 0, 512, 512);
  for (int i = 0; i < 6; i++) {
    Mat4 view = get_view_mat(&capture_pos, &capture_targets[i], &capture_ups[i]);
    env_shader.set_unif_mat4("u_view", &view);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cube_map_tid, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_cube();
  }

  glDeleteFramebuffers(1, &capture_fb);
  glDeleteRenderbuffers(1, &capture_rb);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  printf("Created environment map texture with tid %d\n", cube_map_tid);
  return cube_map_tid;
}

uint32_t create_irradiance_map(Shader &irr_shader, uint32 env_map_tid) {
  uint32_t irr_map_tid;
  glGenTextures(1, &irr_map_tid);
  glBindTexture(GL_TEXTURE_CUBE_MAP, irr_map_tid);
  for (int i = 0; i < 6; i++) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, NULL);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  uint32_t capture_fb;
  uint32_t capture_rb;
  glGenFramebuffers(1, &capture_fb);
  glGenRenderbuffers(1, &capture_rb);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fb);
  glBindRenderbuffer(GL_RENDERBUFFER, capture_rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rb);

  irr_shader.use();
  irr_shader.set_unif_1i("environment_map", 0);
  irr_shader.set_unif_mat4("u_projection", &capture_persp_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, env_map_tid);
  glViewport(0, 0, 32, 32);
  for (int i = 0; i < 6; i++) {
    Mat4 view = get_view_mat(&capture_pos, &capture_targets[i], &capture_ups[i]);
    irr_shader.set_unif_mat4("u_view", &view);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irr_map_tid, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_cube();
  }

  glDeleteFramebuffers(1, &capture_fb);
  glDeleteRenderbuffers(1, &capture_rb);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  printf("Created irradiance map texture with tid %d\n", irr_map_tid);

  return irr_map_tid;
}

uint32_t create_prefilter_map(Shader &prefilter_shader, uint32_t env_map_tid) {
  uint32_t capture_fb;
  uint32_t capture_rb;
  glGenFramebuffers(1, &capture_fb);
  glGenRenderbuffers(1, &capture_rb);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fb);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rb);

  uint32_t prefilter_map_tid;
  glGenTextures(1, &prefilter_map_tid);
  glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map_tid);
  for (int i = 0; i < 6; i++) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, NULL);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

  prefilter_shader.use();
  prefilter_shader.set_unif_1i("environment_map", 0);
  prefilter_shader.set_unif_mat4("u_projection", &capture_persp_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, env_map_tid);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fb);
  constexpr uint32_t max_mip_level = 5;
  for (int mip = 0; mip < max_mip_level; mip++) {
    uint32_t mip_width = 128 * std::pow(0.5, mip);
    uint32_t mip_height = mip_width;
    glBindRenderbuffer(GL_RENDERBUFFER, capture_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mip_width, mip_height);
    glViewport(0, 0, mip_width, mip_height);

    float roughness = (float) mip / (float) (max_mip_level - 1);
    prefilter_shader.set_unif_1f("roughness", roughness);
    for (int j = 0; j < 6; j++) {
      Mat4 view = get_view_mat(&capture_pos, &capture_targets[j], &capture_ups[j]);
      prefilter_shader.set_unif_mat4("u_view", &view);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, prefilter_map_tid, mip);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      render_cube();
    }
  }

  glDeleteFramebuffers(1, &capture_fb);
  glDeleteRenderbuffers(1, &capture_rb);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  printf("Created prefilter map texture with tid %d\n", prefilter_map_tid);

  return prefilter_map_tid;
}

uint32_t create_brdf_lut(Shader &brdf_shader) {
  uint32_t brdf_tid;
  glGenTextures(1, &brdf_tid);
  glBindTexture(GL_TEXTURE_2D, brdf_tid);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  uint32_t capture_fb;
  uint32_t capture_rb;
  glGenFramebuffers(1, &capture_fb);
  glGenRenderbuffers(1, &capture_rb);
  glBindFramebuffer(GL_FRAMEBUFFER, capture_fb);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdf_tid, 0);

  glViewport(0, 0, 512, 512);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  brdf_shader.use();
  render_quad();

  glDeleteFramebuffers(1, &capture_fb);
  glDeleteRenderbuffers(1, &capture_rb);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  printf("Created BRDF LUT texture with tid %d\n", brdf_tid);

  return brdf_tid;
}

GBuffer create_g_buffer() {
  GBuffer result;
  glGenFramebuffers(1, &result.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, result.fbo);
  glGenTextures(1, &result.g_pos);
  glBindTexture(GL_TEXTURE_2D, result.g_pos);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, result.g_pos, 0);
  glGenTextures(1, &result.g_normal);
  glBindTexture(GL_TEXTURE_2D, result.g_normal);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, result.g_normal, 0);
  glGenTextures(1, &result.g_albedo);
  glBindTexture(GL_TEXTURE_2D, result.g_albedo);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, result.g_albedo, 0);
  glGenTextures(1, &result.g_metallic);
  glBindTexture(GL_TEXTURE_2D, result.g_metallic);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, result.g_metallic, 0);
  glGenTextures(1, &result.g_roughness);
  glBindTexture(GL_TEXTURE_2D, result.g_roughness);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, result.g_roughness, 0);
  glGenTextures(1, &result.g_ao);
  glBindTexture(GL_TEXTURE_2D, result.g_ao);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, result.g_ao, 0);
  uint32_t attachments[6] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 };
  glDrawBuffers(6, attachments);
  glGenRenderbuffers(1, &result.rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, result.rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCREEN_WIDTH, SCREEN_HEIGHT);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, result.rbo);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("g-buffer not created properly\n");
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return result;
}

uint32_t create_shadow_map() {
  uint32_t depth_fbo;
  glGenFramebuffers(1, &depth_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo);

  uint32_t depth_tid;
  glGenTextures(1, &depth_tid);
  glBindTexture(GL_TEXTURE_2D, depth_tid);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  float border_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_tid, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("depth fb not created properly\n");
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return depth_fbo;
}

uint32_t create_shadow_cube_map() {
  uint32_t depth_fbo;
  glGenFramebuffers(1, &depth_fbo);

  uint32_t depth_cube_map_tid;
  glGenTextures(1, &depth_cube_map_tid);
  glBindTexture(GL_TEXTURE_CUBE_MAP, depth_cube_map_tid);
  for (int i = 0; i < 6; i++) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth_cube_map_tid, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return depth_cube_map_tid;
}



void render_scene(Shader *cur_shader) {
  Transform ts = rwtr_trs(
    rwm_v3_init(0.0, 0.0, 10.0),
    rwm_v3_init(2.0, 2.0, 2.0),
    RWTR_NO_AXIS, 0.0f
  );
  Quaternion q1 = rwm_q_init_rotation(rwm_v3_init(0.0, 1.0, 0.0), rwm_to_radians(-90.0f));
  Quaternion q2 = rwm_q_init_rotation(rwm_v3_init(1.0, 0.0, 0.0), rwm_to_radians(45.0f));
  Transform r = rwtr_init_rotate_q(rwm_q_mult(q1, q2));
  Transform model_tr = rwtr_compose(&ts, &r);
  cur_shader->set_unif_mat4("u_model", &model_tr.t);
  render_mesh_full(cerberus);

  model_tr = rwtr_trs(
    rwm_v3_init(1.0, 0.0, 8.6),
    rwm_v3_init(1.0, 1.0, 1.0),
    RWTR_NO_AXIS, 0.0f
  );
  cur_shader->set_unif_mat4("u_model", &model_tr.t);
  render_mesh_full(bunny);

  bind_pbr_textures(concrete);
  render_plane();

  constexpr int num_rows = 7;
  constexpr int num_cols = 7;
  constexpr float spacing = 2.5;
#if 1
  bind_pbr_textures(aluminium);

  for (int i = 0; i < num_rows; i++) {
    float metallic = (float)i/(float)num_rows;
    cur_shader->set_unif_1f("u_metallic", metallic);
    for (int j = 0; j < num_cols; j++) {
      float roughness = rwm_clamp((float)j/(float)num_cols, 0.05f, 1.0f);
      cur_shader->set_unif_1f("u_roughness", roughness);
      Mat4 model = rwm_m4_identity();
      model.e[0][3] = (j - (num_cols/2)) * spacing;
      model.e[1][3] = (i - (num_rows/2)) * spacing;
      cur_shader->set_unif_mat4("u_model", &model);
      render_sphere();
    }
  }
#endif
}

int main(int argc, char* argv[]) {
  rwtm_init();
  puts("Hello");
  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5 );
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );

  // NOTE(ray): Passing in NULL means we're loading in the default OpenGL library.
  // Even if we don't explicity call this function, the default OpenGL library will
  // be loaded anyway upon window creation.
  // SDL_GL_LoadLibrary(NULL);
  win = SDL_CreateWindow("render3d_01", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  gl_context = SDL_GL_CreateContext(win);
  gladLoadGLLoader(SDL_GL_GetProcAddress);
  printf("Vendor:   %s\n", glGetString(GL_VENDOR));
  printf("Renderer: %s\n", glGetString(GL_RENDERER));
  printf("Version:  %s\n", glGetString(GL_VERSION));

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  GBuffer g_buffer = create_g_buffer();
  // Load textures
  // TODO(ray): Load these on another thread
#if 0
  PBRTextures chipped_paint;
  chipped_paint.albedo_tid = load_texture("assets/chipped_paint_metal/chipped-paint-metal-albedo.png");
  chipped_paint.normal_tid = load_texture("assets/chipped_paint_metal/chipped-paint-metal-normal-dx.png");
  chipped_paint.metallic_tid = load_texture("assets/chipped_paint_metal/chipped-paint-metal-metal.png");
  chipped_paint.roughness_tid = load_texture("assets/chipped_paint_metal/chipped-paint-metal-rough2.png");
  chipped_paint.ao_tid = load_texture("assets/chipped_paint_metal/chipped-paint-ao.png");
#endif

  concrete.albedo_tid = load_texture("assets/concrete/concrete_floor_02_diff_1k.jpg");
  concrete.normal_tid = load_texture("assets/concrete/concrete_floor_02_Nor_1k.jpg");
  concrete.metallic_tid = load_texture("assets/concrete/concrete_floor_02_spec_1k.jpg");
  concrete.roughness_tid = load_texture("assets/concrete/concrete_floor_02_rough_1k.jpg");
  concrete.ao_tid = load_texture("assets/concrete/concrete_floor_02_AO_1k.jpg");

  aluminium.albedo_tid = load_texture("assets/scuffed_aluminum/Aluminum-Scuffed_basecolor.png");
  aluminium.normal_tid = load_texture("assets/scuffed_aluminum/Aluminum-Scuffed_normal.png");
  aluminium.metallic_tid = load_texture("assets/scuffed_aluminum/Aluminum-Scuffed_metallic.png");
  aluminium.roughness_tid = load_texture("assets/scuffed_aluminum/Aluminum-Scuffed_roughness.png");
  aluminium.ao_tid = load_texture("assets/scuffed_aluminum/Aluminum-Scuffed_metallic.png");

  uint32_t cube_map_tid = load_cubemap("assets/skybox");

  // Load obj
  cerberus.to_mesh = tinyobj_load("assets/cerberus/cerberus.obj");
  cerberus.textures.albedo_tid = load_texture("assets/cerberus/Cerberus_A.tga");
  cerberus.textures.normal_tid = load_texture("assets/cerberus/Cerberus_N.tga");
  cerberus.textures.metallic_tid = load_texture("assets/cerberus/Cerberus_M.tga");
  cerberus.textures.roughness_tid = load_texture("assets/cerberus/Cerberus_R.tga");
  cerberus.textures.ao_tid = load_texture("assets/cerberus/Cerberus_AO.tga");

  bunny.to_mesh = tinyobj_load("assets/bunny.obj");
  bunny.textures = aluminium;

  // Load shader
  Shader basic_s = Shader("shaders/basic.vert", "shaders/basic_pbr.frag");
  Shader logl_s = Shader("shaders/logl_pbr.vert", "shaders/logl_pbr_ibl_full.frag");
  Shader tex_s = Shader("shaders/logl_pbr.vert", "shaders/texture_pbr.frag");
  Shader tex_ibl_s = Shader("shaders/logl_pbr.vert", "shaders/texture_pbr_ibl.frag");
  Shader tex_ibl_full_s = Shader("shaders/logl_pbr.vert", "shaders/texture_pbr_ibl_full.frag");
  Shader quad_s = Shader("shaders/quad.vert", "shaders/quad.frag");
  Shader skybox_s = Shader("shaders/skybox.vert", "shaders/skybox.frag");
  Shader env_s = Shader("shaders/cubemap.vert", "shaders/equirectangular_to_cubemap.frag");
  Shader irradiance_map_s = Shader("shaders/cubemap.vert", "shaders/irradiance_convolution.frag");
  Shader prefilter_s = Shader("shaders/cubemap.vert", "shaders/prefilter_convolution.frag");
  Shader brdf_s = Shader("shaders/quad.vert", "shaders/brdf.frag");
  Shader solid_s = Shader("shaders/logl_pbr.vert", "shaders/solid.frag");
  Shader deferred_geometry_s = Shader("shaders/logl_pbr.vert", "shaders/deferred_geometry.frag");
  Shader deferred_pbr_s = Shader("shaders/deferred_pbr.vert", "shaders/deferred_pbr.frag");
  Shader shadow_s = Shader("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");

  uint32_t env_map_tid = create_env_map(env_s, "assets/Tokyo_BigSight_3k.hdr");
  //uint32_t env_map_tid = create_env_map(env_s, "assets/20_Subway_Lights_3k.hdr");
  uint32_t irradiance_map_tid = create_irradiance_map(irradiance_map_s, env_map_tid);
  uint32_t prefilter_map_tid = create_prefilter_map(prefilter_s, env_map_tid);
  uint32_t brdf_lut_tid = create_brdf_lut(brdf_s);

  //Shader *cur_shader = &logl_s;
  Shader *cur_shader = &tex_ibl_full_s;
  cur_shader->use();

  cur_shader->set_unif_1i("u_albedo_map", 0);
  cur_shader->set_unif_1i("u_normal_map", 1);
  cur_shader->set_unif_1i("u_metallic_map", 2);
  cur_shader->set_unif_1i("u_roughness_map", 3);
  cur_shader->set_unif_1i("u_ao_map", 4);
  cur_shader->set_unif_1i("u_irradiance_map", 5);
  cur_shader->set_unif_1i("u_prefilter_map", 6);
  cur_shader->set_unif_1i("u_brdf_lut", 7);

  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_map_tid);
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map_tid);
  glActiveTexture(GL_TEXTURE7);
  glBindTexture(GL_TEXTURE_2D, brdf_lut_tid);

  uint32_t depth_fb = create_shadow_map();

  // Create another framebuffer to render to
  uint32_t fb;
  glGenFramebuffers(1, &fb);
  glBindFramebuffer(GL_FRAMEBUFFER, fb);
  // Create texture
  uint32_t tex_color_buf, tex_depth_buf;
  glGenTextures(1, &tex_color_buf);
  printf("tex_color_buf tid: %d\n", tex_color_buf);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_color_buf);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCREEN_WIDTH, SCREEN_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_color_buf, 0);

  // Using a renderbuffer instead
  uint32_t rbo;
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCREEN_WIDTH, SCREEN_HEIGHT);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("Error: Framebuffer %d is not complete\n", fb);
    return 1;
  }

  // Create the camera
  Camera camera = Camera(rwm_v3_init(0,2,15), rwm_v3_init(0,0,-1), rwm_v3_init(0,1.0,0), 45.0, 0.1, 100.0, ASPECT_RATIO);
  puts("main view_mat");
  rwm_m4_puts(&(camera.persp_mat));
  cur_shader->set_unif_mat4("u_view", &camera.view_mat);
  cur_shader->set_unif_mat4("u_projection", &camera.persp_mat);

  int w, h;
  SDL_GetWindowSize(win, &w, &h);
  glViewport(0, 0, w, h);

  SDL_GL_SetSwapInterval(0);

  uint64_t cur_time = rwtm_now();
  uint64_t frame_time = 0;

  int state = 0;
  bool is_next_shader_logl = true;
  bool holding_2 = false;

  std::vector<Vec3> light_positions = {
    rwm_v3_init(-9.0f,  9.0f, 9.0f),
    rwm_v3_init( 9.0f,  9.0f, 9.0f),
    rwm_v3_init(-9.0f, -9.0f, 9.0f),
    rwm_v3_init( 9.0f, -9.0f, 9.0f),
  };
  std::vector<Vec3> light_colors = {
    rwm_v3_init(100.0f, 300.0f, 300.0f),
    rwm_v3_init(300.0f, 147.0f, 0.0f),
    rwm_v3_init(300.0f, 300.0f, 300.0f),
    rwm_v3_init(300.0f, 300.0f, 300.0f)
  };

  Vec3 zero = rwm_v3_zero();
  Vec3 up = rwm_v3_init(0.0, 1.0, 0.0);
  Mat4 light_view_mat = get_view_mat(&light_positions[1], &zero, &up);
  Mat4 light_projection_mat = orthographic(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 7.5f);

  bool use_texture_pbr = true;
  while (!quit) {
    int64_t new_time = rwtm_now();
    frame_time = new_time - cur_time;
    cur_time = new_time;
    printf("frame time %f\n", rwtm_to_ms(frame_time));

    process_raw_input(&quit);

    if (is_down(SDL_SCANCODE_ESCAPE)) {
      quit = true;
    }
    if (is_pressed(SDL_SCANCODE_1)) {
      puts("PRESSED 1");
      // set_clear_color(state);
      state = ++state % 7;
    }
    if (is_down(SDL_SCANCODE_2)) {
      set_clear_color(5);
      holding_2 = true;
    } else if (holding_2 && is_up(SDL_SCANCODE_2)) {
      set_clear_color(state);
    }
    if (is_pressed(SDL_SCANCODE_3)) {
      if (is_next_state_wire) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      }
      is_next_state_wire = !is_next_state_wire;
    }

    if (is_pressed(SDL_SCANCODE_4)) {
      cur_shader = &logl_s;
      cur_shader->use();
      cur_shader->set_unif_mat4("u_projection", &camera.persp_mat);
      cur_shader->set_unif_1i("u_irradiance_map", 0);
      cur_shader->set_unif_1i("u_prefilter_map", 1);
      cur_shader->set_unif_1i("u_brdf_lut", 2);
      use_texture_pbr = false;
    }

    if (is_pressed(SDL_SCANCODE_5)) {
      cur_shader = &tex_ibl_full_s;
      cur_shader->use();
      cur_shader->set_unif_1i("u_albedo_map", 0);
      cur_shader->set_unif_1i("u_normal_map", 1);
      cur_shader->set_unif_1i("u_metallic_map", 2);
      cur_shader->set_unif_1i("u_roughness_map", 3);
      cur_shader->set_unif_1i("u_ao_map", 4);
      cur_shader->set_unif_1i("u_irradiance_map", 5);
      cur_shader->set_unif_1i("u_prefilter_map", 6);
      cur_shader->set_unif_1i("u_brdf_lut", 7);
      cur_shader->set_unif_mat4("u_projection", &camera.persp_mat);
      use_texture_pbr = true;
    }

    if (is_mouse_scrolling() != 0) {
      printf("SCROLLING %d\n", is_mouse_scrolling());
    }

    camera.update(rwtm_to_ms(frame_time));

#if 0
    // Forward rendering
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    cur_shader->use();

    // Need to load the albedo map back in because we've replaced it when rendering the quad
    if (!use_texture_pbr) {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_map_tid);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map_tid);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, brdf_lut_tid);
    }

    cur_shader->set_unif_3fv("u_cam_pos", &camera.pos);
    cur_shader->set_unif_mat4("u_view", &camera.view_mat);
    cur_shader->set_unif_mat4("u_projection", &camera.persp_mat);

    Vec3 alb = rwm_v3_init(0.5f, 0, 0);
    cur_shader->set_unif_3fv("u_albedo", &alb);
    cur_shader->set_unif_1f("u_ao", 1.0f);
    cur_shader->set_unif_1f("u_metallic", 0.8f);
    cur_shader->set_unif_1f("u_roughness", 0.1f);

    for (int i = 0; i < sizeof(light_positions) / sizeof(light_positions[0]); i++) {
      std::string pos_name = "u_light_pos[" + std::to_string(i) + "]";
      std::string col_name = "u_light_color[" + std::to_string(i) + "]";
      cur_shader->set_unif_3fv(pos_name.c_str(), &light_positions[i]);
      cur_shader->set_unif_3fv(col_name.c_str(), &light_colors[i]);
    }

    Transform ts = rwtr_trs(
      rwm_v3_init(0.0, 0.0, 10.0),
      rwm_v3_init(2.0, 2.0, 2.0),
      RWTR_NO_AXIS, 0.0f
    );
    Quaternion q1 = rwm_q_init_rotation(rwm_v3_init(0.0, 1.0, 0.0), rwm_to_radians(-90.0f));
    Quaternion q2 = rwm_q_init_rotation(rwm_v3_init(1.0, 0.0, 0.0), rwm_to_radians(45.0f));
    Transform r = rwtr_init_rotate_q(rwm_q_mult(q1, q2));
    Transform model_tr = rwtr_compose(&ts, &r);
    cur_shader->set_unif_mat4("u_model", &model_tr.t);
    render_mesh_full(cerberus);

    model_tr = rwtr_trs(
      rwm_v3_init(1.0, 0.0, 8.6),
      rwm_v3_init(1.0, 1.0, 1.0),
      RWTR_NO_AXIS, 0.0f
    );
    cur_shader->set_unif_mat4("u_model", &model_tr.t);
    render_mesh_full(bunny);

    //bind_pbr_textures(aluminium);
    //render_plane();

#if 1
    bind_pbr_textures(aluminium);

    for (int i = 0; i < num_rows; i++) {
      float metallic = (float)i/(float)num_rows;
      cur_shader->set_unif_1f("u_metallic", metallic);
      for (int j = 0; j < num_cols; j++) {
        float roughness = rwm_clamp((float)j/(float)num_cols, 0.05f, 1.0f);
        cur_shader->set_unif_1f("u_roughness", roughness);
        Mat4 model = rwm_m4_identity();
        model.e[0][3] = (j - (num_cols/2)) * spacing;
        model.e[1][3] = (i - (num_rows/2)) * spacing;
        cur_shader->set_unif_mat4("u_model", &model);
        render_sphere();
      }
    }
#endif

    // Render the light spheres
    for (int i = 0; i < sizeof(light_positions) / sizeof(light_positions[0]); i++) {
      solid_s.use();
      solid_s.set_unif_3fv("u_light_color", &light_colors[i]);
      Mat4 model = rwm_m4_identity();
      model.e[0][3] = light_positions[i].x;
      model.e[1][3] = light_positions[i].y;
      model.e[2][3] = light_positions[i].z;
      model.e[0][0] = 0.5;
      model.e[1][1] = 0.5;
      model.e[2][2] = 0.5;
      solid_s.set_unif_mat4("u_model", &model);
      solid_s.set_unif_mat4("u_view", &camera.view_mat);
      solid_s.set_unif_mat4("u_projection", &camera.persp_mat);
      render_sphere();
    }

    render_skybox(skybox_s, camera, env_map_tid);
    render_to_quad(quad_s, tex_color_buf);
#else

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depth_fb);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    shadow_s.use();
    shadow_s.set_unif_mat4("u_light_view_mat", &light_view_mat);
    shadow_s.set_unif_mat4("u_light_projection_mat", &light_projection_mat);
    render_scene(cur_shader);

    // Deferred rendering
    // phase 1 - deferred geometry
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, g_buffer.fbo);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    cur_shader = &deferred_geometry_s;
    cur_shader->use();
    cur_shader->set_unif_1i("u_albedo_map", 0);
    cur_shader->set_unif_1i("u_normal_map", 1);
    cur_shader->set_unif_1i("u_metallic_map", 2);
    cur_shader->set_unif_1i("u_roughness_map", 3);
    cur_shader->set_unif_1i("u_ao_map", 4);
    cur_shader->set_unif_1i("u_irradiance_map", 5);
    cur_shader->set_unif_mat4("u_projection", &camera.persp_mat);
    cur_shader->set_unif_mat4("u_view", &camera.view_mat);

    render_scene(cur_shader);

    // Phase 2 - Lighting pass
#if 1
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    cur_shader = &deferred_pbr_s;
    cur_shader->use();
    cur_shader->set_unif_1i("g_position", 0);
    cur_shader->set_unif_1i("g_normal", 1);
    cur_shader->set_unif_1i("g_albedo", 2);
    cur_shader->set_unif_1i("g_metallic", 3);
    cur_shader->set_unif_1i("g_roughness", 4);
    cur_shader->set_unif_1i("g_ao", 5);
    cur_shader->set_unif_1i("u_irradiance_map", 6);
    cur_shader->set_unif_1i("u_prefilter_map", 7);
    cur_shader->set_unif_1i("u_brdf_lut", 8);
    cur_shader->set_unif_3fv("u_cam_pos", &camera.pos);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_buffer.g_pos);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_buffer.g_normal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_buffer.g_albedo);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, g_buffer.g_metallic);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, g_buffer.g_roughness);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, g_buffer.g_ao);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_map_tid);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map_tid);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, brdf_lut_tid);
    for (int i = 0; i < light_positions.size(); i++) {
      std::string pos_name = "u_light_pos[" + std::to_string(i) + "]";
      std::string col_name = "u_light_color[" + std::to_string(i) + "]";
      cur_shader->set_unif_3fv(pos_name.c_str(), &light_positions[i]);
      cur_shader->set_unif_3fv(col_name.c_str(), &light_colors[i]);
    }
    render_quad();
#endif

    // Copy depth buffer to the draw framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_buffer.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb);
    glBlitFramebuffer(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    // Render the light spheres
    for (int i = 0; i < light_positions.size(); i++) {
      solid_s.use();
      Vec3 red = rwm_v3_init(1.f, 0, 0);
      solid_s.set_unif_3fv("u_light_color", &light_colors[i]);
      Mat4 model = rwm_m4_identity();
      model.e[0][3] = light_positions[i].x;
      model.e[1][3] = light_positions[i].y;
      model.e[2][3] = light_positions[i].z;
      model.e[0][0] = 0.5;
      model.e[1][1] = 0.5;
      model.e[2][2] = 0.5;
      solid_s.set_unif_mat4("u_model", &model);
      solid_s.set_unif_mat4("u_view", &camera.view_mat);
      solid_s.set_unif_mat4("u_projection", &camera.persp_mat);
      render_sphere();
    }

    render_skybox(skybox_s, camera, env_map_tid);

    // phase 3 - finally render to default framebuffer quad
    switch (state) {
      case 0:
        render_to_quad(quad_s, tex_color_buf);
        break;
      case 1:
        render_to_quad(quad_s, g_buffer.g_pos);
        break;
      case 2:
        render_to_quad(quad_s, g_buffer.g_normal);
        break;
      case 3:
        render_to_quad(quad_s, g_buffer.g_albedo);
        break;
      case 4:
        render_to_quad(quad_s, g_buffer.g_metallic);
        break;
      case 5:
        render_to_quad(quad_s, g_buffer.g_roughness);
        break;
      case 6:
        render_to_quad(quad_s, g_buffer.g_ao);
        break;
      default:
        break;
    }
#endif

    SDL_GL_SwapWindow(win);
  }

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}

void render_plane() {
  if (plane_vao == 0) {
    constexpr float plane_vertices[] = {
      // positions            // texcoords   // normals
       10.0f, -0.5f,  10.0f,  10.0f,  0.0f,   0.0f, 1.0f, 0.0f,
      -10.0f, -0.5f,  10.0f,   0.0f,  0.0f,   0.0f, 1.0f, 0.0f,
      -10.0f, -0.5f, -10.0f,   0.0f, 10.0f,   0.0f, 1.0f, 0.0f,
       10.0f, -0.5f,  10.0f,  10.0f,  0.0f,   0.0f, 1.0f, 0.0f,
      -10.0f, -0.5f, -10.0f,   0.0f, 10.0f,   0.0f, 1.0f, 0.0f,
       10.0f, -0.5f, -10.0f,  10.0f, 10.0f,   0.0f, 1.0f, 0.0f
    };
    glGenVertexArrays(1, &plane_vao);
    uint32_t vbo;
    glGenBuffers(1, &vbo);
    glBindVertexArray(plane_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), &plane_vertices, GL_STATIC_DRAW);
    constexpr size_t stride =  8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void *)(5 * sizeof(float)));
  }
  glBindVertexArray(plane_vao);
  //glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

void render_sphere() {
  if (sphere_vao == 0) {
    glGenVertexArrays(1, &sphere_vao);
    //uint32_t vbo, ebo;
    glGenBuffers(1, &sphere_vbo);
    glGenBuffers(1, &sphere_ebo);
    std::vector<Vec3> positions;
    std::vector<Vec2> uv;
    std::vector<Vec3> normals;
    std::vector<unsigned int> indices;

    const uint32_t x_segments = 64;
    const uint32_t y_segments = 64;
    const float pi = 3.14159265359;
    for (int y = 0; y <= y_segments; y++) {
      for (int x = 0; x <= x_segments; x++) {
        float x_seg = (float)x/(float)x_segments;
        float y_seg = (float)y/(float)y_segments;
        float x_pos = std::cos(x_seg * 2.0f * pi) * std::sin(y_seg * pi);
        float y_pos = std::cos(y_seg * pi);
        float z_pos = std::sin(x_seg * 2.0f * pi) * std::sin(y_seg * pi);
        positions.push_back(rwm_v3_init(x_pos, y_pos, z_pos));
        normals.push_back(rwm_v3_init(x_pos, y_pos, z_pos));
        uv.push_back(rwm_v2_init(x_seg, y_seg));
      }
    }

    bool is_odd_row = false;
    for (int y = 0; y < y_segments; y++) {
      if (!is_odd_row) {
        for (int x = 0; x <= x_segments; x++) {
          indices.push_back(y * (x_segments+1) + x);
          indices.push_back((y+1) * (x_segments+1) + x);
        }
      } else {
        for (int x = x_segments; x >= 0; x--) {
          indices.push_back((y+1) * (x_segments+1) + x);
          indices.push_back(y * (x_segments+1) + x);
        }

      }
      is_odd_row = !is_odd_row;
    }
    idx_count = indices.size();
    std::vector<float> data;
    for (int i = 0; i < positions.size(); i++) {
      data.push_back(positions[i].x);
      data.push_back(positions[i].y);
      data.push_back(positions[i].z);
      if (uv.size() > 0) {
        data.push_back(uv[i].x);
        data.push_back(uv[i].y);
      }
      if (normals.size() > 0) {
        data.push_back(normals[i].x);
        data.push_back(normals[i].y);
        data.push_back(normals[i].z);
      }
    }
    glBindVertexArray(sphere_vao);
    glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
    constexpr size_t stride = (3 + 2 + 3) * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float)));
  }

  glBindVertexArray(sphere_vao);
  //glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
  //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo);
  glDrawElements(GL_TRIANGLE_STRIP, idx_count, GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void render_quad() {
  if (quad_vao == 0) {
    constexpr float quad_vertices[] = {
       // positions        // texture Coords
      -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
      -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
       1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
       1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    glGenVertexArrays(1, &quad_vao);
    uint32_t vbo;
    glGenBuffers(1, &vbo);
    glBindVertexArray(quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    constexpr size_t stride = 5 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
  }
  glBindVertexArray(quad_vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindVertexArray(0);

}

void render_to_quad(Shader &quad_shader, uint32_t tex_color_buffer) {
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_DEPTH_TEST);
  quad_shader.use();
  quad_shader.set_unif_1i("screen_tex", 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex_color_buffer);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  render_quad();
  if (!is_next_state_wire) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
}

void render_skybox(Shader &skybox_shader, Camera &camera, uint32_t skybox_tid) {
  glDepthFunc(GL_LEQUAL);
  skybox_shader.use();
  // Remove the translation part of the view matrix
  // We don't want the box to move along with the camera
  Mat4 view = camera.view_mat;
  view.e[0][3] = 0;
  view.e[1][3] = 0;
  view.e[2][3] = 0;
  skybox_shader.set_unif_mat4("u_view", &view);
  skybox_shader.set_unif_mat4("u_projection", &camera.persp_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_tid);
  render_cube();
  glDepthFunc(GL_LESS);
}

void render_cube() {
  if (cube_vao == 0) {
    glGenVertexArrays(1, &cube_vao);
    uint32_t vbo;
    constexpr float vertices[] = {
      // back face
      -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
       1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
       1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right
       1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
      -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
      -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
      // front face
      -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
       1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
       1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
       1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
      -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
      -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
      // left face
      -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
      -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
      -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
      -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
      -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
      -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
      // right face
       1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
       1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
       1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right
       1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
       1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
       1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left
      // bottom face
      -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
       1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
       1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
       1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
      -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
      -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
      // top face
      -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
       1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
       1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right
       1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
      -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
      -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left
    };
    glGenBuffers(1, &vbo);
    glBindVertexArray(cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    constexpr size_t stride = (3 + 3 + 2) * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
  }

  glBindVertexArray(cube_vao);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
}

void render_rw_mesh(Mesh &m) {
  if (mesh_vao == 0) {
    glGenVertexArrays(1, &mesh_vao);
    glBindVertexArray(mesh_vao);
    glGenBuffers(1, &mesh_vbo);
    glGenBuffers(1, &mesh_ebo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ebo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
  }
  glBindVertexArray(mesh_vao);
  glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * NUM_PACKED_ELEMENTS * m.v.size(), &(m.packed[0]), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(float) * m.v_idx.size(), &(m.v_idx[0]), GL_STATIC_DRAW);
  glDrawElements(GL_TRIANGLES, m.v_idx.size(), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void render_tinyobj_mesh(TinyObjMesh &m) {
  if (mesh_vao == 0) {
    glGenVertexArrays(1, &mesh_vao);
    glBindVertexArray(mesh_vao);
    glGenBuffers(1, &mesh_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
  }
  glBindVertexArray(mesh_vao);
  glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * NUM_PACKED_ELEMENTS * m.total_vertices, m.packed.data(), GL_STATIC_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, m.total_vertices);
  glBindVertexArray(0);
}

void render_mesh_full(MeshFull &m) {
  bind_pbr_textures(m.textures);
  render_tinyobj_mesh(m.to_mesh);
}
