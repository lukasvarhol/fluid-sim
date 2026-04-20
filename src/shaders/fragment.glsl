#version 330 core
in vec2 uv;
in vec3 vColor;

out vec4 screenColor;

void main() {
    float d = length(uv) - 0.5; 
    float w = fwidth(d);
    float a = 1.0 - smoothstep(0.0, w, d);

    if (a <= 0.0) discard;
    
    screenColor = vec4(vColor,a);
}
