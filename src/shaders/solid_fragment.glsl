#version 330 core

in vec3 vNormalWorld;
in vec4 vColor;
in vec3 vFragPos;

uniform vec3 uCameraPos;
out vec4 fragColor;

void main()
{
  // Same light direction as the particle shader
  vec3 N = normalize(vNormalWorld);
  vec3 lightDir = normalize(vec3(-2.0, -4.0, -2.0));
  vec3 viewDir = normalize(uCameraPos - vFragPos);
  vec3 halfDir = normalize(lightDir + viewDir);
  float spec = pow(max(dot(N, halfDir), 0.0), 128.0);
  float fresnel = pow(1.0 - max(dot(viewDir, N), 0.0), 5.0);

  float ambient  = 0.85;
  float diffuse  = max(0.0, dot(N, lightDir)) * 0.70;
  float lit      = ambient + diffuse;

  vec3 baseColor = vColor.rgb * (ambient + diffuse);
  vec3 specColor = vec3(1.0) * spec * 1.2;
  vec3 fresnelColor = vec3(1.0) * fresnel * 0.3;
  fragColor = vec4(baseColor + specColor + fresnelColor, vColor.a);
  //fragColor = vec4(N * 0.5 + 0.5, vColor.a);

}
