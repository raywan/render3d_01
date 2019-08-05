#version 410

layout (location = 0) in vec3 i_pos;
layout (location = 1) in vec2 i_tex_coord;

out vec2 TexCoord;

void main() {
  TexCoord = i_tex_coord;
  gl_Position = vec4(i_pos, 1.0);
}