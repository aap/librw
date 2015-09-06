#ifndef MATH_VECTOR4_H
#define MATH_VECTOR4_H

#include <iostream>
#include <cmath>

class Vec3;
class Quat;

class Vec4 {
public:
	float x, y, z, w;

	Vec4(void);
	Vec4(float x, float y, float z, float w);
	Vec4(float *v);
	Vec4(const Vec3 &v, float w = 0.0f);
	Vec4(const Quat &w);
	float *ptr(void);
	Vec4 operator-(void) const;
	Vec4 operator+(const Vec4 &rhs) const;
	Vec4 operator-(const Vec4 &rhs) const;
	Vec4 operator*(float rhs) const;
	Vec4 operator/(float rhs) const;
	Vec4 &operator+=(const Vec4 &rhs);
	Vec4 &operator-=(const Vec4 &rhs);
	Vec4 &operator*=(float rhs);
	Vec4 &operator/=(float rhs);
	bool operator==(const Vec4 &rhs) const;
	bool operator!=(const Vec4 &rhs) const;
	float norm(void) const;
	float normsq(void) const;
	Vec4 normalized(void) const;
	float dot(const Vec4 &rhs) const;
	void print(std::ostream &of) const;
};

#endif

