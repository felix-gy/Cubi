#ifndef BLACKHOLE_H
#define BLACKHOLE_H

#include "Shader.h"

// Vertex Shader para el fondo (simple Quad)
const char* bhVertexSource = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos.x, aPos.y, 1.0, 1.0); // Z=1.0 para estar al fondo
}
)glsl";

// Fragment Shader del Agujero Negro
const char* bhFragmentSource = R"glsl(
#version 330 core
out vec4 FragColor;

uniform vec2 u_resolution;
uniform float u_time;

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

float noise(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a)* u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution.xy) / u_resolution.y;
    float r = length(uv);
    float angle = atan(uv.y, uv.x);

    float blackHoleRadius = 0.22; 
    vec3 spaceColor = vec3(0.0, 0.02, 0.15); // Azul espacio profundo
    
    // Estrellas y ruido
    float bgNoise = noise(uv * 3.0 + u_time * 0.05);
    vec3 bg = spaceColor + vec3(0.0, 0.05, 0.1) * bgNoise;
    float stars = pow(random(uv * 10.0), 200.0) * 0.8; 
    bg += vec3(stars);

    // Disco de acreción
    float rotationSpeed = 1.5;
    float spiralAngle = angle + u_time * rotationSpeed;
    float distFromHole = r - blackHoleRadius;

    float ringNoise = noise(vec2(spiralAngle * 4.0, r * 10.0 - u_time * 3.0));
    float ringStructure = sin(spiralAngle * 10.0 + ringNoise * 5.0);
    
    float glow = 0.012 / max(0.0, distFromHole);
    glow *= smoothstep(0.8, 0.0, distFromHole);
    glow *= (0.8 + 0.4 * ringStructure);

    vec3 diskColorInner = vec3(0.8, 0.9, 1.0);
    vec3 diskColorOuter = vec3(0.1, 0.4, 0.9);
    vec3 finalRingColor = mix(diskColorOuter, diskColorInner, 2.0 / (glow + 1.0));
    finalRingColor *= glow;

    float doppler = 1.0 + 0.5 * sin(angle + 1.5);
    finalRingColor *= doppler;

    vec3 finalColor = bg + finalRingColor;

    // Horizonte de sucesos
    float holeCutout = smoothstep(blackHoleRadius, blackHoleRadius - 0.01, r);
    finalColor = mix(finalColor, vec3(0.0), holeCutout);
    
    // Anillo fotones
    float photonRing = smoothstep(0.005, 0.0, abs(r - blackHoleRadius)) * 1.5;
    finalColor += vec3(1.0) * photonRing * (1.0 - holeCutout);

    FragColor = vec4(finalColor, 1.0);
}
)glsl";

class BlackHole {
public:
    Shader* shader;
    unsigned int VAO, VBO;

    BlackHole() {
        shader = new Shader(bhVertexSource, bhFragmentSource);
        setupMesh();
    }

    ~BlackHole() {
        delete shader;
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void setupMesh() {
        float vertices[] = {
            -1.0f,  1.0f, 
            -1.0f, -1.0f, 
             1.0f, -1.0f, 

            -1.0f,  1.0f, 
             1.0f, -1.0f, 
             1.0f,  1.0f  
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    void draw(float time, float width, float height) {
        shader->use();
        shader->setFloat("u_time", time);
        shader->setVec2("u_resolution", width, height);
        
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
};

#endif