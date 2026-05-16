#version 330 core

in vec3 vNormalWorld;
in vec4 vColor;
in vec3 vFragPos;

uniform vec3 uCameraPos;
out vec4 fragColor;

void main()
{
    // Same light direction as the particle shader
    vec3 lightDir = normalize(vec3(0.6, 0.8, 1.0));
    vec3 viewDir = normalize(uCameraPos - vFragPos);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(vNormalWorld, halfDir), 0.0), 256.0);
    float fresnel = pow(1.0 - max(dot(viewDir, vNormalWorld), 0.0), 5.0);

    float ambient  = 1.20;
    float diffuse  = max(0.0, dot(vNormalWorld, lightDir)) * 0.70;
    float lit      = ambient + diffuse;

    vec3 baseColor = vColor.rgb * (ambient + diffuse * 0.3);
    vec3 specColor = vec3(1.0) * spec * 1.2;
    vec3 fresnelColor = vec3(1.0) * fresnel * 0.3;
    fragColor = vec4(baseColor + specColor + fresnelColor, vColor.a);

}
