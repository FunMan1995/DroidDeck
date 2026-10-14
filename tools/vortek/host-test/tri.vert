#version 450
layout(push_constant) uniform PC { vec4 offset; } pc;
layout(set = 0, binding = 0) uniform U { vec4 scale; } u;
void main() {
    vec2 p[3] = vec2[](vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));
    gl_Position = vec4(p[gl_VertexIndex % 3] * u.scale.xy + pc.offset.xy, 0.0, 1.0);
}
