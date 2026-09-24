#pragma once
// ============================================================
// DiamondSquare.h - 菱形-方形中点位移分形算法
// ============================================================
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace fractal {

// 生成 (size+1) x (size+1) 的随机高度图
// size 必须是 2 的整数次幂：2^n，n >= 1
// roughness: 粗糙度，0.5~0.7 表现较自然；越大越崎岖
// 输出范围归一化到 [0, 1]
inline std::vector<float> diamondSquare(int size, float roughness,
                                        uint32_t seed = 2024u) {
    const int N = size + 1;
    std::vector<float> h(N * N, 0.0f);

    // xorshift32，保证同 seed 结果可复现
    uint32_t s = seed ? seed : 1u;
    auto rnd01 = [&s]() -> float {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return static_cast<float>(s & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
    };
    auto rndRange = [&rnd01](float lo, float hi) { return lo + (hi - lo) * rnd01(); };

    auto at = [&h, N](int x, int y) -> float& {
        // 坐标环绕，保证边界能取到对角邻居
        int xx = (x + N) % N;
        int yy = (y + N) % N;
        return h[yy * N + xx];
    };

    // ---- 1. 初始化四角 ----
    const float lo = -0.5f, hi = 0.5f;
    at(0, 0)     = rndRange(lo, hi);
    at(N - 1, 0) = rndRange(lo, hi);
    at(0, N - 1) = rndRange(lo, hi);
    at(N - 1, N - 1) = rndRange(lo, hi);

    // ---- 2. 迭代细分 ----
    int step = N - 1;               // 当前方块边长
    float scale = 1.0f;             // 当前层的随机偏移幅度
    while (step > 1) {
        int half = step / 2;

        // Diamond 步：取正方形中心
        for (int y = 0; y < N - 1; y += step) {
            for (int x = 0; x < N - 1; x += step) {
                float avg = at(x, y) + at(x + step, y) +
                            at(x, y + step) + at(x + step, y + step);
                avg *= 0.25f;
                at(x + half, y + half) = avg + rndRange(-scale, scale);
            }
        }

        // Square 步：取菱形中心（各边中点）
        for (int y = 0; y < N; y += half) {
            int xStart = ((y / half) % 2 == 0) ? half : 0;   // 奇偶行错开
            for (int x = xStart; x < N; x += step) {
                float sum = at(x - half, y) + at(x + half, y) +
                            at(x, y - half) + at(x, y + half);
                at(x, y) = sum * 0.25f + rndRange(-scale, scale);
            }
        }

        step /= 2;
        scale *= roughness;         // 每层幅度衰减，形成分形特征
    }

    // ---- 3. 归一化到 [0, 1] ----
    float mn = h[0], mx = h[0];
    for (float v : h) { mn = std::min(mn, v); mx = std::max(mx, v); }
    const float span = (mx - mn) > 1e-6f ? (mx - mn) : 1.0f;
    for (float& v : h) v = (v - mn) / span;

    return h;
}

} // namespace fractal
