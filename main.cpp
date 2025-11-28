#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <functional>

#include "matLibrary.h"
#include "ShaderUtils.h"
#include "Geometry.h"
#include "Scene.h"
#include "RubiksCube.h" // Incluir la cabecera del Rubik

// -----------------------------------------------------------------------------
// DEFINICIÓN DE ESTRUCTURA LIGHT
// (Necesaria para pasar la luz al shader)
// -----------------------------------------------------------------------------
struct Light {
    Vec3 position;
    Vec3 ambient;
    Vec3 diffuse;
    Vec3 specular;
};

// -----------------------------------------------------------------------------
// HELPER: CUBOS VIAJEROS
// Ahora acepta 'solveTime' para saber cuándo empezar a resolverse
// -----------------------------------------------------------------------------
std::function<void(SceneNode*, float, float)> createTravelerBehavior(float startDelay, float solveTime, Vec3 startPos, Vec3 glowColor) {
    // Variable 'solved' capturada por valor (copia) para controlar el estado dentro de la lambda
    bool solved = false; 
    
    return [startDelay, solveTime, startPos, glowColor, solved](SceneNode* n, float dt, float time) mutable {
        
        // --- LÓGICA DE RESOLUCIÓN AUTOMÁTICA ---
        // Si no se ha resuelto aún y ya pasamos el tiempo designado...
        if (!solved && time >= solveTime) {
            if (n->rubiksCube) {
                // Llamamos al solver (Kociemba)
                // Esto generará la cola de animaciones para resolverlo
                n->rubiksCube->solve();
            }
            solved = true; // Marcamos como resuelto para no llamarlo en cada frame
        }

        // --- CONFIGURACIÓN DE MATERIAL ---
        n->material.emission = glowColor * 0.3f; 
        n->material.diffuse = Vec3(0.2f, 0.2f, 0.2f);
        n->material.ambient = Vec3(0.1f, 0.1f, 0.1f);

        // --- LÓGICA DE MOVIMIENTO (TRAYECTORIA) ---
        float attractionDuration = 12.0f;
        float vortexDuration = 20.0f;
        float vortexEntryRadius = 12.0f; 
        float disappearanceRadius = 5.5f; 

        float t_startAttraction = startDelay;
        float t_startVortex = startDelay + attractionDuration;

        float entryAngle = std::atan2(startPos.z, startPos.x);

        Vec3 transitionPos;
        transitionPos.x = std::cos(entryAngle) * vortexEntryRadius;
        transitionPos.y = 0.0f;
        transitionPos.z = std::sin(entryAngle) * vortexEntryRadius;

        Vec3 currentPos;
        float baseSize = 1.0f; 
        Vec3 currentScale(baseSize, baseSize, baseSize);

        if (time < t_startAttraction) {
            currentPos = startPos;
        } 
        else if (time < t_startVortex) {
            float t = (time - t_startAttraction) / attractionDuration;
            currentPos = lerp(startPos, transitionPos, t);
        } 
        else {
            float spiralTime = time - t_startVortex;
            float t_spiral = spiralTime / vortexDuration;
            if (t_spiral > 1.0f) t_spiral = 1.0f;

            float initialRadius = vortexEntryRadius;
            float currentRadius = lerp(initialRadius, 0.5f, t_spiral);

            float startAngle = entryAngle;
            float orbitSpeed = 2.0f + (t_spiral * 10.0f); 
            float currentAngle = startAngle + (spiralTime * orbitSpeed);

            currentPos.x = std::cos(currentAngle) * currentRadius;
            currentPos.z = std::sin(currentAngle) * currentRadius;
            currentPos.y = 0.0f; 

            if (currentRadius < disappearanceRadius) {
                currentScale = Vec3(0.0f, 0.0f, 0.0f);
            } 
            else {
                float intensity = t_spiral * t_spiral; 
                float stretchY = 1.0f + (intensity * 4.0f); 
                float thinXZ = 1.0f / (1.0f + (intensity * 3.0f));
                
                currentScale.x = baseSize * thinXZ;
                currentScale.y = baseSize * stretchY;
                currentScale.z = baseSize * thinXZ;
            }
        }
        
        Mat4 mat;
        mat = translate(mat, currentPos);
        float localFloat = std::sin(time * 3.0f + startPos.x) * 0.1f; 
        mat = translate(mat, Vec3(0, localFloat, 0));
        float rotSpeed = 2.0f + (time - t_startVortex > 0 ? (time - t_startVortex)*1.5f : 0);
        mat = rotate(mat, time * rotSpeed, Vec3(0.5f, 1.0f, 0.2f)); 
        mat = scale(mat, currentScale);
        n->transform = mat;
    };
}

// -----------------------------------------------------------------------------
// MAIN
// -----------------------------------------------------------------------------
int main() {
    if (!glfwInit()) { return -1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Space Lighting: Rubiks Fleet", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) { return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    unsigned int shaderProgram = createShaderProgram();
    
    Mesh sphereMesh = createSphere(1.0f, 64, 48); 
    
    SceneNode rootNode; 
    
    // --- AGUJERO NEGRO ---
    SceneNode* blackHole = new SceneNode(&sphereMesh);
    blackHole->material.type = 1; 

    blackHole->onUpdate = [](SceneNode* n, float dt, float time) {
        float t_wakeUp = 6.0f;        
        float t_implosionEnd = 6.5f;  
        float t_phase2 = 12.0f;       
        float t_phase3 = 18.0f;
        float t_evaporationStart = 42.0f; 
        float t_death = 45.0f;

        if (time < t_wakeUp) {
            float vibration = std::sin(time * 40.0f) * 0.02f; 
            float scaleBase = 0.5f + vibration;
            n->transform = scale(Mat4(), Vec3(scaleBase, scaleBase, scaleBase));
            n->material.rimPower = 0.8f; 
            n->material.rimColor = Vec3(0.8f, 0.9f, 1.0f); 
        }
        else if (time < t_implosionEnd) {
            float t = (time - t_wakeUp) / (t_implosionEnd - t_wakeUp);
            float scaleVal = lerp(0.5f, 0.05f, t);
            n->transform = scale(Mat4(), Vec3(scaleVal, scaleVal, scaleVal));
            n->material.rimPower = 0.1f; 
            n->material.rimColor = Vec3(1.0f, 1.0f, 1.0f);
        }
        else if (time < t_phase2) {
            float t = (time - t_implosionEnd) / (t_phase2 - t_implosionEnd);
            float easeOut = 1.0f - (1.0f - t) * (1.0f - t);
            float currentScale = lerp(0.1f, 3.5f, easeOut); 
            n->transform = scale(Mat4(), Vec3(currentScale, currentScale, currentScale));
            n->material.rimPower = lerp(1.0f, 3.0f, t); 
            n->material.rimColor = lerp(Vec3(1.0f, 1.0f, 1.0f), Vec3(0.4f, 0.1f, 0.9f), t);
        }
        else if (time < t_phase3) {
            float t = (time - t_phase2) / (t_phase3 - t_phase2);
            float currentScale = lerp(3.5f, 5.0f, t);
            float pulse = std::sin(time * 1.5f) * 0.1f; 
            currentScale += pulse;
            n->transform = scale(Mat4(), Vec3(currentScale, currentScale, currentScale));
            n->material.rimPower = 3.5f;
            n->material.rimColor = Vec3(0.3f, 0.05f, 0.8f);
        }
        else if (time < t_evaporationStart) {
            float baseScale = 6.0f;
            float pulse = std::sin(time * 0.8f) * 0.2f; 
            float finalScale = baseScale + pulse;
            n->transform = scale(Mat4(), Vec3(finalScale, finalScale, finalScale));
            n->material.rimPower = 4.0f + (std::sin(time * 8.0f)*0.5f);
            n->material.rimColor = Vec3(0.2f, 0.0f, 1.0f);
        }
        else if (time < t_death) {
            float t = (time - t_evaporationStart) / (t_death - t_evaporationStart);
            float currentScale = lerp(6.0f, 0.0f, t);
            float deathVibration = std::sin(time * 100.0f) * (0.5f * t); 
            currentScale += deathVibration;
            n->transform = scale(Mat4(), Vec3(currentScale, currentScale, currentScale));
            n->material.rimPower = lerp(4.0f, 0.1f, t); 
            n->material.rimColor = lerp(Vec3(0.2f, 0.0f, 1.0f), Vec3(5.0f, 5.0f, 10.0f), t); 
        } 
        else {
            n->transform = scale(Mat4(), Vec3(0,0,0));
        }
    };
    rootNode.addChild(blackHole);

    // --- RUBIKS CUBES ---
    
    // Helper actualizado con 'solveTime'
    auto createRubiksNode = [&](float moveDelay, float solveTime, Vec3 pos, Vec3 color) {
        SceneNode* node = new SceneNode(nullptr); 
        node->rubiksCube = new RubiksCube();
        node->rubiksCube->scramble(25, true); 
        // Pasamos moveDelay para la trayectoria y solveTime para el inicio de la resolución
        node->onUpdate = createTravelerBehavior(moveDelay, solveTime, pos, color);
        rootNode.addChild(node);
    };

    // [GRUPO 1] 
    createRubiksNode(8.0f, 6.0f, Vec3(20.0f, 5.0f, 0.0f), Vec3(0.0f, 1.0f, 1.0f));

    // [GRUPO 2] - empieza 6s
    createRubiksNode(11.0f, 9.0f, Vec3(-26.0f, -5.0f, 8.0f), Vec3(1.0f, 0.0f, 0.0f));
    createRubiksNode(11.0f, 9.0f, Vec3(26.0f, 5.0f, -8.0f), Vec3(1.0f, 0.2f, 0.0f));

    // [GRUPO 3] - empieza 9s
    createRubiksNode(14.0f, 12.0f, Vec3(0.0f, 15.0f, 32.0f), Vec3(0.0f, 1.0f, 0.2f));
    createRubiksNode(14.0f, 12.0f, Vec3(0.0f, -15.0f, -32.0f), Vec3(0.5f, 1.0f, 0.0f));
    createRubiksNode(14.0f, 12.0f, Vec3(32.0f, 0.0f, 15.0f), Vec3(0.2f, 0.8f, 0.2f));
    createRubiksNode(14.0f, 12.0f, Vec3(-32.0f, 0.0f, -15.0f), Vec3(0.8f, 1.0f, 0.0f));
    createRubiksNode(14.0f, 12.0f, Vec3(20.0f, 20.0f, 20.0f), Vec3(0.4f, 1.0f, 0.4f));
    
    // [GRUPO 4] - empieza 12s
    
    createRubiksNode(17.0f, 15.0f, Vec3(38.0f, 10.0f, 38.0f), Vec3(1.0f, 0.0f, 1.0f));
    createRubiksNode(17.0f, 15.0f, Vec3(-38.0f, -10.0f, -38.0f), Vec3(0.6f, 0.0f, 1.0f));


    // CONFIGURACIÓN DE LUZ GLOBAL (Espacio)
    Light light;
    light.position = Vec3(20.0f, 20.0f, 20.0f); 
    light.ambient = Vec3(0.05f, 0.05f, 0.08f); 
    light.diffuse = Vec3(0.8f, 0.8f, 1.0f);
    light.specular = Vec3(1.0f, 1.0f, 1.0f);

    float startTime = (float)glfwGetTime();
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        float totalTime = currentFrame - startTime;

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        rootNode.update(deltaTime, totalTime);

        Mat4 view = lookAt(Vec3(40, 30, 40), Vec3(0, 0, 0), Vec3(0, 1, 0));
        Mat4 projection = perspective(toRadians(45.0f), 1024.0f / 768.0f, 0.1f, 300.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view.value_ptr());
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection.value_ptr());
        glUniform3f(glGetUniformLocation(shaderProgram, "viewPos"), 40, 30, 40);

        glUniform3f(glGetUniformLocation(shaderProgram, "light.position"), light.position.x, light.position.y, light.position.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "light.ambient"), light.ambient.x, light.ambient.y, light.ambient.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "light.diffuse"), light.diffuse.x, light.diffuse.y, light.diffuse.z);
        glUniform3f(glGetUniformLocation(shaderProgram, "light.specular"), light.specular.x, light.specular.y, light.specular.z);

        rootNode.draw(shaderProgram, Mat4(), view, projection);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    sphereMesh.cleanup();
    delete blackHole;
    
    glfwTerminate();
    return 0;
}