#ifndef MATH_QUAT_H
#define MATH_QUAT_H

#include <iostream>
#include <cmath>

class Vec3;
class Vec4;
class Mat3;

/* Hamilton style */

class Quat {
public:
	float w, x, y, z;

	Quat(void);
	Quat(float w);
	Quat(float x, float y, float z);
	Quat(float w, float x, float y, float z);
	Quat(float w, const Vec3 &v);
	Quat(const Vec3 &v);
	Quat(const Vec4 &v);
	Quat(const Mat3 &m);
	float *ptr(void);
	Quat operator-(void) const;
	Quat operator+(const Quat &rhs) const;
	Quat operator-(const Quat &rhs) const;
	Quat operator*(const Quat &rhs) const;
	Quat operator*(float rhs) const;
	Quat operator/(float rhs) const;
	Quat &operator+=(const Quat &rhs);
	Quat &operator-=(const Quat &rhs);
	Quat &operator*=(const Quat &rhs);
	Quat &operator*=(float rhs);
	Quat &operator/=(float rhs);
	bool operator==(const Quat &rhs) const;
	bool operator!=(const Quat &rhs) const;
	Quat inv(void) const;
	Quat K(void) const;	/* conjugate */
	Quat S(void) const;	/* scalar */
	Quat V(void) const;	/* vector */
	float T(void) const;	/* tensor */
	float N(void) const;	/* norm = tensor^2 */
	Quat U(void) const;	/* versor */
	Quat wedge(const Quat &rhs) const;
	float inner(const Quat &rhs) const;
	Quat lerp(const Quat &rhs, float t) const;
	Quat slerp(const Quat &rhs, float t) const;
	void print(std::ostream &of) const;
};

#endif

