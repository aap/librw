#ifndef MATH_VECTOR3_H
#define MATH_VECTOR3_H

#include <iostream>
#include <cmath>

class Vec4;
class Quat;

class Vec3 {
public:
	float x, y, z;

	Vec3(void);
	Vec3(float x, float y, float z);
	Vec3(float *v);
	Vec3(const Vec4 &v);
	Vec3(const Quat &q);
	float *ptr(void);
	Vec3 operator-(void) const;
	Vec3 operator+(const Vec3 &rhs) const;
	Vec3 operator-(const Vec3 &rhs) const;
	Vec3 operator*(float rhs) const;
	Vec3 operator/(float rhs) const;
	Vec3 &operator+=(const Vec3 &rhs);
	Vec3 &operator-=(const Vec3 &rhs);
	Vec3 &operator*=(float rhs);
	Vec3 &operator/=(float rhs);
	bool operator==(const Vec3 &rhs) const;
	bool operator!=(const Vec3 &rhs) const;
	float norm(void) const;
	float normsq(void) const;
	Vec3 normalized(void) const;
	float dot(const Vec3 &rhs) const;
	Vec3 cross(const Vec3 &rhs) const;
	void print(std::ostream &of) const;
};

#endif

