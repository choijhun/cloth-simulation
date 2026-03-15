#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 modelMat = mat4(1);
uniform mat4 viewMat = mat4(1);
uniform mat4 projMat = mat4(1);

out vec3 worldPos;
out vec3 worldNormal;

void main(void) {
    worldPos = (modelMat * vec4(aPos, 1)).xyz;
    worldNormal = (modelMat * vec4(aNormal, 1)).xyz;
    gl_Position = projMat * viewMat * modelMat * vec4(aPos, 1.0f);
    
}