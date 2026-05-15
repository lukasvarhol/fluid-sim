#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColorAlpha;  // rgb + alpha

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 vNormalWorld;
out vec4 vColorAlpha;

void main()
{
    vNormalWorld = normalize(aNormal);
    vColorAlpha  = aColorAlpha;
    gl_Position  = uProjection * uView * vec4(aPos, 1.0);
}
