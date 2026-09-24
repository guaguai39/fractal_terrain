// ============================================================================
//  GLLoader.h —— OpenGL 3.3 Core 函数加载器（GLAD 的轻量替代实现）
// ----------------------------------------------------------------------------
//  为什么需要它？
//    Windows 自带的 opengl32.dll 只导出了 OpenGL 1.1 的函数。OpenGL 1.2 以上
//    的所有函数（着色器、VAO/VBO 等）都由显卡驱动提供，必须通过
//    wglGetProcAddress / GetProcAddress 在**运行时**查询函数地址。
//    GLAD 做的事就是这件事，只是它生成了几十万行代码。本项目只用到三十多个
//    函数，因此手写一份最小实现即可，还省去下载第三方源码。
//
//  命名策略：
//    真实指针变量叫 ft_glXxx（避免与 GL/gl.h 里的同名声明冲突），
//    然后用 #define glXxx ft_glXxx 让业务代码写起来和用 GLAD 完全一样。
//
//  用法：
//    GLFW 创建上下文并 makeCurrent 成功后，调用 gl::loadFunctions()。
//    返回 true 表示全部函数地址查询成功。
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <cstddef>

namespace gl {

// ---------------------------------------------------------------------------
// 1. 补齐 GL/gl.h（只到 1.1）缺失的类型
// ---------------------------------------------------------------------------
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef char      GLchar;

}  // namespace gl

// 把补的类型导出到全局，方便直接写 GLsizeiptr
typedef gl::GLsizeiptr GLsizeiptr;
typedef gl::GLintptr   GLintptr;
typedef gl::GLchar     GLchar;

namespace gl {

// ---------------------------------------------------------------------------
// 2. 常量定义（补齐 1.5+ 缺失部分；已存在的用 #ifndef 保护）
// ---------------------------------------------------------------------------
#ifndef GL_ARRAY_BUFFER
#  define GL_ARRAY_BUFFER              0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#  define GL_ELEMENT_ARRAY_BUFFER      0x8893
#endif
#ifndef GL_STATIC_DRAW
#  define GL_STATIC_DRAW               0x88E4
#endif
#ifndef GL_DYNAMIC_DRAW
#  define GL_DYNAMIC_DRAW              0x88E8
#endif
#ifndef GL_FRAGMENT_SHADER
#  define GL_FRAGMENT_SHADER           0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#  define GL_VERTEX_SHADER             0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#  define GL_COMPILE_STATUS            0x8B81
#endif
#ifndef GL_LINK_STATUS
#  define GL_LINK_STATUS               0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#  define GL_INFO_LOG_LENGTH           0x8B84
#endif
#ifndef GL_MULTISAMPLE
#  define GL_MULTISAMPLE               0x809D
#endif
#ifndef GL_CLAMP_TO_EDGE
#  define GL_CLAMP_TO_EDGE             0x812F
#endif
#ifndef GL_TEXTURE0
#  define GL_TEXTURE0                  0x84C0
#endif
#ifndef GL_SHADING_LANGUAGE_VERSION
#  define GL_SHADING_LANGUAGE_VERSION  0x8B8C
#endif

// ---------------------------------------------------------------------------
// 3. 函数指针类型
// ---------------------------------------------------------------------------
typedef GLuint (APIENTRY *PFN_glCreateShader        )(GLenum);
typedef void   (APIENTRY *PFN_glShaderSource        )(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (APIENTRY *PFN_glCompileShader       )(GLuint);
typedef void   (APIENTRY *PFN_glGetShaderiv         )(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFN_glGetShaderInfoLog    )(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (APIENTRY *PFN_glDeleteShader        )(GLuint);
typedef GLuint (APIENTRY *PFN_glCreateProgram       )(void);
typedef void   (APIENTRY *PFN_glAttachShader        )(GLuint, GLuint);
typedef void   (APIENTRY *PFN_glLinkProgram         )(GLuint);
typedef void   (APIENTRY *PFN_glGetProgramiv        )(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFN_glGetProgramInfoLog   )(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (APIENTRY *PFN_glDeleteProgram       )(GLuint);
typedef void   (APIENTRY *PFN_glUseProgram          )(GLuint);
typedef GLint  (APIENTRY *PFN_glGetUniformLocation  )(GLuint, const GLchar*);
typedef void   (APIENTRY *PFN_glUniform1i           )(GLint, GLint);
typedef void   (APIENTRY *PFN_glUniform1f           )(GLint, GLfloat);
typedef void   (APIENTRY *PFN_glUniform2fv          )(GLint, GLsizei, const GLfloat*);
typedef void   (APIENTRY *PFN_glUniform3f           )(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *PFN_glUniform3fv          )(GLint, GLsizei, const GLfloat*);
typedef void   (APIENTRY *PFN_glUniform4fv          )(GLint, GLsizei, const GLfloat*);
typedef void   (APIENTRY *PFN_glUniformMatrix3fv    )(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void   (APIENTRY *PFN_glUniformMatrix4fv    )(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void   (APIENTRY *PFN_glGenVertexArrays     )(GLsizei, GLuint*);
typedef void   (APIENTRY *PFN_glBindVertexArray     )(GLuint);
typedef void   (APIENTRY *PFN_glDeleteVertexArrays  )(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFN_glGenBuffers          )(GLsizei, GLuint*);
typedef void   (APIENTRY *PFN_glBindBuffer          )(GLenum, GLuint);
typedef void   (APIENTRY *PFN_glBufferData          )(GLenum, GLsizeiptr, const void*, GLenum);
typedef void   (APIENTRY *PFN_glDeleteBuffers       )(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFN_glEnableVertexAttribArray)(GLuint);
typedef void   (APIENTRY *PFN_glVertexAttribPointer )(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void   (APIENTRY *PFN_glActiveTexture       )(GLenum);
typedef void   (APIENTRY *PFN_glGenerateMipmap      )(GLenum);

// ---------------------------------------------------------------------------
// 4. 全局函数指针（定义在 GLLoader.cpp）
//     变量名加 ft_ 前缀，避免与 GL/gl.h 的同名声明冲突。
// ---------------------------------------------------------------------------
extern PFN_glCreateShader           ft_glCreateShader;
extern PFN_glShaderSource           ft_glShaderSource;
extern PFN_glCompileShader          ft_glCompileShader;
extern PFN_glGetShaderiv            ft_glGetShaderiv;
extern PFN_glGetShaderInfoLog       ft_glGetShaderInfoLog;
extern PFN_glDeleteShader           ft_glDeleteShader;
extern PFN_glCreateProgram          ft_glCreateProgram;
extern PFN_glAttachShader           ft_glAttachShader;
extern PFN_glLinkProgram            ft_glLinkProgram;
extern PFN_glGetProgramiv           ft_glGetProgramiv;
extern PFN_glGetProgramInfoLog      ft_glGetProgramInfoLog;
extern PFN_glDeleteProgram          ft_glDeleteProgram;
extern PFN_glUseProgram             ft_glUseProgram;
extern PFN_glGetUniformLocation     ft_glGetUniformLocation;
extern PFN_glUniform1i              ft_glUniform1i;
extern PFN_glUniform1f              ft_glUniform1f;
extern PFN_glUniform2fv             ft_glUniform2fv;
extern PFN_glUniform3f              ft_glUniform3f;
extern PFN_glUniform3fv             ft_glUniform3fv;
extern PFN_glUniform4fv             ft_glUniform4fv;
extern PFN_glUniformMatrix3fv       ft_glUniformMatrix3fv;
extern PFN_glUniformMatrix4fv       ft_glUniformMatrix4fv;
extern PFN_glGenVertexArrays        ft_glGenVertexArrays;
extern PFN_glBindVertexArray        ft_glBindVertexArray;
extern PFN_glDeleteVertexArrays     ft_glDeleteVertexArrays;
extern PFN_glGenBuffers             ft_glGenBuffers;
extern PFN_glBindBuffer             ft_glBindBuffer;
extern PFN_glBufferData             ft_glBufferData;
extern PFN_glDeleteBuffers          ft_glDeleteBuffers;
extern PFN_glEnableVertexAttribArray ft_glEnableVertexAttribArray;
extern PFN_glVertexAttribPointer    ft_glVertexAttribPointer;
extern PFN_glActiveTexture          ft_glActiveTexture;
extern PFN_glGenerateMipmap         ft_glGenerateMipmap;

// ---------------------------------------------------------------------------
// 5. 加载入口
// ---------------------------------------------------------------------------
//  必须在 OpenGL 上下文 makeCurrent 之后调用。
bool loadFunctions();

//  返回第一个未找到的函数名；全部成功时返回 nullptr
const char* missingFunctionName();

//  返回未能加载的函数数量
int missingFunctionCount();

}  // namespace gl

// ---------------------------------------------------------------------------
// 6. 名字映射：让业务代码可以像用 GLAD 一样直接写 glXxx(...)
// ---------------------------------------------------------------------------
#define glCreateShader            ::gl::ft_glCreateShader
#define glShaderSource            ::gl::ft_glShaderSource
#define glCompileShader           ::gl::ft_glCompileShader
#define glGetShaderiv             ::gl::ft_glGetShaderiv
#define glGetShaderInfoLog        ::gl::ft_glGetShaderInfoLog
#define glDeleteShader            ::gl::ft_glDeleteShader
#define glCreateProgram           ::gl::ft_glCreateProgram
#define glAttachShader            ::gl::ft_glAttachShader
#define glLinkProgram             ::gl::ft_glLinkProgram
#define glGetProgramiv            ::gl::ft_glGetProgramiv
#define glGetProgramInfoLog       ::gl::ft_glGetProgramInfoLog
#define glDeleteProgram           ::gl::ft_glDeleteProgram
#define glUseProgram              ::gl::ft_glUseProgram
#define glGetUniformLocation      ::gl::ft_glGetUniformLocation
#define glUniform1i               ::gl::ft_glUniform1i
#define glUniform1f               ::gl::ft_glUniform1f
#define glUniform2fv              ::gl::ft_glUniform2fv
#define glUniform3f               ::gl::ft_glUniform3f
#define glUniform3fv              ::gl::ft_glUniform3fv
#define glUniform4fv              ::gl::ft_glUniform4fv
#define glUniformMatrix3fv        ::gl::ft_glUniformMatrix3fv
#define glUniformMatrix4fv        ::gl::ft_glUniformMatrix4fv
#define glGenVertexArrays         ::gl::ft_glGenVertexArrays
#define glBindVertexArray         ::gl::ft_glBindVertexArray
#define glDeleteVertexArrays      ::gl::ft_glDeleteVertexArrays
#define glGenBuffers              ::gl::ft_glGenBuffers
#define glBindBuffer              ::gl::ft_glBindBuffer
#define glBufferData              ::gl::ft_glBufferData
#define glDeleteBuffers           ::gl::ft_glDeleteBuffers
#define glEnableVertexAttribArray ::gl::ft_glEnableVertexAttribArray
#define glVertexAttribPointer     ::gl::ft_glVertexAttribPointer
#define glActiveTexture           ::gl::ft_glActiveTexture
#define glGenerateMipmap          ::gl::ft_glGenerateMipmap
