#pragma once
// ============================================================
// Noise.h - Perlin 噪声 / FBM / 山脊噪声
// ============================================================
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace fractal {

// 经典 Perlin 噪声（Ken Perlin 1997 改进版），二维实现
class Perlin2D {
public:
    explicit Perlin2D(uint32_t seed = 1337u) { reseed(seed); }

    void reseed(uint32_t seed) {
        // 用 xorshift 打乱 0..255 置换表，保证可复现
        uint32_t s = seed ? seed : 1u;
        for (int i = 0; i < 256; ++i) p_[i] = static_cast<uint8_t>(i);
        for (int i = 255; i > 0; --i) {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;   // xorshift32
            int j = static_cast<int>(s % static_cast<uint32_t>(i + 1));
            std::swap(p_[i], p_[j]);
        }
        for (int i = 0; i < 256; ++i) p_[256 + i] = p_[i];   // 复制一份避免取模
    }

    // 返回 [-1, 1]
    float noise(float x, float y) const {
        int xi = fastFloor(x);
        int yi = fastFloor(y);
        float xf = x - static_cast<float>(xi);
        float yf = y - static_cast<float>(yi);

        float u = fade(xf);
        float v = fade(yf);

        int aa = hash(xi,     yi);
        int ab = hash(xi,     yi + 1);
        int ba = hash(xi + 1, yi);
        int bb = hash(xi + 1, yi + 1);

        float x1 = lerp(grad(aa, xf,     yf),     grad(ba, xf - 1.0f, yf),     u);
        float x2 = lerp(grad(ab, xf,     yf - 1.0f), grad(bb, xf - 1.0f, yf - 1.0f), u);
        return lerp(x1, x2, v);   // 归一化后落在 [-1, 1]
    }

private:
    static int fastFloor(float v) {
        int i = static_cast<int>(v);
        return (v < static_cast<float>(i)) ? i - 1 : i;
    }
    static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float lerp(float a, float b, float t) { return a + t * (b - a); }

    int hash(int x, int y) const {
        return p_[(p_[x & 255] + static_cast<unsigned>(y)) & 255];
    }
    // 8 个方向的梯度，足够二维 Perlin 使用且开销低于 16 方向表
    static float grad(int h, float x, float y) {
        switch (h & 7) {   // 仅用低 3 位选方向
        case 0: return  x + y;
        case 1: return  x - y;
        case 2: return -x + y;
        case 3: return -x - y;
        case 4: return  x;
        case 5: return -x;
        case 6: return  y;
        default: return -y;
        }
    }

    uint8_t p_[512]{};
};

// FBM 分形布朗运动参数
struct FbmParams {
    int   octaves     = 6;      // 叠加层数
    float frequency   = 2.0f;   // 基础频率
    float lacunarity  = 2.0f;   // 频率倍增
    float gain        = 0.5f;   // 振幅衰减
    float amplitude   = 1.0f;   // 总振幅系数
    float ridged      = 0.0f;   // >0 时启用山脊模式（0 = 关闭）
    float warp        = 0.0f;   // 域扭曲强度，产生更自然的蜿蜒
};

// 采样 FBM 高度，返回 [-1, 1] 区间（ridged 模式返回 [0, 1]）
inline float fbm(const Perlin2D& perlin, float x, float y, const FbmParams& prm) {
    float freq  = prm.frequency;
    float amp   = 1.0f;
    float sum   = 0.0f;
    float norm  = 0.0f;

    // 域扭曲：用另一层噪声扰动采样坐标，避免规则的"格子感"
    float wx = x, wy = y;
    if (prm.warp > 0.0f) {
        wx = x + prm.warp * perlin.noise(x * 0.5f + 11.7f, y * 0.5f + 3.1f);
        wy = y + prm.warp * perlin.noise(x * 0.5f - 7.3f,  y * 0.5f - 5.9f);
    }

    for (int i = 0; i < prm.octaves; ++i) {
        float n = perlin.noise(wx * freq, wy * freq);
        if (prm.ridged > 0.0f) {
            // 山脊：绝对值反转 + 平方增强脊线
            n = 1.0f - std::fabs(n);
            n = n * n;
        }
        sum  += n * amp;
        norm += amp;
        freq *= prm.lacunarity;
        amp  *= prm.gain;
    }
    if (norm <= 0.0f) return 0.0f;
    float r = sum / norm;
    // 山脊模式输出 [0,1]，普通模式映射回 [-1,1]
    return (prm.ridged > 0.0f) ? (r * 2.0f - 1.0f) : r;
}

} // namespace fractal
