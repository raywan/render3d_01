#version 410

layout (location = 0) in vec3 i_pos;

uniform mat4 u_model;

void main() {
    gl_Position = u_model * vec4(i_pos, 1.0);
}