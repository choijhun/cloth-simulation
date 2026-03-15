#version 410 core

in vec3 worldPos;
in vec3 cameraPos;
in vec3 worldNormal;

uniform vec3 lightPos;

out vec4 out_Color; 


void main(void) {              
   
    vec3 L = normalize(lightPos- worldPos);
    vec3 N = normalize(worldNormal);
    vec3 V = normalize(cameraPos - worldPos);
    vec3 R = reflect(-L, N);
    float diff = max(dot(L, N), 0.f);
    float spec = pow(max(dot(R, V), 0), 10);
    out_Color = diff * vec4(1, 0, 0, 1) + spec * vec4(1);

    
}