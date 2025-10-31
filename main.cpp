// ----------------------------------------------------------------------------
// CUBO DE RUBIK
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
#include <cstddef>      

// --- CONFIGURACIÓN ---
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// --- LIBRERÍA MATEMÁTICA---
#include "matLibrary.h"

// --- SHADERS (INTERNOS) ---

// Vertex Shader
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 2) in int aFaceID;

    flat out int v_FaceID; 
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
	uniform float u_isBorder;

    void main() {
		if (u_isBorder > 0.5) {
            // PASE 2: Estamos dibujando el borde, forzar a negro.
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
			
        } else {
            // PASE 1: Estamos dibujando el relleno, usar el color de la cara.
            vec3 objectColor = u_faceColors[v_FaceID];
            
            // Si el fondo es negro (caras internas), píntalo de un gris oscuro.
            if (objectColor == vec3(0.0, 0.0, 0.0)) {
                 FragColor = vec4(0.05, 0.05, 0.05, 1.0); // Plástico base
            } else {
                 FragColor = vec4(objectColor, 1.0);
			}
		}
    }
)glsl";


// --- CLASE SHADER (SIMPLIFICADA) ---
#include "shader.h"

// --- ENUMS Y CLASES DEL CUBO ---

enum class Color { WHITE, YELLOW, RED, ORANGE, GREEN, BLUE, BLACK };
enum class Face { UP, DOWN, LEFT, RIGHT, FRONT, BACK };
//using FaceColors = std::map<Face, Color>;

// CLASE CUBIE: Almacena el estado de color de 1 pieza
class Cubie {
public:
    Cubie() {
        identity(this->modelMatrix);
        m_faces[Face::UP]    = Color::BLACK;
        m_faces[Face::DOWN]  = Color::BLACK;
        m_faces[Face::LEFT]  = Color::BLACK;
        m_faces[Face::RIGHT] = Color::BLACK;
        m_faces[Face::FRONT] = Color::BLACK;
        m_faces[Face::BACK]  = Color::BLACK;
    }

    float modelMatrix[16];

    void setFaceColor(Face face, Color color) { m_faces[face] = color; }
    Color getFaceColor(Face face) const { return m_faces.at(face); }

    void init(int x, int y, int z, float spacing) {
        float px = (x - 1.0f) * spacing;
        float py = (y - 1.0f) * spacing;
        float pz = (z - 1.0f) * spacing;
        translate(this->modelMatrix, px, py, pz);
    }

private:
    std::map<Face, Color> m_faces;
};

//
// CLASE RUBIKSCUBE
//
	
class RubiksCube {
public:
    RubiksCube() {
        for (int z = 0; z <= 2; z++) { 
			for (int y = 0; y <= 2; y++) { 
				for (int x = 0; x <= 2; x++) {
					//if (x == 1 && y == 1 && z == 1) continue;

					int i = getIndex(x, y, z);

					m_cubies[i].init(x, y, z, m_spacing);

					if (z == 2) m_cubies[i].setFaceColor(Face::FRONT, Color::RED);
					if (z == 0) m_cubies[i].setFaceColor(Face::BACK, Color::ORANGE);
					if (y == 2) m_cubies[i].setFaceColor(Face::UP, Color::WHITE);
					if (y == 0) m_cubies[i].setFaceColor(Face::DOWN, Color::YELLOW);
					if (x == 0) m_cubies[i].setFaceColor(Face::LEFT, Color::GREEN);
					if (x == 2) m_cubies[i].setFaceColor(Face::RIGHT, Color::BLUE);
				}
			}
		}
    }

    ~RubiksCube() {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
		glDeleteBuffers(1, &m_EBO_relleno);
		glDeleteBuffers(1, &m_EBO_bordes);
    }

	
    void setupMesh() {
        float s = 0.5f;
		class Vertex { 
		public:
			Vec3 pos; 
			GLint faceID; 
		};
        std::vector<Vertex> vertices = {
            {{s, s, s}, 0}, {{s,-s, s}, 0}, {{s,-s,-s}, 0}, //Right Face
            {{s,-s,-s}, 0}, {{s, s,-s}, 0}, {{s, s, s}, 0},
            {{-s, s, s}, 1}, {{-s,-s,-s}, 1}, {{-s,-s, s}, 1}, //Left Face
            {{-s,-s,-s}, 1}, {{-s, s, s}, 1}, {{-s, s,-s}, 1},
            {{-s, s,-s}, 2}, {{s, s, s}, 2}, {{s, s,-s}, 2}, //Up Face
            {{s, s, s}, 2}, {{-s, s,-s}, 2}, {{-s, s, s}, 2},
            {{-s,-s,-s}, 3}, {{s,-s,-s}, 3}, {{s,-s, s}, 3}, //Down Face
            {{s,-s, s}, 3}, {{-s,-s, s}, 3}, {{-s,-s,-s}, 3},
            {{-s,-s, s}, 4}, {{s,-s, s}, 4}, {{s, s, s}, 4}, //Front Face 
            {{s, s, s}, 4}, {{-s, s, s}, 4}, {{-s,-s, s}, 4},
            {{-s,-s,-s}, 5}, {{-s, s,-s}, 5}, {{s, s,-s}, 5},// Back Face 
            {{s, s,-s}, 5}, {{s,-s,-s}, 5}, {{-s,-s,-s}, 5}
        };
		
		const unsigned int INDICES_RELLENO[36] = {
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 
			18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35
		};
		
		const unsigned int BORDER_INDICES[24] = {

		// Cara Frontal
		24, 25,  // Inferior
		25, 26,  // Derecha (v2)
		26, 28,  // Superior (v4)
		28, 24,  // Izquierda (v1)
		// 2. Contorno de la Cara Trasera (Z-) - Usando los índices 30-35
		30, 31, // Inferior (de (-s, -s, -s) a (s, -s, -s))
		31, 32, // Derecha (de (s, -s, -s) a (s, s, -s))
		32, 34, // Superior (de (s, s, -s) a (-s, s, -s))
		34, 30, // Izquierda (de (-s, s, -s) a (-s, -s, -s))
		
		24, 30, // Inferior-Izquierda
		25, 34, // Inferior-Derecha
		26, 32, // Superior-Derecha
		28, 31  // Superior-Izquierda


		};
		
		

        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
		glGenBuffers(1, &m_EBO_relleno);
		glGenBuffers(1, &m_EBO_bordes);
        glBindVertexArray(m_VAO);
        // VBO
		glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
		// EBO INDICES_RELLENO
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_relleno);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(INDICES_RELLENO), INDICES_RELLENO, GL_STATIC_DRAW);
		// EBO BORDER_INDICES
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_bordes); 
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(BORDER_INDICES), BORDER_INDICES, GL_STATIC_DRAW);
		//
        GLsizei stride = sizeof(Vertex);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(0);
        glVertexAttribIPointer(2, 1, GL_INT, stride, (void*)offsetof(Vertex, faceID));
        glEnableVertexAttribArray(2);
        glBindVertexArray(0);
    }

    void draw(Shader& shader) {
        //shader.use();

        glBindVertexArray(m_VAO);
        Vec3 faceColors[6];

        for (int i = 0; i < 27; i++){
            Cubie& cubie = m_cubies[i];
			// Enviamos la matriz model de cada cubie
            shader.setMat4("model", cubie.modelMatrix);
			// Enviamos los colores del cubie al shader
            faceColors[0] = getVec3FromColor(cubie.getFaceColor(Face::RIGHT));
            faceColors[1] = getVec3FromColor(cubie.getFaceColor(Face::LEFT));
            faceColors[2] = getVec3FromColor(cubie.getFaceColor(Face::UP));
            faceColors[3] = getVec3FromColor(cubie.getFaceColor(Face::DOWN));
            faceColors[4] = getVec3FromColor(cubie.getFaceColor(Face::FRONT));
            faceColors[5] = getVec3FromColor(cubie.getFaceColor(Face::BACK));
			shader.setVec3Array("u_faceColors", 6, faceColors);
			
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_relleno); 
			shader.setFloat("u_isBorder", 0.0f);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
			 
			glLineWidth(3.0f);
			shader.setFloat("u_isBorder", 1.0f);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_bordes); 
			glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
            //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }

private:
    std::array<Cubie, 27> m_cubies;
    GLuint m_VAO, m_VBO;
	GLuint m_EBO_relleno; // EBO para 36 índices (triángulos)
	GLuint m_EBO_bordes;
    const float m_spacing = 1.0f;

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
};


// --- 7. GLOBALES Y CALLBACKS ---

RubiksCube* g_rubiksCube = nullptr;

bool keyProcessed[348] = {false};

Vec3 g_cameraPos(0.0f, 0.0f, 5.0f); 

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

    if (action == GLFW_PRESS && !keyProcessed[key]) {
        keyProcessed[key] = true;
        switch (key) {
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


    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Vec3 eye = g_cameraPos;
        Vec3 center(g_cameraPos.x, g_cameraPos.y, g_cameraPos.z - 1.0f); 
  
        Vec3 up(0.0f, 1.0f, 0.0f);

        Mat4 view = lookAt(eye, center, up);
        Mat4 proj = perspective(45.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        cubieShader.use();       
		cubieShader.setMat4("projection", proj.m);
        cubieShader.setMat4("view", view.m);
        rubiksCube.draw(cubieShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
