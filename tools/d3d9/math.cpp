#include "math/math.h"

/*
 * Vec3
 */

std::ostream&
operator<<(std::ostream& of, const Vec3 &v)
{
	v.print(of);
	return of;
}


Vec3::Vec3(void)
 : x(0.0f), y(0.0f), z(0.0f)
{
}

Vec3::Vec3(float x, float y, float z)
 : x(x), y(y), z(z)
{
}

Vec3::Vec3(float *v)
 : x(v[0]), y(v[1]), z(v[2])
{
}

float*
Vec3::ptr(void)
{
	return &x;
}

Vec3
Vec3::operator-(void) const
{
	return Vec3(-x, -y, -z);
}

Vec3
Vec3::operator+(const Vec3 &rhs) const
{
	return Vec3(this->x+rhs.x, this->y+rhs.y, this->z+rhs.z);
}

Vec3
Vec3::operator-(const Vec3 &rhs) const
{
	return Vec3(this->x-rhs.x, this->y-rhs.y, this->z-rhs.z);
}

Vec3
Vec3::operator*(float rhs) const
{
	return Vec3(this->x*rhs, this->y*rhs, this->z*rhs);
}

Vec3
Vec3::operator/(float rhs) const
{
	return Vec3(this->x/rhs, this->y/rhs, this->z/rhs);
}

Vec3&
Vec3::operator+=(const Vec3 &rhs)
{
	*this = *this + rhs;
	return *this;
}

Vec3&
Vec3::operator-=(const Vec3 &rhs)
{
	*this = *this - rhs;
	return *this;
}

Vec3&
Vec3::operator*=(float rhs)
{
	*this = *this * rhs;
	return *this;
}

Vec3&
Vec3::operator/=(float rhs)
{
	*this = *this / rhs;
	return *this;
}

bool
Vec3::operator==(const Vec3 &rhs) const
{
	return (this->x == rhs.x) &&
	       (this->y == rhs.y) &&
	       (this->z == rhs.z);
}

bool
Vec3::operator!=(const Vec3 &rhs) const
{
	return (this->x != rhs.x) ||
	       (this->y != rhs.y) ||
	       (this->z != rhs.z);
}

float
Vec3::norm(void) const
{
	return sqrt(normsq());
}

float
Vec3::normsq(void) const
{
	return x*x + y*y + z*z;
}

Vec3
Vec3::normalized(void) const
{
	return Vec3(*this)/norm();
}

float
Vec3::dot(const Vec3 &rhs) const
{
	return this->x*rhs.x + this->y*rhs.y + this->z*rhs.z;
}

Vec3
Vec3::cross(const Vec3 &rhs) const
{
	return Vec3(this->y*rhs.z - this->z*rhs.y,
	            this->z*rhs.x - this->x*rhs.z,
	            this->x*rhs.y - this->y*rhs.x);
}

void
Vec3::print(std::ostream &of) const
{
	of << "V3(" << x << ", " << y << ", " << z << ")";
}

/*
 * Vec4
 */

std::ostream&
operator<<(std::ostream& of, const Vec4 &v)
{
	v.print(of);
	return of;
}


Vec4::Vec4(void)
 : x(0.0f), y(0.0f), z(0.0f), w(0.0f)
{
}

Vec4::Vec4(float x, float y, float z, float w)
 : x(x), y(y), z(z), w(w)
{
}

Vec4::Vec4(float *v)
 : x(v[0]), y(v[1]), z(v[2]), w(v[3])
{
}

float*
Vec4::ptr(void)
{
	return &x;
}

Vec4
Vec4::operator-(void) const
{
	return Vec4(-x, -y, -z, -w);
}

Vec4
Vec4::operator+(const Vec4 &rhs) const
{
	return Vec4(this->x+rhs.x, this->y+rhs.y, this->z+rhs.z, this->w+rhs.w);
}

Vec4
Vec4::operator-(const Vec4 &rhs) const
{
	return Vec4(this->x-rhs.x, this->y-rhs.y, this->z-rhs.z, this->w-rhs.w);
}

Vec4
Vec4::operator*(float rhs) const
{
	return Vec4(this->x*rhs, this->y*rhs, this->z*rhs, this->w*rhs);
}

Vec4
Vec4::operator/(float rhs) const
{
	return Vec4(this->x/rhs, this->y/rhs, this->z/rhs, this->w/rhs);
}

Vec4&
Vec4::operator+=(const Vec4 &rhs)
{
	*this = *this + rhs;
	return *this;
}

Vec4&
Vec4::operator-=(const Vec4 &rhs)
{
	*this = *this - rhs;
	return *this;
}

Vec4&
Vec4::operator*=(float rhs)
{
	*this = *this * rhs;
	return *this;
}

Vec4&
Vec4::operator/=(float rhs)
{
	*this = *this / rhs;
	return *this;
}

bool
Vec4::operator==(const Vec4 &rhs) const
{
	return (this->x == rhs.x) &&
	       (this->y == rhs.y) &&
	       (this->z == rhs.z) &&
	       (this->w == rhs.w);
}

bool
Vec4::operator!=(const Vec4 &rhs) const
{
	return (this->x != rhs.x) ||
	       (this->y != rhs.y) ||
	       (this->z != rhs.z) ||
	       (this->w != rhs.w);
}

float
Vec4::norm(void) const
{
	return sqrt(normsq());
}

float
Vec4::normsq(void) const
{
	return x*x + y*y + z*z + w*w;
}

Vec4
Vec4::normalized(void) const
{
	return Vec4(*this)/norm();
}

float
Vec4::dot(const Vec4 &rhs) const
{
	return this->x*rhs.x + this->y*rhs.y + this->z*rhs.z + this->w*rhs.w;
}

void
Vec4::print(std::ostream &of) const
{
	of << "V4(" << x << ", " << y << ", " << z << ", " << w << ")";
}

/*
 * Quat
 */

std::ostream&
operator<<(std::ostream& of, const Quat &v)
{
	v.print(of);
	return of;
}


Quat::Quat(void)
 : w(0.0f), x(0.0f), y(0.0f), z(0.0f)
{
}

Quat::Quat(float w)
 : w(w), x(0.0f), y(0.0f), z(0.0f)
{
}

Quat::Quat(float x, float y, float z)
 : w(0.0f), x(x), y(y), z(z)
{
}

Quat::Quat(float w, float x, float y, float z)
 : w(w), x(x), y(y), z(z)
{
}

float*
Quat::ptr(void)
{
	return &w;
}

Quat
Quat::operator-(void) const
{
	return Quat(-w, -x, -y, -z);
}

Quat
Quat::operator+(const Quat &rhs) const
{
	return Quat(this->w+rhs.w, this->x+rhs.x, this->y+rhs.y, this->z+rhs.z);
}

Quat
Quat::operator-(const Quat &rhs) const
{
	return Quat(this->w-rhs.w, this->x-rhs.x, this->y-rhs.y, this->z-rhs.z);
}

Quat
Quat::operator*(const Quat &rhs) const
{
	return Quat(
		this->w*rhs.w - this->x*rhs.x - this->y*rhs.y - this->z*rhs.z,
		this->w*rhs.x + this->x*rhs.w + this->y*rhs.z - this->z*rhs.y,
		this->w*rhs.y + this->y*rhs.w + this->z*rhs.x - this->x*rhs.z,
		this->w*rhs.z + this->z*rhs.w + this->x*rhs.y - this->y*rhs.x);
}

Quat
Quat::operator*(float rhs) const
{
	return Quat(this->w*rhs, this->x*rhs, this->y*rhs, this->z*rhs);
}

Quat
Quat::operator/(float rhs) const
{
	return Quat(this->w/rhs, this->x/rhs, this->y/rhs, this->z/rhs);
}

Quat&
Quat::operator+=(const Quat &rhs)
{
	*this = *this + rhs;
	return *this;
}

Quat&
Quat::operator-=(const Quat &rhs)
{
	*this = *this - rhs;
	return *this;
}

Quat&
Quat::operator*=(const Quat &rhs)
{
	*this = *this * rhs;
	return *this;
}

Quat&
Quat::operator*=(float rhs)
{
	*this = *this * rhs;
	return *this;
}

Quat&
Quat::operator/=(float rhs)
{
	*this = *this / rhs;
	return *this;
}

bool
Quat::operator==(const Quat &rhs) const
{
	return (this->w == rhs.w) &&
	       (this->x == rhs.x) &&
	       (this->y == rhs.y) &&
	       (this->z == rhs.z);
}

bool
Quat::operator!=(const Quat &rhs) const
{
	return (this->w != rhs.w) ||
	       (this->x != rhs.x) ||
	       (this->y != rhs.y) ||
	       (this->z != rhs.z);
}

Quat
Quat::inv(void) const
{
	return K() / N();
}

Quat
Quat::K(void) const
{
	return Quat(w, -x, -y, -z);
}

Quat
Quat::S(void) const
{
	return Quat(w);
}

Quat
Quat::V(void) const
{
	return Quat(x, y, z);
}

float
Quat::T(void) const
{
	return sqrt(N());
}

float
Quat::N(void) const
{
	return w*w + x*x + y*y + z*z;
}

Quat
Quat::U(void) const
{
	return Quat(*this)/T();
}

Quat
Quat::wedge(const Quat &rhs) const
{
	return Quat(0.0f,
	            this->y*rhs.z - this->z*rhs.y,
	            this->z*rhs.x - this->x*rhs.z,
	            this->x*rhs.y - this->y*rhs.x);
}

float
Quat::inner(const Quat &rhs) const
{
	return this->w*rhs.w + this->x*rhs.x + this->y*rhs.y + this->z*rhs.z;
}

Quat
Quat::lerp(const Quat &q2, float t) const
{
	Quat q1 = *this;
	float cos = q1.inner(q2);
	if(cos < 0)
		q1 = -q1;
	return (q1*(1.0f - t) + q2*t).U();
}

Quat
Quat::slerp(const Quat &q2, float t) const
{
	Quat q1 = *this;
	float cos = q1.inner(q2);
	if(cos < 0){
		cos = -cos;
		q1 = -q1;
	}
	float phi = acos(cos);
	if(phi > 0.00001){
		float s = sin(phi);
		q1 = q1*sin((1.0f-t)*phi)/s + q2*sin(t*phi)/s;
	}
	return q1;
}

void
Quat::print(std::ostream &of) const
{
	of << "Q(" << w << ", " << x << ", " << y << ", " << z << ")";
}

/*
 * Quat
 */

std::ostream&
operator<<(std::ostream& of, const DQuat &v)
{
	v.print(of);
	return of;
}


DQuat::DQuat(void)
 : q1(), q2()
{
}

DQuat::DQuat(const Quat &q1, const Quat &q2)
 : q1(q1), q2(q2)
{
}

DQuat
DQuat::operator-(void) const
{
	return DQuat(-q1, -q2);
}

DQuat
DQuat::operator+(const DQuat &rhs) const
{
	return DQuat(this->q1+rhs.q1, this->q2+rhs.q2);
}

DQuat
DQuat::operator-(const DQuat &rhs) const
{
	return DQuat(this->q1-rhs.q1, this->q2-rhs.q2);
}

DQuat
DQuat::operator*(const DQuat &rhs) const
{
	return DQuat(this->q1*rhs.q1, this->q1*rhs.q2 + this->q2*rhs.q1);
}

DQuat
DQuat::operator*(float rhs) const
{
	return DQuat(this->q1*rhs, this->q2*rhs);
}

DQuat
DQuat::operator/(float rhs) const
{
	return DQuat(this->q1/rhs, this->q2/rhs);
}

DQuat&
DQuat::operator+=(const DQuat &rhs)
{
	*this = *this + rhs;
	return *this;
}

DQuat&
DQuat::operator-=(const DQuat &rhs)
{
	*this = *this - rhs;
	return *this;
}

DQuat&
DQuat::operator*=(const DQuat &rhs)
{
	*this = *this * rhs;
	return *this;
}

DQuat&
DQuat::operator*=(float rhs)
{
	*this = *this * rhs;
	return *this;
}

DQuat&
DQuat::operator/=(float rhs)
{
	*this = *this / rhs;
	return *this;
}

bool
DQuat::operator==(const DQuat &rhs) const
{
	return (this->q1 == rhs.q1) &&
	       (this->q2 == rhs.q2);
}

bool
DQuat::operator!=(const DQuat &rhs) const
{
	return (this->q1 != rhs.q1) ||
	       (this->q2 != rhs.q2);
}

DQuat
DQuat::K(void) const
{
	return DQuat(q1.K(), -q2.K());
}

void
DQuat::print(std::ostream &of) const
{
	of << "DQ(" << q1 << ", " << q2 << ")";
}

/*
 * Mat3
 */

std::ostream&
operator<<(std::ostream& of, const Mat3 &v)
{
	v.print(of);
	return of;
}

Mat3::Mat3(void)
{
	for(int i = 0; i < 3*3; i++)
		cr[i] = 0.0f;
}

Mat3::Mat3(float f)
{
	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 3; j++)
			e[i][j] = (i == j) ? f : 0.0f;
}

Mat3::Mat3(float *f)
{
	for(int i = 0; i < 3*3; i++)
		cr[i] = f[i];
}

Mat3::Mat3(float e00, float e10, float e20,
           float e01, float e11, float e21,
           float e02, float e12, float e22)
{
	e[0][0] = e00; e[1][0] = e10; e[2][0] = e20;
	e[0][1] = e01; e[1][1] = e11; e[2][1] = e21;
	e[0][2] = e02; e[1][2] = e12; e[2][2] = e22;
}

float*
Mat3::ptr(void)
{
	return &e[0][0];
}

Mat3
Mat3::rotation(float theta, const Vec3 &v)
{
	Mat3 m(1.0f);
	float c = cos(theta);
	float s = sin(theta);
	m.e[0][0] = v.x*v.x*(1-c) + c;
	m.e[1][0] = v.x*v.y*(1-c) - v.z*s;
	m.e[2][0] = v.x*v.z*(1-c) + v.y*s;

	m.e[0][1] = v.y*v.x*(1-c) + v.z*s;
	m.e[1][1] = v.y*v.y*(1-c) + c;
	m.e[2][1] = v.y*v.z*(1-c) - v.x*s;

	m.e[0][2] = v.z*v.x*(1-c) - v.y*s;
	m.e[1][2] = v.z*v.y*(1-c) + v.x*s;
	m.e[2][2] = v.z*v.z*(1-c) + c;
	return m;
}

Mat3	
Mat3::scale(const Vec3 &v)
{
	Mat3 m(1.0f);
	m.e[0][0] = v.x;
	m.e[1][1] = v.y;
	m.e[2][2] = v.z;
	return m;
}

Mat3
Mat3::transpose(void) const
{
	float e[3][3];
	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 3; j++){
			e[j][i] = this->e[i][j];
			e[i][j] = this->e[j][i];
		}
	return Mat3(&e[0][0]);
}

Mat3
Mat3::operator+(const Mat3 &rhs) const
{
	float e[9];
	for(int i = 0; i < 3*3; i++)
		e[i] = this->cr[i] + rhs.cr[i];
	return Mat3(e);
}

Mat3
Mat3::operator-(const Mat3 &rhs) const
{
	float e[9];
	for(int i = 0; i < 3*3; i++)
		e[i] = this->cr[i] - rhs.cr[i];
	return Mat3(e);
}

Mat3
Mat3::operator*(float rhs) const
{
	float e[9];
	for(int i = 0; i < 3*3; i++)
		e[i] = this->cr[i]*rhs;
	return Mat3(e);
}

Mat3
Mat3::operator/(float rhs) const
{
	float e[9];
	for(int i = 0; i < 3*3; i++)
		e[i] = this->cr[i]/rhs;
	return Mat3(e);
}

Mat3
Mat3::operator*(const Mat3 &rhs) const
{
	float e[3][3];
	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 3; j++)
			e[i][j] = this->e[0][j]*rhs.e[i][0] +
			          this->e[1][j]*rhs.e[i][1] +
			          this->e[2][j]*rhs.e[i][2];
	return Mat3(&e[0][0]);
}

Vec3
Mat3::operator*(const Vec3 &rhs) const
{
	float e[3];
	for(int i = 0; i < 3; i++)
		e[i] = this->e[0][i]*rhs.x +
		       this->e[1][i]*rhs.y +
		       this->e[2][i]*rhs.z;
	return Vec3(e);
}

Mat3&
Mat3::operator+=(const Mat3 &rhs)
{
	*this = *this + rhs;
	return *this;
}

Mat3&
Mat3::operator-=(const Mat3 &rhs)
{
	*this = *this - rhs;
	return *this;
}

Mat3&
Mat3::operator*=(float rhs)
{
	*this = *this * rhs;
	return *this;
}

Mat3&
Mat3::operator/=(float rhs)
{
	*this = *this / rhs;
	return *this;
}

Mat3&
Mat3::operator*=(const Mat3 &rhs)
{
	*this = *this * rhs;
	return *this;
}

void
Mat3::print(std::ostream &of) const
{
	#define CM << ", " <<
	of << "M3(" << e[0][0] CM e[1][0] CM e[2][0] << std::endl;
	of << "   " << e[0][1] CM e[1][1] CM e[2][1] << std::endl;
	of << "   " << e[0][2] CM e[1][2] CM e[2][2] << ")";
	#undef CM
}

/*
 * Mat3
 */

std::ostream&
operator<<(std::ostream& of, const Mat4 &v)
{
	v.print(of);
	return of;
}

Mat4::Mat4(void)
{
	for(int i = 0; i < 4*4; i++)
		cr[i] = 0.0f;
}

Mat4::Mat4(float f)
{
	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++)
			e[i][j] = (i == j) ? f : 0.0f;
}

Mat4::Mat4(float *f)
{
	for(int i = 0; i < 4*4; i++)
		cr[i] = f[i];
}

Mat4::Mat4(float e00, float e10, float e20, float e30,
           float e01, float e11, float e21, float e31,
           float e02, float e12, float e22, float e32,
           float e03, float e13, float e23, float e33)
{
	e[0][0] = e00; e[1][0] = e10; e[2][0] = e20; e[3][0] = e30;
	e[0][1] = e01; e[1][1] = e11; e[2][1] = e21; e[3][1] = e31;
	e[0][2] = e02; e[1][2] = e12; e[2][2] = e22; e[3][2] = e32;
	e[0][3] = e03; e[1][3] = e13; e[2][3] = e23; e[3][3] = e33;
}

float*
Mat4::ptr(void)
{
	return &e[0][0];
}

Mat4
Mat4::perspective(float fov, float aspect, float n, float f)
{
	float r = n*tan(fov*3.14159f/360.0f);
	float t = r/aspect;
	return frustum(-r, r, -t, t, n, f);
}

Mat4
Mat4::frustum(float l, float r, float b, float t,
              float n, float f)
{
	Mat4 m(1.0f);
	m.e[0][0] = (2.0f*n)/(r-l);
	m.e[1][1] = (2.0f*n)/(t-b);
	m.e[2][0] = (r+l)/(r-l);
	m.e[2][1] = (t+b)/(t-b);
// TODO:
//	m.e[2][2] = -(f+n)/(f-n);
//	m.e[2][3] = -1.0f;
//	m.e[3][2] = -2.0f*f*n/(f-n);
	m.e[2][2] = (f+n)/(f-n);
	m.e[2][3] = 1.0f;
	m.e[3][2] = -1.0f*f*n/(f-n);
	m.e[3][3] = 0.0f;
	return m;
}

Mat4
Mat4::ortho(float l, float r, float b, float t,
            float n, float f)
{
	Mat4 m(1.0f);
	m.e[0][0] = 2.0f/(r-l);
	m.e[3][0] = -(r+l)/(r-l);
	m.e[1][1] = 2.0f/(t-b);
	m.e[3][1] = -(t+b)/(t-b);
	m.e[2][2] = -2.0f/(f-n);
	m.e[3][2] = -(f+n)/(f-n);
	return m;
}

Mat4
Mat4::lookat(const Vec3 &pos, const Vec3 &target, const Vec3 &up)
{
	Vec3 forward = (target - pos).normalized();
	Vec3 side = forward.cross(up).normalized();
	Vec3 nup = side.cross(forward);
	Mat4 m(1.0f);
	m.e[0][0] = side.x;
	m.e[1][0] = side.y;
	m.e[2][0] = side.z;
	m.e[0][1] = nup.x;
	m.e[1][1] = nup.y;
	m.e[2][1] = nup.z;
	m.e[0][2] = -forward.x;
	m.e[1][2] = -forward.y;
	m.e[2][2] = -forward.z;
	m = m*Mat4::translation(-pos);
// TODO:
	m.e[0][2] *= -1.0f;
	m.e[1][2] *= -1.0f;
	m.e[2][2] *= -1.0f;
	m.e[3][2] *= -1.0f;
	return m;
}

Mat4
Mat4::translation(const Vec3 &v)
{
	Mat4 m(1.0f);
	m.e[3][0] = v.x;
	m.e[3][1] = v.y;
	m.e[3][2] = v.z;
	return m;
}

Mat4
Mat4::rotation(float theta, const Vec3 &v)
{
	Mat4 m(1.0f);
	float c = cos(theta);
	float s = sin(theta);
	m.e[0][0] = v.x*v.x*(1-c) + c;
	m.e[1][0] = v.x*v.y*(1-c) - v.z*s;
	m.e[2][0] = v.x*v.z*(1-c) + v.y*s;

	m.e[0][1] = v.y*v.x*(1-c) + v.z*s;
	m.e[1][1] = v.y*v.y*(1-c) + c;
	m.e[2][1] = v.y*v.z*(1-c) - v.x*s;

	m.e[0][2] = v.z*v.x*(1-c) - v.y*s;
	m.e[1][2] = v.z*v.y*(1-c) + v.x*s;
	m.e[2][2] = v.z*v.z*(1-c) + c;
	return m;
}

Mat4
Mat4::scale(const Vec3 &v)
{
	Mat4 m(1.0f);
	m.e[0][0] = v.x;
	m.e[1][1] = v.y;
	m.e[2][2] = v.z;
	return m;
}

Mat4
Mat4::transpose(void) const
{
	float e[4][4];
	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++){
			e[j][i] = this->e[i][j];
			e[i][j] = this->e[j][i];
		}
	return Mat4(&e[0][0]);
}

Mat4
Mat4::operator+(const Mat4 &rhs) const
{
	float e[16];
	for(int i = 0; i < 4*4; i++)
		e[i] = this->cr[i] + rhs.cr[i];
	return Mat4(e);
}

Mat4
Mat4::operator-(const Mat4 &rhs) const
{
	float e[16];
	for(int i = 0; i < 4*4; i++)
		e[i] = this->cr[i] - rhs.cr[i];
	return Mat4(e);
}

Mat4
Mat4::operator*(float rhs) const
{
	float e[16];
	for(int i = 0; i < 4*4; i++)
		e[i] = this->cr[i]*rhs;
	return Mat4(e);
}

Mat4
Mat4::operator/(float rhs) const
{
	float e[16];
	for(int i = 0; i < 4*4; i++)
		e[i] = this->cr[i]/rhs;
	return Mat4(e);
}

Mat4
Mat4::operator*(const Mat4 &rhs) const
{
	float e[4][4];
	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++)
			e[i][j] = this->e[0][j]*rhs.e[i][0] +
			          this->e[1][j]*rhs.e[i][1] +
			          this->e[2][j]*rhs.e[i][2] +
			          this->e[3][j]*rhs.e[i][3];
	return Mat4(&e[0][0]);
}

Vec4
Mat4::operator*(const Vec4 &rhs) const
{
	float e[4];
	for(int i = 0; i < 4; i++)
		e[i] = this->e[0][i]*rhs.x +
		       this->e[1][i]*rhs.y +
		       this->e[2][i]*rhs.z +
		       this->e[3][i]*rhs.w;
	return Vec4(e);
}

Mat4&
Mat4::operator+=(const Mat4 &rhs)
{
	*this = *this + rhs;
	return *this;
}

Mat4&
Mat4::operator-=(const Mat4 &rhs)
{
	*this = *this - rhs;
	return *this;
}

Mat4&
Mat4::operator*=(float rhs)
{
	*this = *this * rhs;
	return *this;
}

Mat4&
Mat4::operator/=(float rhs)
{
	*this = *this / rhs;
	return *this;
}

Mat4&
Mat4::operator*=(const Mat4 &rhs)
{
	*this = *this * rhs;
	return *this;
}

void
Mat4::print(std::ostream &of) const
{
	#define CM << ", " <<
	of << "M4(" << e[0][0] CM e[1][0] CM e[2][0] CM e[3][0] << std::endl;
	of << "   " << e[0][1] CM e[1][1] CM e[2][1] CM e[3][1] << std::endl;
	of << "   " << e[0][2] CM e[1][2] CM e[2][2] CM e[3][2] << std::endl;
	of << "   " << e[0][3] CM e[1][3] CM e[2][3] CM e[3][3] << ")";
	#undef CM
}





Vec3::Vec3(const Vec4 &v)
 : x(v.x), y(v.y), z(v.z)
{
}

Vec3::Vec3(const Quat &q)
 : x(q.x), y(q.y), z(q.z)
{
}

Vec4::Vec4(const Vec3 &v, float w)
 : x(v.x), y(v.y), z(v.z), w(w)
{
}

Vec4::Vec4(const Quat &q)
 : x(q.x), y(q.y), z(q.z), w(q.w)
{
}

Quat::Quat(const Vec3 &v)
 : w(0), x(v.x), y(v.y), z(v.z)
{
}

Quat::Quat(float w, const Vec3 &v)
 : w(w), x(v.x), y(v.y), z(v.z)
{
}

Quat::Quat(const Vec4 &v)
 : w(v.w), x(v.x), y(v.y), z(v.z)
{
}

Quat::Quat(const Mat3 &m)
{
	float trace, s, q[4];
	int i, j, k;

	int nxt[3] = {1, 2, 0};

	trace = m.e[0][0] + m.e[1][1] + m.e[2][2];
	if(trace > 0.0f){
		s = sqrt(trace + 1.0f);
		this->w = s / 2.0f;
		s = 0.5f / s;
		this->x = (m.e[2][1] - m.e[1][2]) * s;
		this->y = (m.e[0][2] - m.e[2][0]) * s;
		this->z = (m.e[1][0] - m.e[0][1]) * s;
	}else{
		i = 0;
		if(m.e[1][1] > m.e[0][0]) i = 1;
		if(m.e[2][2] > m.e[i][i]) i = 2;
		j = nxt[i];
		k = nxt[j];
		s = sqrt((m.e[i][i] - m.e[j][j] - m.e[k][k]) + 1.0);
		q[i] = s*0.5f;
		if(q[i] != 0.0f) s = 0.5f / s;
		q[3] = (m.e[k][j] - m.e[j][k]) * s;
		q[j] = (m.e[j][i] + m.e[i][j]) * s;
		q[k] = (m.e[k][i] + m.e[i][k]) * s;
		this->w = q[3];
		this->x = q[0];
		this->y = q[1];
		this->z = q[2];
	}
}

Mat3::Mat3(const Mat4 &m)
{
	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 3; j++)
			this->e[i][j] = m.e[i][j];
}

Mat4::Mat4(const Mat3 &m)
{
	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 3; j++)
			this->e[i][j] = m.e[i][j];
	this->e[0][3] = 0.0f;
	this->e[1][3] = 0.0f;
	this->e[2][3] = 0.0f;
	this->e[3][3] = 1.0f;
	this->e[3][2] = 0.0f;
	this->e[3][1] = 0.0f;
	this->e[3][0] = 0.0f;
}

Mat3
Mat3::rotation(const Quat &v)
{
	Mat3 m(1.0f);
	m.e[0][0] = v.w*v.w + v.x*v.x - v.y*v.y - v.z*v.z;
	m.e[1][0] = 2*v.x*v.y - 2*v.w*v.z;
	m.e[2][0] = 2*v.w*v.y + 2*v.x*v.z;
	m.e[0][1] = 2*v.w*v.z + 2*v.x*v.y;
	m.e[1][1] = v.w*v.w - v.x*v.x + v.y*v.y - v.z*v.z;
	m.e[2][1] = 2*v.y*v.z - 2*v.w*v.x;
	m.e[0][2] = 2*v.x*v.z - 2*v.w*v.y;
	m.e[1][2] = 2*v.w*v.x + 2*v.y*v.z;
	m.e[2][2] = v.w*v.w - v.x*v.x - v.y*v.y + v.z*v.z;
	return m;
}

Mat4
Mat4::rotation(const Quat &q)
{
	Mat4 m(1.0f);
	m.e[0][0] = q.w*q.w + q.x*q.x - q.y*q.y - q.z*q.z;
	m.e[1][0] = 2*q.x*q.y - 2*q.w*q.z;
	m.e[2][0] = 2*q.w*q.y + 2*q.x*q.z;
	m.e[0][1] = 2*q.w*q.z + 2*q.x*q.y;
	m.e[1][1] = q.w*q.w - q.x*q.x + q.y*q.y - q.z*q.z;
	m.e[2][1] = 2*q.y*q.z - 2*q.w*q.x;
	m.e[0][2] = 2*q.x*q.z - 2*q.w*q.y;
	m.e[1][2] = 2*q.w*q.x + 2*q.y*q.z;
	m.e[2][2] = q.w*q.w - q.x*q.x - q.y*q.y + q.z*q.z;
	return m;
}

Mat4
Mat4::transrot(const DQuat &q)
{
	const Quat &q1 = q.q1;
	const Quat &q2 = q.q2;
	Mat4 m(1.0f);
	m.e[0][0] = q1.w*q1.w + q1.x*q1.x - q1.y*q1.y - q1.z*q1.z;
	m.e[1][0] = 2*q1.x*q1.y - 2*q1.w*q1.z;
	m.e[2][0] = 2*q1.w*q1.y + 2*q1.x*q1.z;
	m.e[0][1] = 2*q1.w*q1.z + 2*q1.x*q1.y;
	m.e[1][1] = q1.w*q1.w - q1.x*q1.x + q1.y*q1.y - q1.z*q1.z;
	m.e[2][1] = 2*q1.y*q1.z - 2*q1.w*q1.x;
	m.e[0][2] = 2*q1.x*q1.z - 2*q1.w*q1.y;
	m.e[1][2] = 2*q1.w*q1.x + 2*q1.y*q1.z;
	m.e[2][2] = q1.w*q1.w - q1.x*q1.x - q1.y*q1.y + q1.z*q1.z;
	m.e[3][0] = 2*(q1.w*q2.x - q2.w*q1.x + q1.y*q2.z - q1.z*q2.y);
	m.e[3][1] = 2*(q1.w*q2.y - q2.w*q1.y + q1.z*q2.x - q1.x*q2.z);
	m.e[3][2] = 2*(q1.w*q2.z - q2.w*q1.z + q1.x*q2.y - q1.y*q2.x);
	return m;
}

