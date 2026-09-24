#pragma once
// ============================================================
// Shader.h - OpenGL 着色器程序封装
// ============================================================
#include "GLLoader.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

namespace fractal {

class Shader {
public:
    GLuint id = 0;

    Shader() = default;
    ~Shader() { if (id) glDeleteProgram(id); }

    // 禁止拷贝（避免重复释放 GL 资源），允许移动
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept : id(o.id) { o.id = 0; }
    Shader& operator=(Shader&& o) noexcept {
        if (this != &o) { if (id) glDeleteProgram(id); id = o.id; o.id = 0; }
        return *this;
    }

    // 从源码字符串编译链接
    bool build(const char* vsSrc, const char* fsSrc, const std::string& tag = "") {
        GLuint vs = compile(GL_VERTEX_SHADER, vsSrc, tag + ".vert");
        if (!vs) return false;
        GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc, tag + ".frag");
        if (!fs) { glDeleteShader(vs); return false; }

        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);

        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(std::max(len, 1));
            glGetProgramInfoLog(prog, len, nullptr, log.data());
            std::cerr << "[Shader] 链接失败 (" << tag << "):\n" << log.data() << std::endl;
            glDeleteProgram(prog);
            glDeleteShader(vs);
            glDeleteShader(fs);
            return false;
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (id) glDeleteProgram(id);
        id = prog;
        return true;
    }

    // 从文件加载（相对路径基于工作目录）
    bool loadFromFile(const std::string& vsPath, const std::string& fsPath) {
        std::string vsSrc, fsSrc;
        if (!readFile(vsPath, vsSrc)) { std::cerr << "[Shader] 无法读取 " << vsPath << "\n"; return false; }
        if (!readFile(fsPath, fsSrc)) { std::cerr << "[Shader] 无法读取 " << fsPath << "\n"; return false; }
        return build(vsSrc.c_str(), fsSrc.c_str(), vsPath);
    }

    void use() const { glUseProgram(id); }

    void setBool (const char* n, bool v)               const { glUniform1i(loc(n), v ? 1 : 0); }
    void setInt  (const char* n, int v)                const { glUniform1i(loc(n), v); }
    void setFloat(const char* n, float v)              const { glUniform1f(loc(n), v); }
    void setVec2 (const char* n, const glm::vec2& v)   const { glUniform2fv(loc(n), 1, glm::value_ptr(v)); }
    void setVec3 (const char* n, const glm::vec3& v)   const { glUniform3fv(loc(n), 1, glm::value_ptr(v)); }
    void setVec4 (const char* n, const glm::vec4& v)   const { glUniform4fv(loc(n), 1, glm::value_ptr(v)); }
    void setMat3 (const char* n, const glm::mat3& v)   const { glUniformMatrix3fv(loc(n), 1, GL_FALSE, glm::value_ptr(v)); }
    void setMat4 (const char* n, const glm::mat4& v)   const { glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(v)); }

    bool valid() const { return id != 0; }

private:
    GLint loc(const char* name) const {
        GLint l = glGetUniformLocation(id, name);
        // -1 表示该 uniform 被优化掉或名字写错，静默忽略即可（避免刷屏）
        return l;
    }

    static bool readFile(const std::string& path, std::string& out) {
        std::ifstream f(path, std::ios::in | std::ios::binary);
        if (!f.is_open()) return false;
        std::ostringstream ss;
        ss << f.rdbuf();
        out = ss.str();
        return true;
    }

    static GLuint compile(GLenum type, const char* src, const std::string& tag) {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);

        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
            std::vector<char> log(std::max(len, 1));
            glGetShaderInfoLog(sh, len, nullptr, log.data());
            std::cerr << "[Shader] 编译失败 (" << tag << "):\n" << log.data() << std::endl;
            glDeleteShader(sh);
            return 0;
        }
        return sh;
    }
};

} // namespace fractal
