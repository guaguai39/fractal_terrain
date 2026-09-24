#version 330 core

// ============================================================
// wire.vert - 线框 / 网格辅助着色器
// ============================================================
layout (location = 0) in vec3 aPos;
layout (location = 2) in float aHeightRatio;

uniform mat4  uMVP;
uniform float uMinRatio;
uniform float uMaxRatio;

out float vT;

void main() {
    // 把高度比归一化到 [0,1]，供线框按高度着色
    float span = max(uMaxRatio - uMinRatio, 1e-5);
    vT = clamp((aHeightRatio - uMinRatio) / span, 0.0, 1.0);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
