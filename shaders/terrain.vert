#version 330 core

// ============================================================
// terrain.vert - 地形顶点着色器
// ============================================================
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in float aHeightRatio;   // 归一化高度 [0,1]

uniform mat4  uModel;
uniform mat4  uView;
uniform mat4  uProj;
uniform mat3  uNormalMatrix;      // 法线变换矩阵（模型矩阵的逆转置）

out VS_OUT {
    vec3  worldPos;
    vec3  normal;
    float heightRatio;
} vs_out;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vs_out.worldPos    = world.xyz;
    vs_out.normal      = normalize(uNormalMatrix * aNormal);
    vs_out.heightRatio = aHeightRatio;

    gl_Position = uProj * uView * world;
}
