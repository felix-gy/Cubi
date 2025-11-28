#ifndef MATLIBRARY_H
#define MATLIBRARY_H

#include <cmath>
#include <iostream>
#include <algorithm>

// Declaración de constantes
extern const float PI;

// --- Estructuras Básicas ---

struct Vec3 {
    float x, y, z;

    Vec3();
    Vec3(float _x, float _y, float _z);

    // Operadores
    Vec3 operator+(const Vec3& r) const;
    Vec3 operator-(const Vec3& r) const;
    Vec3 operator*(float s) const;
    
    // Normalización
    Vec3 normalize() const;
};

// Funciones utilitarias de vectores
float dot(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);

struct Mat4 {
    float m[4][4]; // Column-major para OpenGL

    Mat4(); // Constructor identidad

    float* value_ptr();
    const float* value_ptr() const;

    // Multiplicación Matriz-Matriz
    Mat4 operator*(const Mat4& r) const;
};

// --- Transformaciones ---

float toRadians(float deg);
Mat4 translate(const Mat4& in, const Vec3& v);
Mat4 scale(const Mat4& in, const Vec3& v);
Mat4 rotate(const Mat4& in, float angle, const Vec3& axis);
Mat4 perspective(float fov, float aspect, float near, float far);
Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up);

// --- Animación / Utilidades ---

Vec3 lerp(const Vec3& start, const Vec3& end, float t);
float lerp(float start, float end, float t);
Vec3 oscillate(float time, float speed, float amplitude, const Vec3& direction);

#endif