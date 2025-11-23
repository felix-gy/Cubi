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
#include <algorithm>

extern "C" {
    #include "kociemba/ckociemba/include/search.h"
}
#include <queue>
#include <string>
#include <sstream>

std::deque<std::string> g_animationQueue;

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
	layout (location = 1) in vec3 anormal;
    layout (location = 2) in int aFaceID;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    out vec3 v_FragPos;
    flat out vec3 v_Normal;
    flat out int v_FaceID;

    void main() {
        // Posición transformada
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        v_FragPos = vec3(model * vec4(aPos, 1.0));

        // Transformar normal con la matriz del modelo
        mat3 normalMatrix = mat3(transpose(inverse(model)));
        v_Normal = normalize(normalMatrix * anormal);
        v_FaceID = aFaceID;
    }
)glsl";


const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;

    flat in int v_FaceID;
    flat in vec3 v_Normal;
    in vec3 v_FragPos;

    uniform vec3 u_faceColors[6];
    uniform float u_isBorder;

    // --- Iluminación ---
    uniform vec3 u_lightPos;
    uniform vec3 u_viewPos;
    uniform vec3 u_lightColor;
    uniform float u_ambientStrength;
    uniform float u_specularStrength;

    void main() {
        if (u_isBorder > 0.5) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }

        vec3 objectColor = u_faceColors[v_FaceID];
        if (objectColor == vec3(0.0)) objectColor = vec3(0.05);

        // vectores principales
        vec3 norm = normalize(v_Normal);
        vec3 lightDir = normalize(u_lightPos - v_FragPos);
        vec3 viewDir  = normalize(u_viewPos - v_FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);

        // componentes
        float diff = max(dot(norm, lightDir), 0.0);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 256.0);

        vec3 ambient = u_ambientStrength * u_lightColor;
        vec3 diffuse = diff * u_lightColor;
        vec3 specular = u_specularStrength * spec * u_lightColor;

        // vec3 result = (ambient + diffuse + specular) * objectColor;
		vec3 ambient_diffuse_part = (ambient + diffuse) * objectColor;
		vec3 result = ambient_diffuse_part + specular; // 

        FragColor = vec4(result, 1.0);
    }
)glsl";


// --- CLASE SHADER (SIMPLIFICADA) ---
#include "shader.h"

// --- ENUMS Y CLASES DEL CUBO ---

enum class Color { WHITE, YELLOW, RED, ORANGE, GREEN, BLUE, BLACK };
enum class Face { UP, DOWN, LEFT, RIGHT, FRONT, BACK };
enum class Axis { X, Y, Z };
char colorEnumToChar(Color c) {
    switch (c) {
        case Color::WHITE:  return 'U';
        case Color::YELLOW: return 'D';
        case Color::RED:    return 'F';
        case Color::ORANGE: return 'B'; // Asumiendo Naranja (Orange) es Back
        case Color::GREEN:  return 'L'; // Asumiendo Verde (Green) es Left
        case Color::BLUE:   return 'R'; // Asumiendo Azul (Blue) es Right
        default:            return ' '; // Error
    }
}

//using FaceColors = std::map<Face, Color>;

bool g_counterClockwise = false;


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
	
	// --- NUEVOS: rotaciones de colores internas (no tocan modelMatrix) ---
    // Rotación de colores alrededor de Y: clockwise visto desde arriba
    void rotateFacesYClockwise() {
        Color oldLeft  = m_faces[Face::LEFT];
        Color oldFront = m_faces[Face::FRONT];
        Color oldRight = m_faces[Face::RIGHT];
        Color oldBack  = m_faces[Face::BACK];

        // mapping clockwise: LEFT <- FRONT, FRONT <- RIGHT, RIGHT <- BACK, BACK <- LEFT
        m_faces[Face::RIGHT] = oldFront;
        m_faces[Face::BACK]  = oldRight;
        m_faces[Face::LEFT]  = oldBack;
        m_faces[Face::FRONT] = oldLeft;
        // UP and DOWN remain unchanged
    }

    void rotateFacesYCounterClockwise() {
        // inverse of clockwise
        Color oldLeft  = m_faces[Face::LEFT];
        Color oldFront = m_faces[Face::FRONT];
        Color oldRight = m_faces[Face::RIGHT];
        Color oldBack  = m_faces[Face::BACK];

        m_faces[Face::RIGHT] = oldBack;
        m_faces[Face::BACK]  = oldLeft;
        m_faces[Face::LEFT]  = oldFront;
        m_faces[Face::FRONT] = oldRight;
    }

    void rotateFacesXClockwise() {
		Color oldUp    = m_faces[Face::UP];
		Color oldFront = m_faces[Face::FRONT];
		Color oldDown  = m_faces[Face::DOWN];
		Color oldBack  = m_faces[Face::BACK];

		// UP --> BACK --> DOWN --> FRONT --> UP
		m_faces[Face::BACK]  = oldUp;
		m_faces[Face::DOWN]  = oldBack;
		m_faces[Face::FRONT] = oldDown;
		m_faces[Face::UP]    = oldFront;
		// LEFT/RIGHT stay the same
	}

	
	void rotateFacesXCounterClockwise() {
		Color oldUp    = m_faces[Face::UP];
		Color oldFront = m_faces[Face::FRONT];
		Color oldDown  = m_faces[Face::DOWN];
		Color oldBack  = m_faces[Face::BACK];

		// UP --> FRONT --> DOWN --> BACK --> UP
		m_faces[Face::FRONT] = oldUp;
		m_faces[Face::DOWN]  = oldFront;
		m_faces[Face::BACK]  = oldDown;
		m_faces[Face::UP]    = oldBack;
		// LEFT/RIGHT stay the same
	}
	
	// ---------------- ROTACIÓN DE COLORES EN EJE Z ----------------
	void rotateFacesZClockwise() {
		Color oldUp    = m_faces[Face::UP];
		Color oldRight = m_faces[Face::RIGHT];
		Color oldDown  = m_faces[Face::DOWN];
		Color oldLeft  = m_faces[Face::LEFT];
		
		m_faces[Face::UP] = oldLeft;
		m_faces[Face::LEFT] = oldDown;
		m_faces[Face::DOWN] = oldRight;
		m_faces[Face::RIGHT] = oldUp;
	}

	void rotateFacesZCounterClockwise() {
		Color oldUp    = m_faces[Face::UP];
		Color oldRight = m_faces[Face::RIGHT];
		Color oldDown  = m_faces[Face::DOWN];
		Color oldLeft  = m_faces[Face::LEFT];
		
		m_faces[Face::UP] = oldRight;
		m_faces[Face::RIGHT] = oldDown;
		m_faces[Face::DOWN] = oldLeft;
		m_faces[Face::LEFT] = oldUp;
	}




private:
    std::map<Face, Color> m_faces;
};

// CLASE ROTATIONGROUP
class RotationGroup {
public:
    bool     m_isAnimating = false;
    float    m_currentAngle = 0.0f;
    float    m_targetAngle = 0.0f;
    float    m_animationSpeed = 4.0f; // Velocidad (4 radianes/segundo)
   
    // Array de punteros a los cubies que se mueven (Tu idea)
    std::vector<Cubie*> m_cubiesToAnimate; 
   
    Axis     m_axis;
    int      m_slice;
    bool     m_clockwise;

public:
    void start(std::vector<Cubie*>& cubies, Axis axis, int slice, bool clockwise) {
        if (m_isAnimating) return;

        m_isAnimating = true;
        m_currentAngle = 0.0f;
        m_targetAngle = clockwise ? (M_PI / 2.0f) : (-M_PI / 2.0f); // +/- 90 grados
       
        m_cubiesToAnimate = cubies;
        m_axis = axis;
        m_slice = slice;
        m_clockwise = clockwise;
    }

    bool update(float deltaTime) {
        if (!m_isAnimating) return false;


        float direction = (m_targetAngle > 0) ? 1.0f : -1.0f;
        float angle_to_rotate = direction * m_animationSpeed * deltaTime;
       
        if ( (m_targetAngle > 0 && m_currentAngle + angle_to_rotate >= m_targetAngle) ||
             (m_targetAngle < 0 && m_currentAngle + angle_to_rotate <= m_targetAngle) ) 
        {
            angle_to_rotate = m_targetAngle - m_currentAngle; 

            m_currentAngle = m_targetAngle;
            m_isAnimating = false;
            
            // Aplicar la rotaciÃ³n final a las matrices
            applyAnimationToCubies(angle_to_rotate);
            return true; 
        }
       
        m_currentAngle += angle_to_rotate;
        // Aplicar la rotaciÃ³n incremental
        applyAnimationToCubies(angle_to_rotate);
        
        return false;
    }

    // Nueva funciÃ³n para aplicar la rotaciÃ³n incremental
    void applyAnimationToCubies(float angle_step) {
        float incrementalRotation[16];
        identity(incrementalRotation);
        if (m_axis == Axis::X) rotateX(incrementalRotation, angle_step);
        else if (m_axis == Axis::Y) rotateY(incrementalRotation, angle_step);
        else if (m_axis == Axis::Z) rotateZ(incrementalRotation, angle_step);

        for (Cubie* cubie : m_cubiesToAnimate) {
            float tempModel[16];
            
            // Multiplicamos T * R 
            multiply(tempModel, cubie->modelMatrix, incrementalRotation);
            
            for(int k=0; k<16; ++k) cubie->modelMatrix[k] = tempModel[k];
        }
    }

    // Esta funciÃ³n ahora solo se usa para la lÃ³gica de finalizaciÃ³n
    void getFinalRotationMatrix(float M[16]) {
        identity(M);
        if (m_axis == Axis::X) rotateX(M, m_targetAngle);
        else if (m_axis == Axis::Y) rotateY(M, m_targetAngle);
        else if (m_axis == Axis::Z) rotateZ(M, m_targetAngle);
    }
   
    bool isCubieInGroup(Cubie* c) {
        if (!m_isAnimating) return false;
        return std::find(m_cubiesToAnimate.begin(), m_cubiesToAnimate.end(), c) != m_cubiesToAnimate.end();
    }
};

//--------------------------------CLASE RUBIKSCUBE-----------------------------------

	
class RubiksCube {
public:
	RotationGroup m_animator;

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
			Vec3 normal;
			GLint faceID; 
		};
		std::vector<Vertex> vertices = {
			// Cara DERECHA (+X)
			{{s, s, s},     {1, 0, 0}, 0}, {{s,-s, s},     {1, 0, 0}, 0}, {{s,-s,-s},    {1, 0, 0}, 0},
			{{s,-s,-s},    {1, 0, 0}, 0}, {{s, s,-s},     {1, 0, 0}, 0}, {{s, s, s},     {1, 0, 0}, 0},

			// Cara IZQUIERDA (-X)
			{{-s, s, s},   {-1, 0, 0}, 1}, {{-s,-s,-s},   {-1, 0, 0}, 1}, {{-s,-s, s},   {-1, 0, 0}, 1},
			{{-s,-s,-s},   {-1, 0, 0}, 1}, {{-s, s, s},   {-1, 0, 0}, 1}, {{-s, s,-s},   {-1, 0, 0}, 1},

			// Cara SUPERIOR (+Y)
			{{-s, s,-s},   {0, 1, 0}, 2}, {{s, s, s},     {0, 1, 0}, 2}, {{s, s,-s},     {0, 1, 0}, 2},
			{{s, s, s},     {0, 1, 0}, 2}, {{-s, s,-s},   {0, 1, 0}, 2}, {{-s, s, s},    {0, 1, 0}, 2},

			// Cara INFERIOR (-Y)
			{{-s,-s,-s},   {0,-1, 0}, 3}, {{s,-s,-s},     {0,-1, 0}, 3}, {{s,-s, s},     {0,-1, 0}, 3},
			{{s,-s, s},     {0,-1, 0}, 3}, {{-s,-s, s},   {0,-1, 0}, 3}, {{-s,-s,-s},   {0,-1, 0}, 3},

			// Cara FRONTAL (+Z)
			{{-s,-s, s},   {0, 0, 1}, 4}, {{s,-s, s},     {0, 0, 1}, 4}, {{s, s, s},     {0, 0, 1}, 4},
			{{s, s, s},     {0, 0, 1}, 4}, {{-s, s, s},   {0, 0, 1}, 4}, {{-s,-s, s},   {0, 0, 1}, 4},

			// Cara TRASERA (-Z)
			{{-s,-s,-s},   {0, 0,-1}, 5}, {{-s, s,-s},    {0, 0,-1}, 5}, {{s, s,-s},     {0, 0,-1}, 5},
			{{s, s,-s},     {0, 0,-1}, 5}, {{s,-s,-s},    {0, 0,-1}, 5}, {{-s,-s,-s},   {0, 0,-1}, 5}
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
		
        GLsizei stride = sizeof(Vertex);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, pos));
        glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));
		glEnableVertexAttribArray(1);
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
			
			// --- 1. Calcular modelMatrix final (con animación si aplica) ---
			float finalModel[16];
			for (int k = 0; k < 16; ++k)
				finalModel[k] = cubie.modelMatrix[k];

			if (m_animator.isCubieInGroup(&cubie)) {
				float R[16];
				m_animator.getFinalRotationMatrix(R);
				float temp[16];
				multiply(temp, finalModel, R);
				for (int k = 0; k < 16; ++k)
					finalModel[k] = temp[k];
			}

			// Enviamos la matriz model de cada cubie
            shader.setMat4("model", cubie.modelMatrix);
			//shader.setMat4("model", finalModel);
			// Enviamos los colores del cubie al shader
            faceColors[0] = getVec3FromColor(cubie.getFaceColor(Face::RIGHT));
            faceColors[1] = getVec3FromColor(cubie.getFaceColor(Face::LEFT));
            faceColors[2] = getVec3FromColor(cubie.getFaceColor(Face::UP));
            faceColors[3] = getVec3FromColor(cubie.getFaceColor(Face::DOWN));
            faceColors[4] = getVec3FromColor(cubie.getFaceColor(Face::FRONT));
            faceColors[5] = getVec3FromColor(cubie.getFaceColor(Face::BACK));
			shader.setVec3Array("u_faceColors", 6, faceColors);
			
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(1.0f,1.0f);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_relleno); 
			shader.setFloat("u_isBorder", 0.0f);
			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
			glDisable(GL_POLYGON_OFFSET_FILL);
			 
			glLineWidth(10.0f);
			shader.setFloat("u_isBorder", 1.0f);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_bordes); 
			glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
            //glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }
        glBindVertexArray(0);
    }
	
	void update(float deltaTime) {
		if (m_animator.update(deltaTime)) {
			// Cuando termina la animación, aplicar la rotación lógica
			switch (m_animator.m_axis) {
				case Axis::X:
					if (m_animator.m_slice == 0) rotateLeftLayerClockwise();
					else if (m_animator.m_slice == 1) rotateMiddleVerticalClockwise();
					else if (m_animator.m_slice == 2) rotateRightLayerClockwise();
					break;
				case Axis::Y:
					if (m_animator.m_slice == 0) rotateDownLayerClockwise();
					else if (m_animator.m_slice == 1) rotateMiddleLayerClockwise();
					else if (m_animator.m_slice == 2) rotateUpLayerClockwise();
					break;
				case Axis::Z:
					if (m_animator.m_slice == 0) rotateBackLayerClockwise();
					else if (m_animator.m_slice == 1) rotateMiddleDepthClockwise();
					else if (m_animator.m_slice == 2) rotateFrontLayerClockwise();
					break;
			}
		}
	}
	
	void startRotation(Axis axis, int slice, bool clockwise) {
		if (m_animator.m_isAnimating) return;

		// Invertir sentido si es capa opuesta
		if (axis == Axis::X && slice == 0) clockwise = !clockwise;
		if (axis == Axis::Y && slice == 0) clockwise = !clockwise;
		if (axis == Axis::Z && slice == 0) clockwise = !clockwise;

		std::vector<Cubie*> group;
		for (int i = 0; i < 27; ++i) {
			int x = (i % 3);
			int y = (i / 3) % 3;
			int z = (i / 9);
			if ((axis == Axis::X && x == slice) ||
				(axis == Axis::Y && y == slice) ||
				(axis == Axis::Z && z == slice)) {
				group.push_back(&m_cubies[i]);
			}
		}

		m_animator.start(group, axis, slice, clockwise);
	}

	// Rotar la capa UP (y == 2) 90° clockwise visto desde arriba
	void rotateUpLayerClockwise() {
		// Guardamos la capa en una matriz 3x3 (índices por x,z)
		std::array<Cubie, 9> layerOld;
		for (int z = 0; z < 3; ++z) {
			for (int x = 0; x < 3; ++x) {
				int idx = getIndex(x, 2, z);
				layerOld[x + z*3] = m_cubies[idx]; // copia
			}
		}

		// Creamos la nueva disposición: mapping clockwise visto desde arriba:
		// newX = z; newZ = 2 - x
		std::array<Cubie, 9> layerNew;
		for (int z = 0; z < 3; ++z) {
			for (int x = 0; x < 3; ++x) {
				int newX, newZ;

				if (g_counterClockwise) {
					// Sentido antihorario (CounterClockwise)
					newX = 2 - z;
					newZ = x;
				} else {
					// Sentido horario (Clockwise)
					newX = z;
					newZ = 2 - x;
				}

				layerNew[newX + newZ*3] = layerOld[x + z*3];
			}
		}

		// Colocamos las piezas en m_cubies con las nuevas posiciones y actualizamos modelMatrix e faces
		for (int newZ = 0; newZ < 3; ++newZ) {
			for (int newX = 0; newX < 3; ++newX) {
				int destIdx = getIndex(newX, 2, newZ);
				// Re-init la posición física en mundo (centros en x,y,z)
				layerNew[newX + newZ*3].init(newX, 2, newZ, m_spacing);
				// Rotamos la asignación de colores internamente (porque la pieza gira)
				if (g_counterClockwise)
					layerNew[newX + newZ*3].rotateFacesYCounterClockwise();
				else
					layerNew[newX + newZ*3].rotateFacesYClockwise();

				// Copiamos de vuelta
				m_cubies[destIdx] = layerNew[newX + newZ*3];
			} 
		}
	}
	
	void rotateMiddleLayerClockwise() {
		// y == 1
		std::array<Cubie, 9> layerOld;
		for (int z = 0; z < 3; ++z) {
			for (int x = 0; x < 3; ++x) {
				int idx = getIndex(x, 1, z);
				layerOld[x + z*3] = m_cubies[idx];
			}
		}

		// (x,z) -> (z, 2 - x)
		std::array<Cubie, 9> layerNew;
		for (int z = 0; z < 3; ++z) {
			for (int x = 0; x < 3; ++x) {
				int newX, newZ;

				if (g_counterClockwise) {
					// Sentido antihorario (CounterClockwise)
					newX = 2 - z;
					newZ = x;
				} else {
					// Sentido horario (Clockwise)
					newX = z;
					newZ = 2 - x;
				}

				layerNew[newX + newZ*3] = layerOld[x + z*3];
			}
		}

		// Aplicar nueva disposición
		for (int newZ = 0; newZ < 3; ++newZ) {
			for (int newX = 0; newX < 3; ++newX) {
				int destIdx = getIndex(newX, 1, newZ);
				layerNew[newX + newZ*3].init(newX, 1, newZ, m_spacing);
				if (g_counterClockwise)
					layerNew[newX + newZ*3].rotateFacesYCounterClockwise();
				else
					layerNew[newX + newZ*3].rotateFacesYClockwise();

				m_cubies[destIdx] = layerNew[newX + newZ*3];
			}
		}
	}


	// ---------------- ROTACIÓN CAPA INFERIOR (DOWN) ----------------
	void rotateDownLayerClockwise() {
		// y == 0
		std::array<Cubie, 9> layerOld;
		for (int z = 0; z < 3; ++z) {
			for (int x = 0; x < 3; ++x) {
				int idx = getIndex(x, 0, z);
				layerOld[x + z*3] = m_cubies[idx];
			}
		}

		// (x,z) -> (z, 2 - x)
		std::array<Cubie, 9> layerNew;
		for (int z = 0; z < 3; ++z) {
			for (int x = 0; x < 3; ++x) {
				int newX, newZ;

				if (g_counterClockwise) {
					// Sentido antihorario (CounterClockwise)
					newX = 2 - z;
					newZ = x;
				} else {
					// Sentido horario (Clockwise)
					newX = z;
					newZ = 2 - x;
				}

				layerNew[newX + newZ*3] = layerOld[x + z*3];
			}
		}

		for (int newZ = 0; newZ < 3; ++newZ) {
			for (int newX = 0; newX < 3; ++newX) {
				int destIdx = getIndex(newX, 0, newZ);
				layerNew[newX + newZ*3].init(newX, 0, newZ, m_spacing);
				if (g_counterClockwise)
					layerNew[newX + newZ*3].rotateFacesYCounterClockwise();
				else
					layerNew[newX + newZ*3].rotateFacesYClockwise();

				m_cubies[destIdx] = layerNew[newX + newZ*3];
			}
		}
	}
	
	// ---------------- ROTACIÓN CAPA DERECHA (x == 2) ----------------
	void rotateRightLayerClockwise() {
		std::array<Cubie, 9> layerOld;
		for (int z = 0; z < 3; ++z)
			for (int y = 0; y < 3; ++y)
				layerOld[y + z*3] = m_cubies[getIndex(2, y, z)];

		// (y,z) -> (z, 2 - y)
		std::array<Cubie, 9> layerNew;
		for (int z = 0; z < 3; ++z)
			for (int y = 0; y < 3; ++y) {
				int newY, newZ;
				if (g_counterClockwise) {
					// visto desde la derecha (−X)
					newY = 2 - z;
					newZ = y;
				} else {
					// horario visto desde la derecha (−X)
					newY = z;
					newZ = 2 - y;
				}
				layerNew[newY + newZ*3] = layerOld[y + z*3];
			}

		for (int newZ = 0; newZ < 3; ++newZ)
			for (int newY = 0; newY < 3; ++newY) {
				int destIdx = getIndex(2, newY, newZ);
				layerNew[newY + newZ*3].init(2, newY, newZ, m_spacing);
				if (g_counterClockwise)
					layerNew[newY + newZ*3].rotateFacesXCounterClockwise();
				else
					layerNew[newY + newZ*3].rotateFacesXClockwise();

				m_cubies[destIdx] = layerNew[newY + newZ*3];
			}
	}

	// ---------------- ROTACIÓN CAPA MEDIA VERTICAL (x == 1) ----------------
	void rotateMiddleVerticalClockwise() {
		std::array<Cubie, 9> layerOld;
		for (int z = 0; z < 3; ++z)
			for (int y = 0; y < 3; ++y)
				layerOld[y + z*3] = m_cubies[getIndex(1, y, z)];

		std::array<Cubie, 9> layerNew;
		for (int z = 0; z < 3; ++z)
			for (int y = 0; y < 3; ++y) {
				int newY, newZ;
				if (g_counterClockwise) {
					newY = 2 - z;
					newZ = y;
				} else {
					newY = z;
					newZ = 2 - y;
				}
				layerNew[newY + newZ*3] = layerOld[y + z*3];
			}

		for (int newZ = 0; newZ < 3; ++newZ)
			for (int newY = 0; newY < 3; ++newY) {
				int destIdx = getIndex(1, newY, newZ);
				layerNew[newY + newZ*3].init(1, newY, newZ, m_spacing);
				if (g_counterClockwise)
					layerNew[newY + newZ*3].rotateFacesXCounterClockwise();
				else
					layerNew[newY + newZ*3].rotateFacesXClockwise();

				m_cubies[destIdx] = layerNew[newY + newZ*3];
			}
	}

	// ---------------- ROTACIÓN CAPA IZQUIERDA (x == 0) ----------------
	void rotateLeftLayerClockwise() {
		std::array<Cubie, 9> layerOld;
		for (int z = 0; z < 3; ++z)
			for (int y = 0; y < 3; ++y)
				layerOld[y + z*3] = m_cubies[getIndex(0, y, z)];

		std::array<Cubie, 9> layerNew;
		for (int z = 0; z < 3; ++z)
			for (int y = 0; y < 3; ++y) {
				int newY, newZ;
				if (g_counterClockwise) {
					newY = 2 - z;
					newZ = y;
				} else {
					newY = z;
					newZ = 2 - y;
				}
				layerNew[newY + newZ*3] = layerOld[y + z*3];
			}

		for (int newZ = 0; newZ < 3; ++newZ)
			for (int newY = 0; newY < 3; ++newY) {
				int destIdx = getIndex(0, newY, newZ);
				layerNew[newY + newZ*3].init(0, newY, newZ, m_spacing);
				if (g_counterClockwise)
					layerNew[newY + newZ*3].rotateFacesXCounterClockwise();
				else
					layerNew[newY + newZ*3].rotateFacesXClockwise();

				m_cubies[destIdx] = layerNew[newY + newZ*3];
			}
	}
	
	// ---------------- ROTACIÓN CAPA FRONTAL (z == 2) ----------------
	void rotateFrontLayerClockwise() {
		std::array<Cubie, 9> layerOld;
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x)
				layerOld[x + y * 3] = m_cubies[getIndex(x, y, 2)];

		std::array<Cubie, 9> layerNew;
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x) {
				int newX, newY;
				if (g_counterClockwise) {
					newX = 2 - y;
					newY = x;
				} else {
					newX = y;
					newY = 2 - x;
				}
				layerNew[newX + newY * 3] = layerOld[x + y * 3];
			}

		for (int newY = 0; newY < 3; ++newY)
			for (int newX = 0; newX < 3; ++newX) {
				int destIdx = getIndex(newX, newY, 2);
				layerNew[newX + newY * 3].init(newX, newY, 2, m_spacing);
				if (g_counterClockwise)
					layerNew[newX + newY * 3].rotateFacesZCounterClockwise();
				else
					layerNew[newX + newY * 3].rotateFacesZClockwise();

				m_cubies[destIdx] = layerNew[newX + newY * 3];
			}
	}

	// ---------------- ROTACIÓN CAPA MEDIA PROFUNDIDAD (z == 1) ----------------
	void rotateMiddleDepthClockwise() {
		std::array<Cubie, 9> layerOld;
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x)
				layerOld[x + y * 3] = m_cubies[getIndex(x, y, 1)];

		std::array<Cubie, 9> layerNew;
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x) {
				int newX, newY;
				if (g_counterClockwise) {
					newX = 2 - y;
					newY = x;
				} else {
					newX = y;
					newY = 2 - x;
				}
				layerNew[newX + newY * 3] = layerOld[x + y * 3];
			}

		for (int newY = 0; newY < 3; ++newY)
			for (int newX = 0; newX < 3; ++newX) {
				int destIdx = getIndex(newX, newY, 1);
				layerNew[newX + newY * 3].init(newX, newY, 1, m_spacing);
				if (g_counterClockwise)
					layerNew[newX + newY * 3].rotateFacesZCounterClockwise();
				else
					layerNew[newX + newY * 3].rotateFacesZClockwise();

				m_cubies[destIdx] = layerNew[newX + newY * 3];
			}
	}
	
	// ---------------- ROTACIÓN CAPA TRASERA (z == 0) ----------------
	void rotateBackLayerClockwise() {
		std::array<Cubie, 9> layerOld;
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x)
				layerOld[x + y * 3] = m_cubies[getIndex(x, y, 0)];

		std::array<Cubie, 9> layerNew;
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 3; ++x) {
				int newX, newY;
				if (g_counterClockwise) {
					// Ojo: visto desde atrás, el sentido se invierte
					newX = 2 - y;
					newY = x;
				} else {
					newX = y;
					newY = 2 - x;
				}
				layerNew[newX + newY * 3] = layerOld[x + y * 3];
			}

		for (int newY = 0; newY < 3; ++newY)
			for (int newX = 0; newX < 3; ++newX) {
				int destIdx = getIndex(newX, newY, 0);
				layerNew[newX + newY * 3].init(newX, newY, 0, m_spacing);
				if (g_counterClockwise)
					layerNew[newX + newY * 3].rotateFacesZCounterClockwise();
				else
					layerNew[newX + newY * 3].rotateFacesZClockwise();

				m_cubies[destIdx] = layerNew[newX + newY * 3];
			}
	}
	std::string getFaceletString() {
		char facelets[55]; // 54 + 1 para el '\0'
			int idx = 0;


			// PRIMERO: Determinar el mapeo dinámico basado en los centros
			std::map<Color, char> colorToChar;
			
			// Los centros definen la correspondencia
			colorToChar[getCubieAt(1, 2, 1).getFaceColor(Face::UP)]    = 'U';  // Centro U
			colorToChar[getCubieAt(1, 0, 1).getFaceColor(Face::DOWN)]  = 'D';  // Centro D  
			colorToChar[getCubieAt(1, 1, 2).getFaceColor(Face::FRONT)] = 'F';  // Centro F
			colorToChar[getCubieAt(1, 1, 0).getFaceColor(Face::BACK)]  = 'B';  // Centro B
			colorToChar[getCubieAt(0, 1, 1).getFaceColor(Face::LEFT)]  = 'L';  // Centro L
			colorToChar[getCubieAt(2, 1, 1).getFaceColor(Face::RIGHT)] = 'R';  // Centro R
			// --- Cara U (Up, y=2) ---
			// Correcto: Lee U1..U9 (z=0, z=1, z=2)
			for (int z = 0; z <= 2; z++) { // z=0 (Back), z=1, z=2 (Front)
				for (int x = 0; x <= 2; x++) { // x=0 (Left), x=1, x=2 (Right)
					//facelets[idx++] = colorEnumToChar(getCubieAt(x, 2, z).getFaceColor(Face::UP));
					facelets[idx++] = colorToChar[getCubieAt(x, 2, z).getFaceColor(Face::UP)];

				}
			}

			// --- Cara R (Right, x=2) ---
			// CORREGIDO: Lee R1..R9 (z=2, z=1, z=0)
			for (int y = 2; y >= 0; y--) { 
				for (int z = 2; z >= 0; z--) { // ¡CAMBIADO! de 0..2 a 2..0
					//facelets[idx++] = colorEnumToChar(getCubieAt(2, y, z).getFaceColor(Face::RIGHT));
					facelets[idx++] = colorToChar[getCubieAt(2, y, z).getFaceColor(Face::RIGHT)];
				}
			}

			// --- Cara F (Front, z=2) ---
			// Correcto: Lee F1..F9 (y=2, y=1, y=0)
			for (int y = 2; y >= 0; y--) {
				for (int x = 0; x <= 2; x++) {
					//facelets[idx++] = colorEnumToChar(getCubieAt(x, y, 2).getFaceColor(Face::FRONT));
					facelets[idx++] = colorToChar[getCubieAt(x, y, 2).getFaceColor(Face::FRONT)];
				}
			}

			// --- Cara D (Down, y=0) ---
			// Correcto: Lee D1..D9 (z=2, z=1, z=0)
			for (int z = 2; z >= 0; z--) {
				for (int x = 0; x <= 2; x++) {
					//facelets[idx++] = colorEnumToChar(getCubieAt(x, 0, z).getFaceColor(Face::DOWN));
					facelets[idx++] = colorToChar[getCubieAt(x, 0, z).getFaceColor(Face::DOWN)];
				}
			}

			// --- Cara L (Left, x=0) ---
			// CORREGIDO: Lee L1..L9 (z=0, z=1, z=2)
			for (int y = 2; y >= 0; y--) {
				for (int z = 0; z <= 2; z++) { // ¡CAMBIADO! de 2..0 a 0..2
					//facelets[idx++] = colorEnumToChar(getCubieAt(0, y, z).getFaceColor(Face::LEFT));
					facelets[idx++] = colorToChar[getCubieAt(0, y, z).getFaceColor(Face::LEFT)];
				}
			}

			// --- Cara B (Back, z=0) ---
			// Correcto: Lee B1..B9 (x=2, x=1, x=0)
			for (int y = 2; y >= 0; y--) {
				for (int x = 2; x >= 0; x--) {
					//facelets[idx++] = colorEnumToChar(getCubieAt(x, y, 0).getFaceColor(Face::BACK));
					facelets[idx++] = colorToChar[getCubieAt(x, y, 0).getFaceColor(Face::BACK)];

				}
			}

			facelets[54] = '\0';
			return std::string(facelets);
	}
	
private:
    std::array<Cubie, 27> m_cubies;
    GLuint m_VAO, m_VBO;
	GLuint m_EBO_relleno; // EBO para 36 índices (triángulos)
	GLuint m_EBO_bordes;
    const float m_spacing = 1.0f;

    int getIndex(int x, int y, int z) const { 
		return x + y * 3 + z * 9; 
	}
	Cubie& getCubieAt(int x, int y, int z) {
		return m_cubies[getIndex(x, y, z)];
	}

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



// ----SOLVER---
void resolverCubo(RubiksCube* g_rubiksCube) {
    if (!g_animationQueue.empty()) return; // Ya hay una solución en cola
    //if (g_rubiksCube->m_animator.m_isAnimating) return; // Espera a que termine el movimiento actual

    // 1. TRADUCIR
    std::string s = g_rubiksCube->getFaceletString();
	std::cout << "Facelets: " << s << std::endl;

	char facelets_cstr[55];
	snprintf(facelets_cstr, sizeof(facelets_cstr), "%s", s.c_str());


    // 2. RESOLVER
    // (Asegúrate de que la ruta "kociemba/cprunetables" sea correcta desde donde ejecutas el .exe)
	char* sol = solution(facelets_cstr, 21, 45000, 0, "kociemba/cprunetables");

    if (sol && strncmp(sol, "Error", 5) != 0) {
        std::cout << "Solucion encontrada: " << sol << std::endl;
        
        // 3. PARSEAR Y ENCOLAR
        std::stringstream ss(sol);
		//std::cout <<"s: " << ss << std::endl;
        std::string move;
        while (ss >> move) { // Lee la solución palabra por palabra (ej. "R", "U2", "F'")
            g_animationQueue.push_back(move);
        }
    } else {
        std::cout << "Error del solver: " << (sol ? sol : "Desconocido") << std::endl;
    }

    if (sol) {
        free(sol); // ¡MUY IMPORTANTE! Libera la memoria que el C asignó.
    }
}
//-------------------variables globales-----------------------
RubiksCube* g_rubiksCube = nullptr;
bool keyProcessed[348] = {false};
Vec3 g_cameraPos(0.0f, 0.0f, 5.0f);

enum class ActiveFace { FRONT = 1, BACK, LEFT, RIGHT, UP, DOWN };

ActiveFace g_activeFace = ActiveFace::FRONT;

bool adjustClockwiseForView(Axis axis, int slice, bool clockwise, ActiveFace view) {
    bool visualClockwise = clockwise;

    switch (axis) {
        case Axis::X:
            if (view == ActiveFace::LEFT) visualClockwise = !clockwise;
            break;
        case Axis::Y:
            if (view == ActiveFace::FRONT || view == ActiveFace::RIGHT || view == ActiveFace::LEFT)
                visualClockwise = !clockwise;
            break;
    }

    if (slice == 0) visualClockwise = !visualClockwise;

    return visualClockwise;
}

void rotateFromActiveFace(int key) {
    if (!g_rubiksCube) return;

    bool userClockwise = !g_counterClockwise;

    auto rot = [&](Axis axis, int slice) {
        bool adjusted = adjustClockwiseForView(axis, slice, userClockwise, g_activeFace);
        g_rubiksCube->startRotation(axis, slice, adjusted);
    };

    switch (g_activeFace) {
        case ActiveFace::FRONT:
            if (key == GLFW_KEY_U) rot(Axis::Y, 2);
            if (key == GLFW_KEY_M) rot(Axis::Y, 1);
            if (key == GLFW_KEY_D) rot(Axis::Y, 0);
            if (key == GLFW_KEY_L) rot(Axis::X, 0);
            if (key == GLFW_KEY_V) rot(Axis::X, 1);
            if (key == GLFW_KEY_R) rot(Axis::X, 2);
            break;

        case ActiveFace::BACK:
            if (key == GLFW_KEY_U) rot(Axis::Y, 2);
            if (key == GLFW_KEY_M) rot(Axis::Y, 1);
            if (key == GLFW_KEY_D) rot(Axis::Y, 0);
            if (key == GLFW_KEY_L) rot(Axis::X, 2);
            if (key == GLFW_KEY_V) rot(Axis::X, 1);
            if (key == GLFW_KEY_R) rot(Axis::X, 0);
            break;

        case ActiveFace::LEFT:
            if (key == GLFW_KEY_U) rot(Axis::Y, 2);
            if (key == GLFW_KEY_M) rot(Axis::Y, 1);
            if (key == GLFW_KEY_D) rot(Axis::Y, 0);
            if (key == GLFW_KEY_L) rot(Axis::Z, 0);
            if (key == GLFW_KEY_V) rot(Axis::Z, 1);
            if (key == GLFW_KEY_R) rot(Axis::Z, 2);
            break;

        case ActiveFace::RIGHT:
            if (key == GLFW_KEY_U) rot(Axis::Y, 2);
            if (key == GLFW_KEY_M) rot(Axis::Y, 1);
            if (key == GLFW_KEY_D) rot(Axis::Y, 0);
            if (key == GLFW_KEY_L) rot(Axis::Z, 2);
            if (key == GLFW_KEY_V) rot(Axis::Z, 1);
            if (key == GLFW_KEY_R) rot(Axis::Z, 0);
            break;

        case ActiveFace::UP:
            if (key == GLFW_KEY_U) rot(Axis::Z, 0);
            if (key == GLFW_KEY_M) rot(Axis::Z, 1);
            if (key == GLFW_KEY_D) rot(Axis::Z, 2);
            if (key == GLFW_KEY_L) rot(Axis::X, 0);
            if (key == GLFW_KEY_V) rot(Axis::X, 1);
            if (key == GLFW_KEY_R) rot(Axis::X, 2);
            break;

        case ActiveFace::DOWN:
            if (key == GLFW_KEY_U) rot(Axis::Z, 2);
            if (key == GLFW_KEY_M) rot(Axis::Z, 1);
            if (key == GLFW_KEY_D) rot(Axis::Z, 0);
            if (key == GLFW_KEY_L) rot(Axis::X, 0);
            if (key == GLFW_KEY_V) rot(Axis::X, 1);
            if (key == GLFW_KEY_R) rot(Axis::X, 2);
            break;
    }
}


void applyMoveAsKeyCallback(const std::string& mov) {

    char m = mov[0];
    bool prime = (mov.size() == 2 && mov[1] == '\'');

    switch (m) {

        case 'U':
            g_activeFace = ActiveFace::FRONT;
            g_counterClockwise = !prime;
            rotateFromActiveFace(GLFW_KEY_U);
            break;

        case 'D':
            g_activeFace = ActiveFace::FRONT;
            g_counterClockwise = prime;
            rotateFromActiveFace(GLFW_KEY_D);
            break;

        case 'L':
            g_activeFace = ActiveFace::FRONT;
            g_counterClockwise = !prime;
            rotateFromActiveFace(GLFW_KEY_L);
            break;

        case 'R':
            g_activeFace = ActiveFace::FRONT;
            g_counterClockwise = prime;
            rotateFromActiveFace(GLFW_KEY_R);
            break;

        case 'F':
            g_activeFace = ActiveFace::UP;
            g_counterClockwise = prime;
            rotateFromActiveFace(GLFW_KEY_D);
            break;

        case 'B':
            g_activeFace = ActiveFace::UP;
            g_counterClockwise = !prime;
            rotateFromActiveFace(GLFW_KEY_U);
            break;
    }
}



void processAnimationQueue() {

    if (!g_rubiksCube) return;

    // si está animando, NO avanzar
    if (g_rubiksCube->m_animator.m_isAnimating)
        return;

    if (g_animationQueue.empty())
        return;

    // Leo el siguiente movimiento
    std::string mov = g_animationQueue.front();
    g_animationQueue.pop_front();

    // Si es movimiento doble --> R2, F2, U2...
    if (mov.size() == 2 && mov[1] == '2') {

        std::string m1(1, mov[0]);
        std::string m2(1, mov[0]);

        // Primero se ejecuta m1 (ahora)
        // Y m2 se inserta al frente para que sea el siguiente
        g_animationQueue.push_front(m2);

        applyMoveAsKeyCallback(m1);
        return;
    }

    // Movimiento simple
    applyMoveAsKeyCallback(mov);
}




	float yaw   = -90.0f;   // mira hacia -Z
	float pitch = 0.0f;
	Vec3 cameraFront(0.0f, 0.0f, -1.0f);
	Vec3 cameraUp(0.0f, 1.0f, 0.0f);


//--------------------CALLBACKS ------------------------------

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (g_rubiksCube == nullptr) return;

    // --- Lógica de Cámara (Responde a PRESIONAR y REPETIR) ---
    float cameraSpeed = 0.1f;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) {
			
			case GLFW_KEY_F: std::cout <<g_rubiksCube->getFaceletString()<< std::endl; break;
			case GLFW_KEY_X: resolverCubo(g_rubiksCube); break;

            case GLFW_KEY_W:
				g_cameraPos = g_cameraPos + cameraFront * cameraSpeed;
				break;

			case GLFW_KEY_S:
				g_cameraPos = g_cameraPos - cameraFront * cameraSpeed;
				break;

			case GLFW_KEY_LEFT:
				g_cameraPos = g_cameraPos - normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
				break;

			case GLFW_KEY_RIGHT:
				g_cameraPos = g_cameraPos + normalize(cross(cameraFront, cameraUp)) * cameraSpeed;
				break;

			// --- Pitch (flecha arriba / abajo) ---
			case GLFW_KEY_UP:
				pitch += 2.0f;
				if (pitch > 89.0f) pitch = 89.0f;
				break;

			case GLFW_KEY_DOWN:
				pitch -= 2.0f;
				if (pitch < -89.0f) pitch = -89.0f;
				break;

			case GLFW_KEY_P:
				yaw -= 2.0f;
				break;

			case GLFW_KEY_I:
				yaw += 2.0f;
				break;

			case GLFW_KEY_C:
				g_counterClockwise = !g_counterClockwise;
				std::cout << "Modo de rotacion cambiado a: "
						  << (g_counterClockwise ? "CounterClockwise" : "Clockwise")
						  << std::endl;
				break;
					

			// ---------------- SELECCIÓN DE CARA ACTIVA ----------------
            case GLFW_KEY_1: g_activeFace = ActiveFace::FRONT; std::cout << "Cara activa: FRONT\n"; break;
            case GLFW_KEY_2: g_activeFace = ActiveFace::BACK;  std::cout << "Cara activa: BACK\n"; break;
            case GLFW_KEY_3: g_activeFace = ActiveFace::LEFT;  std::cout << "Cara activa: LEFT\n"; break;
            case GLFW_KEY_4: g_activeFace = ActiveFace::RIGHT; std::cout << "Cara activa: RIGHT\n"; break;
            case GLFW_KEY_5: g_activeFace = ActiveFace::UP;    std::cout << "Cara activa: UP\n"; break;
            case GLFW_KEY_6: g_activeFace = ActiveFace::DOWN;  std::cout << "Cara activa: DOWN\n"; break;

            // ---------------- ROTACIONES DEPENDIENDO DE LA CARA ACTIVA ----------------
            case GLFW_KEY_U:
            case GLFW_KEY_M:
            case GLFW_KEY_D:
            case GLFW_KEY_L:
            case GLFW_KEY_V:
            case GLFW_KEY_R:
                rotateFromActiveFace(key);
                break;

        }
		
		/// Convertir grados a radianes (sin usar radians())
		float radYaw   = yaw   * 3.1415926535f / 180.0f;
		float radPitch = pitch * 3.1415926535f / 180.0f;

		// Recalcular la dirección
		Vec3 dir;
		dir.x = cos(radYaw) * cos(radPitch);
		dir.y = sin(radPitch);
		dir.z = sin(radYaw) * cos(radPitch);

		cameraFront = normalize(dir);
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

	double lastTime = glfwGetTime();
	cubieShader.use();
	cubieShader.setVec3("u_lightPos", Vec3(0.0f, 0.75f, 5.0f));  // posicion de luz
	cubieShader.setVec3("u_lightColor", Vec3(1.0f, 1.0f, 1.0f)); // luz blanca
	cubieShader.setFloat("u_ambientStrength", 0.4f);
	cubieShader.setFloat("u_specularStrength", 10.1f);
	
	std::cout << "\n\n===================== CONTROLES DEL CUBO =====================\n";

	std::cout << "\n--- SELECCIONAR CARA ACTIVA ---\n";
	std::cout << "  1 --> FRONT  (frontal)\n";
	std::cout << "  2 --> BACK   (posterior)\n";
	std::cout << "  3 --> LEFT   (izquierda)\n";
	std::cout << "  4 --> RIGHT  (derecha)\n";
	std::cout << "  5 --> UP     (superior)\n";
	std::cout << "  6 --> DOWN   (inferior)\n";

	std::cout << "\n--- REALIZAR MOVIMIENTOS DE LA CARA ACTIVA ---\n";
	std::cout << "  U --> Girar capa superior de la cara activa\n";
	std::cout << "  M --> Girar capa del medio\n";
	std::cout << "  D --> Girar capa inferior\n";
	std::cout << "  L --> Girar capa izquierda\n";
	std::cout << "  V --> Girar capa central vertical\n";
	std::cout << "  R --> Girar capa derecha\n";

	std::cout << "\n--- SENTIDO DEL GIRO (Clockwise / CounterClockwise) ---\n";
	std::cout << "  C --> Alterna entre Clockwise y CounterClockwise\n";

	std::cout << "\n--- USO DEL SOLVER ---\n";
	std::cout << "  X --> Lanza el solver Kociemba y ejecuta los movimientos\n";
	std::cout << "  F --> Imprime el facelet string (estado del cubo)\n";

	std::cout << "\n===================== CONTROLES DE LA CAMARA =====================\n";

	std::cout << "\n--- Movimiento FPS ---\n";
	std::cout << "  W --> Avanza segun la direccion de la camara\n";
	std::cout << "  S --> Retrocede\n";
	std::cout << "  Flecha left (<-) izquierda\n";
	std::cout << "  Flecha right (->) derecha\n";

	std::cout << "\n--- Rotacion de la camara ---\n";
	std::cout << "  Flecha (up) arriba\n";
	std::cout << "  Flecha (down) abajo\n";
	std::cout << "  I --> izquierda (giro horizontal)\n";
	std::cout << "  P --> derecha (giro horizontal)\n";

	std::cout << "\n===============================================================\n\n";

	
    while (!glfwWindowShouldClose(window)) {
		// --- Calculo de DeltaTime ---
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;
		
        glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
        //Vec3 eye = g_cameraPos;
        //Vec3 center(g_cameraPos.x, g_cameraPos.y, g_cameraPos.z - 1.0f); 
  
        Vec3 up(0.0f, 1.0f, 0.0f);

        //Mat4 view = lookAt(eye, center, up);
		Mat4 view = lookAt(g_cameraPos, g_cameraPos + cameraFront, cameraUp);

        Mat4 proj = perspective(45.0f, (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		
		rubiksCube.update(deltaTime);
		
        cubieShader.use();       
		cubieShader.setMat4("projection", proj.m);
        cubieShader.setMat4("view", view.m);

		cubieShader.setVec3("u_viewPos", g_cameraPos);
		processAnimationQueue();

        rubiksCube.draw(cubieShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}