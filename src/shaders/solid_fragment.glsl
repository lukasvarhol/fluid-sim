#version 330 core

in vec3 vNormalWorld;
in vec4 vColorAlpha;

out vec4 fragColor;

void main()
{
    // Same light direction as the particle shader
    vec3 lightDir = normalize(vec3(0.6, 0.8, 1.0));

    float ambient  = 0.30;
    float diffuse  = max(0.0, dot(vNormalWorld, lightDir)) * 0.70;
    float lit      = ambient + diffuse;

    fragColor = vec4(vColorAlpha.rgb * lit, vColorAlpha.a);
}
