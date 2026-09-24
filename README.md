# 基于 OpenGL 的分形地形生成与三维漫游演示系统

Fractal Terrain Generation & 3D Roaming Demo System based on OpenGL 3.3 Core.

---

## 一、当前进度

| 阶段 | 内容 | 状态 |
| --- | --- | --- |
| **Stage 0** | 环境准备（Visual Studio 2026 / CMake / Git） | ✅ 完成 |
| **Stage 1** | 环境验证（OpenGL 3.3 通路） | ✅ 通过 |
| **Stage 2** | 分形算法 + 地形网格 | ✅ 完成 |
| **Stage 3** | 着色器（分层调色 + 光照 + 雾） | ✅ 完成 |
| **Stage 4** | 三维漫游相机（Fly / Walk 双模式） | ✅ 完成 |
| **Stage 5** | 主程序整合、完整交互与渲染循环 | ✅ 完成 |
| **Stage 6** | 实验报告 / 演示 PPT | ⏳ 待做 |

**运行状态**：已完整编译链接并**成功运行**，实测结果：

```
 OpenGL 版本 : 3.3.0 NVIDIA 610.88
 渲染器      : NVIDIA GeForce RTX 4060 Laptop GPU/PCIe/SSE2
 GLSL 版本   : 3.30 NVIDIA via Cg compiler
 着色器      : 加载成功
 地形生成    : 131072 个三角形，耗时 2 ms
```

编译零错误、零警告（MSVC `/std:c++17 /W4`）。

> **关于依赖下载**：本机无法访问 GitHub，因此 GLFW / GLM 改为从 **Gitee 镜像**拉取，
> **GLAD 完全移除**，改用自写的 `src/GLLoader.{h,cpp}` 在运行时向显卡驱动查询
> OpenGL 3.3 函数地址（功能等价，约 200 行，无需任何第三方加载器源码）。

---

## 二、环境要求

### 必需

| 软件 | 版本要求 | 说明 |
| --- | --- | --- |
| **Visual Studio** | 2022 或 2026 社区版 | 安装时**必须勾选「使用 C++ 的桌面开发」**工作负载 |
| **CMake** | 3.16+ | Visual Studio 自带 CMake 工具，通常无需单独安装 |
| **Git** | 任意 | CMake 从 Gitee 拉取 GLFW / GLM 源码时需要 |

### 硬件

本项目的 OpenGL 需求为 **3.3 Core Profile**，属于 2010 年的技术水平，现代核显即可流畅运行。

参考基准：256×256 地形网格约 6.6 万顶点、13 万三角形，在中端独显上可稳定满帧。

---

## 三、快速开始

### 方式 0：一键脚本（推荐，本机已验证可用）

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

脚本会自动处理两件本机特有的坑（见「常见问题」第 1、2 条），然后配置 + 编译 Release。
其他用法：

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Config Debug
powershell -ExecutionPolicy Bypass -File build.ps1 -Clean      # 先删 build 再来
```

### 方式 1：CMake 预设

```bash
cmake --preset vs2026          # 本机实际可用的生成器
cmake --build --preset vs2026-release
```

首次配置会从 **Gitee 镜像**自动下载 GLFW（3.3.9）、GLM（0.9.9.8）源码，需要联网。

**通用配置命令**（不依赖预设）：

```bash
cmake -B build -S .
```

### 2. 构建

```bash
cmake --build --preset vs2026-release
```

等价通用命令：

```bash
cmake --build build --config Release
```

### 3. 运行

```bash
./build/bin/fractal_terrain.exe
```

**注意**：程序需要 `shaders/` 目录。CMake 已配置为构建后自动把 `shaders/`
拷到可执行文件目录旁边（`build/bin/shaders/`），所以直接双击 exe 或从 `bin/`
目录运行都可以。

---

## 四、Visual Studio 图形界面操作

1. **打开项目**：`文件` → `打开` → `CMake`，选择项目根目录的 `CMakeLists.txt`
2. VS 会自动开始配置（首次较慢，在下载依赖）
3. 等待底部状态栏显示「CMake 生成已完成」
4. **选择启动项**：顶部工具栏把启动目标设为 `fractal_terrain.exe`
5. **运行**：按 `F5`（调试）或 `Ctrl+F5`（不调试直接运行）

---

## 五、VS Code 操作

项目已内置 `.vscode/` 配置：

- **`tasks.json`** — `Ctrl+Shift+B` 直接构建
- **`launch.json`** — `F5` 直接构建并运行
- **`settings.json`** — IntelliSense 路径与 GLSL 语法高亮关联

需要安装的扩展：

- **CMake Tools**（必须）
- **C/C++**（IntelliSense，建议）
- **GLSL 语法高亮**（可选，看 shader 代码更舒服）

---

## 六、目录结构

```
.
├── CMakeLists.txt              # 构建配置（Gitee 镜像拉取依赖）
├── CMakePresets.json           # 预设：vs2026 / vs2022 / ninja
├── build.ps1                   # 一键配置+构建脚本（含本机环境坑处理）
├── README.md
├── .gitignore
├── .vscode/                    # VS Code 配置
│   ├── settings.json
│   ├── tasks.json
│   └── launch.json
├── src/
│   ├── main.cpp                # 主程序：窗口、渲染循环、输入回调、命令行参数
│   ├── GLLoader.h              # OpenGL 3.3 函数加载器接口（GLAD 的替代）
│   ├── GLLoader.cpp            # 运行时向驱动查询函数地址
│   ├── Noise.h                 # Perlin 噪声 + FBM + 山脊 + 域扭曲
│   ├── DiamondSquare.h         # 菱形-方形中点位移算法
│   ├── Terrain.h               # 高度场 → 三角网格 + 法线计算
│   ├── TerrainField.h          # 地形字段封装（含贴地高度采样）
│   ├── Camera.h                # 欧拉角 FPS 相机（Fly / Walk）
│   └── Shader.h                # 着色器程序封装
└── shaders/
    ├── terrain.vert            # 地形顶点着色器
    ├── terrain.frag            # 地形片元着色器（分层调色 + 光照 + 雾）
    ├── wire.vert               # 线框顶点着色器
    └── wire.frag               # 线框片元着色器
```

## 七、命令行参数

```
fractal_terrain.exe [选项]

  --width N          窗口宽度（默认 1280）
  --height N         窗口高度（默认 720）
  --shader-dir DIR   shaders 目录路径（默认 shaders）
  --fullscreen       全屏启动
  --help             显示帮助
```

程序启动时会在控制台打印 OpenGL 厂商、渲染器、版本号，
以及地形生成的三角形数量与耗时——这些信息可直接用于实验报告的环境说明。

---

## 八、分形算法说明

### Diamond-Square（菱形-方形中点位移法）

1. 初始化正方形四角为随机高度
2. **Diamond 步**：取正方形中心点，高度 = 四角均值 + 随机偏移
3. **Square 步**：取各边中点，高度 = 边两端与相邻中心点均值 + 随机偏移
4. 递归细分，每层随机偏移幅度乘以 `roughness`（典型 0.5~0.7）

```
   ●───────●           ● 角点
   │   ◇   │           ◇ 菱形步生成的点
   │       │           方 方形步生成的点
   ●───────●
```

每层细分点数翻 4 倍，共 log₂N 层，总复杂度 **O(N²)**。

### FBM（分形布朗运动）

```
height = Σ(i=0..octaves-1) amplitude_i × perlin(x × frequency_i, y × frequency_i)
其中 frequency_i = lacunarity^i，amplitude_i = gain^i
```

| 参数 | 含义 | 典型值 |
| --- | --- | --- |
| `octaves` | 叠加层数，越大细节越丰富 | 6 ~ 8 |
| `lacunarity` | 频率倍增系数 | 2.0 |
| `gain` | 振幅衰减系数（即粗糙度） | 0.5 |
| `freq` | 基础频率，越大山峰越密 | 2.0 |

### Ridged（山脊多分形）

对 Perlin 噪声取绝对值反转：`1 - |noise|`，再平方增强脊线，形成尖锐山脊。
配合权重传递（weight feedback）让脊线在高处更突出、低处更平滑。

### 三种算法对比

| | Diamond-Square | FBM | Ridged |
| --- | --- | --- | --- |
| 原理 | 递归细分 + 中点位移 | 多层噪声叠加 | 噪声绝对值反转 |
| 复杂度 | O(N²) | O(N² × octaves) | 同 FBM |
| 视觉特征 | 山峰尖锐、有格状伪影 | 圆润丘陵、最自然 | 锋利山脊、像高山 |
| 适用于 | 教学演示、快速原型 | 通用地形 | 雪山、险峻地貌 |

---

## 九、渲染管线

```
① 顶点数据（CPU）
      ↓ glBufferData 上传显存
② 顶点着色器（GPU，每顶点执行一次）
      ↓ MVP 矩阵变换：局部 → 世界 → 相机 → 裁剪空间
③ 图元装配 → 每 3 个顶点拼成三角形
      ↓
④ 裁剪 & 透视除法 & 视口变换
      ↓
⑤ 光栅化 → 三角形变成像素，顶点属性线性插值
      ↓
⑥ 片元着色器（GPU，每像素执行一次）
      ↓ 分层调色 + Lambert 光照 + 雾效 + gamma 校正
⑦ 深度测试 & 背面剔除
      ↓
⑧ 写入帧缓冲 → 交换到屏幕
```

### 关键技术点

- **MVP 矩阵**：`proj × view × model`，从右往左对应管线执行顺序
- **法线变换**：用模型矩阵左上 3×3 的**逆转置**，保证非均匀缩放下法线仍垂直表面
- **法线计算**：对高度场用**中心差分** `(hl-hr, 2*cell, hu-hd)`，本质是离散微分
- **缠绕顺序**：CCW 为正面，配合 `glCullFace(GL_BACK)` 剔除约一半三角形
- **gamma 校正**：线性空间算光照，输出前 `pow(c, 1/2.2)` 补偿显示器非线性
- **色调映射**：`c/(c+1)` 把超范围值压回 [0,1]，避免高光死白

---

## 十、操作说明

| 按键 | 功能 |
| --- | --- |
| `W` `A` `S` `D` | 前 / 左 / 后 / 右 移动 |
| `Space` / `Left Ctrl` | 上升 / 下降（飞行模式） |
| 鼠标移动 | 环视（Yaw / Pitch） |
| 鼠标滚轮 | 调整视场角 FOV |
| `Shift` | 加速移动 |
| `Tab` | 切换 飞行 / 行走 模式 |
| `1` / `2` / `3` | 切换算法：Diamond-Square / FBM / Ridged |
| `R` | 用新的随机种子重新生成地形 |
| `+` / `-` | 增大 / 减小地形起伏强度 |
| `[` / `]` | 降低 / 提高地形分辨率 |
| `F` | 切换线框叠加 |
| `,` / `.` | 雾浓度 减 / 增 |
| `F5` | 保存当前地形预设到文件 |
| `F9` | 从文件读回预设（地形 + 外观 + 相机一起还原） |
| `L` | 解锁 / 锁定鼠标 |
| `Esc` | 退出 |

### 地形预设（F5 / F9）

按 `F5` 把当前所有参数存成一份纯文本预设，默认写到工作目录下的
`terrain_preset.txt`，内容形如：

```ini
algo        = 1          # 0=Diamond-Square 1=FBM 2=Ridged
resolution  = 256
heightScale = 34
seed        = 2024
fogDensity  = 0.0022
wireframe   = 0
camY        = 40
```

它记录的是「决定画面长什么样」的全部参数：地形算法、分辨率、起伏、种子、
FBM 各层参数、雾浓度、雪线、水位、线框开关，以及相机的坐标与朝向。

- 按 `F9` 读回，地形会重建、网格重传 GPU、相机归位，**一步回到当时的画面**
- 文件可以直接用文本编辑器改（改完在程序里按 `F9` 生效）
- 若工作目录下已存在该文件，**程序启动时会自动应用**，不用手动按 `F9`
- 用 `--preset FILE` 可以指定别的预设文件路径
- 字段缺失按默认值处理、数值越界会被自动夹到安全范围，所以手改坏了也不至于让程序崩溃

---

## 十一、常见问题

> 以下前两条是本机实际踩到的坑，已在 `build.ps1` 中自动处理。

### 1. MSBuild 报 `error MSB6001: "CL.exe" 的命令行开关无效 ... 已添加具有相同键的项`

**根因**：宿主环境同时导出了 `Path` 和 `PATH` 两个环境变量键（Windows 本身大小写不敏感，
但 .NET 的 `ProcessStartInfo.EnvironmentVariables` 用的是大小写敏感的 Hashtable），
MSBuild 一启动就抛 `System.ArgumentException`，导致 `cl.exe` 根本没被执行。

**解决**：调用 CMake 前把环境变量名去重：

```powershell
$pathValue = $env:PATH
[System.Environment]::SetEnvironmentVariable("Path", $null, "Process")
[System.Environment]::SetEnvironmentVariable("PATH", $null, "Process")
$env:PATH = $pathValue
```

`build.ps1` 已内置这段逻辑。同理，`HTTP_PROXY` / `http_proxy` 等成对出现的变量也会触发该异常。

### 2. 配置时报错 `Compatibility with CMake < 3.5 has been removed`

**根因**：CMake 4.x 移除了对 `cmake_minimum_required(<3.5)` 的兼容，
而 GLM 0.9.9.8 的 `CMakeLists.txt` 声明的是 3.1。

**解决**：配置时加一个策略下限：

```bash
cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

> 用 PowerShell 手敲这条命令时注意：`=3.5` 可能被解析器拆开，
> 建议先存变量再展开：`$p="3.5"; cmake ... "-DCMAKE_POLICY_VERSION_MINIMUM=$p"`。

### 3. 配置时报错找不到编译器

说明 Visual Studio 安装时**没有勾选「使用 C++ 的桌面开发」**。

解决：打开 **Visual Studio Installer** → 找到已安装的版本 → 点「修改」→
在「工作负载」页勾选「使用 C++ 的桌面开发」→ 安装。
**不需要重装整个 Visual Studio。**

### 4. 配置时卡在下载依赖 / 超时

本项目已改用 **Gitee 镜像**（`https://gitee.com/mirrors/glfw.git`），国内可直连。
若仍失败，可手工克隆到 `third_party/` 下，CMake 会优先使用本地副本：

```
third_party/glfw/    ← git clone https://gitee.com/mirrors/glfw.git -b 3.3.9
third_party/glm/     ← git clone https://gitee.com/mirrors/glm.git  -b 0.9.9.8
```

或用 `-DUSE_FETCHCONTENT=OFF` 指定已有库路径：

```bash
cmake -B build -S . -DUSE_FETCHCONTENT=OFF ^
      -DGLFW_ROOT=路径/glfw ^
      -DGLM_ROOT=路径/glm
```

### 5. 运行时报错「无法打开 shaders/xxx.vert」

说明工作目录不对。两种解法：

1. 从可执行文件所在目录运行（`build/bin/`）
2. 或在 Visual Studio 中设置 `项目属性 → 调试 → 工作目录` 为 `$(OutDir)`

CMake 已配置为构建后自动拷贝 `shaders/` 到 exe 旁边，所以按上面做即可。

### 6. 窗口打开后是黑屏

- 确认没开独显直连/驱动异常：更新显卡驱动
- 确认 `glClearColor` 和 `glClear` 都调用了
- 检查着色器编译日志（程序会在控制台输出编译错误详情）

### 7. 编译报错 `xxx: 未声明的标识符`（glXxx 系列）

说明 `GLLoader` 的名字映射没生效。检查：

- 是否在包含 `GL/gl.h` **之后**才包含 `GLLoader.h`（宏必须在 gl.h 之后定义）
- `GLLoader.cpp` 是否已加入 `FT_SOURCES`
- 函数指针名必须是 `ft_glXxx` 形式，宏映射表是否与之一致

### 8. 启动即报「OpenGL 函数加载失败，缺失 N 个」

说明显卡驱动不支持 OpenGL 3.3，或运行在没有 3D 加速的环境（远程桌面、虚拟机）。
先更新显卡驱动；若使用远程桌面，请改到本机运行。

---

## 十二、已知限制

- 未实现 LOD（细节层次）与视锥体剔除，超高分辨率（≥1024）时首次生成耗时较长
- 无纹理贴图与阴影映射（Shadow Map），仅用顶点色 + 光照
- 无水面反射/折射，水位以下仅为静态地形配色
- 地形生成在 CPU 上同步执行，大分辨率重建时会短暂卡顿

---

## 十三、许可

教学演示用途，可自由修改使用。
