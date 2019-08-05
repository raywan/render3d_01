#version 410

in vec3 WorldPos;

out vec4 frag_color;

uniform sampler2D equirectangular_map;

const vec2 inv_atan = vec2(0.1591, 0.3183);

vec2 sample_spherical_map(vec3 v) {
  vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
  uv *= inv_atan;
  uv += 0.5;
  return uv;
}

void main() {
  vec2 uv = sample_spherical_map(normalize(WorldPos));
  vec3 color = texture(equirectangular_map, uv).rgb;
  color = color / (color + vec3(1.0));
  color = pow(color, vec3(1.0/2.2)); 
  frag_color = vec4(color, 1.0);
}