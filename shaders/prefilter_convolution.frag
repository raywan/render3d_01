#version 410
in vec3 WorldPos;

out vec4 frag_color;

uniform samplerCube environment_map;
uniform float roughness;

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
  
  // From tangent-space H vector to world-space sample vector
  vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
  vec3 tangent = normalize(cross(up, N));
  vec3 bitangent = cross(N, tangent);
  
  vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
  return normalize(sampleVec);
}

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

void main() {		
  vec3 N = normalize(WorldPos);
  
  // Assume that V = R = N 
  vec3 R = N;
  vec3 V = R;

  vec3 prefilteredColor = vec3(0.0);
  float totalWeight = 0.0;
  
  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    // Importance sample
    vec2 Xi = sample_hammersley(i, SAMPLE_COUNT);
    vec3 H = importance_sample_ggx(Xi, N, roughness);
    vec3 L  = normalize(2.0 * dot(V, H) * H - V);

    float NdL = max(dot(N, L), 0.0);
    if (NdL > 0.0) {
      // We sample from the environment's mip level based on roughness/pdf
      float D = distribution_ggx(N, H, roughness);
      float NdH = max(dot(N, H), 0.0);
      float HdV = max(dot(H, V), 0.0);
      float pdf = D * NdH / (4.0 * HdV) + 0.0001; 

      // Resolution we defined when we generated
      float resolution = 512.0;
      float saTexel  = 4.0 * PI / (6.0 * resolution * resolution);
      float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

      float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
      
      prefilteredColor += textureLod(environment_map, L, mipLevel).rgb * NdL;
      totalWeight += NdL;
    }
  }

  prefilteredColor = prefilteredColor / totalWeight;

  frag_color = vec4(prefilteredColor, 1.0);
}