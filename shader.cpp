#include "shader.h"
#include <iostream>
#include <stdint.h>
#include <fstream>
#include <sstream>
#include <string>
#include <glad/glad.h>

// NOTE(ray): Don't worry about error handling right now
Shader::Shader(const char *vs_path, const char *fs_path) {
  std::ifstream vs_file(vs_path);
  std::ifstream fs_file(fs_path);

  std::stringstream buf;
  buf << vs_file.rdbuf();
  std::string vs_src_tmp = buf.str();
  buf.str("");
  buf << fs_file.rdbuf();
  std::string fs_src_tmp = buf.str();
  const char *v_src = vs_src_tmp.c_str();
  const char *f_src = fs_src_tmp.c_str();

  int success;
  char info_log[1024];

  // Compile the vertex shader
  uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &v_src, NULL);
  glCompileShader(vs);
  glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vs, 1024, NULL, info_log);
    printf("Error: Vertex shader compilation failed\n%s\n", info_log);
    vs_file.close();
  }

  uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, &f_src, NULL);
  glCompileShader(fs);
  glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fs, 1024, NULL, info_log);
    printf("Error: Fragment shader compilation failed\n%s\n", info_log);
    fs_file.close();
  }

  id = glCreateProgram();
  glAttachShader(id, vs);
  glAttachShader(id, fs);
  glLinkProgram(id);
  glGetProgramiv(id, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(id, 1024, NULL, info_log);
    printf("Error: Shader failed linking\n%s\n", info_log);
  }

  glDeleteShader(vs);
  glDeleteShader(fs);

  vs_file.close();
  fs_file.close();
}

void Shader::use() {
  glUseProgram(id);
}

int Shader::get_unif_loc(const char *unif_name) {
  int location;
  // TODO(ray): Cache
  location = glGetUniformLocation(id, unif_name);
  return location;
}

void Shader::set_unif_1f(const char *unif_name, float f) {
  int location = this->get_unif_loc(unif_name);
  glUniform1f(location, f);
}

void Shader::set_unif_1i(const char *unif_name, int i) {
  int location = this->get_unif_loc(unif_name);
  glUniform1i(location, i);
}

void Shader::set_unif_1u(const char *unif_name, unsigned int u) {
  int location = this->get_unif_loc(unif_name);
  glUniform1ui(location, u);
}

void Shader::set_unif_3fv(const char *unif_name, Vec3 *v) {
  int location = this->get_unif_loc(unif_name);
  glUniform3fv(location, 1, (float *) v);
}

void Shader::set_unif_4fv(const char *unif_name, Vec4 *v) {
  int location = this->get_unif_loc(unif_name);
  glUniform4fv(location, 1, (float *) v);
}

void Shader::set_unif_mat4(const char *unif_name, Mat4 *m) {
  int location = this->get_unif_loc(unif_name);
  glUniformMatrix4fv(location, 1, GL_TRUE, &(m->e[0][0]));
}

