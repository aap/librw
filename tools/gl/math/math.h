#ifndef MATH_H
#define MATH_H

#include "vec3.h"
#include "vec4.h"
#include "quat.h"
#include "dquat.h"
#include "mat3.h"
#include "mat4.h"

std::ostream &operator<<(std::ostream& of, const Vec3 &v);
std::ostream &operator<<(std::ostream& of, const Vec4 &v);
std::ostream &operator<<(std::ostream& of, const Quat &v);
std::ostream &operator<<(std::ostream& of, const DQuat &v);
std::ostream &operator<<(std::ostream& of, const Mat3 &v);
std::ostream &operator<<(std::ostream& of, const Mat4 &v);

#define PI 3.14159265359f


#endif
