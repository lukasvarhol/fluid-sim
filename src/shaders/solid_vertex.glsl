#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColorAlpha;

uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uModel;
uniform vec4 uColor;

out vec3 vNormalWorld;
out vec4 vColor;
out vec3 vFragPos;

void main()
{
  vFragPos = vec3(uModel * vec4(aPos, 1.0));
  vNormalWorld = normalize(aNormal);
  vColor  = uColor;
  gl_Position  = uProjection * uView * uModel * vec4(aPos, 1.0);
}
