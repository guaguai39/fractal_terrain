#pragma once
// ============================================================
// Camera.h - 三维漫游相机（欧拉角 FPS 相机）
// ============================================================
#include <cmath>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace fractal {

// 相机移动方向（位运算组合，支持同时按住多个键）
enum class CameraMove {
    None     = 0,
    Forward  = 1 << 0,
    Backward = 1 << 1,
    Left     = 1 << 2,
    Right    = 1 << 3,
    Up       = 1 << 4,
    Down     = 1 << 5,
};

inline int operator|(CameraMove a, CameraMove b) {
    return static_cast<int>(a) | static_cast<int>(b);
}

// 漫游模式
enum class RoamMode {
    Fly     = 0,   // 自由飞行
    Walk    = 1,   // 沿地形表面行走（自动贴地）
};

class Camera {
public:
    // 位置 / 姿态
    glm::vec3 position{ 0.0f, 40.0f, 120.0f };
    glm::vec3 up{ 0.0f, 1.0f, 0.0f };
    float yaw   = -90.0f;   // 偏航角（绕 Y 轴），-90 度朝 -Z
    float pitch = -18.0f;   // 俯仰角（绕 X 轴），限制在 ±89 度避免万向节死锁

    // 投影参数
    float fovY    = 60.0f;
    float nearPlane = 0.5f;
    float farPlane  = 6000.0f;

    // 运动参数
    float baseSpeed   = 45.0f;   // 基础移动速度（单位/秒）
    float boostFactor = 3.2f;    // Shift 加速倍率
    float sensitivity = 0.10f;   // 鼠标灵敏度（度/像素）
    float eyeHeight   = 2.6f;    // 行走模式下距地表的高度

    RoamMode mode = RoamMode::Fly;

    // 由鼠标位移更新朝向（dx/dy 为像素增量）
    void processMouse(float dx, float dy) {
        yaw   += dx * sensitivity;
        pitch -= dy * sensitivity;      // 屏幕 y 向下为正，取反
        pitch = glm::clamp(pitch, -89.0f, 89.0f);
    }

    // 由滚轮调整 FOV，模拟缩放
    void processScroll(float yoffset) {
        fovY = glm::clamp(fovY - yoffset * 2.0f, 12.0f, 100.0f);
    }

    // 前方向（由欧拉角推出）
    glm::vec3 front() const {
        const float ry = glm::radians(yaw);
        const float rp = glm::radians(pitch);
        return glm::normalize(glm::vec3(
            std::cos(rp) * std::cos(ry),
            std::sin(rp),
            std::cos(rp) * std::sin(ry)));
    }
    glm::vec3 right() const {
        return glm::normalize(glm::cross(front(), up));
    }

    // 更新位置。dt 为秒。terrainHeight 为查询回调（世界 xz → 地表高度）
    template <typename HeightFn>
    void update(float dt, int moveMask, bool boost, HeightFn terrainHeight) {
        float speed = baseSpeed * (boost ? boostFactor : 1.0f);
        const glm::vec3 f = front();
        const glm::vec3 r = right();

        // 飞行模式下水平移动；行走模式下忽略前向的 y 分量，避免"钻地飞行"
        glm::vec3 fwd = f;
        if (mode == RoamMode::Walk) fwd = glm::normalize(glm::vec3(f.x, 0.0f, f.z));

        glm::vec3 delta(0.0f);
        if (moveMask & static_cast<int>(CameraMove::Forward))  delta += fwd * speed;
        if (moveMask & static_cast<int>(CameraMove::Backward)) delta -= fwd * speed;
        if (moveMask & static_cast<int>(CameraMove::Right))    delta += r * speed;
        if (moveMask & static_cast<int>(CameraMove::Left))     delta -= r * speed;

        if (mode == RoamMode::Fly) {
            if (moveMask & static_cast<int>(CameraMove::Up))   delta += up * speed;
            if (moveMask & static_cast<int>(CameraMove::Down)) delta -= up * speed;
        }

        position += delta * dt;

        // 行走模式：自动贴地
        if (mode == RoamMode::Walk) {
            const float ground = terrainHeight(position.x, position.z);
            position.y = ground + eyeHeight;
            // 飞行模式下把 y 锁在地面高度，切回飞行时不会突然弹起
            pitch = glm::clamp(pitch, -85.0f, 85.0f);
        }

        // 防止相机飞到地形外太空，做一个软限制
        const float limit = 4000.0f;
        position.x = glm::clamp(position.x, -limit, limit);
        position.y = glm::clamp(position.y, -500.0f, limit);
        position.z = glm::clamp(position.z, -limit, limit);
    }

    glm::mat4 viewMatrix() const {
        return glm::lookAt(position, position + front(), up);
    }
    glm::mat4 projMatrix(float aspect) const {
        return glm::perspective(glm::radians(fovY), aspect, nearPlane, farPlane);
    }
};

} // namespace fractal
