
#ifndef MATLIBRARY_H
#define MATLIBRARY_H

#include <cmath>
#include <stdexcept>
#include <cstring>

//-------------------------------------libreria-----------------------------------------
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
class Vec3 
{
public:
    float x,y,z;
    Vec3() : x(0),y(0),z(0) {}
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    Vec3 operator+(const Vec3& o) const { return {x+o.x,y+o.y,z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x,y-o.y,z-o.z}; }
    Vec3 operator*(float s) const { return {x*s,y*s,z*s}; }
};

class Mat4 
{
public:
    // Column-major 4x4 
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


#endif 