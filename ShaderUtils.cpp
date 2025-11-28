#include "ShaderUtils.h"
#include <glad/gl.h>
#include <iostream>

// -----------------------------------------------------------------------------
// CÓDIGO FUENTE DE LOS SHADERS (Oculto dentro del .cpp)
// -----------------------------------------------------------------------------

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

out vec3 FragPos;
out vec3 Normal;
out vec3 ObjectColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal; 
    ObjectColor = aColor;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 ObjectColor;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    
    int type;        // 0 = Phong, 1 = BlackHole
    vec3 rimColor;   
    float rimPower;
    vec3 emission;   
};

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    if (material.type == 0) {
        // --- STANDARD PHONG + EMISSION ---
        vec3 ambient = light.ambient * material.ambient * ObjectColor;
        vec3 lightDir = normalize(light.position - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = light.diffuse * (diff * material.diffuse * ObjectColor);
        
        vec3 reflectDir = reflect(-lightDir, norm);  
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specular = light.specular * (spec * material.specular);  
        
        vec3 emission = material.emission;
        vec3 result = ambient + diffuse + specular + emission;
        FragColor = vec4(result, 1.0);
    } 
    else if (material.type == 1) {
        // --- BLACK HOLE / RIM LIGHTING ---
        vec3 bodyColor = vec3(0.0, 0.0, 0.0);
        float rimFactor = 1.0 - max(dot(viewDir, norm), 0.0);
        rimFactor = pow(rimFactor, material.rimPower);
        vec3 rimEmission = material.rimColor * rimFactor;
        FragColor = vec4(bodyColor + rimEmission, 1.0);
    }
}
)";

// -----------------------------------------------------------------------------
// FUNCIONES INTERNAS (Helpers)
// -----------------------------------------------------------------------------

// Función estática auxiliar (solo visible en este archivo)
static unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    
    int success;
    char infoLog[512];
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(id, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return id;
}

// -----------------------------------------------------------------------------
// IMPLEMENTACIÓN DE LA FUNCIÓN PÚBLICA
// -----------------------------------------------------------------------------

unsigned int createShaderProgram() {
    unsigned int vertex = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return program;
}