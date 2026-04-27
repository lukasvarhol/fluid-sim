#version 330 core
in vec2 uv;
in vec3 fragPos;

out vec4 fragColor;

void main(){
  float dist = length(fragPos.xz);
  float mask = 1.0 - smoothstep(3.0, 7.0, dist); 
  
  fragColor = vec4(0.2, 0.2, 0.2, mask);
}
