#ifndef MATH_DQUAT_H
#define MATH_DQUAT_H

#include <iostream>
#include <cmath>

class Vec3;
class Vec4;

class DQuat {
public:
	Quat q1, q2;

	DQuat(void);
	DQuat(const Quat &q1, const Quat &q2);
	DQuat operator-(void) const;
	DQuat operator+(const DQuat &rhs) const;
	DQuat operator-(const DQuat &rhs) const;
	DQuat operator*(const DQuat &rhs) const;
	DQuat operator*(float rhs) const;
	DQuat operator/(float rhs) const;
	DQuat &operator+=(const DQuat &rhs);
	DQuat &operator-=(const DQuat &rhs);
	DQuat &operator*=(const DQuat &rhs);
	DQuat &operator*=(float rhs);
	DQuat &operator/=(float rhs);
	bool operator==(const DQuat &rhs) const;
	bool operator!=(const DQuat &rhs) const;

//	DQuat inv(void) const;
	DQuat K(void) const;	/* conjugate */
//	DQuat S(void) const;	/* scalar */
//	DQuat V(void) const;	/* vector */
//	float T(void) const;	/* tensor */
//	float N(void) const;	/* norm = tensor^2 */
//	DQuat U(void) const;	/* versor */
//	DQuat wedge(const Quat &rhs) const;
//	float inner(const Quat &rhs) const;
//	DQuat slerp(const Quat &rhs, float t) const;

	void print(std::ostream &of) const;
};

#endif

