#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>
#include "matLibrary.h"

// -----------------------------------------------------------------------------
// ESTRUCTURA DE VÉRTICE
// -----------------------------------------------------------------------------
struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec3 color;
};

// -----------------------------------------------------------------------------
// CLASE MESH
// -----------------------------------------------------------------------------
class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    unsigned int VAO, VBO, EBO;

    // Constructores
    Mesh(std::vector<Vertex> verts, std::vector<unsigned int> inds);
    Mesh();
    
    // Gestión de recursos
    void cleanup();
    
    // Utilidades
    void calculateNormals();
    
    // Renderizado
    void draw();

private:
    void setupMesh();
};

// -----------------------------------------------------------------------------
// PROTOTIPOS DE GENERADORES
// -----------------------------------------------------------------------------
Mesh createColorCube();
Mesh createSphere(float radius, int sectors, int stacks);

#endif