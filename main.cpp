#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "linmath.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cmath>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// multiplicar vector por matriz
void transformVertex(float out[3], const float M[16], const float v[3]) {
    float x = v[0], y = v[1], z = v[2];
    out[0] = M[0]*x + M[4]*y + M[8]*z + M[12];
    out[1] = M[1]*x + M[5]*y + M[9]*z + M[13];
    out[2] = M[2]*x + M[6]*y + M[10]*z + M[14];
}
const char *vertexShaderSource = R"(
	#version 330 core
	layout (location = 0) in vec3 aPos;    // Posición del vértice (de -0.5 a 0.5)
	layout (location = 1) in vec3 aNormal; // Normal del vértice
	layout (location = 2) in int aFaceID; // ID de la cara (0 a 5)

	// Salidas para el Fragment Shader
	out vec3 v_FragPos;
	out vec3 v_Normal;
	flat out int v_FaceID; // 'flat' significa que no se interpola

	// Matrices de transformación
	uniform mat4 model;
	uniform mat4 view;
	uniform mat4 projection;

	void main() {
		v_FragPos = vec3(model * vec4(aPos, 1.0));
		v_Normal = mat3(transpose(inverse(model))) * aNormal;
		v_FaceID = aFaceID;
		
		gl_Position = projection * view * model * vec4(aPos, 1.0);
	}
	)";
	
	
	
const char *fragmentShaderSource = R"(
	#version 330 core
	out vec4 FragColor;

	// Entradas del Vertex Shader
	in vec3 v_FragPos;
	in vec3 v_Normal;
	flat in int v_FaceID;

	// ¡EL TRUCO! Un array de 6 colores, uno para cada cara
	// Lo actualizaremos para CADA cubie que dibujemos.
	uniform vec3 u_faceColors[6]; // [0]=+X, [1]=-X, [2]=+Y, [3]=-Y, [4]=+Z, [5]=-Z

	// (Para iluminación simple)
	uniform vec3 u_lightPos;
	uniform vec3 u_viewPos;

	void main() {
		// 1. Seleccionar el color de la cara
		vec3 objectColor = u_faceColors[v_FaceID];

		// 2. Si es negro (cara interna), no lo dibujes
		if (objectColor == vec3(0.0, 0.0, 0.0)) {
			discard;
		}

		// 3. Iluminación (Phong Básico)
		float ambientStrength = 0.2;
		vec3 ambient = ambientStrength * vec3(1.0);

		vec3 norm = normalize(v_Normal);
		vec3 lightDir = normalize(u_lightPos - v_FragPos);
		float diff = max(dot(norm, lightDir), 0.0);
		vec3 diffuse = diff * vec3(1.0); // Luz blanca

		vec3 result = (ambient + diffuse) * objectColor;
		FragColor = vec4(result, 1.0);
	}
	)";


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
    translate(T, -eye.x, -eye.y, -eye.z);

    float R[16];
    multiply(R, M, T);

    // Convertir a Mat4 antes de retornar
    Mat4 result;
    for (int i = 0; i < 16; ++i)
        result.m[i] = R[i];

    return result;
}

static inline Mat4 ortho(float left, float right, float bottom, float top, float znear, float zfar)
{
    Mat4 M;
    for (int i = 0; i < 16; ++i) M.m[i] = 0.0f;
    M.m[0] = 2.0f / (right - left);
    M.m[5] = 2.0f / (top - bottom);
    M.m[10] = -2.0f / (zfar - znear);
    M.m[12] = -(right + left) / (right - left);
    M.m[13] = -(top + bottom) / (top - bottom);
    M.m[14] = -(zfar + znear) / (zfar - znear);
    M.m[15] = 1.0f;
    return M;
}


//-----------------------------------------cubo-----------------------------------------
class Cubie {
public:
    // Matriz de transformación que representa la posición y rotación
    // actual de este cubie en el mundo.
    glm::mat4 modelMatrix;

    // Posición lógica en la cuadrícula 3x3x3 (usando -1, 0, 1).
    // Esto es inmutable después de la creación y se usa para
    // identificar qué cubies pertenecen a una cara.
    const glm::ivec3 logicalPosition;

    // Colores de las 6 caras (p.ej., +X, -X, +Y, -Y, +Z, -Z)
    // Puedes usar un enum para los índices.
    glm::vec3 faceColors[6];

public:
    Cubie(glm::ivec3 pos) : logicalPosition(pos) {
        // La matriz de modelo inicial es solo una traslación
        // a su posición lógica en la cuadrícula.
        // (Multiplica por un 'espaciado' para que no estén pegados).
        float spacing = 2.05f; // Un poco más de 2.0 para los huecos
        this->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(pos) * spacing);

        // ... aquí iría la lógica para asignar los colores
        // correctos (rojo, azul, etc.) basado en la posición lógica.
        // p.ej., si pos.z == 1 (cara frontal), faceColors[Z+] = ROJO
        // si pos.z == -1 (cara trasera), faceColors[Z-] = NARANJA
        // ...etc. Las caras internas se quedan en NEGRO.
    }

    // El Cubie en sí no se dibuja. El 'RubiksCube' lo dibujará.
};


class RubiksCube {
public:
    RubiksCube();
    ~RubiksCube();

    // Lógica (como antes)
    void rotateFace(Face face, bool clockwise);

    // --- ¡NUEVO! ---
    // Métodos de OpenGL
    void setupMesh(Shader& shader); // Configura el VAO/VBO del cubo
    void draw(Shader& shader, const glm::mat4& view, const glm::mat4& projection);

private:
    std::array<Cubie, 27> m_cubies;
    int getIndex(int x, int y, int z) const;
    void rotateSliceZ(int slice, bool clockwise);
    // (helpers para rotateSliceX y rotateSliceY)

    // ¡NUEVO! IDs de OpenGL
    GLuint m_VAO;
    GLuint m_VBO;
    
    // Espaciado entre cubies
    const float m_spacing = 1.05f; 

    // Helper para convertir nuestro Color enum a un vec3 para el shader
    glm::vec3 getVec3FromColor(Color color);
};

RubiksCube::~RubiksCube() {
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
}

// ¡NUEVO! Define los 36 vértices de UN cubo
void RubiksCube::setupMesh(Shader& shader) {
    // 3 floats para Pos, 3 para Normal, 1 para FaceID (usaremos float)
    // 7 floats por vértice. 6 vértices por cara. 6 caras.
    // 7 * 6 * 6 = 252 floats
    
    // Tamaño de un solo cubie (0.5 = medio cubo)
    float s = 0.5f; 

    // (Pos x, y, z), (Normal x, y, z), (FaceID)
    std::vector<float> vertices = {
        // Cara Derecha (+X, ID 0)
        s,  s,  s,  1.0, 0.0, 0.0, 0.0,
        s, -s,  s,  1.0, 0.0, 0.0, 0.0,
        s, -s, -s,  1.0, 0.0, 0.0, 0.0,
        s, -s, -s,  1.0, 0.0, 0.0, 0.0,
        s,  s, -s,  1.0, 0.0, 0.0, 0.0,
        s,  s,  s,  1.0, 0.0, 0.0, 0.0,

        // Cara Izquierda (-X, ID 1)
       -s,  s,  s, -1.0, 0.0, 0.0, 1.0,
       -s, -s, -s, -1.0, 0.0, 0.0, 1.0,
       -s, -s,  s, -1.0, 0.0, 0.0, 1.0,
       -s, -s, -s, -1.0, 0.0, 0.0, 1.0,
       -s,  s,  s, -1.0, 0.0, 0.0, 1.0,
       -s,  s, -s, -1.0, 0.0, 0.0, 1.0,

        // Cara Arriba (+Y, ID 2)
       -s,  s, -s,  0.0, 1.0, 0.0, 2.0,
        s,  s,  s,  0.0, 1.0, 0.0, 2.0,
        s,  s, -s,  0.0, 1.0, 0.0, 2.0,
        s,  s,  s,  0.0, 1.0, 0.0, 2.0,
       -s,  s, -s,  0.0, 1.0, 0.0, 2.0,
       -s,  s,  s,  0.0, 1.0, 0.0, 2.0,

        // Cara Abajo (-Y, ID 3)
       -s, -s, -s,  0.0, -1.0, 0.0, 3.0,
        s, -s, -s,  0.0, -1.0, 0.0, 3.0,
        s, -s,  s,  0.0, -1.0, 0.0, 3.0,
        s, -s,  s,  0.0, -1.0, 0.0, 3.0,
       -s, -s,  s,  0.0, -1.0, 0.0, 3.0,
       -s, -s, -s,  0.0, -1.0, 0.0, 3.0,

        // Cara Frontal (+Z, ID 4)
       -s, -s,  s,  0.0, 0.0, 1.0, 4.0,
        s, -s,  s,  0.0, 0.0, 1.0, 4.0,
        s,  s,  s,  0.0, 0.0, 1.0, 4.0,
        s,  s,  s,  0.0, 0.0, 1.0, 4.0,
       -s,  s,  s,  0.0, 0.0, 1.0, 4.0,
       -s, -s,  s,  0.0, 0.0, 1.0, 4.0,

        // Cara Trasera (-Z, ID 5)
       -s, -s, -s,  0.0, 0.0, -1.0, 5.0,
       -s,  s, -s,  0.0, 0.0, -1.0, 5.0,
        s,  s, -s,  0.0, 0.0, -1.0, 5.0,
        s,  s, -s,  0.0, 0.0, -1.0, 5.0,
        s, -s, -s,  0.0, 0.0, -1.0, 5.0,
       -s, -s, -s,  0.0, 0.0, -1.0, 5.0
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Stride (paso) es 7 floats (3 pos + 3 normal + 1 id)
    GLsizei stride = 7 * sizeof(float);

    // layout (location = 0) in vec3 aPos;
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    
    // layout (location = 1) in vec3 aNormal;
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // layout (location = 2) in int aFaceID;
    // ¡Importante! Usamos glVertexAttribIPointer (con 'I') porque es un entero
    // Lo guardamos como float, pero lo leemos como int.
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// ¡NUEVO! El bucle de dibujado
void RubiksCube::draw(Shader& shader, const glm::mat4& view, const glm::mat4& projection) {
    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    // (Uniforms de iluminación, puedes ponerlos una vez aquí)
    shader.setVec3("u_lightPos", glm::vec3(5.0f, 10.0f, 5.0f));
    shader.setVec3("u_viewPos", glm::vec3(0.0f, 0.0f, 10.0f)); // Asume que la cámara está en (0,0,10)

    // Prepara el mesh (VAO) que vamos a dibujar 26 veces
    glBindVertexArray(m_VAO);

    // Array temporal para guardar los 6 colores del cubie
    glm::vec3 faceColors[6];

    // Bucle 3x3x3
    for (int z = 0; z <= 2; z++) {
        for (int y = 0; y <= 2; y++) {
            for (int x = 0; x <= 2; x++) {
                
                // 1. Omitir el centro invisible
                if (x == 1 && y == 1 && z == 1) continue;

                // 2. Obtener el cubie en esta posición lógica
                Cubie& cubie = m_cubies[getIndex(x, y, z)];

                // 3. Calcular la matriz 'model' para este cubie
                // Mapeamos (0,1,2) a (-1, 0, 1) para centrar el cubo
                glm::vec3 position(
                    (x - 1.0f) * m_spacing,
                    (y - 1.0f) * m_spacing,
                    (z - 1.0f) * m_spacing
                );
                glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
                
                // (¡Aquí es donde entraría la animación!)
                // (Si este cubie está animando, multiplicaríamos 'model'
                // por una matriz de rotación temporal)
                
                shader.setMat4("model", model);

                // 4. Obtener sus 6 colores y enviarlos al shader
                faceColors[0] = getVec3FromColor(cubie.getFaceColor(Face::RIGHT));
                faceColors[1] = getVec3FromColor(cubie.getFaceColor(Face::LEFT));
                faceColors[2] = getVec3FromColor(cubie.getFaceColor(Face::UP));
                faceColors[3] = getVec3FromColor(cubie.getFaceColor(Face::DOWN));
                faceColors[4] = getVec3FromColor(cubie.getFaceColor(Face::FRONT));
                faceColors[5] = getVec3FromColor(cubie.getFaceColor(Face::BACK));

                shader.setVec3Array("u_faceColors", 6, faceColors);

                // 5. ¡Dibujar!
                glDrawArrays(GL_TRIANGLES, 0, 36); // 36 vértices
            }
        }
    }

    glBindVertexArray(0);
}

// Helper para convertir el enum en un vector RGB
glm::vec3 RubiksCube::getVec3FromColor(Color color) {
    switch (color) {
        case Color::WHITE:  return glm::vec3(1.0f, 1.0f, 1.0f);
        case Color::YELLOW: return glm::vec3(1.0f, 1.0f, 0.0f);
        case Color::RED:    return glm::vec3(1.0f, 0.0f, 0.0f);
        case Color::ORANGE: return glm::vec3(1.0f, 0.5f, 0.0f);
        case Color::GREEN:  return glm::vec3(0.0f, 1.0f, 0.0f);
        case Color::BLUE:   return glm::vec3(0.0f, 0.0f, 1.0f);
        case Color::BLACK:
        default:            return glm::vec3(0.0f, 0.0f, 0.0f);
    }
}


//--------------------------------------------teclado-----------------------------------------
void key(GLFWwindow* window, int key, int scancode, int action, int mods)
{

    // cerrar ventana
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

}


//--------------------------------------programa main------------------------------------
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
	
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
	
    // link shaders
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
	int colorLocation = glGetUniformLocation(shaderProgram, "uColor");
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
	

	//----------------declarando figuras---------------

	RubiksCube rubiksCube;
    rubiksCube.setupMesh(cubieShader); // ¡Importante!

	glLineWidth(2.0);
	glPointSize(5.0);
	
    //teclas para movimiento
	std::cout<<"Seleccione alguna opcion para el movimiento: "<<std::endl;
	std::cout<<"1 para estrella: "<<std::endl;
	std::cout<<"2 para la piramide: "<<std::endl; 		
	std::cout<<"Traslacion: A W S D"<<std::endl;
	std::cout<<"Rotacion: up down left right"<<std::endl;
	std::cout<<"Escala: U I"<<std::endl;

	
	while (!glfwWindowShouldClose(window))
    {
		
        // render
		int w,h; 
		glfwGetFramebufferSize(window,&w,&h);
		glViewport(0,0,w,h);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
		
		static double lastClear = 0.0;
		double currentTime = glfwGetTime(); 

		// limpia cada 0.2 segundos (ajusta a gusto)
		if (currentTime - lastClear > 0.2) {
			glClear(GL_COLOR_BUFFER_BIT);
			lastClear = currentTime;
		}
		
		//camara
		Vec3 target(0, 0, 0);
		Vec3 eye(0, 0, 5);
		Vec3 up(0.0f, 1.0f, 0.0f);
		Mat4 V = lookAt(eye, target, up);
		
		//con perspectiva
		//Mat4 P = perspective(90.0f, (float)w / (float)h, 0.1f, 100.0f);
		
		//sin perspectiva
		float aspect = (float)w / (float)h;
		Mat4 P = ortho(-aspect, aspect, -1.0f, 1.0f, 0.1f, 100.0f);

		
		unsigned int locV = glGetUniformLocation(shaderProgram, "view");
		unsigned int locP = glGetUniformLocation(shaderProgram, "projection");

		glUseProgram(shaderProgram);
		glUniformMatrix4fv(locV, 1, GL_FALSE, V.m);
		glUniformMatrix4fv(locP, 1, GL_FALSE, P.m);
		
		rubiksCube.draw(cubieShader, view, projection);
			
		glBindVertexArray(0);
		glfwSwapBuffers(window);
		glfwPollEvents();
		
	
    }

/*
    glDeleteVertexArrays(1, &pyramid.VAO);
    glDeleteBuffers(1, &pyramid.VBO);
    glDeleteBuffers(1, &pyramid.EBO);
	
	glDeleteVertexArrays(1, &star.VAO);
    glDeleteBuffers(1, &star.VBO);
    glDeleteBuffers(1, &star.EBO);
	
	*/
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}




