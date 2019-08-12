#version 410

in vec3 WorldPos;
in vec2 TexCoords;
in vec3 Normal;

layout (location = 0) out vec3 g_position;
layout (location = 1) out vec3 g_normal;
layout (location = 2) out vec3 g_albedo;
layout (location = 3) out vec3 g_metallic;
layout (location = 4) out vec3 g_roughness;
layout (location = 5) out vec3 g_ao;

uniform sampler2D u_albedo_map;
uniform sampler2D u_normal_map;
uniform sampler2D u_metallic_map;
uniform sampler2D u_roughness_map;
uniform sampler2D u_ao_map;

vec3 convert_normal_from_map() {
  vec3 tangentNormal = texture(u_normal_map, TexCoords).xyz * 2.0 - 1.0;

  vec3 Q1  = dFdx(WorldPos);
  vec3 Q2  = dFdy(WorldPos);
  vec2 st1 = dFdx(TexCoords);
  vec2 st2 = dFdy(TexCoords);

  vec3 N = normalize(Normal);
  vec3 T = normalize(Q1*st2.t - Q2*st1.t);
  vec3 B = -normalize(cross(N, T));
  mat3 TBN = mat3(T, B, N);

  return normalize(TBN * tangentNormal);
}

void main() {
  g_position = WorldPos;
  g_normal = convert_normal_from_map();
  g_albedo = pow(texture(u_albedo_map, TexCoords).rgb, vec3(2.2));
  g_metallic = texture(u_metallic_map, TexCoords).rgb;
  g_roughness = texture(u_roughness_map, TexCoords).rgb;
  g_ao = texture(u_ao_map, TexCoords).rgb;
}