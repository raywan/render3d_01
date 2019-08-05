#version 410

in vec3 WorldPos;

out vec4 frag_color;

uniform samplerCube environment_map;

const float PI = 3.14159265359;

void main() {
  vec3 N = normalize(WorldPos);
  vec3 irradiance = vec3(0.0);

  // Tangent space calculation
  vec3 up = vec3(0.0, 1.0, 0.0);
  vec3 right = cross(up, N);
  up = cross(N, right);
  
  float sample_dt = 0.025;
  float num_samples = 0.0;
  for (float phi = 0.0; phi < 2.0 * PI; phi += sample_dt) {
    for (float theta = 0.0; theta < 0.5 * PI; theta += sample_dt) {
      // Spherical coords -> cartesian coords
      vec3 tangent_sample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
      // Tangent space to world space
      vec3 sample_vec = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * N;
      irradiance += texture(environment_map, sample_vec).rgb * cos(theta) * sin(theta);
      num_samples++;
    }
  }
  irradiance = PI * irradiance *(1.0/num_samples);
  frag_color = vec4(irradiance, 1.0);
}