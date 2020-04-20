#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragCircle;

layout(location = 0) out vec4 outColor;

void main() {
    float rr = dot(fragCircle, fragCircle);
    outColor = vec4(fragColor, rr < 1.0 ? 1.0 : 0.0);
}

