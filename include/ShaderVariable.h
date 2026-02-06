#ifndef SHADER_VARIABLE_H
#define SHADER_VARIABLE_H

#include <glad/glad.h>
#include <linmath.h>
#include <iostream>
#include <string>
#include <utility>

// Template class to store a generic variable and its corresponding shader location
template <typename T>
class ShaderVariable {
public:
    GLint location;      // Location of the variable in the shader
    T* value;            // Pointer to the actual value
    std::string name;    // Name of the uniform

    ShaderVariable() : location(-1), value(nullptr), name("UN-INITIALIZED") {}

    // Constructor
    ShaderVariable(const GLuint shaderProgram, T* val, std::string uniformName)
        : location(glGetUniformLocation(shaderProgram, uniformName.c_str())), value(val), name(std::move(uniformName)) {
        if (location == -1) {
            std::cerr << "ShaderVariable Initialization: Invalid location for shader variable: " << name << std::endl;
        }
    }

    // Disable copying to avoid raw array issues
    ShaderVariable(const ShaderVariable&) = delete;
    ShaderVariable& operator=(const ShaderVariable&) = delete;

    // Allow moving
    ShaderVariable(ShaderVariable&& other) noexcept
        : location(other.location), value(other.value), name(std::move(other.name)) {
        other.value = nullptr; // Nullify the source pointer
    }

    ShaderVariable& operator=(ShaderVariable&& other) noexcept {
        if (this != &other) {
            location = other.location;
            value = other.value;
            name = std::move(other.name);
            other.value = nullptr; // Nullify the source pointer
        }
        return *this;
    }

    // Upload the value to the shader
    void uploadToShader(bool muteError = false) const {
        if (location == -1) {
            if (!muteError) {
                std::cerr << "ShaderVariable: Invalid location for shader variable: " << name << std::endl;
            }
            return;
        }
        upload();
    }


private:
    void upload() const {
        static_assert(sizeof(T) == 0, "ShaderVariable: No upload logic defined for this type.");
    }
};


// Template specializations for supported types (header-only so they link on all platforms/toolchains)
template <>
inline void ShaderVariable<vec2>::upload() const {
    glUniform2f(location, (*value)[0], (*value)[1]);
}

template <>
inline void ShaderVariable<vec3>::upload() const {
    glUniform3f(location, (*value)[0], (*value)[1], (*value)[2]);
}

template <>
inline void ShaderVariable<float>::upload() const {
    glUniform1f(location, *value);
}

template <>
inline void ShaderVariable<int>::upload() const {
    glUniform1i(location, *value);
}

#endif // SHADER_VARIABLE_H
