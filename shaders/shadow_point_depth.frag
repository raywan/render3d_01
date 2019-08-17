#version 410

in vec3 Pos;

uniform vec3 u_light_pos;
uniform float u_far_plane;

void main() {
  float light_dist = length(Pos - u_light_pos);
  light_dist = light_dist/u_far_plane;
  gl_FragDepth = light_dist;
}