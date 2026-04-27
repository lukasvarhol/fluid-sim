#version 330 core

layout (location=0) in vec3 vertexPos;
layout (location=1) in vec3 instancePos;
layout (location=2) in vec3 instanceColor;

out vec3 vColor;
out vec2 uv;

uniform mat4 projection;
uniform mat4 view;
uniform float radius;

void main(){
  vec4 viewPos = view * vec4(instancePos, 1.0);
  viewPos.xy += vertexPos.xy * radius;
  gl_Position = projection * viewPos;
  uv = vertexPos.xy;
  vColor = instanceColor;
}
