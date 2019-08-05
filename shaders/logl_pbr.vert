#version 410

layout (location = 0) in vec3 i_pos;
layout (location = 1) in vec2 i_tex_coord;
layout (location = 2) in vec3 i_normal;

out vec3 WorldPos;
out vec2 TexCoords;
out vec3 Normal;

uniform mat4 u_model = mat4(1.0);
uniform mat4 u_view = mat4(1.0);
uniform mat4 u_projection;

void main() {
  vec4 world_pos = u_model * vec4(i_pos, 1.0);
  WorldPos = world_pos.xyz;
  Normal = (mat4(transpose(inverse(u_model))) * vec4(i_normal, 0.0)).xyz;
  TexCoords = i_tex_coord;

  gl_Position = u_projection * u_view * world_pos;
}
