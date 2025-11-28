#include "Scene.h"
#include "Geometry.h" 
#include "RubiksCube.h" // Incluimos la definición completa aquí
#include <glad/gl.h>  
#include <cstddef>    

// -----------------------------------------------------------------------------
// IMPLEMENTACIÓN DE MATERIAL
// -----------------------------------------------------------------------------
Material::Material() {
    ambient = Vec3(0.1f, 0.1f, 0.1f);
    diffuse = Vec3(0.8f, 0.8f, 0.8f);
    specular = Vec3(0.5f, 0.5f, 0.5f);
    shininess = 32.0f;
    type = 0; 
    rimColor = Vec3(0,0,0);
    rimPower = 2.0f;
    emission = Vec3(0,0,0);
}

// -----------------------------------------------------------------------------
// IMPLEMENTACIÓN DE SCENENODE
// -----------------------------------------------------------------------------
SceneNode::SceneNode(Mesh* m) : mesh(m), rubiksCube(nullptr), parent(nullptr) {}

void SceneNode::addChild(SceneNode* child) {
    child->parent = this;
    children.push_back(child);
}

void SceneNode::update(float dt, float totalTime) {
    // 1. Ejecutar animación de trayectoria (el comportamiento del viajero)
    if(onUpdate) {
        onUpdate(this, dt, totalTime);
    }

    // 2. Si es un Cubo de Rubik, actualizamos sus animaciones internas (rotación de caras)
    if (rubiksCube) {
        rubiksCube->update(dt);
    }

    // 3. Actualizar hijos
    for(auto child : children) {
        child->update(dt, totalTime);
    }
}

void SceneNode::draw(unsigned int shaderProgram, Mat4 parentTransform, const Mat4& view, const Mat4& projection) {
    Mat4 globalTransform = parentTransform * transform;

    if (rubiksCube) {
        // --- CASO 1: DIBUJAR CUBO DE RUBIK ---
        // El Rubik tiene su propio shader, así que llamamos a su método draw.
        // Le pasamos 'material.emission' para que brille con el color asignado en el main.
        rubiksCube->draw(globalTransform, view, projection, material.emission);
        
        // IMPORTANTE: El Rubik cambió el shader activo. Debemos restaurar el shader principal
        // para que los siguientes nodos (como el agujero negro u otros hijos) se dibujen bien.
        glUseProgram(shaderProgram);
    } 
    else if (mesh) {
        // --- CASO 2: DIBUJAR MESH STANDARD (Agujero Negro, etc.) ---
        // Aseguramos que el shader principal esté activo
        glUseProgram(shaderProgram);

        unsigned int modelLoc = glGetUniformLocation(shaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, globalTransform.value_ptr());

        // Enviar Standard Uniforms
        glUniform3f(glGetUniformLocation(shaderProgram, "material.ambient"), material.ambient.x, material.ambient.y, material.ambient.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "material.diffuse"), material.diffuse.x, material.diffuse.y, material.diffuse.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "material.specular"), material.specular.x, material.specular.y, material.specular.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "material.shininess"), material.shininess);

        // Enviar Nuevos Uniforms (Tipo, Rim, Emisión)
        glUniform1i(glGetUniformLocation(shaderProgram, "material.type"), material.type);
        glUniform3f(glGetUniformLocation(shaderProgram, "material.rimColor"), material.rimColor.x, material.rimColor.y, material.rimColor.z);
        glUniform1f(glGetUniformLocation(shaderProgram, "material.rimPower"), material.rimPower);
        glUniform3f(glGetUniformLocation(shaderProgram, "material.emission"), material.emission.x, material.emission.y, material.emission.z);

        mesh->draw();
    }

    for(auto child : children) {
        child->draw(shaderProgram, globalTransform, view, projection);
    }
}