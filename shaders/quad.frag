#version 410

in vec2 TexCoord;

out vec4 frag_color;

uniform sampler2D screen_tex;

void main() {
  frag_color = texture(screen_tex, TexCoord);
  // frag_color = vec4(1.0, 0.0, 0.0, 1.0);
}