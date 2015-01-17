#ifndef MATH_MATRIX4_H
#define MATH_MATRIX4_H

#include <iostream>
#include <cmath>

class Mat3;

class Mat4 {
public:
	union {
	float e[4][4];
	float cr[16];
	};

	Mat4(void);
	Mat4(float f);
	Mat4(float *f);
	Mat4(float e00, float e10, float e20, float e30,
	     float e01, float e11, float e21, float e31,
	     float e02, float e12, float e22, float e32,
	     float e03, float e13, float e23, float e33);
	Mat4(const Mat3 &m);
	float *ptr(void);
	static Mat4 perspective(float fov, float aspect, float n, float f);
	static Mat4 frustum(float l, float r, float b, float t,
	                    float n, float f);
	static Mat4 ortho(float l, float r, float b, float t,
	                  float n, float f);
	static Mat4 lookat(const Vec3 &pos, const Vec3 &target, const Vec3 &up);
	static Mat4 translation(const Vec3 &v);
	static Mat4 rotation(float theta, const Vec3 &v);
	static Mat4 rotation(const Quat &q);
	static Mat4 transrot(const DQuat &q);
	static Mat4 scale(const Vec3 &v);
	Mat4 transpose(void) const;
	Mat4 operator+(const Mat4 &rhs) const;
	Mat4 operator-(const Mat4 &rhs) const;
	Mat4 operator*(float rhs) const;
	Mat4 operator/(float rhs) const;
	Mat4 operator*(const Mat4 &rhs) const;
	Vec4 operator*(const Vec4 &rhs) const;
	Mat4 &operator+=(const Mat4 &rhs);
	Mat4 &operator-=(const Mat4 &rhs);
	Mat4 &operator*=(float rhs);
	Mat4 &operator/=(float rhs);
	Mat4 &operator*=(const Mat4 &rhs);
	void print(std::ostream &of) const;
};

#endif

