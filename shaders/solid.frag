#version 410

out vec4 o_frag_color;

uniform vec3 u_light_color;

void main() {
  vec3 color = u_light_color;
	// HDR tonemapping
	color = color / (color + vec3(1.0));
	// gamma correct
	color = pow(color, vec3(1.0/2.2));
  o_frag_color = vec4(color, 1.0);
}