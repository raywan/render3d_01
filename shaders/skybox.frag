#version 410

in vec3 TexCoord;

out vec4 frag_color;

uniform samplerCube skybox_tex;

void main() {
  // vec3 envColor = textureLod(skybox_tex, TexCoord, 1.2).rgb; 
  // frag_color = vec4(envColor, 1.0);
  frag_color = texture(skybox_tex, TexCoord);
}