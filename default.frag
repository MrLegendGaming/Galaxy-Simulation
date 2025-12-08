#version 330 core
out vec4 FragColor;
in vec3 ourColor;

uniform vec3 color;

void main()
{
	gl_FragColor = vec4(color, 1.0f);
}