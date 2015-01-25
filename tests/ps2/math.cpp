#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "math.h"

Matrix mathModelViewMat;
Matrix mathNormalMat;
Matrix mathProjectionMat;

void
matDump(Matrix m)
{
	int i, j;
#define M(i,j) m[i*4+j]
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			printf("%f ", M(j,i));
		printf("\n");
	}
#undef M
	printf("\n");
}

void
matMakeIdentity(Matrix m)
{
	int i, j;
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			m[i*4+j] = (i == j) ? 1.0f : 0.0f;
}

void
matCopy(Matrix m1, Matrix m2)
{
	int i;
	for (i = 0; i < 16; i++)
		m1[i] = m2[i];
}

void
matMult(Matrix m1, Matrix m2)
{
	int i, j;
	Matrix newmat;
#define M1(i,j) m1[i*4+j]
#define M2(i,j) m2[i*4+j]
#define NEW(i,j) newmat[i*4+j]
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			NEW(i,j) = M1(0,j)*M2(i,0)
			         + M1(1,j)*M2(i,1)
			         + M1(2,j)*M2(i,2)
			         + M1(3,j)*M2(i,3);
#undef M1
#undef M2
#undef NEW
	matCopy(m1, newmat);
}

void
matMultVec(Matrix m, Vector4f v)
{
	int i;
	Vector4f newmat;
#define M(i,j) m[i*4+j]
	for (i = 0; i < 4; i++)
		newmat[i] = M(0,i)*v[0]
		          + M(1,i)*v[1]
		          + M(2,i)*v[2]
		          + M(3,i)*v[3];
#undef M
	vec4fCopy(v, newmat);
}

void
matInverse(Matrix m1, Matrix m2)
{
	float det;

#define N(i,j) m1[i*4+j]
#define M(i,j) m2[i*4+j]
	det = M(0,0)*(M(2,2)*M(1,1) - M(1,2)*M(2,1))
	    - M(0,1)*(M(2,2)*M(1,0) - M(1,2)*M(2,0))
	    + M(0,2)*(M(2,1)*M(1,0) - M(1,1)*M(2,0));
	matMakeIdentity(m1);
	N(0,0) = (M(2,2)*M(1,1) - M(1,2)*M(2,1))/det;
	N(0,1) = (M(1,2)*M(2,0) - M(2,2)*M(1,0))/det;
	N(0,2) = (M(2,1)*M(1,0) - M(1,1)*M(2,0))/det;
	N(1,0) = (M(0,2)*M(2,1) - M(2,2)*M(0,1))/det;
	N(1,1) = (M(2,2)*M(0,0) - M(0,2)*M(2,0))/det;
	N(1,2) = (M(0,1)*M(2,0) - M(2,1)*M(0,0))/det;
	N(2,0) = (M(1,2)*M(0,1) - M(0,2)*M(1,1))/det;
	N(2,1) = (M(0,2)*M(1,0) - M(1,2)*M(0,0))/det;
	N(2,2) = (M(1,1)*M(0,0) - M(0,1)*M(1,0))/det;
#undef M
#undef N
}

void
vec4fCopy(Vector4f v1, Vector4f v2)
{
	v1[0] = v2[0];
	v1[1] = v2[1];
	v1[2] = v2[2];
	v1[3] = v2[3];
}

float
vec3fDot(Vector3f v1, Vector3f v2)
{
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void
vec3fNormalize(Vector3f v)
{
	float d;

	d = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
	v[0] /= d;
	v[1] /= d;
	v[2] /= d;
}

void
matRotateX(Matrix m, float ang)
{
	Matrix rot;
	matMakeIdentity(rot);
#define M(i,j) rot[i*4+j]
	M(1,1) = cosf(ang);
	M(1,2) = sinf(ang);
	M(2,1) = -sinf(ang);
	M(2,2) = cosf(ang);
#undef M
	matMult(m, rot);
}

void
matRotateY(Matrix m, float ang)
{
	Matrix rot;
	matMakeIdentity(rot);
#define M(i,j) rot[i*4+j]
	M(0,0) = cosf(ang);
	M(2,0) = sinf(ang);
	M(0,2) = -sinf(ang);
	M(2,2) = cosf(ang);
#undef M
	matMult(m, rot);
}

void
matRotateZ(Matrix m, float ang)
{
	Matrix rot;
	matMakeIdentity(rot);
#define M(i,j) rot[i*4+j]
	M(0,0) = cosf(ang);
	M(0,1) = sinf(ang);
	M(1,0) = -sinf(ang);
	M(1,1) = cosf(ang);
#undef M
	matMult(m, rot);
}

void
matTranslate(Matrix m, float x, float y, float z)
{
	Matrix trans;
	matMakeIdentity(trans);
#define M(i,j) trans[i*4+j]
	M(3,0) = x;
	M(3,1) = y;
	M(3,2) = z;
#undef M
	matMult(m, trans);
}

void
matFrustum2(Matrix m, float l, float r, float xl, float xr,
            float b, float t, float yb, float yt,
            float n, float f, float zn, float zf)
{
	matMakeIdentity(m);
#define M(i,j) m[i*4+j]
	M(0,0) = ((xr-xl)*n)/(r-l);
	M(1,1) = ((yt-yb)*n)/(t-b);
	M(2,0) = (l*xr-r*xl)/(r-l);
	M(2,1) = (b*yt-t*yb)/(t-b);
	M(2,2) = -(f*zf+n*zn)/(f-n);
	M(2,3) = -1.0f;
	M(3,2) = (zn-zf)*f*n/(f-n);
	M(3,3) = 0.0f;
#undef M
}

void
matFrustum(Matrix m, float l, float r, float b, float t,
                float n, float f)
{
	matMakeIdentity(m);
#define M(i,j) m[i*4+j]
	M(0,0) = (2.0f*n)/(r-l);
	M(1,1) = (2.0f*n)/(t-b);
	M(2,0) = (r+l)/(r-l);
	M(2,1) = (t+b)/(t-b);
	M(2,2) = -(f+n)/(f-n);
	M(2,3) = -1.0f;
	M(3,2) = -2.0f*f*n/(f-n);
	M(3,3) = 0.0f;
#undef M
}

void
matPerspective2(Matrix m, float fov, float ratio, float width, float height,
                float n, float f, float znear, float zfar)
{
	float r, t;

	r = n*tanf(fov*M_PI/360.0f);
	t = r/ratio;

	matFrustum2(m, -r, r, 0, width,
	               -t, t, height, 0,
	                n, f, znear, zfar);
}

void
matPerspective(Matrix m, float fov, float ratio, float n, float f)
{
	float r, t;

	r = n*tanf(fov*M_PI/360.0f);
	t = r/ratio;

	matFrustum(m, -r, r, -t, t, n, f);
}
