#ifndef MATH_CONVERSION_H
#define MATH_CONVERSION_H

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
 : x(v.x), y(v.y), z(v.z)
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

Mat4
Mat4::rotation(const Quat &v)
{
	Mat4 m(1.0f);
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

#endif

