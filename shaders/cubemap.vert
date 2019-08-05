#version 410

layout (location = 0) in vec3 i_pos;

out vec3 WorldPos;

uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
  WorldPos = i_pos;
  gl_Position = u_projection * u_view * vec4(i_pos, 1.0);
}