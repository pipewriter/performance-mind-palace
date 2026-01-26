#version 450

layout(binding = 0) uniform UniformBufferObject {
    float time;
} ubo;

// Hardcoded triangle vertices
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

// Hardcoded triangle colors
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),  // Red
    vec3(0.0, 1.0, 0.0),  // Green
    vec3(0.0, 0.0, 1.0)   // Blue
);

layout(location = 0) out vec3 fragColor;

void main() {
    vec2 pos = positions[gl_VertexIndex];

    // Rotate the triangle
    float angle = ubo.time;
    mat2 rotation = mat2(
        cos(angle), -sin(angle),
        sin(angle), cos(angle)
    );
    pos = rotation * pos;

    gl_Position = vec4(pos, 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
