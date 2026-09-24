// ============================================================================
//  GLLoader.cpp —— OpenGL 3.3 Core 函数加载器实现
// ----------------------------------------------------------------------------
//  查址策略（Windows）：
//    1) wglGetProcAddress()  —— 驱动提供的扩展函数走这里（绝大多数 3.3 函数）
//       ⚠ 陷阱：失败时可能返回 1 / 2 / 3 / -1 这类垃圾值，必须过滤，
//         否则调用时会直接崩。GLAD 内部也是这么处理的。
//    2) GetProcAddress(opengl32.dll) —— OpenGL 1.1 的老函数（如 glClear）
//       直接从 opengl32.dll 导出，wglGetProcAddress 反而查不到。
// ============================================================================
#include "GLLoader.h"

namespace gl {

// ---------------------------------------------------------------------------
// 函数指针定义
// ---------------------------------------------------------------------------
PFN_glCreateShader           ft_glCreateShader           = nullptr;
PFN_glShaderSource           ft_glShaderSource           = nullptr;
PFN_glCompileShader          ft_glCompileShader          = nullptr;
PFN_glGetShaderiv            ft_glGetShaderiv            = nullptr;
PFN_glGetShaderInfoLog       ft_glGetShaderInfoLog       = nullptr;
PFN_glDeleteShader           ft_glDeleteShader           = nullptr;
PFN_glCreateProgram          ft_glCreateProgram          = nullptr;
PFN_glAttachShader           ft_glAttachShader           = nullptr;
PFN_glLinkProgram            ft_glLinkProgram            = nullptr;
PFN_glGetProgramiv           ft_glGetProgramiv           = nullptr;
PFN_glGetProgramInfoLog      ft_glGetProgramInfoLog      = nullptr;
PFN_glDeleteProgram          ft_glDeleteProgram          = nullptr;
PFN_glUseProgram             ft_glUseProgram             = nullptr;
PFN_glGetUniformLocation     ft_glGetUniformLocation     = nullptr;
PFN_glUniform1i              ft_glUniform1i              = nullptr;
PFN_glUniform1f              ft_glUniform1f              = nullptr;
PFN_glUniform2fv             ft_glUniform2fv             = nullptr;
PFN_glUniform3f              ft_glUniform3f              = nullptr;
PFN_glUniform3fv             ft_glUniform3fv             = nullptr;
PFN_glUniform4fv             ft_glUniform4fv             = nullptr;
PFN_glUniformMatrix3fv       ft_glUniformMatrix3fv       = nullptr;
PFN_glUniformMatrix4fv       ft_glUniformMatrix4fv       = nullptr;
PFN_glGenVertexArrays        ft_glGenVertexArrays        = nullptr;
PFN_glBindVertexArray        ft_glBindVertexArray        = nullptr;
PFN_glDeleteVertexArrays     ft_glDeleteVertexArrays     = nullptr;
PFN_glGenBuffers             ft_glGenBuffers             = nullptr;
PFN_glBindBuffer             ft_glBindBuffer             = nullptr;
PFN_glBufferData             ft_glBufferData             = nullptr;
PFN_glDeleteBuffers          ft_glDeleteBuffers          = nullptr;
PFN_glEnableVertexAttribArray ft_glEnableVertexAttribArray = nullptr;
PFN_glVertexAttribPointer    ft_glVertexAttribPointer    = nullptr;
PFN_glActiveTexture          ft_glActiveTexture          = nullptr;
PFN_glGenerateMipmap         ft_glGenerateMipmap         = nullptr;

// ---------------------------------------------------------------------------
// 内部记录
// ---------------------------------------------------------------------------
static const char* g_firstMissing = nullptr;
static int         g_missingCount = 0;

// ---------------------------------------------------------------------------
// 核心查址：先查驱动，再查 opengl32.dll
// ---------------------------------------------------------------------------
typedef void* (APIENTRY *PFN_wglGetProcAddress)(const char*);

static void* getProcAddress(const char* name)
{
    // ---- 第一路：驱动（通过 wglGetProcAddress）----
    static PFN_wglGetProcAddress wglGetProcAddressPtr =
        reinterpret_cast<PFN_wglGetProcAddress>(
            reinterpret_cast<void*>(
                GetProcAddress(GetModuleHandleA("opengl32.dll"), "wglGetProcAddress")));

    if (wglGetProcAddressPtr)
    {
        void* p = reinterpret_cast<void*>(wglGetProcAddressPtr(name));

        // 过滤驱动返回的无效值（1~3 以及 -1 都是"没找到"的错误码）
        if (p != nullptr &&
            p != reinterpret_cast<void*>(1) &&
            p != reinterpret_cast<void*>(2) &&
            p != reinterpret_cast<void*>(3) &&
            p != reinterpret_cast<void*>(-1))
        {
            return p;
        }
    }

    // ---- 第二路：opengl32.dll 直接导出（OpenGL 1.1 老函数）----
    static HMODULE glModule = LoadLibraryA("opengl32.dll");
    if (glModule)
        return reinterpret_cast<void*>(GetProcAddress(glModule, name));

    return nullptr;
}

// 取址宏。注意宏名不能叫 GL_LOAD —— Windows 的 GL/gl.h 已定义同名宏。
#define FT_GL_LOAD(fn)                                                    \
    do {                                                                  \
        fn = reinterpret_cast<decltype(fn)>(                              \
                 getProcAddress(reinterpret_cast<const char*>(#fn) + 3)); \
        if (!fn) {                                                        \
            if (!g_firstMissing) g_firstMissing = #fn + 3;                \
            ++g_missingCount;                                             \
        }                                                                 \
    } while (0)

bool loadFunctions()
{
    g_firstMissing = nullptr;
    g_missingCount = 0;

    // ---- 着色器 / 程序 ----
    FT_GL_LOAD(ft_glCreateShader);
    FT_GL_LOAD(ft_glShaderSource);
    FT_GL_LOAD(ft_glCompileShader);
    FT_GL_LOAD(ft_glGetShaderiv);
    FT_GL_LOAD(ft_glGetShaderInfoLog);
    FT_GL_LOAD(ft_glDeleteShader);
    FT_GL_LOAD(ft_glCreateProgram);
    FT_GL_LOAD(ft_glAttachShader);
    FT_GL_LOAD(ft_glLinkProgram);
    FT_GL_LOAD(ft_glGetProgramiv);
    FT_GL_LOAD(ft_glGetProgramInfoLog);
    FT_GL_LOAD(ft_glDeleteProgram);
    FT_GL_LOAD(ft_glUseProgram);

    // ---- uniform ----
    FT_GL_LOAD(ft_glGetUniformLocation);
    FT_GL_LOAD(ft_glUniform1i);
    FT_GL_LOAD(ft_glUniform1f);
    FT_GL_LOAD(ft_glUniform2fv);
    FT_GL_LOAD(ft_glUniform3f);
    FT_GL_LOAD(ft_glUniform3fv);
    FT_GL_LOAD(ft_glUniform4fv);
    FT_GL_LOAD(ft_glUniformMatrix3fv);
    FT_GL_LOAD(ft_glUniformMatrix4fv);

    // ---- VAO / VBO / 顶点属性 ----
    FT_GL_LOAD(ft_glGenVertexArrays);
    FT_GL_LOAD(ft_glBindVertexArray);
    FT_GL_LOAD(ft_glDeleteVertexArrays);
    FT_GL_LOAD(ft_glGenBuffers);
    FT_GL_LOAD(ft_glBindBuffer);
    FT_GL_LOAD(ft_glBufferData);
    FT_GL_LOAD(ft_glDeleteBuffers);
    FT_GL_LOAD(ft_glEnableVertexAttribArray);
    FT_GL_LOAD(ft_glVertexAttribPointer);

    // ---- 纹理（本项目当前未使用）----
    FT_GL_LOAD(ft_glActiveTexture);
    FT_GL_LOAD(ft_glGenerateMipmap);

    return g_missingCount == 0;
}

const char* missingFunctionName()
{
    return g_firstMissing;
}

int missingFunctionCount()
{
    return g_missingCount;
}

#undef FT_GL_LOAD

}  // namespace gl
