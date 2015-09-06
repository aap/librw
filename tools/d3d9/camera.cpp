#include "math/math.h"
#include "camera.h"

using namespace std;

void
Camera::look(void)
{
	projMat = Mat4::perspective(fov, aspectRatio, n, f);
	viewMat = Mat4::lookat(position, target, up);

//	state->mat4(PMAT,true)->val = Mat4::perspective(fov, aspectRatio, n, f);
//	Mat4 mv = Mat4::lookat(position, target, up);
//	state->mat4(MVMAT, true)->val = mv;
//	state->mat3(NORMALMAT, true)->val = Mat3(mv);
}

void
Camera::setPosition(Vec3 q)
{
	position = q;
}

Vec3
Camera::getPosition(void)
{
	return position;
}

void
Camera::setTarget(Vec3 q)
{
	position -= target - q;
	target = q;
}

Vec3
Camera::getTarget(void)
{
	return target;
}

float
Camera::getHeading(void)
{
	Vec3 dir = target - position;
	float a = atan2(dir.y, dir.x)-PI/2.0f;
	return local_up.z < 0.0f ? a-PI : a;
}

void
Camera::turn(float yaw, float pitch)
{
	yaw /= 2.0f;
	pitch /= 2.0f;
	Quat dir = Quat(target - position);
	Quat r(cos(yaw), 0.0f, 0.0f, sin(yaw));
	dir = r*dir*r.K();
	local_up = Vec3(r*Quat(local_up)*r.K());

	Quat right = dir.wedge(Quat(local_up)).U();
	r = Quat(cos(pitch), right*sin(pitch));
	dir = r*dir*r.K();
	local_up = Vec3(right.wedge(dir).U());
	if(local_up.z >=0) up.z = 1;
	else up.z = -1;

	target = position + Vec3(dir);
}

void
Camera::orbit(float yaw, float pitch)
{
	yaw /= 2.0f;
	pitch /= 2.0f;
	Quat dir = Quat(target - position);
	Quat r(cos(yaw), 0.0f, 0.0f, sin(yaw));
	dir = r*dir*r.K();
	local_up = Vec3(r*Quat(local_up)*r.K());

	Quat right = dir.wedge(Quat(local_up)).U();
	r = Quat(cos(-pitch), right*sin(-pitch));
	dir = r*dir*r.K();
	local_up = Vec3(right.wedge(dir).U());
	if(local_up.z >=0) up.z = 1;
	else up.z = -1;

	position = target - Vec3(dir);
}

void
Camera::dolly(float dist)
{
	Vec3 dir = (target - position).normalized()*dist;
	position += dir;
	target += dir;
}

void
Camera::zoom(float dist)
{
	Vec3 dir = target - position;
	float curdist = dir.norm();
	if(dist >= curdist)
		dist = curdist-0.01f;
	dir = dir.normalized()*dist;
	position += dir;
}

void
Camera::pan(float x, float y)
{
	Vec3 dir = (target-position).normalized();
	Vec3 right = dir.cross(up).normalized();
//	Vec3 local_up = right.cross(dir).normalized();
	dir = right*x + local_up*y;
	position += dir;
	target += dir;

}

float
Camera::sqDistanceTo(Vec3 q)
{
	return (position - q).normsq();
}

float
Camera::distanceTo(Vec3 q)
{
	return (position - q).norm();
}

void
Camera::setFov(float f)
{
	fov = f;
}

float
Camera::getFov(void)
{
	return fov;
}

void
Camera::setAspectRatio(float r)
{
	aspectRatio = r;
}

void
Camera::setNearFar(float n, float f)
{
	this->n = n;
	this->f = f;
}

Camera::Camera()
{
	position = Vec3(0.0f, 6.0f, 0.0f);
	target = Vec3(0.0f, 0.0f, 0.0f);
	local_up = up = Vec3(0.0f, 0.0f, 1.0f);
	fov = 70.0f;
	aspectRatio = 1.0f;
	n = 0.1f;
	f = 100.0f;
}

