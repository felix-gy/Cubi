#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include "RubiksCube.h"
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio> 
#include <vector>
#include <cmath>
#include <iostream> 
#include <map> // Aseguramos incluir map

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Helper Functions ---
static unsigned int compileShaderInternal(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, NULL);
    glCompileShader(id);
    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(id, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return id;
}

static void stabilizeMatrix(Mat4& m, float spacing) {
    if (spacing > 0.001f) {
        m.m[3][0] = std::round(m.m[3][0] / spacing) * spacing;
        m.m[3][1] = std::round(m.m[3][1] / spacing) * spacing;
        m.m[3][2] = std::round(m.m[3][2] / spacing) * spacing;
    }
    for(int col=0; col<3; col++) {
        for(int row=0; row<3; row++) {
            float val = m.m[col][row];
            if(val > 0.5f) m.m[col][row] = 1.0f;
            else if(val < -0.5f) m.m[col][row] = -1.0f;
            else m.m[col][row] = 0.0f;
        }
    }
}

// --- SHADERS ---
const char* rbVertex = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 anormal;
    layout (location = 2) in int aFaceID;
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    out vec3 v_FragPos;
    out vec3 v_LocalPos; 
    flat out vec3 v_Normal;
    flat out int v_FaceID;
    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        v_FragPos = vec3(model * vec4(aPos, 1.0));
        v_LocalPos = aPos; 
        v_Normal = normalize(mat3(model) * anormal);
        v_FaceID = aFaceID;
    }
)glsl";

const char* rbFragment = R"glsl(
    #version 330 core
    out vec4 FragColor;
    flat in int v_FaceID;
    flat in vec3 v_Normal;
    in vec3 v_FragPos;
    in vec3 v_LocalPos;
    uniform vec3 u_faceColors[6]; 
    uniform vec3 u_emission; 
    void main() {
        vec3 absPos = abs(v_LocalPos);
        vec3 isBorderVec = step(vec3(0.42), absPos); 
        float borderMask = dot(isBorderVec, vec3(1.0));
        vec3 finalObjectColor;
        if (borderMask > 1.9) finalObjectColor = vec3(0.1); 
        else finalObjectColor = u_faceColors[v_FaceID];
        vec3 norm = normalize(v_Normal);
        vec3 lightDir = normalize(vec3(10, 20, 20) - v_FragPos);
        float diff = max(dot(norm, lightDir), 0.2);
        vec3 result = diff * finalObjectColor + u_emission;
        FragColor = vec4(result, 1.0);
    }
)glsl";

// --- CUBIE ---
Cubie::Cubie() {
    m_faces[Face::UP] = Color::WHITE;
    m_faces[Face::DOWN] = Color::YELLOW;
    m_faces[Face::LEFT] = Color::GREEN;
    m_faces[Face::RIGHT] = Color::BLUE;
    m_faces[Face::FRONT] = Color::RED;
    m_faces[Face::BACK] = Color::ORANGE;
}

void Cubie::init(int x, int y, int z, float spacing) {
    float px = (x - 1.0f) * spacing;
    float py = (y - 1.0f) * spacing;
    float pz = (z - 1.0f) * spacing;
    Mat4 id;
    this->modelMatrix = translate(id, Vec3(px, py, pz));
}

void Cubie::setFaceColor(Face face, Color color) { m_faces[face] = color; }
Color Cubie::getFaceColor(Face face) const { 
    if(m_faces.find(face) != m_faces.end()) return m_faces.at(face);
    return Color::BLACK;
}

// Funciones vacías (Legacy)
void Cubie::rotateFacesYClockwise() { }
void Cubie::rotateFacesYCounterClockwise() { }
void Cubie::rotateFacesXClockwise() { }
void Cubie::rotateFacesXCounterClockwise() { }
void Cubie::rotateFacesZClockwise() { }
void Cubie::rotateFacesZCounterClockwise() { }

// --- ROTATION GROUP ---
void RotationGroup::start(std::vector<Cubie*>& cubies, Axis axis, int slice, bool clockwise) {
    if (m_isAnimating) return;
    m_isAnimating = true; 
    m_currentAngle = 0.0f;
    m_targetAngle = clockwise ? (-M_PI / 2.0f) : (M_PI / 2.0f);
    m_cubiesToAnimate = cubies; 
    m_axis = axis; 
    m_slice = slice; 
    m_clockwise = clockwise;
}

bool RotationGroup::update(float deltaTime) {
    if (!m_isAnimating) return false;
    float direction = (m_targetAngle > 0) ? 1.0f : -1.0f;
    float step = direction * m_animationSpeed * deltaTime;
    bool finished = false;
    
    if ((m_targetAngle > 0 && m_currentAngle + step >= m_targetAngle) || 
        (m_targetAngle < 0 && m_currentAngle + step <= m_targetAngle)) {
        step = m_targetAngle - m_currentAngle; 
        m_currentAngle = m_targetAngle; 
        finished = true;
    } else {
        m_currentAngle += step;
    }
    
    applyAnimationToCubies(step);
    
    if (finished) {
        m_isAnimating = false;
        m_cubiesToAnimate.clear(); 
    }
    return finished;
}

void RotationGroup::applyAnimationToCubies(float angle_step) {
    Vec3 axisVec;
    if (m_axis == Axis::X) axisVec = Vec3(1,0,0);
    else if (m_axis == Axis::Y) axisVec = Vec3(0,1,0);
    else axisVec = Vec3(0,0,1);
    Mat4 rot = rotate(Mat4(), angle_step, axisVec);
    for (Cubie* c : m_cubiesToAnimate) {
        c->modelMatrix = rot * c->modelMatrix; 
    }
}

// --- RUBIKS CUBE ---
RubiksCube::RubiksCube() {
    unsigned int v = compileShaderInternal(GL_VERTEX_SHADER, rbVertex);
    unsigned int f = compileShaderInternal(GL_FRAGMENT_SHADER, rbFragment);
    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, v); 
    glAttachShader(m_shaderProgram, f); 
    glLinkProgram(m_shaderProgram);

    for (int z = 0; z <= 2; z++) {
        for (int y = 0; y <= 2; y++) {
            for (int x = 0; x <= 2; x++) {
                int i = getIndex(x, y, z); 
                m_cubies[i].init(x, y, z, m_spacing);
                // Colores ESTÁTICOS
                m_cubies[i].setFaceColor(Face::FRONT, Color::RED);
                m_cubies[i].setFaceColor(Face::BACK, Color::ORANGE);
                m_cubies[i].setFaceColor(Face::UP, Color::WHITE);
                m_cubies[i].setFaceColor(Face::DOWN, Color::YELLOW);
                m_cubies[i].setFaceColor(Face::LEFT, Color::GREEN);
                m_cubies[i].setFaceColor(Face::RIGHT, Color::BLUE);
            }
        }
    }
    setupMesh();
}

RubiksCube::~RubiksCube() {
    glDeleteVertexArrays(1, &m_VAO); 
    glDeleteBuffers(1, &m_VBO); 
    glDeleteBuffers(1, &m_EBO);
    glDeleteProgram(m_shaderProgram);
}

int RubiksCube::getIndex(int x, int y, int z) const { return x + y * 3 + z * 9; }
Cubie& RubiksCube::getCubieAt(int x, int y, int z) { return m_cubies[getIndex(x, y, z)]; }

void RubiksCube::setupMesh() {
    float s = 0.5f;
    struct Vertex { Vec3 pos; Vec3 normal; int faceID; };
    std::vector<Vertex> vertices = {
        {{s,s,s},{1,0,0},0}, {{s,-s,s},{1,0,0},0}, {{s,-s,-s},{1,0,0},0}, {{s,-s,-s},{1,0,0},0}, {{s,s,-s},{1,0,0},0}, {{s,s,s},{1,0,0},0},
        {{-s,s,s},{-1,0,0},1}, {{-s,-s,-s},{-1,0,0},1}, {{-s,-s,s},{-1,0,0},1}, {{-s,-s,-s},{-1,0,0},1}, {{-s,s,s},{-1,0,0},1}, {{-s,s,-s},{-1,0,0},1},
        {{-s,s,-s},{0,1,0},2}, {{s,s,s},{0,1,0},2}, {{s,s,-s},{0,1,0},2}, {{s,s,s},{0,1,0},2}, {{-s,s,-s},{0,1,0},2}, {{-s,s,s},{0,1,0},2},
        {{-s,-s,-s},{0,-1,0},3}, {{s,-s,-s},{0,-1,0},3}, {{s,-s,s},{0,-1,0},3}, {{s,-s,s},{0,-1,0},3}, {{-s,-s,s},{0,-1,0},3}, {{-s,-s,-s},{0,-1,0},3},
        {{-s,-s,s},{0,0,1},4}, {{s,-s,s},{0,0,1},4}, {{s,s,s},{0,0,1},4}, {{s,s,s},{0,0,1},4}, {{-s,s,s},{0,0,1},4}, {{-s,-s,s},{0,0,1},4},
        {{-s,-s,-s},{0,0,-1},5}, {{-s,s,-s},{0,0,-1},5}, {{s,s,-s},{0,0,-1},5}, {{s,s,-s},{0,0,-1},5}, {{s,-s,-s},{0,0,-1},5}, {{-s,-s,-s},{0,0,-1},5}
    };
    unsigned int indices[36]; 
    for(int i=0;i<36;i++) indices[i]=i;
    glGenVertexArrays(1, &m_VAO); glGenBuffers(1, &m_VBO); glGenBuffers(1, &m_EBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO); glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO); glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(Vec3))); glEnableVertexAttribArray(1);
    glVertexAttribIPointer(2, 1, GL_INT, sizeof(Vertex), (void*)(2*sizeof(Vec3))); glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void RubiksCube::startMove(const std::string& move) {
    if(move.empty()) return;
    m_animationQueue.push_back(move);
}

void RubiksCube::scramble(int moves, bool instant) {
    if (!instant && (m_animator.m_isAnimating || !m_animationQueue.empty())) return;
    const std::vector<std::string> valid = { "U", "L", "R", "F", "B", "D" };
    for(int i=0; i<moves; ++i) {
        std::string mv = valid[rand() % valid.size()];
        if(instant) applyMove(mv, true);
        else m_animationQueue.push_back(mv);
    }
}

void RubiksCube::executeSequence(const std::string& sequence, bool instant) {
    std::stringstream ss(sequence);
    std::string move;
    while (ss >> move) {
        if (move.size() >= 2 && move[1] == '2') {
            std::string baseMove(1, move[0]);
            if (instant) { applyMove(baseMove, true); applyMove(baseMove, true); }
            else { m_animationQueue.push_back(move); }
        } else {
            if (instant) applyMove(move, true);
            else m_animationQueue.push_back(move);
        }
    }
}

void RubiksCube::startAnimationSequence(const std::string& sequence) {
    std::stringstream ss(sequence);
    std::string move;
    while (ss >> move) if (!move.empty()) m_animationQueue.push_back(move);
}

void RubiksCube::solve() {
    if (m_animator.m_isAnimating || !m_animationQueue.empty()) return;
    debugPrintFacelets();
    std::string s = getFaceletString();
    std::cout << "Kociemba Input: " << s << std::endl;
    char* sol = solution(const_cast<char*>(s.c_str()), 24, 20000, 0, "kociemba/cprunetables");
    if (sol && strncmp(sol, "Error", 5) != 0) {
        std::stringstream ss(sol); std::string m;
        while (ss >> m) m_animationQueue.push_back(m);
        std::cout << "Solution found: " << sol << std::endl;
    } else {
        std::cout << "No solution found or error." << std::endl;
    }
    if (sol) free(sol);
}

void RubiksCube::update(float dt) {
    if (m_animator.update(dt)) {
        for(auto& cubie : m_cubies) stabilizeMatrix(cubie.modelMatrix, m_spacing); 
        performRotation(m_animator.m_axis, m_animator.m_slice, m_animator.m_clockwise);
    }
    processQueue();
}

void RubiksCube::processQueue() {
    if (m_animator.m_isAnimating || m_animationQueue.empty()) return;
    std::string mov = m_animationQueue.front(); 
    m_animationQueue.pop_front();
    if (mov.size() >= 2 && mov[1] == '2') {
        std::string m1(1, mov[0]); 
        m_animationQueue.push_front(m1); 
        applyMove(m1, false);
    } else {
        applyMove(mov, false);
    }
}

void RubiksCube::applyMove(const std::string& move, bool instant) {
    char m = move[0]; 
    bool prime = (move.size() > 1 && move[1] == '\''); 
    bool cw = !prime; 
    Axis ax; int sl; bool geoCw;

    switch(m) {
        case 'U': ax = Axis::Y; sl = 2; geoCw = cw; break;
        case 'D': ax = Axis::Y; sl = 0; geoCw = !cw; break; 
        case 'R': ax = Axis::X; sl = 2; geoCw = cw; break;
        case 'L': ax = Axis::X; sl = 0; geoCw = !cw; break; 
        case 'F': ax = Axis::Z; sl = 2; geoCw = cw; break;
        case 'B': ax = Axis::Z; sl = 0; geoCw = !cw; break; 
        default: return;
    }
    
    if(instant) {
        float angle = geoCw ? (-M_PI/2.0f) : (M_PI/2.0f);
        Vec3 axisVec;
        if(ax == Axis::X) axisVec = Vec3(1,0,0);
        else if(ax == Axis::Y) axisVec = Vec3(0,1,0);
        else axisVec = Vec3(0,0,1);
        Mat4 R = rotate(Mat4(), angle, axisVec);
        for (int i = 0; i < 27; ++i) {
            int x = (i % 3); int y = (i / 3) % 3; int z = (i / 9);
            if ((ax == Axis::X && x == sl) || (ax == Axis::Y && y == sl) || (ax == Axis::Z && z == sl)) {
                m_cubies[i].modelMatrix = R * m_cubies[i].modelMatrix; 
                stabilizeMatrix(m_cubies[i].modelMatrix, m_spacing); 
            }
        }
        performRotation(ax, sl, geoCw);
    } else {
        startRotation(ax, sl, geoCw);
    }
}

void RubiksCube::startRotation(Axis axis, int slice, bool clockwise) {
    std::vector<Cubie*> group;
    for (int i = 0; i < 27; ++i) {
        int x = (i % 3); int y = (i / 3) % 3; int z = (i / 9);
        if ((axis == Axis::X && x == slice) || (axis == Axis::Y && y == slice) || (axis == Axis::Z && z == slice)) {
            group.push_back(&m_cubies[i]);
        }
    }
    m_animator.start(group, axis, slice, clockwise);
}

void RubiksCube::performRotation(Axis axis, int slice, bool clockwise) {
    if(axis == Axis::X) {
        if(slice == 0) rotateLeftSlice(clockwise);
        else if(slice == 1) rotateMiddleVerticalSlice(clockwise);
        else if(slice == 2) rotateRightSlice(clockwise);
    } else if(axis == Axis::Y) {
        if(slice == 0) rotateDownSlice(clockwise);
        else if(slice == 1) rotateMiddleHorizontalSlice(clockwise);
        else if(slice == 2) rotateUpSlice(clockwise);
    } else { 
        if(slice == 0) rotateBackSlice(clockwise);
        else if(slice == 1) rotateMiddleDepthSlice(clockwise);
        else if(slice == 2) rotateFrontSlice(clockwise);
    }
}

// --- PERMUTACIONES DE ARRAY (LÓGICA) ---
void RubiksCube::rotateUpSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int z = 0; z < 3; ++z) for (int x = 0; x < 3; ++x) layerOld[x + z*3] = m_cubies[getIndex(x, 2, z)];
    std::array<Cubie, 9> layerNew;
    for (int z = 0; z < 3; ++z) {
        for (int x = 0; x < 3; ++x) {
            int newX, newZ;
            if (clockwise) { newX = 2 - z; newZ = x; } 
            else { newX = z; newZ = 2 - x; }
            layerNew[newX + newZ*3] = layerOld[x + z*3];
        }
    }
    for (int newZ = 0; newZ < 3; ++newZ) for (int newX = 0; newX < 3; ++newX) {
        m_cubies[getIndex(newX, 2, newZ)] = layerNew[newX + newZ*3];
    }
}

void RubiksCube::rotateDownSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int z = 0; z < 3; ++z) for (int x = 0; x < 3; ++x) layerOld[x + z*3] = m_cubies[getIndex(x, 0, z)];
    std::array<Cubie, 9> layerNew;
    for (int z = 0; z < 3; ++z) {
        for (int x = 0; x < 3; ++x) {
            int newX, newZ;
            if (clockwise) { newX = 2 - z; newZ = x; } 
            else { newX = z; newZ = 2 - x; }
            layerNew[newX + newZ*3] = layerOld[x + z*3];
        }
    }
    for (int newZ = 0; newZ < 3; ++newZ) for (int newX = 0; newX < 3; ++newX) {
        m_cubies[getIndex(newX, 0, newZ)] = layerNew[newX + newZ*3];
    }
}

void RubiksCube::rotateMiddleHorizontalSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int z = 0; z < 3; ++z) for (int x = 0; x < 3; ++x) layerOld[x + z*3] = m_cubies[getIndex(x, 1, z)];
    std::array<Cubie, 9> layerNew;
    for (int z = 0; z < 3; ++z) {
        for (int x = 0; x < 3; ++x) {
            int newX, newZ;
            if (clockwise) { newX = 2 - z; newZ = x; }
            else { newX = z; newZ = 2 - x; }
            layerNew[newX + newZ*3] = layerOld[x + z*3];
        }
    }
    for (int newZ = 0; newZ < 3; ++newZ) for (int newX = 0; newX < 3; ++newX) {
        m_cubies[getIndex(newX, 1, newZ)] = layerNew[newX + newZ*3];
    }
}

void RubiksCube::rotateRightSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int z = 0; z < 3; ++z) for (int y = 0; y < 3; ++y) layerOld[y + z*3] = m_cubies[getIndex(2, y, z)];
    std::array<Cubie, 9> layerNew;
    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 3; ++y) {
            int newY, newZ;
            if (clockwise) { newY = z; newZ = 2 - y; }
            else { newY = 2 - z; newZ = y; }
            layerNew[newY + newZ*3] = layerOld[y + z*3];
        }
    }
    for (int newZ = 0; newZ < 3; ++newZ) for (int newY = 0; newY < 3; ++newY) {
        m_cubies[getIndex(2, newY, newZ)] = layerNew[newY + newZ*3];
    }
}

void RubiksCube::rotateLeftSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int z = 0; z < 3; ++z) for (int y = 0; y < 3; ++y) layerOld[y + z*3] = m_cubies[getIndex(0, y, z)];
    std::array<Cubie, 9> layerNew;
    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 3; ++y) {
            int newY, newZ;
            if (clockwise) { newY = z; newZ = 2 - y; }
            else { newY = 2 - z; newZ = y; }
            layerNew[newY + newZ*3] = layerOld[y + z*3];
        }
    }
    for (int newZ = 0; newZ < 3; ++newZ) for (int newY = 0; newY < 3; ++newY) {
        m_cubies[getIndex(0, newY, newZ)] = layerNew[newY + newZ*3];
    }
}

void RubiksCube::rotateMiddleVerticalSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int z = 0; z < 3; ++z) for (int y = 0; y < 3; ++y) layerOld[y + z*3] = m_cubies[getIndex(1, y, z)];
    std::array<Cubie, 9> layerNew;
    for (int z = 0; z < 3; ++z) {
        for (int y = 0; y < 3; ++y) {
            int newY, newZ;
            if (clockwise) { newY = z; newZ = 2 - y; }
            else { newY = 2 - z; newZ = y; }
            layerNew[newY + newZ*3] = layerOld[y + z*3];
        }
    }
    for (int newZ = 0; newZ < 3; ++newZ) for (int newY = 0; newY < 3; ++newY) {
        m_cubies[getIndex(1, newY, newZ)] = layerNew[newY + newZ*3];
    }
}

void RubiksCube::rotateFrontSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x) layerOld[x + y * 3] = m_cubies[getIndex(x, y, 2)];
    std::array<Cubie, 9> layerNew;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            int newX, newY;
            if (clockwise) { newX = y; newY = 2 - x; }
            else { newX = 2 - y; newY = x; }
            layerNew[newX + newY * 3] = layerOld[x + y * 3];
        }
    }
    for (int newY = 0; newY < 3; ++newY) for (int newX = 0; newX < 3; ++newX) {
        m_cubies[getIndex(newX, newY, 2)] = layerNew[newX + newY * 3];
    }
}

void RubiksCube::rotateBackSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x) layerOld[x + y * 3] = m_cubies[getIndex(x, y, 0)];
    std::array<Cubie, 9> layerNew;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            int newX, newY;
            if (clockwise) { newX = y; newY = 2 - x; }
            else { newX = 2 - y; newY = x; }
            layerNew[newX + newY * 3] = layerOld[x + y * 3];
        }
    }
    for (int newY = 0; newY < 3; ++newY) for (int newX = 0; newX < 3; ++newX) {
        m_cubies[getIndex(newX, newY, 0)] = layerNew[newX + newY * 3];
    }
}

void RubiksCube::rotateMiddleDepthSlice(bool clockwise) { 
    std::array<Cubie, 9> layerOld;
    for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x) layerOld[x + y * 3] = m_cubies[getIndex(x, y, 1)];
    std::array<Cubie, 9> layerNew;
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            int newX, newY;
            if (clockwise) { newX = y; newY = 2 - x; }
            else { newX = 2 - y; newY = x; }
            layerNew[newX + newY * 3] = layerOld[x + y * 3];
        }
    }
    for (int newY = 0; newY < 3; ++newY) for (int newX = 0; newX < 3; ++newX) {
        m_cubies[getIndex(newX, newY, 1)] = layerNew[newX + newY * 3];
    }
}

void RubiksCube::draw(const Mat4& globalParent, const Mat4& view, const Mat4& proj, Vec3 emissionColor) {
    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "view"), 1, GL_FALSE, view.value_ptr());
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "projection"), 1, GL_FALSE, proj.value_ptr());
    glUniform3f(glGetUniformLocation(m_shaderProgram, "u_emission"), emissionColor.x, emissionColor.y, emissionColor.z);
    Vec3 palette[6] = {
        {0,0,0.8f},     // 0: Right (Blue)
        {0,0.6f,0},     // 1: Left (Green)
        {0.9f,0.9f,0.9f}, // 2: Up (White)
        {0.9f,0.9f,0},  // 3: Down (Yellow)
        {0.8f,0,0},     // 4: Front (Red)
        {1.0f,0.5f,0}   // 5: Back (Orange)
    };
    glUniform3fv(glGetUniformLocation(m_shaderProgram, "u_faceColors"), 6, (float*)palette);
    glBindVertexArray(m_VAO);
    for (int i=0; i<27; ++i) {
        Mat4 finalModel = globalParent * m_cubies[i].modelMatrix;
        glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "model"), 1, GL_FALSE, finalModel.value_ptr());
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

std::string RubiksCube::getFaceletString() {
    char facelets[55]; 
    int idx = 0;
    
    // Normales locales estándar
    Vec3 localNormals[6] = {
        Vec3(1,0,0), Vec3(-1,0,0), Vec3(0,1,0), Vec3(0,-1,0), Vec3(0,0,1), Vec3(0,0,-1)
    };
    // Direcciones globales
    Vec3 globalDirs[6] = {
        Vec3(0,1,0), Vec3(1,0,0), Vec3(0,0,1), Vec3(0,-1,0), Vec3(-1,0,0), Vec3(0,0,-1)
    };

    // Mapeo dinámico: Leemos los centros para saber qué color es qué cara
    std::map<Color, char> c2c;
    // Centro Arriba (x=1,y=2,z=1) -> Debe dar Color::WHITE (si no ha rotado centro)
    // Pero si rotamos centros, debemos buscar qué cara está mirando ARRIBA en el centro.
    
    // Función auxiliar para saber qué color tiene un cubie apuntando a una dirección global
    auto getColorLookingAt = [&](int x, int y, int z, Vec3 dir) -> Color {
        Cubie& c = getCubieAt(x, y, z);
        float maxDot = -2.0f;
        int bestFaceID = -1;
        for(int f=0; f<6; ++f) {
            Vec3 n = localNormals[f];
            // R * n
            float rx = c.modelMatrix.m[0][0]*n.x + c.modelMatrix.m[1][0]*n.y + c.modelMatrix.m[2][0]*n.z;
            float ry = c.modelMatrix.m[0][1]*n.x + c.modelMatrix.m[1][1]*n.y + c.modelMatrix.m[2][1]*n.z;
            float rz = c.modelMatrix.m[0][2]*n.x + c.modelMatrix.m[1][2]*n.y + c.modelMatrix.m[2][2]*n.z;
            Vec3 rotatedN(rx, ry, rz);
            if(dot(rotatedN, dir) > maxDot) { maxDot = dot(rotatedN, dir); bestFaceID = f; }
        }
        return c.getFaceColor((Face)bestFaceID);
    };

    // Construir mapa basado en los centros actuales
    c2c[getColorLookingAt(1, 2, 1, globalDirs[0])] = 'U'; // Centro UP mira a UP
    c2c[getColorLookingAt(1, 2, 1, globalDirs[3])] = 'D'; // Centro UP mira a DOWN (si el centro tiene color abajo)
    // Corrección: Leer cada centro
    c2c[getColorLookingAt(1, 2, 1, globalDirs[0])] = 'U'; 
    c2c[getColorLookingAt(1, 0, 1, globalDirs[3])] = 'D'; 
    c2c[getColorLookingAt(2, 1, 1, globalDirs[1])] = 'R'; 
    c2c[getColorLookingAt(0, 1, 1, globalDirs[4])] = 'L'; 
    c2c[getColorLookingAt(1, 1, 2, globalDirs[2])] = 'F'; 
    c2c[getColorLookingAt(1, 1, 0, globalDirs[5])] = 'B'; 

    // Ahora leemos todas las caras
    auto add = [&](int x, int y, int z, int dirIndex) {
        Color c = getColorLookingAt(x, y, z, globalDirs[dirIndex]);
        facelets[idx++] = c2c[c];
    };

    // U Face (Global Up)
    add(0, 2, 0, 0); add(1, 2, 0, 0); add(2, 2, 0, 0);
    add(0, 2, 1, 0); add(1, 2, 1, 0); add(2, 2, 1, 0);
    add(0, 2, 2, 0); add(1, 2, 2, 0); add(2, 2, 2, 0);

    // R Face (Global Right)
    add(2, 2, 2, 1); add(2, 2, 1, 1); add(2, 2, 0, 1);
    add(2, 1, 2, 1); add(2, 1, 1, 1); add(2, 1, 0, 1);
    add(2, 0, 2, 1); add(2, 0, 1, 1); add(2, 0, 0, 1);

    // F Face (Global Front)
    add(0, 2, 2, 2); add(1, 2, 2, 2); add(2, 2, 2, 2);
    add(0, 1, 2, 2); add(1, 1, 2, 2); add(2, 1, 2, 2);
    add(0, 0, 2, 2); add(1, 0, 2, 2); add(2, 0, 2, 2);

    // D Face (Global Down)
    add(0, 0, 2, 3); add(1, 0, 2, 3); add(2, 0, 2, 3);
    add(0, 0, 1, 3); add(1, 0, 1, 3); add(2, 0, 1, 3);
    add(0, 0, 0, 3); add(1, 0, 0, 3); add(2, 0, 0, 3);

    // L Face (Global Left)
    add(0, 2, 0, 4); add(0, 2, 1, 4); add(0, 2, 2, 4);
    add(0, 1, 0, 4); add(0, 1, 1, 4); add(0, 1, 2, 4);
    add(0, 0, 0, 4); add(0, 0, 1, 4); add(0, 0, 2, 4);

    // B Face (Global Back)
    add(2, 2, 0, 5); add(1, 2, 0, 5); add(0, 2, 0, 5);
    add(2, 1, 0, 5); add(1, 1, 0, 5); add(0, 1, 0, 5);
    add(2, 0, 0, 5); add(1, 0, 0, 5); add(0, 0, 0, 5);

    facelets[54] = '\0';
    return std::string(facelets);
}

void RubiksCube::debugPrintFacelets() {
    std::string s = getFaceletString();
    std::cout << "\n--- CURRENT FACELETS ---\n";
    std::cout << "      " << s.substr(0, 3) << "\n";
    std::cout << "      " << s.substr(3, 3) << "\n";
    std::cout << "      " << s.substr(6, 3) << "\n";
    std::cout << s.substr(36,3) << " " << s.substr(18,3) << " " << s.substr(9,3) << " " << s.substr(45,3) << "\n";
    std::cout << s.substr(39,3) << " " << s.substr(21,3) << " " << s.substr(12,3) << " " << s.substr(48,3) << "\n";
    std::cout << s.substr(42,3) << " " << s.substr(24,3) << " " << s.substr(15,3) << " " << s.substr(51,3) << "\n";
    std::cout << "      " << s.substr(27, 3) << "\n";
    std::cout << "      " << s.substr(30, 3) << "\n";
    std::cout << "      " << s.substr(33, 3) << "\n";
    std::cout << "------------------------\n";
}

Vec3 RubiksCube::getVec3FromColor(Color color) { return Vec3(0,0,0); }