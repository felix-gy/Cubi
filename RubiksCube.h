#ifndef RUBIKSCUBE_H
#define RUBIKSCUBE_H

#include <vector>
#include <string>
#include <deque>
#include <map>
#include <array>
#include <algorithm>
#include <iostream>

#include "matLibrary.h"

// Solver Kociemba (C Linkage)
extern "C" {
    #include "kociemba/ckociemba/include/search.h"
}

enum class Color { WHITE, YELLOW, RED, ORANGE, GREEN, BLUE, BLACK };
enum class Face { UP, DOWN, LEFT, RIGHT, FRONT, BACK };
enum class Axis { X, Y, Z };

class Cubie {
public:
    Mat4 modelMatrix; 
    std::map<Face, Color> m_faces;

    Cubie();
    void init(int x, int y, int z, float spacing);
    void setFaceColor(Face face, Color color);
    Color getFaceColor(Face face) const;
    
    // Rotaciones lógicas (actualizan qué color apunta a qué dirección del mundo)
    // Esencial para mantener el estado lógico sincronizado con el visual
    void rotateFacesYClockwise();
    void rotateFacesYCounterClockwise();
    void rotateFacesXClockwise();
    void rotateFacesXCounterClockwise();
    void rotateFacesZClockwise();
    void rotateFacesZCounterClockwise();
};

class RotationGroup {
public:
    bool     m_isAnimating = false;
    float    m_currentAngle = 0.0f;
    float    m_targetAngle = 0.0f;
    float    m_animationSpeed = 5.0f;
    
    std::vector<Cubie*> m_cubiesToAnimate; 
    Axis     m_axis;
    int      m_slice;
    bool     m_clockwise;

    void start(std::vector<Cubie*>& cubies, Axis axis, int slice, bool clockwise);
    bool update(float deltaTime);
    void applyAnimationToCubies(float angle_step);void debugPrintFacelets();

};

class RubiksCube {
public:
    RubiksCube();
    ~RubiksCube();

    void setupMesh();
    void update(float deltaTime);
    void draw(const Mat4& globalParent, const Mat4& view, const Mat4& projection, Vec3 emissionColor);

    // Control
    void scramble(int moves = 25, bool instant = false);
    void solve();
    void startMove(const std::string& move);
	void executeSequence(const std::string& sequence, bool instant);
	void startAnimationSequence(const std::string& sequence);
	void debugPrintFacelets();
private:
    std::array<Cubie, 27> m_cubies;
    RotationGroup m_animator;
    std::deque<std::string> m_animationQueue;
    unsigned int m_VAO, m_VBO, m_EBO;
    unsigned int m_shaderProgram; 
    const float m_spacing = 1.0f;

    // Helpers internos
    int getIndex(int x, int y, int z) const;
    Cubie& getCubieAt(int x, int y, int z);
    Vec3 getVec3FromColor(Color color);
    std::string getFaceletString();

    void processQueue();
    void applyMove(const std::string& move, bool instant);
    void startRotation(Axis axis, int slice, bool clockwise);
    void performRotation(Axis axis, int slice, bool clockwise);
    
    // Permutaciones de array (Lógica pura)
    // Renombradas para claridad (Slice en lugar de Layer cuando aplica)
    void rotateUpSlice(bool g_counterClockwise);
    void rotateMiddleHorizontalSlice(bool g_counterClockwise);
    void rotateDownSlice(bool g_counterClockwise);
    void rotateRightSlice(bool g_counterClockwise);
    void rotateMiddleVerticalSlice(bool g_counterClockwise);
    void rotateLeftSlice(bool g_counterClockwise);
    void rotateFrontSlice(bool g_counterClockwise);
    void rotateMiddleDepthSlice(bool g_counterClockwise);
    void rotateBackSlice(bool g_counterClockwise);
};

#endif