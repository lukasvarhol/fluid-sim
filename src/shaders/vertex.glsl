#version 330 core

layout (location=0) in vec3 vertexPos;
layout (location=1) in vec2 instancePos;
layout (location=2) in vec3 instanceColor;

out vec3 vColor;
out vec2 uv;

uniform vec2 scale;

void main(){
    vec2 scaled = vertexPos.xy * scale;
    gl_Position = vec4(scaled + instancePos, 0.0, 1.0);
    uv = vertexPos.xy;
    vColor = instanceColor;
}