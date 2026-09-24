#pragma once
// ============================================================
// TerrainField.h - 地形字段封装
//
// 解决的问题：原先 sampleHeight() 是自由函数，调用方必须手工传
// res / worldSize / heightScale 三个参数，行走模式贴地时极易传错
// 分辨率，导致相机高度计算失误、出现漂移或穿模。
//
// 这里把「参数 + 高度场 + 采样」打包成一个类，调用方只需给出
// 世界坐标，内部保证分辨率与生成时完全一致。
// ============================================================
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <iostream>

#include "Terrain.h"

namespace fractal {

class TerrainField {
public:
    TerrainField() = default;

    explicit TerrainField(const TerrainParams& params) { regenerate(params); }

    // 用新参数重新生成高度场与网格。返回是否成功。
    bool regenerate(const TerrainParams& params) {
        params_ = params;
        // 分辨率必须是 2 的幂，这里统一向上取整并记录实际值
        res_ = toPowerOfTwo(std::max(4, params_.resolution));
        params_.resolution = res_;

        try {
            mesh_ = buildTerrainMesh(params_);
            heightField_ = generateHeightField(params_);
        } catch (const std::exception& e) {
            std::cerr << "[TerrainField] 生成失败: " << e.what() << std::endl;
            valid_ = false;
            return false;
        }

        // 防御：网格与高度场尺寸必须一致
        const size_t expected = static_cast<size_t>(res_ + 1) * (res_ + 1);
        if (heightField_.size() != expected) {
            std::cerr << "[TerrainField] 高度场尺寸异常: 期望 " << expected
                      << " 实际 " << heightField_.size() << std::endl;
            valid_ = false;
            return false;
        }

        minRatio_ = *std::min_element(heightField_.begin(), heightField_.end());
        maxRatio_ = *std::max_element(heightField_.begin(), heightField_.end());
        valid_ = true;
        return true;
    }

    const TerrainMesh&               mesh()        const { return mesh_; }
    const std::vector<float>&        heightField() const { return heightField_; }
    const TerrainParams&             params()      const { return params_; }
    int                              resolution()  const { return res_; }
    bool                             valid()       const { return valid_; }
    float                            minRatio()    const { return minRatio_; }
    float                            maxRatio()    const { return maxRatio_; }

    // 采样地形高度（世界坐标 xz → 高度 y）。分辨率与生成时严格一致。
    float heightAt(float worldX, float worldZ) const {
        if (!valid_) return 0.0f;

        const int N = res_ + 1;
        const float world = params_.worldSize;

        // 世界坐标 → 网格浮点坐标
        float gx = (worldX / world + 0.5f) * static_cast<float>(res_);
        float gz = (worldZ / world + 0.5f) * static_cast<float>(res_);
        gx = std::clamp(gx, 0.0f, static_cast<float>(res_ - 1));
        gz = std::clamp(gz, 0.0f, static_cast<float>(res_ - 1));

        const int x0 = static_cast<int>(gx);
        const int z0 = static_cast<int>(gz);
        const float tx = gx - static_cast<float>(x0);
        const float tz = gz - static_cast<float>(z0);
        const int x1 = std::min(x0 + 1, res_);
        const int z1 = std::min(z0 + 1, res_);

        // 双线性插值，避免行走时高度出现阶梯跳变
        const float h00 = heightField_[static_cast<size_t>(z0) * N + x0];
        const float h10 = heightField_[static_cast<size_t>(z0) * N + x1];
        const float h01 = heightField_[static_cast<size_t>(z1) * N + x0];
        const float h11 = heightField_[static_cast<size_t>(z1) * N + x1];

        const float a = h00 + (h10 - h00) * tx;
        const float b = h01 + (h11 - h01) * tx;
        return (a + (b - a) * tz) * params_.heightScale;
    }

private:
    TerrainParams         params_{};
    TerrainMesh           mesh_{};
    std::vector<float>    heightField_{};
    int                   res_       = 0;
    float                 minRatio_  = 0.0f;
    float                 maxRatio_  = 1.0f;
    bool                  valid_     = false;
};

} // namespace fractal
