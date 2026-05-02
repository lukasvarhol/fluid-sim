#version 330 core

layout (location=0) in vec3 vertexPos;
layout (location=1) in vec3 instancePos;
layout (location=2) in vec3 instanceVel;

out vec3 vColor;
out vec2 uv;

uniform mat4 projection;
uniform mat4 view;
uniform float radius;

const float MAX_SPEED = 3.0;
const vec3 c0 = vec3(117.0/255.0, 14.0/255.0,  227.0/255.0); // purple
const vec3 c1 = vec3( 80.0/255.0, 199.0/255.0, 187.0/255.0); // teal
const vec3 c2 = vec3(230.0/255.0, 213.0/255.0,  25.0/255.0); // yellow
const vec3 c3 = vec3(1.0, 0.0, 0.0);                          // red

vec3 velocityColor(float s) {
    if (s < 0.15) return mix(c0, c1, s / 0.15);
    if (s < 0.5)  return mix(c1, c2, (s - 0.15) / 0.35);
    return mix(c2, c3, (s - 0.5) / 0.5);
}

void main(){
    vec4 viewPos = view * vec4(instancePos, 1.0);
    viewPos.xy += vertexPos.xy * radius;
    gl_Position = projection * viewPos;
    uv = vertexPos.xy;

    float speed = clamp(length(instanceVel) / MAX_SPEED, 0.0, 1.0);
    vColor = velocityColor(speed);
}
