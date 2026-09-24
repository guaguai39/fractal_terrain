#pragma once
// ============================================================
// TerrainPreset.h - 地形预设（配置）的保存与读取
//
// 解决的问题：运行时调出来的地形（算法 + 种子 + 起伏 + 雾浓度 …
// 全是一堆分散的全局变量和 TerrainParams 字段），关掉程序就全丢了。
// 每次演示前都得重新按一遍键。
//
// 这里把所有「决定画面长什么样」的参数打包成一份纯文本预设，
// 按 F5 存盘、按 F9 读回，下次启动能还原成一模一样的地形。
//
// 格式：key = value 的纯文本，逐行解析，未知键忽略、缺失键用默认值。
// 这样即使将来加了新参数，旧的预设文件也不会读崩。
// ============================================================
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <algorithm>

#include "Terrain.h"
#include "Camera.h"

namespace fractal {

// 一份完整的场景预设：地形的「参数」+ 画面的「外观」+ 相机「站位」
struct TerrainPreset {
    // 格式版本，用于将来做兼容升级
    int version = 1;

    // ---- 地形参数（对应 TerrainParams）----
    TerrainAlgo algo         = TerrainAlgo::DiamondSquare;
    int         resolution   = 256;
    float       worldSize    = 240.0f;
    float       heightScale  = 34.0f;
    float       roughness    = 0.55f;
    uint32_t    seed         = 2024u;

    // FBM / Ridged 专用
    int   fbmOctaves     = 7;
    float fbmFrequency   = 3.0f;
    float fbmLacunarity  = 2.0f;
    float fbmGain        = 0.5f;
    float fbmAmplitude   = 1.0f;
    float fbmWarp        = 0.35f;

    // ---- 外观参数 ----
    float fogDensity = 0.0022f;
    float snowLine   = 0.82f;
    float waterLevel = 0.06f;
    bool  wireframe  = false;

    // ---- 相机 ----
    float camX = 0.0f, camY = 40.0f, camZ = 120.0f;
    float camYaw = -90.0f, camPitch = -22.0f;
    int   camMode = 0;          // 0=飞行 1=行走
    float camFovY = 60.0f;
};

// ------------------------------------------------------------
// 把预设写成文件。成功返回 true。
// ------------------------------------------------------------
inline bool savePreset(const std::string& path, const TerrainPreset& ps) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    if (!f) {
        std::cerr << "[预设] 无法写入文件: " << path << std::endl;
        return false;
    }

    f << "# 分形地形预设 - 程序内按 F5 生成\n";
    f << "# 程序内按 F9 读回；也可直接编辑本文件\n";
    f << "version = " << ps.version << "\n";
    f << "\n# ---------- 地形 ----------\n";
    f << "algo        = " << static_cast<int>(ps.algo) << "\n";
    f << "resolution  = " << ps.resolution << "\n";
    f << "worldSize   = " << ps.worldSize << "\n";
    f << "heightScale = " << ps.heightScale << "\n";
    f << "roughness   = " << ps.roughness << "\n";
    f << "seed        = " << ps.seed << "\n";
    f << "\n# ---------- FBM / Ridged ----------\n";
    f << "fbmOctaves    = " << ps.fbmOctaves << "\n";
    f << "fbmFrequency  = " << ps.fbmFrequency << "\n";
    f << "fbmLacunarity = " << ps.fbmLacunarity << "\n";
    f << "fbmGain       = " << ps.fbmGain << "\n";
    f << "fbmAmplitude  = " << ps.fbmAmplitude << "\n";
    f << "fbmWarp       = " << ps.fbmWarp << "\n";
    f << "\n# ---------- 外观 ----------\n";
    f << "fogDensity  = " << ps.fogDensity << "\n";
    f << "snowLine    = " << ps.snowLine << "\n";
    f << "waterLevel  = " << ps.waterLevel << "\n";
    f << "wireframe   = " << (ps.wireframe ? 1 : 0) << "\n";
    f << "\n# ---------- 相机 ----------\n";
    f << "camX     = " << ps.camX << "\n";
    f << "camY     = " << ps.camY << "\n";
    f << "camZ     = " << ps.camZ << "\n";
    f << "camYaw   = " << ps.camYaw << "\n";
    f << "camPitch = " << ps.camPitch << "\n";
    f << "camMode  = " << ps.camMode << "\n";
    f << "camFovY  = " << ps.camFovY << "\n";

    f.flush();
    if (!f) {
        std::cerr << "[预设] 写入过程出错: " << path << std::endl;
        return false;
    }
    return true;
}

// ------------------------------------------------------------
// 从文件读取预设。字段缺失时保留 out 里的原值（即调用方给的默认值）。
// 成功返回 true。
// ------------------------------------------------------------
inline bool loadPreset(const std::string& path, TerrainPreset& out) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[预设] 找不到文件: " << path << std::endl;
        return false;
    }

    std::string line;
    int lineNo = 0;
    int parsed = 0;

    while (std::getline(f, line)) {
        ++lineNo;

        // 去掉行尾 \r（Windows 换行）与首尾空白
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();

        const size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos) continue;   // 空行
        if (line[firstNonSpace] == '#') continue;           // 注释

        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            std::cerr << "[预设] 第 " << lineNo << " 行缺少 '='，已跳过\n";
            continue;
        }

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        // trim
        auto trim = [](std::string& s) {
            const size_t b = s.find_first_not_of(" \t");
            const size_t e = s.find_last_not_of(" \t");
            s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
        };
        trim(key);
        trim(val);
        if (key.empty() || val.empty()) continue;

        // 用 stringstream 解析：解析失败就跳过这一行，不影响其他字段
        std::istringstream ss(val);
        bool ok = true;

        if      (key == "version")        ok = static_cast<bool>(ss >> out.version);
        else if (key == "algo") {
            int a = 0;
            if (ss >> a) {
                a = std::max(0, std::min(a, static_cast<int>(TerrainAlgo::AlgoCount) - 1));
                out.algo = static_cast<TerrainAlgo>(a);
            } else ok = false;
        }
        else if (key == "resolution")     ok = static_cast<bool>(ss >> out.resolution);
        else if (key == "worldSize")      ok = static_cast<bool>(ss >> out.worldSize);
        else if (key == "heightScale")    ok = static_cast<bool>(ss >> out.heightScale);
        else if (key == "roughness")      ok = static_cast<bool>(ss >> out.roughness);
        else if (key == "seed")           ok = static_cast<bool>(ss >> out.seed);
        else if (key == "fbmOctaves")     ok = static_cast<bool>(ss >> out.fbmOctaves);
        else if (key == "fbmFrequency")   ok = static_cast<bool>(ss >> out.fbmFrequency);
        else if (key == "fbmLacunarity")  ok = static_cast<bool>(ss >> out.fbmLacunarity);
        else if (key == "fbmGain")        ok = static_cast<bool>(ss >> out.fbmGain);
        else if (key == "fbmAmplitude")   ok = static_cast<bool>(ss >> out.fbmAmplitude);
        else if (key == "fbmWarp")        ok = static_cast<bool>(ss >> out.fbmWarp);
        else if (key == "fogDensity")     ok = static_cast<bool>(ss >> out.fogDensity);
        else if (key == "snowLine")       ok = static_cast<bool>(ss >> out.snowLine);
        else if (key == "waterLevel")     ok = static_cast<bool>(ss >> out.waterLevel);
        else if (key == "wireframe") {
            int w = 0;
            if (ss >> w) out.wireframe = (w != 0); else ok = false;
        }
        else if (key == "camX")           ok = static_cast<bool>(ss >> out.camX);
        else if (key == "camY")           ok = static_cast<bool>(ss >> out.camY);
        else if (key == "camZ")           ok = static_cast<bool>(ss >> out.camZ);
        else if (key == "camYaw")         ok = static_cast<bool>(ss >> out.camYaw);
        else if (key == "camPitch")       ok = static_cast<bool>(ss >> out.camPitch);
        else if (key == "camMode")        ok = static_cast<bool>(ss >> out.camMode);
        else if (key == "camFovY")        ok = static_cast<bool>(ss >> out.camFovY);
        else continue;   // 未知键：忽略（向前兼容）

        if (!ok) {
            std::cerr << "[预设] 第 " << lineNo << " 行 \"" << key
                      << "\" 的值无法解析: \"" << val << "\"，已跳过\n";
        } else {
            ++parsed;
        }
    }

    if (parsed == 0) {
        std::cerr << "[预设] 文件里没有解析到任何有效字段: " << path << std::endl;
        return false;
    }

    // ---- 值域保护：手改坏了也不至于让程序崩 ----
    out.resolution  = std::max(4, std::min(out.resolution, 4096));
    out.worldSize   = std::max(1.0f, std::min(out.worldSize, 100000.0f));
    out.heightScale = std::max(0.1f, std::min(out.heightScale, 10000.0f));
    out.roughness   = std::max(0.0f, std::min(out.roughness, 1.0f));
    out.fbmOctaves  = std::max(1, std::min(out.fbmOctaves, 16));
    out.fbmFrequency= std::max(0.01f, std::min(out.fbmFrequency, 1000.0f));
    out.fbmLacunarity=std::max(1.0f, std::min(out.fbmLacunarity, 8.0f));
    out.fbmGain     = std::max(0.0f, std::min(out.fbmGain, 1.0f));
    out.fbmWarp     = std::max(0.0f, std::min(out.fbmWarp, 10.0f));
    out.fogDensity  = std::max(0.0f, std::min(out.fogDensity, 0.02f));
    out.snowLine    = std::max(0.05f, std::min(out.snowLine, 1.0f));
    out.waterLevel  = std::max(0.0f, std::min(out.waterLevel, 0.5f));
    out.camPitch    = std::max(-89.0f, std::min(out.camPitch, 89.0f));
    out.camFovY     = std::max(12.0f, std::min(out.camFovY, 100.0f));
    out.camMode     = (out.camMode != 0) ? 1 : 0;

    return true;
}

// ------------------------------------------------------------
// 预设 → TerrainParams（把散装字段组装成生成器能吃的结构）
// ------------------------------------------------------------
inline TerrainParams toParams(const TerrainPreset& ps) {
    TerrainParams p;
    p.algo        = ps.algo;
    p.resolution  = ps.resolution;
    p.worldSize   = ps.worldSize;
    p.heightScale = ps.heightScale;
    p.roughness   = ps.roughness;
    p.seed        = ps.seed;

    p.fbm.octaves    = ps.fbmOctaves;
    p.fbm.frequency  = ps.fbmFrequency;
    p.fbm.lacunarity = ps.fbmLacunarity;
    p.fbm.gain       = ps.fbmGain;
    p.fbm.amplitude  = ps.fbmAmplitude;
    p.fbm.warp       = ps.fbmWarp;
    // ridged 模式由 algo 决定，不由预设单独指定
    p.fbm.ridged = (ps.algo == TerrainAlgo::Ridged) ? 1.0f : 0.0f;

    return p;
}

} // namespace fractal
