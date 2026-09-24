#pragma once
// ============================================================
// Terrain.h - 分形地形：高度场生成 + 三角网格构建
// ============================================================
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

#include "Noise.h"
#include "DiamondSquare.h"

namespace fractal {

// 地形生成算法枚举
enum class TerrainAlgo {
    DiamondSquare = 0,   // 菱形-方形中点位移
    Fbm           = 1,   // 分形布朗运动（Perlin 叠加）
    Ridged        = 2,   // 山脊噪声
    AlgoCount
};

inline const char* algoName(TerrainAlgo a) {
    switch (a) {
    case TerrainAlgo::DiamondSquare: return "Diamond-Square";
    case TerrainAlgo::Fbm:           return "FBM (Perlin)";
    case TerrainAlgo::Ridged:        return "Ridged Multi-Fractal";
    default:                         return "Unknown";
    }
}

// 地形生成参数
struct TerrainParams {
    TerrainAlgo algo   = TerrainAlgo::DiamondSquare;
    int   resolution   = 256;    // 高度场边长上的格子数（会向上取最近的 2^n）
    float worldSize    = 200.0f; // 世界坐标下的地形边长
    float heightScale  = 32.0f;  // 高度放大系数
    float roughness    = 0.55f;  // Diamond-Square 粗糙度
    uint32_t seed      = 2024u;

    // FBM 相关
    FbmParams fbm{};
};

// 单个顶点：位置 + 法线 + 高度比（用于着色）
struct TerrainVertex {
    glm::vec3 position;
    glm::vec3 normal;
    float     heightRatio;   // 归一化高度 [0,1]，供片元着色器分层上色
};

// 地形数据：可直接上传 GPU
struct TerrainMesh {
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t>      indices;

    int   gridSize = 0;         // 每边顶点数 = resolution + 1
    float worldSize = 0.0f;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;

    size_t triangleCount() const { return indices.size() / 3; }
};

// 把任意整数向上取到最近的 2 的幂（至少 4）
inline int toPowerOfTwo(int v) {
    int p = 4;
    while (p < v) p <<= 1;
    return p;
}

// ---------------- 高度场生成 ----------------

// 按当前参数生成归一化高度场，尺寸 (res+1)^2，值域 [0,1]
inline std::vector<float> generateHeightField(const TerrainParams& prm) {
    const int res = toPowerOfTwo(std::max(4, prm.resolution));
    const int N   = res + 1;

    std::vector<float> field(N * N, 0.0f);

    if (prm.algo == TerrainAlgo::DiamondSquare) {
        // Diamond-Square 要求 size 为 2 的幂，正好匹配
        field = diamondSquare(res, prm.roughness, prm.seed);
    } else {
        // FBM / Ridged：用 Perlin 噪声采样
        Perlin2D perlin(prm.seed);
        FbmParams fp = prm.fbm;
        if (prm.algo == TerrainAlgo::Ridged) fp.ridged = 1.0f;

        float mn = 1e30f, mx = -1e30f;
        std::vector<float> raw(N * N, 0.0f);

        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                // 归一化到 [0,1] 后用频率缩放，避免分辨率改变导致地形"缩放"
                const float u = static_cast<float>(x) / static_cast<float>(res);
                const float v = static_cast<float>(y) / static_cast<float>(res);
                const float h = fbm(perlin, u * fp.frequency, v * fp.frequency, fp);
                raw[y * N + x] = h;
                mn = std::min(mn, h);
                mx = std::max(mx, h);
            }
        }
        // 归一化，并做一次幂次拉伸强化对比（山脊模式特别有效）
        const float span = (mx - mn) > 1e-6f ? (mx - mn) : 1.0f;
        const float gamma = (prm.algo == TerrainAlgo::Ridged) ? 1.6f : 1.0f;
        for (size_t i = 0; i < raw.size(); ++i) {
            float n = (raw[i] - mn) / span;
            if (gamma != 1.0f) n = std::pow(std::max(0.0f, n), gamma);
            field[i] = n;
        }
    }

    // 整体平滑边缘：用径向衰减把地形压向海平面，形成"岛屿"效果
    const float half = static_cast<float>(res) * 0.5f;
    const float falloffStart = 0.62f;    // 从半径 62% 处开始衰减
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const float dx = (static_cast<float>(x) - half) / half;
            const float dy = (static_cast<float>(y) - half) / half;
            const float d  = std::sqrt(dx * dx + dy * dy);   // 径向距离，中心 0 边缘 ~1.41
            float k = 1.0f;
            if (d > falloffStart) {
                const float t = (d - falloffStart) / (1.45f - falloffStart);
                k = 1.0f - std::min(1.0f, t) * std::min(1.0f, t) * (3.0f - 2.0f * std::min(1.0f, t));
                k = std::max(0.0f, k);
            }
            field[y * N + x] *= k;
        }
    }
    return field;
}

// ---------------- 网格构建 ----------------

// 由高度场构建三角网格（含法线计算）
inline TerrainMesh buildTerrainMesh(const TerrainParams& prm) {
    TerrainMesh mesh;
    const int res = toPowerOfTwo(std::max(4, prm.resolution));
    const int N   = res + 1;
    const float world = prm.worldSize;
    const float cell  = world / static_cast<float>(res);

    const std::vector<float> hf = generateHeightField(prm);

    // 高度统计
    float mn = hf[0], mx = hf[0];
    for (float v : hf) { mn = std::min(mn, v); mx = std::max(mx, v); }
    mesh.minHeight = mn * prm.heightScale;
    mesh.maxHeight = mx * prm.heightScale;
    mesh.gridSize  = N;
    mesh.worldSize = world;

    // 高度采样（越界钳制到边缘，避免法线计算越界）
    auto heightAt = [&](int x, int y) -> float {
        x = std::clamp(x, 0, N - 1);
        y = std::clamp(y, 0, N - 1);
        return hf[y * N + x] * prm.heightScale;
    };

    // ---- 顶点：位置 + 法线 ----
    mesh.vertices.resize(static_cast<size_t>(N) * N);
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const float wx = (static_cast<float>(x) / static_cast<float>(res) - 0.5f) * world;
            const float wz = (static_cast<float>(y) / static_cast<float>(res) - 0.5f) * world;
            const float wy = heightAt(x, y);
            const float hr = hf[y * N + x];

            // 中心差分求法线，比逐面累加更平滑且开销稳定
            const float hl = heightAt(x - 1, y);
            const float hrgt = heightAt(x + 1, y);
            const float hu = heightAt(x, y - 1);
            const float hd = heightAt(x, y + 1);

            // 切线 (2*cell, dh, 0) 与副切线 (0, dh, 2*cell) 的叉积
            glm::vec3 n = glm::normalize(glm::vec3(hl - hrgt, 2.0f * cell, hu - hd));

            TerrainVertex& v = mesh.vertices[static_cast<size_t>(y) * N + x];
            v.position    = glm::vec3(wx, wy, wz);
            v.normal      = n;
            v.heightRatio = hr;
        }
    }

    // ---- 索引：两个三角形组成一个格子 ----
    mesh.indices.reserve(static_cast<size_t>(res) * res * 6);
    for (int y = 0; y < res; ++y) {
        for (int x = 0; x < res; ++x) {
            const uint32_t i0 = static_cast<uint32_t>(y * N + x);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + static_cast<uint32_t>(N);
            const uint32_t i3 = i2 + 1;

            // 保证逆时针缠绕（CCW），配合 GL_CCW 背面剔除
            mesh.indices.push_back(i0); mesh.indices.push_back(i2); mesh.indices.push_back(i1);
            mesh.indices.push_back(i1); mesh.indices.push_back(i2); mesh.indices.push_back(i3);
        }
    }
    return mesh;
}

// 采样地形高度（世界坐标 xz → 高度 y），供行走模式贴地使用
inline float sampleHeight(const std::vector<float>& hf, int res,
                          float worldSize, float heightScale,
                          float worldX, float worldZ) {
    const int N = res + 1;
    // 世界坐标 → 网格浮点坐标
    float gx = (worldX / worldSize + 0.5f) * static_cast<float>(res);
    float gz = (worldZ / worldSize + 0.5f) * static_cast<float>(res);
    gx = std::clamp(gx, 0.0f, static_cast<float>(res - 1));
    gz = std::clamp(gz, 0.0f, static_cast<float>(res - 1));

    const int x0 = static_cast<int>(gx), z0 = static_cast<int>(gz);
    const float tx = gx - x0, tz = gz - z0;
    const int x1 = std::min(x0 + 1, res), z1 = std::min(z0 + 1, res);

    // 双线性插值
    const float h00 = hf[z0 * N + x0], h10 = hf[z0 * N + x1];
    const float h01 = hf[z1 * N + x0], h11 = hf[z1 * N + x1];
    const float a = h00 + (h10 - h00) * tx;
    const float b = h01 + (h11 - h01) * tx;
    return (a + (b - a) * tz) * heightScale;
}

} // namespace fractal
