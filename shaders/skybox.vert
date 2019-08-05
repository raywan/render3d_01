#version 410

layout (location = 0) in vec3 i_pos;

out vec3 TexCoord;

uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
  TexCoord = i_pos;
  vec4 pos = u_projection * u_view * vec4(i_pos, 1.0);
  gl_Position = pos.xyww;
}