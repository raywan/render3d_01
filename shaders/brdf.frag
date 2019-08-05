#version 410

in vec2 TexCoord;

out vec2 o_frag_color;

const float PI = 3.14159265359;
const int SAMPLE_COUNT = 1024;

float radical_inverse_VdC(uint bits) {
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 sample_hammersley(int i, int N) {
  return vec2(float(i)/float(N), radical_inverse_VdC(i));
}

vec3 importance_sample_ggx(vec2 Xi, vec3 N, float roughness) {
  float a = roughness*roughness;
  
  float phi = 2.0 * PI * Xi.x;
  float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
  float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
  
  vec3 H;
  H.x = cos(phi) * sinTheta;
  H.y = sin(phi) * sinTheta;
  H.z = cosTheta;
  
  vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
  vec3 tangent = normalize(cross(up, N));
  vec3 bitangent = cross(N, tangent);
  
  vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
  return normalize(sampleVec);
}

float geometry_schlick_ggx(float NdV, float roughness) {
  // Specular IBL uses this k
  float k = (roughness*roughness) / 2.0;
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

vec2 IntegrateBRDF(float NdV, float roughness) {
  vec3 V;
  V.x = sqrt(1.0 - NdV*NdV);
  V.y = 0.0;
  V.z = NdV;

  float A = 0.0;
  float B = 0.0; 

  vec3 N = vec3(0.0, 0.0, 1.0);
  
  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    // Generates a sample vector that's biased towards the
    // preferred alignment direction (importance sampling).
    vec2 Xi = sample_hammersley(i, SAMPLE_COUNT);
    vec3 H = importance_sample_ggx(Xi, N, roughness);
    vec3 L = normalize(2.0 * dot(V, H) * H - V);

    float NdL = max(L.z, 0.0);
    float NdH = max(H.z, 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if(NdL > 0.0) {
      float G = geometry_smith(N, V, L, roughness);
      float G_Vis = (G * VdotH) / (NdH * NdV);
      float Fc = pow(1.0 - VdotH, 5.0);

      A += (1.0 - Fc) * G_Vis;
      B += Fc * G_Vis;
    }
  }
  A /= float(SAMPLE_COUNT);
  B /= float(SAMPLE_COUNT);
  return vec2(A, B);
}

void main() {
  vec2 integratedBRDF = IntegrateBRDF(TexCoord.x, TexCoord.y);
  o_frag_color = integratedBRDF;
}