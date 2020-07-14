#version 330 core

in vec4 vcoords;
in vec2 tcoords;

out vec2 uv;
uniform mat4 model, view, proj;

void main()
{
    gl_Position = vcoords * model * view * proj;
    uv = tcoords;
}
