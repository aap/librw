#ifndef MATH_MATRIX3_H
#define MATH_MATRIX3_H

#include <iostream>
#include <cmath>

class Mat4;

class Mat3 {
public:
	union {
	float e[3][3];
	float cr[9];
	};

	Mat3(void);
	Mat3(float f);
	Mat3(float *f);
	Mat3(float e00, float e10, float e20,
	     float e01, float e11, float e21,
	     float e02, float e12, float e22);
	Mat3(const Mat4 &m);
	float *ptr(void);
	static Mat3 rotation(float theta, const Vec3 &v);
	static Mat3 rotation(const Quat &v);
	static Mat3 scale(const Vec3 &v);
	Mat3 transpose(void) const;
	Mat3 operator+(const Mat3 &rhs) const;
	Mat3 operator-(const Mat3 &rhs) const;
	Mat3 operator*(float rhs) const;
	Mat3 operator/(float rhs) const;
	Mat3 operator*(const Mat3 &rhs) const;
	Vec3 operator*(const Vec3 &rhs) const;
	Mat3 &operator+=(const Mat3 &rhs);
	Mat3 &operator-=(const Mat3 &rhs);
	Mat3 &operator*=(float rhs);
	Mat3 &operator/=(float rhs);
	Mat3 &operator*=(const Mat3 &rhs);
	void print(std::ostream &of) const;
};

#endif

