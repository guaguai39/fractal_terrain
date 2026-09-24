// 预设读写自测：不依赖 OpenGL，只验证 TerrainPreset.h 的序列化逻辑
#include "TerrainPreset.h"
#include <cassert>
#include <iostream>

using namespace fractal;

static int g_fail = 0;
static void check(bool cond, const char* what) {
    std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (!cond) ++g_fail;
}

int main() {
    const std::string path = "test_preset_roundtrip.txt";

    // ---- 1. 写一份内容特殊的预设 ----
    TerrainPreset a;
    a.algo         = TerrainAlgo::Ridged;
    a.resolution   = 512;
    a.worldSize    = 333.5f;
    a.heightScale  = 77.0f;
    a.roughness    = 0.61f;
    a.seed         = 98765u;
    a.fbmOctaves     = 9;
    a.fbmFrequency   = 4.5f;
    a.fbmLacunarity  = 2.2f;
    a.fbmGain        = 0.47f;
    a.fbmAmplitude   = 1.2f;
    a.fbmWarp        = 0.8f;
    a.fogDensity  = 0.0031f;
    a.snowLine    = 0.71f;
    a.waterLevel  = 0.09f;
    a.wireframe   = true;
    a.camX = 12.5f; a.camY = 88.0f; a.camZ = -45.25f;
    a.camYaw = 33.0f; a.camPitch = -17.5f;
    a.camMode = 1; a.camFovY = 75.0f;

    check(savePreset(path, a), "savePreset 返回成功");

    // ---- 2. 读回来逐字段比对 ----
    TerrainPreset b;
    check(loadPreset(path, b), "loadPreset 返回成功");

    check(b.algo == TerrainAlgo::Ridged,              "algo 往返一致");
    check(b.resolution == 512,                        "resolution 往返一致");
    check(std::abs(b.worldSize - 333.5f) < 0.01f,     "worldSize 往返一致");
    check(std::abs(b.heightScale - 77.0f) < 0.01f,    "heightScale 往返一致");
    check(std::abs(b.roughness - 0.61f) < 0.001f,     "roughness 往返一致");
    check(b.seed == 98765u,                           "seed 往返一致");
    check(b.fbmOctaves == 9,                          "fbmOctaves 往返一致");
    check(std::abs(b.fbmFrequency - 4.5f) < 0.001f,   "fbmFrequency 往返一致");
    check(std::abs(b.fbmLacunarity - 2.2f) < 0.001f,  "fbmLacunarity 往返一致");
    check(std::abs(b.fbmGain - 0.47f) < 0.001f,       "fbmGain 往返一致");
    check(std::abs(b.fbmAmplitude - 1.2f) < 0.001f,   "fbmAmplitude 往返一致");
    check(std::abs(b.fbmWarp - 0.8f) < 0.001f,        "fbmWarp 往返一致");
    check(std::abs(b.fogDensity - 0.0031f) < 1e-5f,   "fogDensity 往返一致");
    check(std::abs(b.snowLine - 0.71f) < 0.001f,      "snowLine 往返一致");
    check(std::abs(b.waterLevel - 0.09f) < 0.001f,    "waterLevel 往返一致");
    check(b.wireframe == true,                        "wireframe 往返一致");
    check(std::abs(b.camX - 12.5f) < 0.001f,          "camX 往返一致");
    check(std::abs(b.camY - 88.0f) < 0.001f,          "camY 往返一致");
    check(std::abs(b.camZ + 45.25f) < 0.001f,         "camZ 往返一致（负数）");
    check(std::abs(b.camYaw - 33.0f) < 0.001f,        "camYaw 往返一致");
    check(std::abs(b.camPitch + 17.5f) < 0.001f,      "camPitch 往返一致（负数）");
    check(b.camMode == 1,                             "camMode 往返一致");
    check(std::abs(b.camFovY - 75.0f) < 0.001f,       "camFovY 往返一致");

    // ---- 3. 缺字段时保留原值（向前/向后兼容）----
    {
        std::ofstream f("test_preset_partial.txt");
        f << "# 只有两个字段\n";
        f << "seed = 4242\n";
        f << "algo = 1\n";
    }
    TerrainPreset c;
    c.resolution = 128;      // 预设里没有，应该保留
    c.heightScale = 55.0f;   // 同上
    check(loadPreset("test_preset_partial.txt", c), "部分字段预设读取成功");
    check(c.seed == 4242u,                        "缺失文件时 seed 被覆盖");
    check(c.algo == TerrainAlgo::Fbm,             "缺失文件时 algo 被覆盖");
    check(c.resolution == 128,                    "缺失字段保留原值 (resolution)");
    check(std::abs(c.heightScale - 55.0f) < 0.001f, "缺失字段保留原值 (heightScale)");

    // ---- 4. 越界值被夹紧 ----
    {
        std::ofstream f("test_preset_bad.txt");
        f << "resolution = 999999\n";
        f << "roughness = 5.0\n";
        f << "algo = 99\n";
        f << "camPitch = 200\n";
        f << "fogDensity = -1\n";
    }
    TerrainPreset d;
    check(loadPreset("test_preset_bad.txt", d), "越界值预设仍能读取");
    check(d.resolution == 4096,                    "resolution 被夹到上限 4096");
    check(std::abs(d.roughness - 1.0f) < 0.001f,   "roughness 被夹到上限 1.0");
    check(d.algo == TerrainAlgo::Ridged,           "非法 algo 被夹到合法范围");
    check(std::abs(d.camPitch - 89.0f) < 0.001f,   "camPitch 被夹到 89");
    check(std::abs(d.fogDensity) < 1e-6f,          "负 fogDensity 被夹到 0");

    // ---- 5. 文件不存在 ----
    {
        TerrainPreset e;
        check(!loadPreset("___no_such_file___.txt", e), "不存在的文件返回 false");
    }

    // ---- 6. toParams 的 ridged 标志 ----
    {
        TerrainPreset r; r.algo = TerrainAlgo::Ridged;
        check(std::abs(toParams(r).fbm.ridged - 1.0f) < 0.001f, "Ridged 算法 → fbm.ridged=1");
        TerrainPreset n; n.algo = TerrainAlgo::Fbm;
        check(std::abs(toParams(n).fbm.ridged) < 0.001f,        "FBM 算法 → fbm.ridged=0");
    }

    // 清理
    std::remove(path.c_str());
    std::remove("test_preset_partial.txt");
    std::remove("test_preset_bad.txt");

    std::cout << "\n" << (g_fail == 0 ? "全部通过" : "有失败项") << " (失败 " << g_fail << " 项)\n";
    return g_fail == 0 ? 0 : 1;
}
