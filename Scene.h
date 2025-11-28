#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <functional>
#include "matLibrary.h"
#include "Geometry.h"

// Forward declaration para evitar dependencias circulares
class RubiksCube;

// Estructura para propiedades del material
struct Material {
    Vec3 ambient;
    Vec3 diffuse;
    Vec3 specular;
    float shininess;
    
    int type;        // 0 = Phong, 1 = BlackHole
    Vec3 rimColor;   // Para efecto borde (Black Hole)
    float rimPower;
    Vec3 emission;   // Color de emisión (Glow)

    Material();
};

class SceneNode {
public:
    Mesh* mesh;                // Para objetos normales (Agujero negro)
    RubiksCube* rubiksCube;    // NUEVO: Para cubos de Rubik
    
    Mat4 transform;
    Material material;
    SceneNode* parent;
    std::vector<SceneNode*> children;

    // Callback para animación
    std::function<void(SceneNode*, float, float)> onUpdate;

    SceneNode(Mesh* m = nullptr);

    void addChild(SceneNode* child);
    void update(float dt, float totalTime);
    
    // ACTUALIZADO: Ahora recibe view y projection para pasarlos al shader del Rubik
    void draw(unsigned int shaderProgram, Mat4 parentTransform, const Mat4& view, const Mat4& projection);
};

#endif