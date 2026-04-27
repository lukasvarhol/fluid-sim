#version 330 core
in vec2 uv;
in vec3 vColor;

out vec4 screenColor;

uniform vec3 lightDir;

void main() {
    vec2 n_xy = uv * 2.0;
    float r2 = dot(n_xy, n_xy);
    float r = sqrt(r2);
    float w = fwidth(r);
    float a = 1.0 - smoothstep(1.0 - w, 1.0 + w, r);

    
    float z = sqrt(max(0.0, 1.0 - r2));
    vec3 normal = normalize(vec3(n_xy, z));

    float diffuse = max(dot(normal, normalize(lightDir)), 0.0);
    vec3 color = vColor * (0.7 + 0.7 * diffuse);
    
    if ( a < 0.5) discard;
    screenColor = vec4(color,a);
}
