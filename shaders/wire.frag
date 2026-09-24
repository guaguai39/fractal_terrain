#version 330 core

// ============================================================
// wire.frag - 线框着色器
// ============================================================
in float vT;
out vec4 FragColor;

uniform vec3  uWireColor;
uniform float uAlpha;

void main() {
    // 低处偏青、高处偏白，便于观察高度场结构
    vec3 c = mix(uWireColor, vec3(0.95, 0.98, 1.0), vT);
    FragColor = vec4(c, uAlpha);
}
