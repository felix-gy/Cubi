#include "Geometry.h"
#include <glad/gl.h> // Importante: GLAD debe incluirse aquí para las funciones gl*
#include <cmath>
#include <cstddef>

// -----------------------------------------------------------------------------
// IMPLEMENTACIÓN DE MESH
// -----------------------------------------------------------------------------

Mesh::Mesh(std::vector<Vertex> verts, std::vector<unsigned int> inds) 
    : vertices(verts), indices(inds), VAO(0), VBO(0), EBO(0) {
    setupMesh();
}

Mesh::Mesh() : VAO(0), VBO(0), EBO(0) {}

void Mesh::cleanup() {
    if(VAO) glDeleteVertexArrays(1, &VAO);
    if(VBO) glDeleteBuffers(1, &VBO);
    if(EBO) glDeleteBuffers(1, &EBO);
    VAO = 0; VBO = 0; EBO = 0;
}

void Mesh::calculateNormals() {
    // Reiniciar normales
    for(auto& v : vertices) v.normal = Vec3(0,0,0);

    // Calcular normales por cara (flat shading look base)
    for(size_t i = 0; i < indices.size(); i += 3) {
        unsigned int i0 = indices[i];
        unsigned int i1 = indices[i+1];
        unsigned int i2 = indices[i+2];

        Vec3 v0 = vertices[i0].position;
        Vec3 v1 = vertices[i1].position;
        Vec3 v2 = vertices[i2].position;

        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;
        Vec3 normal = cross(edge1, edge2).normalize();

        // Asignar a los vértices (simplificado, para smooth shading se promediaría)
        vertices[i0].normal = normal;
        vertices[i1].normal = normal;
        vertices[i2].normal = normal;
    }
    // Re-enviar datos a la GPU
    setupMesh(); 
}

void Mesh::setupMesh() {
    // Si ya existen buffers, no crear nuevos, solo enlazar
    if(VAO == 0) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.empty() ? nullptr : &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.empty() ? nullptr : &indices[0], GL_STATIC_DRAW);

    // Layout 0: Posición
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    
    // Layout 1: Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    
    // Layout 2: Color
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    
    glBindVertexArray(0);
}

void Mesh::draw() {
    if (VAO == 0) return;
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// -----------------------------------------------------------------------------
// IMPLEMENTACIÓN DE GENERADORES
// -----------------------------------------------------------------------------

Mesh createColorCube() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    Vec3 colors[] = {
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}
    };

    float pos[8][3] = {
        {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f}, { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f}, { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f}
    };

    int faces[6][4] = {
        {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
        {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}
    };

    int idx = 0;
    for(int f=0; f<6; f++) {
        Vec3 color = colors[f];
        for(int v=0; v<4; v++) {
            int pIdx = faces[f][v];
            Vertex vert;
            vert.position = Vec3(pos[pIdx][0], pos[pIdx][1], pos[pIdx][2]);
            vert.color = color;
            vert.normal = Vec3(0,0,0); 
            vertices.push_back(vert);
        }
        indices.push_back(idx); indices.push_back(idx+1); indices.push_back(idx+2);
        indices.push_back(idx+2); indices.push_back(idx+3); indices.push_back(idx);
        idx += 4;
    }

    Mesh mesh(vertices, indices);
    mesh.calculateNormals();
    return mesh;
}

Mesh createSphere(float radius, int sectors, int stacks) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    const float PI = 3.14159265359f;
    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / radius;    // vertex normal
    
    float sectorStep = 2 * PI / sectors;
    float stackStep = PI / stacks;
    float sectorAngle, stackAngle;

    for(int i = 0; i <= stacks; ++i) {
        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = radius * cosf(stackAngle);             // r * cos(u)
        z = radius * sinf(stackAngle);              // r * sin(u)

        for(int j = 0; j <= sectors; ++j) {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // Vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            
            Vertex vert;
            vert.position = Vec3(x, y, z);
            
            // Normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            vert.normal = Vec3(nx, ny, nz);
            
            // Color: White base for sphere
            vert.color = Vec3(1.0f, 1.0f, 1.0f);
            
            vertices.push_back(vert);
        }
    }

    int k1, k2;
    for(int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1);     // beginning of current stack
        k2 = k1 + sectors + 1;      // beginning of next stack

        for(int j = 0; j < sectors; ++j, ++k1, ++k2) {
            // 2 triangles per sector excluding first and last stacks
            if(i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if(i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
    
    return Mesh(vertices, indices);
}