
#version 330 core
layout (location = 0) in vec2 aPos;
void main()
{
    // aPos is in NDC (-1..1)
    gl_Position = vec4(aPos, 0.0, 1.0);
}
