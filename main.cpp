// ----------------------------------------------------------------------------
// ESQUELETO DE CUBO DE RUBIK EN UN SOLO ARCHIVO (CORREGIDO v3 - Cámara Manual)
// ----------------------------------------------------------------------------
// Compilación (ejemplo):
// g++ RubikSkeleton_Fixed.cpp -o rubik -lglfw -lGL -lm -ldl
// (La implementación de GLAD está incluida abajo)
// ----------------------------------------------------------------------------

// --- 1. INCLUDES ---
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
// --------------
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <sstream>
#include <fstream>
#include <cmath>        // Para sinf, cosf, tan, sqrt
#include <cstddef>      // Para offsetof

// Definición de M_PI por si math.h no la incluye
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- 2. CONFIGURACIÓN ---
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// --- 3. LIBRERÍA MATEMÁTICA (PROPORCIONADA POR EL USUARIO) ---

//-------------------------------------libreria-----------------------------------------

void identity(float M[16]) {
    for (int i = 0; i < 16; i++) M[i] = 0.0f;
    M[0] = M[5] = M[10] = M[15] = 1.0f;
}

void translate(float M[16], float tx, float ty, float tz) {
    identity(M);
    M[12] = tx;
    M[13] = ty;
    M[14] = tz;
}

void inverseTranslate(float M[16], float tx, float ty, float tz) {
    identity(M);
    M[12] = -tx;
    M[13] = -ty;
    M[14] = -tz;
}

void scale(float M[16], float sx, float sy, float sz) {
    identity(M);
    M[0] = sx;
    M[5] = sy;
    M[10] = sz;
}

void inverseScale(float M[16], float sx, float sy, float sz) {
    identity(M);
    M[0] = 1.0f / sx;
    M[5] = 1.0f / sy;
    M[10] = 1.0f / sz;
}

void rotateX(float M[16], float angle) {
    identity(M);
    float c = cosf(angle), s = sinf(angle);
    M[5] = c;  M[6] = -s;
    M[9] = s;  M[10] = c;
}

void rotateY(float M[16], float angle) {
    identity(M);
    float c = cosf(angle), s = sinf(angle);
    M[0] = c; M[2] = s;
    M[8] = -s; M[10] = c;
}

void rotateZ(float M[16], float angle) {
    identity(M);
    float c = cosf(angle), s = sinf(angle);
    M[0] = c;  M[1] = -s;
    M[4] = s;  M[5] = c;
}

void inverseRotateX(float M[16], float angle) {
    rotateX(M, -angle);
}

void inverseRotateY(float M[16], float angle) {
    rotateY(M, -angle);
}

void inverseRotateZ(float M[16], float angle) {
    rotateZ(M, -angle);
}

void multiply(float R[16], const float A[16], const float B[16]) {
    float res[16];
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            res[col + row*4] =
                A[row*4+0]*B[col+0] +
                A[row*4+1]*B[col+4] +
                A[row*4+2]*B[col+8] +
                A[row*4+3]*B[col+12];
        }
    }
    for (int i = 0; i < 16; i++) R[i] = res[i];
}


//-----------------Estructuras y funciones inline para la camara------------------------
struct Vec3 
{
    float x,y,z;
    Vec3() : x(0),y(0),z(0) {}
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    Vec3 operator+(const Vec3& o) const { return {x+o.x,y+o.y,z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x,y-o.y,z-o.z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
};

struct Mat4 
{
    // Column-major 4x4 (OpenGL style): m[c*4 + r]
    float m[16];
    Mat4()
    { 
        for(int i=0;i<16;++i)
        {
            m[i]=0;
        }           
    }
    static Mat4 identity()
    {
        Mat4 I; 
        I.m[0]=I.m[5]=I.m[10]=I.m[15]=1.0f; 
        return I;
    }
};

static inline float dot(const Vec3&a,const Vec3&b)
{ 
    return ((a.x*b.x) + (a.y*b.y) + (a.z*b.z)); 
}

static inline Vec3  cross(const Vec3&a,const Vec3&b)
{
    return { ((a.y*b.z) - (a.z*b.y)), ((a.z*b.x) - (a.x*b.z)), ((a.x*b.y - a.y*b.x)) };
}

static inline float length(const Vec3&v)
{ 
    return (std::sqrt(dot(v,v))); 
}

static inline Vec3  normalize(const Vec3&v)
{ 
    float L = length(v); 
    return ((L>0) ? (v*(1.0f/L)) : v); 
}

static inline Mat4 perspective(float fovy_deg, float aspect, float znear, float zfar)
{
    float f = 1.0f / std::tan(fovy_deg*(float)M_PI/180.0f/2.0f);
    Mat4 P;
    P.m[0]=f/aspect; 
    P.m[5]=f; 
    P.m[10]=(zfar+znear)/(znear-zfar); P.m[11]=-1.0f;
    P.m[14]=(2.0f*zfar*znear)/(znear-zfar);
    return P;
}

static inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
{
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    float M[16];
    identity(M);
    M[0] = s.x;  M[4] = s.y;  M[8]  = s.z;
    M[1] = u.x;  M[5] = u.y;  M[9]  = u.z;
    M[2] = -f.x; M[6] = -f.y; M[10] = -f.z;

    float T[16];
    inverseTranslate(T, eye.x, eye.y, eye.z);

    float R[16];
    multiply(R, M, T); // Aplicar T primero, luego M

    Mat4 result;
    for (int i = 0; i < 16; ++i)
        result.m[i] = R[i];

    return result;
}

// --- 4. SHADERS (INTERNOS) ---

// Vertex Shader
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 2) in int aFaceID;

    flat out int v_FaceID; // 'flat' = no interpolar

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        v_FaceID = aFaceID;
    }
)glsl";

// Fragment Shader
const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    flat in int v_FaceID;

    uniform vec3 u_faceColors[6]; // [R, L, U, D, F, B]

    void main() {
        vec3 objectColor = u_faceColors[v_FaceID];
        if (objectColor == vec3(0.0, 0.0, 0.0)) {
            discard;
        }
        FragColor = vec4(objectColor, 1.0);
    }
)glsl";


// --- 5. CLASE SHADER (SIMPLIFICADA) ---
class Shader {
public:
    unsigned int ID;

    Shader(const char* vertexSource, const char* fragmentSource) {
        unsigned int vertex, fragment;
        
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexSource, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");
        
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentSource, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");
        
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    void use() { 
        glUseProgram(ID); 
    }

    void setMat4(const std::string &name, const float* mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, mat);
    }

    void setVec3Array(const std::string &name, int count, const Vec3 *values) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), count, (const GLfloat*)values);
    }

private:
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << std::endl;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
            }
        }
    }
};


// --- 6. ENUMS Y CLASES DEL CUBO ---

enum class Color { WHITE, YELLOW, RED, ORANGE, GREEN, BLUE, BLACK };
enum class Face { UP, DOWN, LEFT, RIGHT, FRONT, BACK };
using FaceColors = std::map<Face, Color>;

//
// CLASE CUBIE: Almacena el estado de color de 1 pieza
//
class Cubie {
public:
    Cubie() {
        m_faces[Face::UP]    = Color::BLACK;
        m_faces[Face::DOWN]  = Color::BLACK;
        m_faces[Face::LEFT]  = Color::BLACK;
        m_faces[Face::RIGHT] = Color::BLACK;
        m_faces[Face::FRONT] = Color::BLACK;
        m_faces[Face::BACK]  = Color::BLACK;
    }

    void setFaceColor(Face face, Color color) { m_faces[face] = color; }
    Color getFaceColor(Face face) const { return m_faces.at(face); }

    void rotateZ(bool clockwise) {
        Color oldUp = m_faces[Face::UP], oldL = m_faces[Face::LEFT], oldD = m_faces[Face::DOWN], oldR = m_faces[Face::RIGHT];
        if (clockwise) { m_faces[Face::UP]=oldL; m_faces[Face::LEFT]=oldD; m_faces[Face::DOWN]=oldR; m_faces[Face::RIGHT]=oldUp; }
        else { m_faces[Face::UP]=oldR; m_faces[Face::RIGHT]=oldD; m_faces[Face::DOWN]=oldL; m_faces[Face::LEFT]=oldUp; }
    }
    void rotateY(bool clockwise) {
        Color oldF = m_faces[Face::FRONT], oldR = m_faces[Face::RIGHT], oldB = m_faces[Face::BACK], oldL = m_faces[Face::LEFT];
        if (clockwise) { m_faces[Face::FRONT]=oldR; m_faces[Face::RIGHT]=oldB; m_faces[Face::BACK]=oldL; m_faces[Face::LEFT]=oldF; }
        else { m_faces[Face::FRONT]=oldL; m_faces[Face::LEFT]=oldB; m_faces[Face::BACK]=oldR; m_faces[Face::RIGHT]=oldF; }
    }
    void rotateX(bool clockwise) {
        Color oldU = m_faces[Face::UP], oldB = m_faces[Face::BACK], oldD = m_faces[Face::DOWN], oldF = m_faces[Face::FRONT];
        if (clockwise) { m_faces[Face::UP]=oldF; m_faces[Face::FRONT]=oldD; m_faces[Face::DOWN]=oldB; m_faces[Face::BACK]=oldU; }
        else { m_faces[Face::UP]=oldB; m_faces[Face::BACK]=oldD; m_faces[Face::DOWN]=oldF; m_faces[Face::FRONT]=oldU; }
    }

private:
    FaceColors m_faces;
};

//
// CLASE RUBIKSCUBE: El "Manager" que controla los 26 cubies y el dibujado
//
class RubiksCube {
public:
    RubiksCube() {
        for (int z = 0; z <= 2; z++) { for (int y = 0; y <= 2; y++) { for (int x = 0; x <= 2; x++) {
            if (x == 1 && y == 1 && z == 1) continue;
            int i = getIndex(x, y, z);
            if (z == 2) m_cubies[i].setFaceColor(Face::FRONT, Color::RED);
            if (z == 0) m_cubies[i].setFaceColor(Face::BACK, Color::ORANGE);
            if (y == 2) m_cubies[i].setFaceColor(Face::UP, Color::WHITE);
            if (y == 0) m_cubies[i].setFaceColor(Face::DOWN, Color::YELLOW);
            if (x == 0) m_cubies[i].setFaceColor(Face::LEFT, Color::GREEN);
            if (x == 2) m_cubies[i].setFaceColor(Face::RIGHT, Color::BLUE);
        }}}
    }

    ~RubiksCube() {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
    }

    void rotateFace(Face face, bool clockwise) {
        switch (face) {
            case Face::FRONT: rotateSliceZ(2, clockwise); break;
            case Face::BACK:  rotateSliceZ(0, !clockwise); break;
            case Face::UP:    rotateSliceY(2, clockwise); break;
            case Face::DOWN:  rotateSliceY(0, !clockwise); break;
            case Face::RIGHT: rotateSliceX(2, clockwise); break;
            case Face::LEFT:  rotateSliceX(0, !clockwise); break;
        }
    }

    void setupMesh() {
        float s = 0.5f;
        struct Vertex { Vec3 pos; GLint faceID; };

        std::vector<Vertex> vertices = {
            {{s, s, s}, 0}, {{s,-s, s}, 0}, {{s,-s,-s}, 0},
            {{s,-s,-s}, 0}, {{s, s,-s}, 0}, {{s, s, s}, 0},
            {{-s, s, s}, 1}, {{-s,-s,-s}, 1}, {{-s,-s, s}, 1},
            {{-s,-s,-s}, 1}, {{-s, s, s}, 1}, {{-s, s,-s}, 1},
            {{-s, s,-s}, 2}, {{s, s, s}, 2}, {{s, s,-s}, 2},
            {{s, s, s}, 2}, {{-s, s,-s}, 2}, {{-s, s, s}, 2},
            {{-s,-s,-s}, 3}, {{s,-s,-s}, 3}, {{s,-s, s}, 3},
            {{s,-s, s}, 3}, {{-s,-s, s}, 3}, {{-s,-s,-s}, 3},
            {{-s,-s, s}, 4}, {{s,-s, s}, 4}, {{s, s, s}, 4},
            {{s, s, s}, 4}, {{-s, s, s}, 4}, {{-s,-s, s}, 4},
            {{-s,-s,-s}, 5}, {{-s, s,-s}, 5}, {{s, s,-s}, 5},
            {{s, s,-s}, 5}, {{s,-s,-s}, 5}, {{-s,-s,-s}, 5}
        };

        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        GLsizei stride = sizeof(Vertex);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(0);
        glVertexAttribIPointer(2, 1, GL_INT, stride, (void*)offsetof(Vertex, faceID));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }

    void draw(Shader& shader, const Mat4& view, const Mat4& projection) {
        shader.use();
        shader.setMat4("projection", projection.m);
        shader.setMat4("view", view.m);

        glBindVertexArray(m_VAO);
        Vec3 faceColors[6];
        float modelMatrix[16];

        for (int z = 0; z <= 2; z++) {
        for (int y = 0; y <= 2; y++) {
        for (int x = 0; x <= 2; x++) {
            if (x == 1 && y == 1 && z == 1) continue;
            Cubie& cubie = m_cubies[getIndex(x, y, z)];

            float px = (x - 1.0f) * m_spacing;
            float py = (y - 1.0f) * m_spacing;
            float pz = (z - 1.0f) * m_spacing;
            translate(modelMatrix, px, py, pz);
            shader.setMat4("model", modelMatrix);

            faceColors[0] = getVec3FromColor(cubie.getFaceColor(Face::RIGHT));
            faceColors[1] = getVec3FromColor(cubie.getFaceColor(Face::LEFT));
            faceColors[2] = getVec3FromColor(cubie.getFaceColor(Face::UP));
            faceColors[3] = getVec3FromColor(cubie.getFaceColor(Face::DOWN));
            faceColors[4] = getVec3FromColor(cubie.getFaceColor(Face::FRONT));
            faceColors[5] = getVec3FromColor(cubie.getFaceColor(Face::BACK));
            shader.setVec3Array("u_faceColors", 6, faceColors);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }}}
        glBindVertexArray(0);
    }

private:
    std::array<Cubie, 27> m_cubies;
    GLuint m_VAO, m_VBO;
    const float m_spacing = 1.05f;

    int getIndex(int x, int y, int z) const { return x + y * 3 + z * 9; }

    Vec3 getVec3FromColor(Color color) {
        switch (color) {
            case Color::WHITE:  return Vec3(1.0f, 1.0f, 1.0f);
            case Color::YELLOW: return Vec3(1.0f, 1.0f, 0.0f);
            case Color::RED:    return Vec3(1.0f, 0.0f, 0.0f);
            case Color::ORANGE: return Vec3(1.0f, 0.5f, 0.0f);
            case Color::GREEN:  return Vec3(0.0f, 1.0f, 0.0f);
            case Color::BLUE:   return Vec3(0.0f, 0.0f, 1.0f);
            case Color::BLACK:
            default:            return Vec3(0.0f, 0.0f, 0.0f);
        }
    }

    void rotateSliceZ(int slice, bool clockwise) {
        std::array<Cubie, 8> tempRing;
        int coords[8][2] = {{0,0}, {1,0}, {2,0}, {2,1}, {2,2}, {1,2}, {0,2}, {0,1}};
        for (int i=0; i<8; ++i) tempRing[i] = m_cubies[getIndex(coords[i][0], coords[i][1], slice)];
        int shift = clockwise ? 2 : -2;
        for (int i=0; i<8; ++i) m_cubies[getIndex(coords[(i+shift+8)%8][0], coords[(i+shift+8)%8][1], slice)] = tempRing[i];
        for (int y=0; y<=2; y++) for (int x=0; x<=2; x++) m_cubies[getIndex(x,y,slice)].rotateZ(clockwise);
    }
    void rotateSliceY(int slice, bool clockwise) {
        std::array<Cubie, 8> tempRing;
        int coords[8][2] = {{0,0}, {1,0}, {2,0}, {2,1}, {2,2}, {1,2}, {0,2}, {0,1}};
        for (int i=0; i<8; ++i) tempRing[i] = m_cubies[getIndex(coords[i][0], slice, coords[i][1])];
        int shift = clockwise ? 2 : -2;
        for (int i=0; i<8; ++i) m_cubies[getIndex(coords[(i+shift+8)%8][0], slice, coords[(i+shift+8)%8][1])] = tempRing[i];
        for (int z=0; z<=2; z++) for (int x=0; x<=2; x++) m_cubies[getIndex(x,slice,z)].rotateY(clockwise);
    }
    void rotateSliceX(int slice, bool clockwise) {
        std::array<Cubie, 8> tempRing;
        int coords[8][2] = {{0,0}, {1,0}, {2,0}, {2,1}, {2,2}, {1,2}, {0,2}, {0,1}};
        for (int i=0; i<8; ++i) tempRing[i] = m_cubies[getIndex(slice, coords[i][0], coords[i][1])];
        int shift = clockwise ? 2 : -2;
        for (int i=0; i<8; ++i) m_cubies[getIndex(slice, coords[(i+shift+8)%8][0], coords[(i+shift+8)%8][1])] = tempRing[i];
        for (int z=0; z<=2; z++) for (int y=0; y<=2; y++) m_cubies[getIndex(slice,y,z)].rotateX(clockwise);
    }
};


// --- 7. GLOBALES Y CALLBACKS ---
RubiksCube* g_rubiksCube = nullptr;
bool keyProcessed[348] = {false};
Vec3 g_cameraPos(0.0f, 0.0f, 5.0f); // <-- ¡NUEVO! Posición inicial de la cámara

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (g_rubiksCube == nullptr) return;

    // --- Lógica de Cámara (Responde a PRESIONAR y REPETIR) ---
    float cameraSpeed = 0.1f;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
            case GLFW_KEY_W: g_cameraPos.z -= cameraSpeed; break;
            case GLFW_KEY_S: g_cameraPos.z += cameraSpeed; break;
            case GLFW_KEY_A: g_cameraPos.x -= cameraSpeed; break;
            case GLFW_KEY_D: g_cameraPos.x += cameraSpeed; break;
            case GLFW_KEY_Q: g_cameraPos.y += cameraSpeed; break; // Arriba
            case GLFW_KEY_E: g_cameraPos.y -= cameraSpeed; break; // Abajo
        }
    }

    // --- Lógica del Cubo/Programa (Responde solo a UNA PRESIÓN) ---
    bool clockwise = !(mods & GLFW_MOD_SHIFT);
    if (action == GLFW_PRESS && !keyProcessed[key]) {
        keyProcessed[key] = true;
        switch (key) {
            case GLFW_KEY_F: g_rubiksCube->rotateFace(Face::FRONT, clockwise); break;
            // (Añadir más teclas aquí: U, D, L, R, B)
            case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, true); break;
        }
    }
    if (action == GLFW_RELEASE) {
        keyProcessed[key] = false;
    }
}


// --- 8. FUNCIÓN MAIN ---
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Esqueleto Cubo Rubik", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

    Shader cubieShader(vertexShaderSource, fragmentShaderSource);

    RubiksCube rubiksCube;
    rubiksCube.setupMesh();
    g_rubiksCube = &rubiksCube;

    std::cout << "Controles del Cubo:" << std::endl;
    std::cout << "  F: Girar cara Frontal (SHIFT para anti-horario)" << std::endl;
    std::cout << "Controles de Cámara:" << std::endl;
    std::cout << "  W/S: Adelante / Atrás" << std::endl;
    std::cout << "  A/D: Izquierda / Derecha" << std::endl;
    std::cout << "  Q/E: Arriba / Abajo" << std::endl;


    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- 6. Configurar Cámara (Usando tu librería y la variable global) ---
        
        // ¡Ya no hay órbita! Usa la variable global.
        Vec3 eye = g_cameraPos;
        Vec3 center(g_cameraPos.x, g_cameraPos.y, g_cameraPos.z - 1.0f); // Mira "hacia adelante" desde la cámara
        // O mira siempre al centro del cubo:
        // Vec3 center(0.0f, 0.0f, 0.0f); 
        Vec3 up(0.0f, 1.0f, 0.0f);

        Mat4 view = lookAt(eye, center, up);
        Mat4 proj = perspective(45.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        
        // 7. Dibujar
        rubiksCube.draw(cubieShader, view, proj);

        // 8. Swap y Poll
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
