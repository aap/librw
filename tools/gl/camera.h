class Camera
{
private:
	Vec3 position;
	Vec3 target;
	Vec3 up;
	Vec3 local_up;

	float fov, aspectRatio;
	float n, f;

public:
	Mat4 projMat;
	Mat4 viewMat;

	void setPosition(Vec3 q);
	Vec3 getPosition(void);
	void setTarget(Vec3 q);
	Vec3 getTarget(void);
	float getHeading(void);

	void turn(float yaw, float pitch);
	void orbit(float yaw, float pitch);
	void dolly(float dist);
	void zoom(float dist);
	void pan(float x, float y);

	void setFov(float f);
	float getFov(void);
	void setAspectRatio(float r);
	void setNearFar(float n, float f);

	void look(void);
	float distanceTo(Vec3 q);
	float sqDistanceTo(Vec3 q);
	Camera(void);
};
