#version 410

layout (location = 0) in vec3 i_pos;
layout (location = 1) in vec2 i_tex_coords;

out vec2 TexCoords;

void main() {
  TexCoords = i_tex_coords;
  gl_Position = vec4(i_pos, 1.0);
}