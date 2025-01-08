#if defined(VERTEX)

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 color;       // Per-instance color
layout(location = 2) in mat4 model;       // Per-instance model matrix

layout(location = 0) out vec3 fragColor;

void main() {
    fragColor = color.xyz; // Pass position to fragment shader if needed
    gl_Position = projection_matrix * view_matrix * model * vec4(inPosition, 1.0f);
    //gl_Position = vec4(inPosition, 1.0); // Transform to clip space
}

#endif


#if defined(FRAGMENT)

layout(location = 0) in vec3 fragColor; // Input from vertex shader
layout(location = 0) out vec4 outColor;    // Output color

void main() {
    outColor = vec4(fragColor, 1.0); // White color for edges
}

#endif