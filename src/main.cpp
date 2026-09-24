// ============================================================
// main.cpp - 基于 OpenGL 的分形地形生成与三维漫游演示系统
//
// 功能：
//   - 三种分形算法生成地形（Diamond-Square / FBM / Ridged）
//   - 三角网格渲染 + 高度分层着色 + Lambert 光照 + 雾效
//   - 三维漫游（飞行模式 / 行走模式）
//   - 运行时实时调参（算法、分辨率、起伏强度、种子、线框）
//
// 依赖：GLFW 3.3+ / 自写 GL 加载器(GLLoader) / GLM 0.9.9+
// 标准：C++17
// ============================================================
#include "GLLoader.h"
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Shader.h"
#include "Camera.h"
#include "Terrain.h"
#include "TerrainField.h"
#include "TerrainPreset.h"

using namespace fractal;

// ------------------------------------------------------------
// 全局状态
// ------------------------------------------------------------
namespace {

// 窗口
int g_winWidth  = 1280;
int g_winHeight = 720;
const char* g_shaderDir = "shaders";   // 可用 --shader-dir 覆盖

// 相机与输入
Camera  g_camera;
bool    g_firstMouse   = true;
float   g_lastMouseX   = 0.0f;
float   g_lastMouseY   = 0.0f;
bool    g_cursorLocked = true;
int     g_moveMask     = 0;      // 由键盘状态每帧重算
bool    g_boost        = false;

// 地形与渲染
TerrainField g_terrain;
bool         g_wireframe   = false;
bool         g_needRebuild = false;
int          g_rebuildReason = 0;   // 1=换种子 2=改分辨率 3=改起伏

// 光照与大气参数
glm::vec3 g_lightDir     = glm::vec3(-0.45f, -0.80f, -0.40f);   // 指向光源
glm::vec3 g_lightColor   = glm::vec3(1.00f, 0.96f, 0.88f);
glm::vec3 g_ambientColor = glm::vec3(0.32f, 0.38f, 0.48f);
glm::vec3 g_fogColor     = glm::vec3(0.53f, 0.72f, 0.87f);
float     g_fogDensity   = 0.0022f;
float     g_snowLine     = 0.82f;   // 雪线高度比
float     g_waterLevel   = 0.06f;   // 水位高度比

// 性能统计
float g_fps        = 0.0f;
float g_fpsTimer   = 0.0f;
int   g_frameCount = 0;
double g_lastTitleUpdate = 0.0;

std::string g_windowTitleBase = "Fractal Terrain";

// 预设文件路径。默认放在工作目录下，可被 --preset 覆盖
std::string g_presetPath = "terrain_preset.txt";

} // anonymous namespace

// ------------------------------------------------------------
// 预设：从当前全局状态收集 / 应用到全局状态
// ------------------------------------------------------------
// 收集当前状态。放在主循环外的场景重建逻辑复用。
static TerrainPreset collectPreset() {
    TerrainPreset ps;
    const TerrainParams& p = g_terrain.params();

    ps.algo        = p.algo;
    ps.resolution  = g_terrain.resolution();   // 用实际值（已被向上取整到 2^n）
    ps.worldSize   = p.worldSize;
    ps.heightScale = p.heightScale;
    ps.roughness   = p.roughness;
    ps.seed        = p.seed;

    ps.fbmOctaves    = p.fbm.octaves;
    ps.fbmFrequency  = p.fbm.frequency;
    ps.fbmLacunarity = p.fbm.lacunarity;
    ps.fbmGain       = p.fbm.gain;
    ps.fbmAmplitude  = p.fbm.amplitude;
    ps.fbmWarp       = p.fbm.warp;

    ps.fogDensity = g_fogDensity;
    ps.snowLine   = g_snowLine;
    ps.waterLevel = g_waterLevel;
    ps.wireframe  = g_wireframe;

    ps.camX     = g_camera.position.x;
    ps.camY     = g_camera.position.y;
    ps.camZ     = g_camera.position.z;
    ps.camYaw   = g_camera.yaw;
    ps.camPitch = g_camera.pitch;
    ps.camMode  = (g_camera.mode == RoamMode::Walk) ? 1 : 0;
    ps.camFovY  = g_camera.fovY;

    return ps;
}

// ------------------------------------------------------------
// GLFW 回调
// ------------------------------------------------------------
static void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    if (height == 0) height = 1;
    g_winWidth  = width;
    g_winHeight = height;
    glViewport(0, 0, width, height);
}

static void mouseCallback(GLFWwindow*, double xpos, double ypos) {
    if (!g_cursorLocked) return;

    // 首次回调只记录位置：光标刚锁定时坐标会突变，直接算增量会导致视角猛地一甩
    if (g_firstMouse) {
        g_lastMouseX = static_cast<float>(xpos);
        g_lastMouseY = static_cast<float>(ypos);
        g_firstMouse = false;
        return;
    }

    const float dx = static_cast<float>(xpos) - g_lastMouseX;
    const float dy = static_cast<float>(ypos) - g_lastMouseY;
    g_lastMouseX = static_cast<float>(xpos);
    g_lastMouseY = static_cast<float>(ypos);

    g_camera.processMouse(dx, dy);
}

static void scrollCallback(GLFWwindow*, double, double yoffset) {
    g_camera.processScroll(static_cast<float>(yoffset));
}

// 打印当前参数（切换后给用户反馈）
static void printStatus() {
    const TerrainParams& p = g_terrain.params();
    std::cout << "[状态] 算法=" << algoName(p.algo)
              << "  分辨率=" << g_terrain.resolution()
              << "  起伏=" << static_cast<int>(p.heightScale)
              << "  种子=" << p.seed
              << "  三角形=" << g_terrain.mesh().triangleCount()
              << "  模式=" << (g_camera.mode == RoamMode::Walk ? "行走" : "飞行")
              << std::endl;
}

static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    TerrainParams p = g_terrain.params();

    switch (key) {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(w, GLFW_TRUE);
        break;

    // ---- 切换算法 ----
    case GLFW_KEY_1:
    case GLFW_KEY_2:
    case GLFW_KEY_3: {
        p.algo = static_cast<TerrainAlgo>(key - GLFW_KEY_1);
        g_terrain.regenerate(p);
        printStatus();
        break;
    }

    // ---- 切换漫游模式 ----
    case GLFW_KEY_TAB:
        g_camera.mode = (g_camera.mode == RoamMode::Fly)
                            ? RoamMode::Walk : RoamMode::Fly;
        if (g_camera.mode == RoamMode::Walk) {
            // 切到行走时，把相机放到当前水平位置的地面上，避免从天上掉下来
            const float ground = g_terrain.heightAt(g_camera.position.x,
                                                    g_camera.position.z);
            g_camera.position.y = ground + g_camera.eyeHeight;
            std::cout << "[模式] 行走模式（自动贴地）" << std::endl;
        } else {
            std::cout << "[模式] 飞行模式（自由升降）" << std::endl;
        }
        break;

    // ---- 用当前参数重新生成 ----
    case GLFW_KEY_R: {
        p.seed = static_cast<uint32_t>(std::rand() % 100000u);
        auto t0 = std::chrono::high_resolution_clock::now();
        g_terrain.regenerate(p);
        auto t1 = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "[重建] 种子=" << p.seed
                  << "  耗时=" << static_cast<int>(ms) << " ms" << std::endl;
        printStatus();
        break;
    }

    // ---- 调节起伏强度 ----
    case GLFW_KEY_EQUAL:      // '=' 也即 '+'（不按 Shift）
    case GLFW_KEY_KP_ADD:
        p.heightScale = std::min(p.heightScale * 1.25f, 400.0f);
        g_terrain.regenerate(p);
        std::cout << "[起伏] " << static_cast<int>(p.heightScale) << std::endl;
        break;

    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT:
        p.heightScale = std::max(p.heightScale * 0.8f, 4.0f);
        g_terrain.regenerate(p);
        std::cout << "[起伏] " << static_cast<int>(p.heightScale) << std::endl;
        break;

    // ---- 调节分辨率 ----
    case GLFW_KEY_LEFT_BRACKET: {     // '[' 降低
        if (p.resolution > 32) {
            p.resolution = std::max(32, p.resolution / 2);
            auto t0 = std::chrono::high_resolution_clock::now();
            g_terrain.regenerate(p);
            auto t1 = std::chrono::high_resolution_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "[分辨率] " << g_terrain.resolution()
                      << "  重建耗时=" << static_cast<int>(ms) << " ms" << std::endl;
        }
        break;
    }

    case GLFW_KEY_RIGHT_BRACKET: {    // ']' 提高
        if (p.resolution < 2048) {
            p.resolution = std::min(2048, p.resolution * 2);
            auto t0 = std::chrono::high_resolution_clock::now();
            g_terrain.regenerate(p);
            auto t1 = std::chrono::high_resolution_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cout << "[分辨率] " << g_terrain.resolution()
                      << "  重建耗时=" << static_cast<int>(ms) << " ms" << std::endl;
        }
        break;
    }

    // ---- 保存地形预设（F5 保存 / F9 读取）----
    case GLFW_KEY_F5: {
        const TerrainPreset ps = collectPreset();
        if (savePreset(g_presetPath, ps)) {
            std::cout << "[预设] 已保存到 " << g_presetPath
                      << "  （算法=" << algoName(ps.algo)
                      << " 种子=" << ps.seed
                      << " 分辨率=" << ps.resolution
                      << " 起伏=" << static_cast<int>(ps.heightScale) << "）"
                      << std::endl;
        }
        break;
    }

    // ---- 读取地形预设 ----
    case GLFW_KEY_F9: {
        TerrainPreset ps = collectPreset();   // 用当前值垫底，缺失字段就保留现状
        if (loadPreset(g_presetPath, ps)) {
            // 1) 应用地形生成参数
            TerrainParams np = toParams(ps);
            auto t0 = std::chrono::high_resolution_clock::now();
            if (!g_terrain.regenerate(np)) {
                std::cerr << "[预设] 地形重建失败，已保留原地形。" << std::endl;
                break;
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

            // 2) 标记需要重传 GPU
            g_needRebuild = true;
            g_rebuildReason = 4;   // 4 = 读预设

            // 3) 应用外观参数
            g_fogDensity = ps.fogDensity;
            g_snowLine   = ps.snowLine;
            g_waterLevel = ps.waterLevel;
            g_wireframe  = ps.wireframe;

            // 4) 应用相机。行走模式要重新贴地，否则可能悬空或穿地
            g_camera.mode     = (ps.camMode == 1) ? RoamMode::Walk : RoamMode::Fly;
            g_camera.position = glm::vec3(ps.camX, ps.camY, ps.camZ);
            g_camera.yaw      = ps.camYaw;
            g_camera.pitch    = glm::clamp(ps.camPitch, -89.0f, 89.0f);
            g_camera.fovY     = ps.camFovY;
            if (g_camera.mode == RoamMode::Walk) {
                const float ground = g_terrain.heightAt(g_camera.position.x,
                                                        g_camera.position.z);
                g_camera.position.y = ground + g_camera.eyeHeight;
            }

            std::cout << "[预设] 已读回 " << g_presetPath
                      << "  重建耗时=" << static_cast<int>(ms) << " ms" << std::endl;
            printStatus();
        }
        break;
    }

    // ---- 线框开关 ----
    case GLFW_KEY_F:
        g_wireframe = !g_wireframe;
        std::cout << "[线框] " << (g_wireframe ? "开" : "关") << std::endl;
        break;

    // ---- 雾浓度微调 ----
    case GLFW_KEY_COMMA:
        g_fogDensity = std::max(0.0f, g_fogDensity * 0.8f);
        break;
    case GLFW_KEY_PERIOD:
        g_fogDensity = std::min(0.02f, g_fogDensity * 1.25f);
        break;

    // ---- 解锁/锁定光标 ----
    case GLFW_KEY_L: {
        g_cursorLocked = !g_cursorLocked;
        glfwSetInputMode(w, GLFW_CURSOR,
            g_cursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        g_firstMouse = true;    // 重新锁定时要重置，避免视角跳变
        break;
    }

    default:
        break;
    }
}

// ------------------------------------------------------------
// 命令行参数
// ------------------------------------------------------------
static void printUsage(const char* prog) {
    std::cout << "用法: " << prog << " [选项]\n"
              << "  --width N          窗口宽度（默认 1280）\n"
              << "  --height N         窗口高度（默认 720）\n"
              << "  --shader-dir DIR   shaders 目录路径（默认 shaders）\n"
              << "  --preset FILE      预设文件路径（默认 terrain_preset.txt）\n"
              << "  --fullscreen       全屏启动\n"
              << "  --help             显示本帮助\n";
}

static bool parseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto needValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "[参数] " << name << " 缺少取值" << std::endl;
                return nullptr;
            }
            return argv[++i];
        };

        if (a == "--width") {
            const char* v = needValue("--width");
            if (!v) return false;
            g_winWidth = std::atoi(v);
        } else if (a == "--height") {
            const char* v = needValue("--height");
            if (!v) return false;
            g_winHeight = std::atoi(v);
        } else if (a == "--shader-dir") {
            const char* v = needValue("--shader-dir");
            if (!v) return false;
            g_shaderDir = v;
        } else if (a == "--preset") {
            const char* v = needValue("--preset");
            if (!v) return false;
            g_presetPath = v;
        } else if (a == "--fullscreen") {
            // 由 main 中 glfwGetPrimaryMonitor 处理
            g_winWidth = -1;
        } else if (a == "--help" || a == "-h") {
            printUsage(argv[0]);
            return false;
        } else {
            std::cerr << "[参数] 未知选项: " << a << "\n";
            printUsage(argv[0]);
            return false;
        }
    }
    return true;
}

// ------------------------------------------------------------
// 地形 GPU 资源
// ------------------------------------------------------------
struct GpuMesh {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;

    void destroy() {
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        ebo = vbo = vao = 0;
        indexCount = 0;
    }
};

// 把 TerrainMesh 上传到 GPU
static bool uploadMesh(const TerrainMesh& mesh, GpuMesh& gpu) {
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        std::cerr << "[GPU] 网格为空，跳过上传" << std::endl;
        return false;
    }

    gpu.destroy();

    glGenVertexArrays(1, &gpu.vao);
    glGenBuffers(1, &gpu.vbo);
    glGenBuffers(1, &gpu.ebo);

    glBindVertexArray(gpu.vao);

    glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(TerrainVertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    // 顶点属性布局，必须与 terrain.vert 的 layout(location=N) 严格对应
    const GLsizei stride = sizeof(TerrainVertex);
    // location 0: 位置
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(TerrainVertex, position)));
    glEnableVertexAttribArray(0);
    // location 1: 法线
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(TerrainVertex, normal)));
    glEnableVertexAttribArray(1);
    // location 2: 高度比
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(TerrainVertex, heightRatio)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    gpu.indexCount = static_cast<GLsizei>(mesh.indices.size());

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[GPU] 上传网格时出现 GL 错误: 0x"
                  << std::hex << err << std::dec << std::endl;
        return false;
    }
    return true;
}

// ------------------------------------------------------------
// 入口
// ------------------------------------------------------------
int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (!parseArgs(argc, argv)) {
        return 0;
    }

    // ---- 1. 初始化 GLFW ----
    if (!glfwInit()) {
        std::cerr << "[错误] GLFW 初始化失败。请确认显卡驱动正常。" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);        // 4x MSAA
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    const bool fullscreen = (g_winWidth == -1);
    GLFWmonitor* monitor = fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    if (fullscreen) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        g_winWidth  = mode ? mode->width  : 1920;
        g_winHeight = mode ? mode->height : 1080;
    }

    GLFWwindow* window = glfwCreateWindow(g_winWidth, g_winHeight,
        "Fractal Terrain", monitor, nullptr);
    if (!window) {
        std::cerr << "[错误] 创建窗口失败。显卡驱动可能不支持 OpenGL 3.3 Core，"
                     "请更新驱动后重试。" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    // 锁定光标以支持无限环视
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    glfwSwapInterval(1);    // 垂直同步

    // ---- 2. 加载 OpenGL 3.3 函数 ----
    if (!gl::loadFunctions()) {
        std::cerr << "[错误] OpenGL 函数加载失败，缺失 " << gl::missingFunctionCount()
                  << " 个，首个缺失函数: "
                  << (gl::missingFunctionName() ? gl::missingFunctionName() : "?")
                  << "\n       请确认显卡驱动已安装且支持 OpenGL 3.3。" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // ---- 3. 环境信息 ----
    const GLubyte* glVersion  = glGetString(GL_VERSION);
    const GLubyte* glRenderer = glGetString(GL_RENDERER);
    const GLubyte* glVendor   = glGetString(GL_VENDOR);

    std::cout << "==================================================\n";
    std::cout << " 基于 OpenGL 的分形地形生成与三维漫游演示系统\n";
    std::cout << "==================================================\n";
    std::cout << " OpenGL 版本 : " << (glVersion  ? (const char*)glVersion  : "?") << "\n";
    std::cout << " 渲染器      : " << (glRenderer ? (const char*)glRenderer : "?") << "\n";
    std::cout << " 厂商        : " << (glVendor   ? (const char*)glVendor   : "?") << "\n";
    std::cout << " GLSL 版本   : " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
    std::cout << "--------------------------------------------------\n";

    // ---- 4. 加载着色器 ----
    const std::string shaderDir = g_shaderDir;
    Shader terrainShader;
    Shader wireShader;

    if (!terrainShader.loadFromFile(shaderDir + "/terrain.vert",
                                    shaderDir + "/terrain.frag")) {
        std::cerr << "[错误] 地形着色器加载失败。\n"
                  << "  查找目录: " << shaderDir << "\n"
                  << "  提示: 若从其他目录运行程序，请用 --shader-dir 指定 shaders 路径。\n"
                  << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    if (!wireShader.loadFromFile(shaderDir + "/wire.vert",
                                 shaderDir + "/wire.frag")) {
        std::cerr << "[错误] 线框着色器加载失败。" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    std::cout << " 着色器      : 加载成功\n";

    // ---- 5. 生成地形 ----
    // 先铺一套默认值，再尝试用预设文件覆盖 —— 这样预设缺字段也不会出问题
    TerrainParams params;
    params.algo        = TerrainAlgo::DiamondSquare;
    params.resolution  = 256;
    params.worldSize   = 240.0f;
    params.heightScale = 34.0f;
    params.roughness   = 0.55f;
    params.seed        = 2024u;
    // FBM 参数
    params.fbm.octaves    = 7;
    params.fbm.frequency  = 3.0f;
    params.fbm.lacunarity = 2.0f;
    params.fbm.gain       = 0.5f;
    params.fbm.amplitude  = 1.0f;
    params.fbm.warp       = 0.35f;   // 域扭曲，削弱格状伪影

    // 启动时若预设文件存在，就用它覆盖默认值（没有也不报错，静默跳过）
    {
        TerrainPreset boot;
        boot.algo         = params.algo;
        boot.resolution   = params.resolution;
        boot.worldSize    = params.worldSize;
        boot.heightScale  = params.heightScale;
        boot.roughness    = params.roughness;
        boot.seed         = params.seed;
        boot.fbmOctaves     = params.fbm.octaves;
        boot.fbmFrequency   = params.fbm.frequency;
        boot.fbmLacunarity  = params.fbm.lacunarity;
        boot.fbmGain        = params.fbm.gain;
        boot.fbmAmplitude   = params.fbm.amplitude;
        boot.fbmWarp        = params.fbm.warp;

        if (loadPreset(g_presetPath, boot)) {
            params = toParams(boot);

            g_fogDensity = boot.fogDensity;
            g_snowLine   = boot.snowLine;
            g_waterLevel = boot.waterLevel;
            g_wireframe  = boot.wireframe;

            std::cout << " 启动预设    : " << g_presetPath << " （已应用）\n";
        }
    }

    {
        auto t0 = std::chrono::high_resolution_clock::now();
        if (!g_terrain.regenerate(params)) {
            std::cerr << "[错误] 地形生成失败。" << std::endl;
            glfwDestroyWindow(window);
            glfwTerminate();
            return -1;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << " 地形生成    : " << g_terrain.mesh().triangleCount()
                  << " 个三角形，耗时 " << static_cast<int>(ms) << " ms\n";
    }
    std::cout << "--------------------------------------------------\n";

    // ---- 6. 上传到 GPU ----
    GpuMesh gpu;
    if (!uploadMesh(g_terrain.mesh(), gpu)) {
        std::cerr << "[错误] 地形数据上传 GPU 失败。" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // ---- 7. 相机初始位置：站在地形外侧俯视 ----
    {
        const float ws = params.worldSize;
        g_camera.position = glm::vec3(0.0f, g_terrain.mesh().maxHeight * 1.6f + 40.0f,
                                      ws * 0.62f);
        g_camera.yaw   = -90.0f;
        g_camera.pitch = -22.0f;
    }

    // ---- 8. 渲染状态 ----
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_MULTISAMPLE);

    // 操作说明
    std::cout << "\n操作说明\n"
              << "  W A S D        前 / 左 / 后 / 右 移动\n"
              << "  Space / Ctrl   上升 / 下降（飞行模式）\n"
              << "  鼠标移动       环视\n"
              << "  鼠标滚轮       调整视场角\n"
              << "  Shift          加速\n"
              << "  Tab            切换 飞行 / 行走 模式\n"
              << "  1 2 3          切换算法 Diamond-Square / FBM / Ridged\n"
              << "  R              用新种子重新生成\n"
              << "  + / -          增大 / 减小起伏强度\n"
              << "  [ / ]          降低 / 提高分辨率\n"
              << "  F              线框开关\n"
              << "  , / .          雾浓度 减 / 增\n"
              << "  F5 / F9        保存 / 读取地形预设\n"
              << "  L              解锁 / 锁定鼠标\n"
              << "  Esc            退出\n"
              << std::endl;

    // ---- 9. 主循环 ----
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const float  dt  = static_cast<float>(now - lastTime);
        lastTime = now;

        glfwPollEvents();

        // ---- 若按 F9 读回了预设，这里把新网格重新传上 GPU ----
        // 地形数据在 CPU 侧已由 g_terrain.regenerate() 更新，
        // 但显存里还是旧网格，必须重传否则画面不变。
        if (g_needRebuild) {
            if (!uploadMesh(g_terrain.mesh(), gpu)) {
                std::cerr << "[GPU] 预设网格上传失败" << std::endl;
            } else {
                std::cout << "[GPU] 网格已重传: " << gpu.indexCount << " 个索引"
                          << std::endl;
            }
            g_needRebuild   = false;
            g_rebuildReason = 0;
        }

        // ---- 输入：每帧重算移动掩码（支持同时按多个键）----
        g_moveMask = 0;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            g_moveMask |= static_cast<int>(CameraMove::Forward);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            g_moveMask |= static_cast<int>(CameraMove::Backward);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            g_moveMask |= static_cast<int>(CameraMove::Left);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            g_moveMask |= static_cast<int>(CameraMove::Right);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            g_moveMask |= static_cast<int>(CameraMove::Up);
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
            g_moveMask |= static_cast<int>(CameraMove::Down);
        g_boost = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                   glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

        // ---- 更新相机（行走模式会调用 heightAt 自动贴地）----
        g_camera.update(dt, g_moveMask, g_boost,
            [](float x, float z) { return g_terrain.heightAt(x, z); });

        // ---- 清屏 ----
        glClearColor(g_fogColor.r, g_fogColor.g, g_fogColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---- 每帧计算矩阵 ----
        const float aspect = (g_winHeight > 0)
            ? static_cast<float>(g_winWidth) / static_cast<float>(g_winHeight)
            : 1.0f;

        const glm::mat4 model = glm::mat4(1.0f);   // 地形已在世界坐标，无需再变换
        const glm::mat4 view  = g_camera.viewMatrix();
        const glm::mat4 proj  = g_camera.projMatrix(aspect);
        const glm::mat4 mvp   = proj * view * model;

        // 法线矩阵：模型矩阵的逆转置。模型是单位阵时结果也是单位阵，
        // 但保持这个写法是正确做法（模型将来若有非均匀缩放仍成立）
        const glm::mat3 normalMatrix =
            glm::transpose(glm::inverse(glm::mat3(model)));

        // ---- Pass 1: 地形实体 ----
        terrainShader.use();
        terrainShader.setMat4("uModel", model);
        terrainShader.setMat4("uView",  view);
        terrainShader.setMat4("uProj",  proj);
        terrainShader.setMat3("uNormalMatrix", normalMatrix);
        terrainShader.setVec3("uLightDir",     g_lightDir);
        terrainShader.setVec3("uLightColor",   g_lightColor);
        terrainShader.setVec3("uAmbientColor", g_ambientColor);
        terrainShader.setVec3("uViewPos",      g_camera.position);
        terrainShader.setVec3("uFogColor",     g_fogColor);
        terrainShader.setFloat("uFogDensity",  g_fogDensity);
        terrainShader.setFloat("uSnowLine",    g_snowLine);
        terrainShader.setFloat("uWaterLevel",  g_waterLevel);

        glBindVertexArray(gpu.vao);
        glDrawElements(GL_TRIANGLES, gpu.indexCount, GL_UNSIGNED_INT, nullptr);

        // ---- Pass 2: 线框叠加 ----
        if (g_wireframe) {
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1.0f, -1.0f);   // 向观察者方向偏移，防止与实体 z-fighting
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

            wireShader.use();
            wireShader.setMat4("uMVP", mvp);
            wireShader.setVec3("uWireColor", glm::vec3(0.15f, 0.55f, 0.65f));
            wireShader.setFloat("uAlpha", 0.85f);
            wireShader.setFloat("uMinRatio", g_terrain.minRatio());
            wireShader.setFloat("uMaxRatio", g_terrain.maxRatio());

            glDrawElements(GL_TRIANGLES, gpu.indexCount, GL_UNSIGNED_INT, nullptr);

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDisable(GL_POLYGON_OFFSET_LINE);
        }

        // ---- 线框叠加完成，恢复实体填充模式 ----
        glfwSwapBuffers(window);

        // ---- FPS 统计与标题更新 ----
        ++g_frameCount;
        g_fpsTimer += dt;
        if (g_fpsTimer >= 0.5f) {
            g_fps = static_cast<float>(g_frameCount) / g_fpsTimer;
            g_frameCount = 0;
            g_fpsTimer = 0.0f;
        }

        if (now - g_lastTitleUpdate > 0.25) {
            g_lastTitleUpdate = now;
            char title[320];
            std::snprintf(title, sizeof(title),
                "Fractal Terrain | %s | %dx%d | %d tris | %s | %.0f FPS",
                algoName(g_terrain.params().algo),
                g_terrain.resolution(), g_terrain.resolution(),
                static_cast<int>(g_terrain.mesh().triangleCount()),
                (g_camera.mode == RoamMode::Walk ? "Walk" : "Fly"),
                g_fps);
            glfwSetWindowTitle(window, title);
        }
    }

    // ---- 10. 清理 ----
    gpu.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "程序正常退出。" << std::endl;
    return 0;
}
