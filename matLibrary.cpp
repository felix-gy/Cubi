#include "matLibrary.h"

// Definición de constantes
const float PI = 3.14159265359f;

// --- Implementación Vec3 ---

Vec3::Vec3() : x(0), y(0), z(0) {}
Vec3::Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

Vec3 Vec3::operator+(const Vec3& r) const { return Vec3(x + r.x, y + r.y, z + r.z); }
Vec3 Vec3::operator-(const Vec3& r) const { return Vec3(x - r.x, y - r.y, z - r.z); }
Vec3 Vec3::operator*(float s) const { return Vec3(x * s, y * s, z * s); }

Vec3 Vec3::normalize() const {
    float len = std::sqrt(x*x + y*y + z*z);
    if(len == 0) return *this;
    return Vec3(x/len, y/len, z/len);
}

float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

// --- Implementación Mat4 ---

Mat4::Mat4() {
    for(int i=0; i<4; i++)
        for(int j=0; j<4; j++) m[i][j] = (i==j) ? 1.0f : 0.0f;
}

float* Mat4::value_ptr() { return &m[0][0]; }
const float* Mat4::value_ptr() const { return &m[0][0]; }

Mat4 Mat4::operator*(const Mat4& r) const {
    Mat4 res;
    for(int col=0; col<4; col++) {
        for(int row=0; row<4; row++) {
            res.m[col][row] = 0;
            for(int k=0; k<4; k++)
                res.m[col][row] += m[k][row] * r.m[col][k];
        }
    }
    return res;
}

// --- Implementación Transformaciones ---

float toRadians(float deg) { return deg * PI / 180.0f; }

Mat4 translate(const Mat4& in, const Vec3& v) {
    Mat4 res = in;
    res.m[3][0] = in.m[0][0] * v.x + in.m[1][0] * v.y + in.m[2][0] * v.z + in.m[3][0];
    res.m[3][1] = in.m[0][1] * v.x + in.m[1][1] * v.y + in.m[2][1] * v.z + in.m[3][1];
    res.m[3][2] = in.m[0][2] * v.x + in.m[1][2] * v.y + in.m[2][2] * v.z + in.m[3][2];
    res.m[3][3] = in.m[0][3] * v.x + in.m[1][3] * v.y + in.m[2][3] * v.z + in.m[3][3];
    return res;
}

Mat4 scale(const Mat4& in, const Vec3& v) {
    Mat4 res = in;
    for(int i=0; i<3; i++) {
        res.m[0][i] *= v.x;
        res.m[1][i] *= v.y;
        res.m[2][i] *= v.z;
    }
    return res;
}

Mat4 rotate(const Mat4& in, float angle, const Vec3& axis) {
    Mat4 res;
    float c = std::cos(angle);
    float s = std::sin(angle);
    Vec3 a = axis.normalize();
    Vec3 temp = a * (1.0f - c);

    Mat4 rot;
    rot.m[0][0] = c + temp.x * a.x;
    rot.m[0][1] = temp.x * a.y + s * a.z;
    rot.m[0][2] = temp.x * a.z - s * a.y;

    rot.m[1][0] = temp.y * a.x - s * a.z;
    rot.m[1][1] = c + temp.y * a.y;
    rot.m[1][2] = temp.y * a.z + s * a.x;

    rot.m[2][0] = temp.z * a.x + s * a.y;
    rot.m[2][1] = temp.z * a.y - s * a.x;
    rot.m[2][2] = c + temp.z * a.z;

    return in * rot; 
}

Mat4 perspective(float fov, float aspect, float near, float far) {
    Mat4 res;
    // Reset to zero (ya que el constructor hace identidad)
    for(int i=0; i<4; i++) for(int j=0; j<4; j++) res.m[i][j] = 0;

    float tanHalfFov = std::tan(fov / 2.0f);
    res.m[0][0] = 1.0f / (aspect * tanHalfFov);
    res.m[1][1] = 1.0f / tanHalfFov;
    res.m[2][2] = -(far + near) / (far - near);
    res.m[2][3] = -1.0f;
    res.m[3][2] = -(2.0f * far * near) / (far - near);
    return res;
}

Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = (center - eye).normalize();
    Vec3 s = cross(f, up).normalize();
    Vec3 u = cross(s, f);

    Mat4 res;
    res.m[0][0] = s.x; res.m[1][0] = s.y; res.m[2][0] = s.z;
    res.m[0][1] = u.x; res.m[1][1] = u.y; res.m[2][1] = u.z;
    res.m[0][2] =-f.x; res.m[1][2] =-f.y; res.m[2][2] =-f.z;
    res.m[3][0] = -dot(s, eye);
    res.m[3][1] = -dot(u, eye);
    res.m[3][2] = dot(f, eye);
    return res;
}

// --- Implementación Animación / Utilidades ---

Vec3 lerp(const Vec3& start, const Vec3& end, float t) {
    return start + (end - start) * t;
}

float lerp(float start, float end, float t) {
    return start + (end - start) * t;
}

Vec3 oscillate(float time, float speed, float amplitude, const Vec3& direction) {
    return direction * (std::sin(time * speed) * amplitude);
}