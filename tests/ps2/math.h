#ifndef MATH_H
#define MATH_H
#include "ps2.h"

typedef float Matrix[16];
typedef float Vector2f[2];
typedef float Vector3f[3];
typedef float Vector4f[4];
typedef uint8 Vector4b[4];

extern Matrix mathModelViewMat;
extern Matrix mathNormalMat;
extern Matrix mathProjectionMat;

void matDump(Matrix m);
void matMakeIdentity(Matrix m);
void matCopy(Matrix m1, Matrix m2);
void matMult(Matrix m1, Matrix m2);
void matMultVec(Matrix m, Vector4f v);
void vec4fCopy(Vector4f v1, Vector4f v2);
float vec3fDot(Vector3f v1, Vector3f v2);
void vec3fNormalize(Vector3f v);
void matRotateX(Matrix m, float ang);
void matRotateY(Matrix m, float ang);
void matRotateZ(Matrix m, float ang);
void matFrustum2(Matrix m, float l, float r, float xl, float xr,
                 float b, float t, float yb, float yt,
                 float n, float f, float zn, float zf);
void matFrustum(Matrix m, float l, float r, float b, float t,
                float n, float f);
void matPerspective2(Matrix m, float fov, float ratio, float width, float height,
                     float n, float f, float znear, float zfar);
void matPerspective(Matrix m, float fov, float ratio, float n, float f);
void matTranslate(Matrix m, float x, float y, float z);
void matInverse(Matrix m1, Matrix m2);

#endif
