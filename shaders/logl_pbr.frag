#version 330 core

in vec3 WorldPos;
in vec2 TexCoords;
in vec3 Normal;

out vec4 o_frag_color;

// material parameters
uniform vec3 u_albedo;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ao;

// lights
uniform vec3 u_light_pos[4];
uniform vec3 u_light_color[4];

uniform vec3 u_cam_pos;

const float PI = 3.14159265359;

float distribution_ggx(vec3 N, vec3 H, float roughness) {
  float a = roughness*roughness;
  float a2 = a*a;
  float NdH = max(dot(N, H), 0.0);
  float NdH2 = NdH*NdH;

  float numer = a2;
  float denom = (NdH2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return numer / denom; 
}

float geometry_schlick_ggx(float NdV, float roughness) {
  float r = roughness + 1;
  float k = (r*r) / 8.0;
  float numer = NdV;
  float denom = NdV * (1.0 - k) + k;
  return numer / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
  float NdV = max(dot(N, V), 0.0);
  float NdL = max(dot(N, L), 0.0);
  float ggx2 = geometry_schlick_ggx(NdV, roughness);
  float ggx1 = geometry_schlick_ggx(NdL, roughness);
  return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
  vec3 N = normalize(Normal);
  vec3 V = normalize(u_cam_pos - WorldPos);

  vec3 F0 = vec3(0.04);
  F0 = mix(F0, u_albedo, u_metallic);

  vec3 Lo = vec3(0.0);
  for (int i = 0; i < 4; ++i) {
    vec3 L = normalize(u_light_pos[i] - WorldPos);
    vec3 H = normalize(V + L);
    float distance = length(u_light_pos[i] - WorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = u_light_color[i] * attenuation;

    // Cook-Torrance BRDF
    float NDF = distribution_ggx(N, H, u_roughness);
    float G = geometry_smith(N, V, L, u_roughness);
    vec3 F = fresnel_schlick(clamp(dot(H, V), 0.0, 1.0), F0);

    vec3 numer = NDF * G * F;
    float denom = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = numer / max(denom, 0.001); 

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - u_metallic;

    float NdL = max(dot(N, L), 0.0);

    Lo += (kD * u_albedo / PI + specular) * radiance * NdL;
  }

  vec3 ambient = vec3(0.03) * u_albedo * u_ao;
  vec3 color = ambient + Lo;

  // HDR tonemapping
  color = color / (color + vec3(1.0));
  // gamma correct
  color = pow(color, vec3(1.0/2.2));

  o_frag_color = vec4(color, 1.0);
}
