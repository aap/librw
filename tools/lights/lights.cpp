#include <rw.h>
#include <skeleton.h>

#include "main.h"

rw::Light *BaseAmbientLight;
bool BaseAmbientLightOn;

rw::Light *CurrentLight;
rw::Light *AmbientLight;
rw::Light *PointLight;
rw::Light *DirectLight;
rw::Light *SpotLight;
rw::Light *SpotSoftLight;

float LightRadius = 100.0f;
float LightConeAngle = 45.0f;
rw::RGBAf LightColor = { 1.0f, 1.0f, 1.0f, 1.0f };

rw::RGBA LightSolidColor = { 255, 255, 0, 255 };
bool LightOn = true;
bool LightDrawOn = true;
rw::V3d LightPos = {0.0f, 0.0f, 75.0f};
rw::int32 LightTypeIndex = 1;

rw::BBox RoomBBox;

rw::Light*
CreateBaseAmbientLight(void)
{
	rw::Light *light = rw::Light::create(rw::Light::AMBIENT);
	assert(light);
	light->setColor(0.5f, 0.5f, 0.5f);
	return light;
}

rw::Light*
CreateAmbientLight(void)
{
	return rw::Light::create(rw::Light::AMBIENT);
}

rw::Light*
CreateDirectLight(void)
{
	rw::Light *light = rw::Light::create(rw::Light::DIRECTIONAL);
	assert(light);
	rw::Frame *frame = rw::Frame::create();
	assert(frame);
	frame->rotate(&Xaxis, 45.0f, rw::COMBINEREPLACE);
	rw::V3d pos = LightPos;
	frame->translate(&pos, rw::COMBINEPOSTCONCAT);
	light->setFrame(frame);
	return light;
}

rw::Light*
CreatePointLight(void)
{
	rw::Light *light = rw::Light::create(rw::Light::POINT);
	assert(light);
	light->radius = LightRadius;
	rw::Frame *frame = rw::Frame::create();
	assert(frame);
	rw::V3d pos = LightPos;
	frame->translate(&pos, rw::COMBINEREPLACE);
	light->setFrame(frame);
	return light;
}

rw::Light*
CreateSpotLight(void)
{
	rw::Light *light = rw::Light::create(rw::Light::SPOT);
	assert(light);
	light->radius = LightRadius;
	light->setAngle(LightConeAngle/180.0f*M_PI);
	rw::Frame *frame = rw::Frame::create();
	assert(frame);
	frame->rotate(&Xaxis, 45.0f, rw::COMBINEREPLACE);
	rw::V3d pos = LightPos;
	frame->translate(&pos, rw::COMBINEPOSTCONCAT);
	light->setFrame(frame);
	return light;
}

rw::Light*
CreateSpotSoftLight(void)
{
	rw::Light *light = rw::Light::create(rw::Light::SOFTSPOT);
	assert(light);
	light->radius = LightRadius;
	light->setAngle(LightConeAngle/180.0f*M_PI);
	rw::Frame *frame = rw::Frame::create();
	assert(frame);
	frame->rotate(&Xaxis, 45.0f, rw::COMBINEREPLACE);
	rw::V3d pos = LightPos;
	frame->translate(&pos, rw::COMBINEPOSTCONCAT);
	light->setFrame(frame);
	return light;
}

void
DestroyLight(rw::Light **light)
{
	if(*light == nil)
		return;
	rw::World *world = (*light)->world;
	if(world)
		world->removeLight(*light);
	rw::Frame *frame = (*light)->getFrame();
	if(frame){
		(*light)->setFrame(nil);
		frame->destroy();
	}

	(*light)->destroy();
	*light = nil;
}

void
LightsDestroy(void)
{
	DestroyLight(&SpotSoftLight);
	DestroyLight(&SpotLight);
	DestroyLight(&PointLight);
	DestroyLight(&DirectLight);
	DestroyLight(&AmbientLight);
	DestroyLight(&BaseAmbientLight);
}

void
LightsUpdate(void)
{
	static rw::int32 oldLightTypeIndex = -1;

	// Switch to a different light
	if((LightOn && oldLightTypeIndex != LightTypeIndex) || CurrentLight == nil){
		oldLightTypeIndex = LightTypeIndex;

		// remove first
		if(CurrentLight)
			CurrentLight->world->removeLight(CurrentLight);

		switch(LightTypeIndex){
		case 0: CurrentLight = AmbientLight; break;
		case 1: CurrentLight = PointLight; break;
		case 2: CurrentLight = DirectLight; break;
		case 3: CurrentLight = SpotLight; break;
		case 4: CurrentLight = SpotSoftLight; break;
		}
		World->addLight(CurrentLight);
	}

	if(CurrentLight){
		CurrentLight->setColor(LightColor.red, LightColor.green, LightColor.blue);
		CurrentLight->radius = LightRadius;
		CurrentLight->setAngle(LightConeAngle / 180.0f * M_PI);
	}

	// Remove light from world if not used
	if(!LightOn && CurrentLight){
		CurrentLight->world->removeLight(CurrentLight);
		CurrentLight = nil;
	}
}

#define POINT_LIGHT_RADIUS_FACTOR 0.05f

void
DrawPointLight(void)
{
	enum { NUMVERTS = 50 };
	rw::RWDEVICE::Im3DVertex shape[NUMVERTS];
	rw::int32 i;
	rw::V3d point;

	rw::V3d *pos = &CurrentLight->getFrame()->getLTM()->pos;
	for(i = 0; i < NUMVERTS; i++){
		point.x = pos->x +
			cosf(i/(NUMVERTS/2.0f) * M_PI) * LightRadius * POINT_LIGHT_RADIUS_FACTOR;
		point.y = pos->y +
			sinf(i/(NUMVERTS/2.0f) * M_PI) * LightRadius * POINT_LIGHT_RADIUS_FACTOR;
		point.z = pos->z;

		shape[i].setColor(LightSolidColor.red, LightSolidColor.green,
			LightSolidColor.blue, LightSolidColor.alpha);
		shape[i].setX(point.x);
		shape[i].setY(point.y);
		shape[i].setZ(point.z);
	}

	rw::im3d::Transform(shape, NUMVERTS, nil, rw::im3d::ALLOPAQUE);
	rw::im3d::RenderPrimitive(rw::PRIMTYPEPOLYLINE);
	rw::im3d::RenderLine(NUMVERTS-1, 0);
	rw::im3d::End();
}

void
DrawCone(float coneAngle, float coneSize, float coneRatio)
{
	enum { NUMVERTS = 10 };
	rw::RWDEVICE::Im3DVertex shape[NUMVERTS+1];
	rw::int16 indices[NUMVERTS*3];
	rw::int32 i;

	rw::Matrix *matrix = CurrentLight->getFrame()->getLTM();
	rw::V3d *pos = &matrix->pos;

	// cone
	for(i = 1; i < NUMVERTS+1; i++){
		float cosValue = cosf(i/(NUMVERTS/2.0f) * M_PI) *
			sinf(coneAngle/180.0f*M_PI);
		float sinValue = sinf(i/(NUMVERTS/2.0f) * M_PI) *
			sinf(coneAngle/180.0f*M_PI);

		float coneAngleD = cosf(coneAngle/180.0f*M_PI);

		rw::V3d up = rw::scale(matrix->up, sinValue*coneSize);
		rw::V3d right = rw::scale(matrix->right, cosValue*coneSize);
		rw::V3d at = rw::scale(matrix->at, coneAngleD*coneSize*coneRatio);

		shape[i].setX(pos->x + at.x + up.x + right.x);
		shape[i].setY(pos->y + at.y + up.y + right.y);
		shape[i].setZ(pos->z + at.z + up.z + right.z);
	}

	for(i = 0; i < NUMVERTS; i++){
		indices[i*3 + 0] = 0;
		indices[i*3 + 1] = i+2;
		indices[i*3 + 2] = i+1;
	}
	indices[NUMVERTS*3-2] = 1;

	for(i = 0; i < NUMVERTS+1; i++)
		shape[i].setColor(LightSolidColor.red, LightSolidColor.green,
			LightSolidColor.blue, 128);

	shape[0].setX(pos->x);
	shape[0].setY(pos->y);
	shape[0].setZ(pos->z);

	rw::SetRenderState(rw::VERTEXALPHA, 1);
	rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
	rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);

	rw::im3d::Transform(shape, NUMVERTS+1, nil, 0);
	rw::im3d::RenderPrimitive(rw::PRIMTYPETRIFAN);
	rw::im3d::RenderTriangle(0, NUMVERTS, 1);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, indices, NUMVERTS*3);
	rw::im3d::End();


	for(i = 0; i < NUMVERTS+1; i++)
		shape[i].setColor(LightSolidColor.red, LightSolidColor.green,
			LightSolidColor.blue, 255);

	float coneAngleD = cosf(coneAngle/180.0f*M_PI);
	rw::V3d at = rw::scale(matrix->at, coneAngleD*coneSize*coneRatio);
	shape[0].setX(pos->x + at.x);
	shape[0].setY(pos->y + at.y);
	shape[0].setZ(pos->z + at.z);

	rw::im3d::Transform(shape, NUMVERTS+1, nil, rw::im3d::ALLOPAQUE);
	if(coneRatio > 0.0f){
		rw::im3d::RenderPrimitive(rw::PRIMTYPETRIFAN);
		rw::im3d::RenderTriangle(0, NUMVERTS, 1);
	}else
		rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRIFAN, indices, NUMVERTS*3);
	rw::im3d::End();


	// lines
	at = rw::scale(matrix->at, -0.05f);
	shape[0].setX(pos->x + at.x);
	shape[0].setY(pos->y + at.y);
	shape[0].setZ(pos->z + at.z);
	rw::im3d::Transform(shape, NUMVERTS+1, nil, rw::im3d::ALLOPAQUE);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPEPOLYLINE, indices, NUMVERTS*3);
	rw::im3d::End();
}

void
DrawDirectLight(void)
{
	enum { NUMVERTS = 20 };
	const float DIAMETER = 1.5f;
	const float CONE_ANGLE = 45.0f;
	const float CONE_SIZE = 3.0f;
	const float LENGTH = 5.0f;
	rw::RWDEVICE::Im3DVertex shape[NUMVERTS*2+1];
	rw::int16 indices[NUMVERTS*3];
	rw::int32 i;

	rw::Matrix *matrix = CurrentLight->getFrame()->getLTM();
	rw::V3d *pos = &matrix->pos;

	// cylinder
	for(i = 0; i < NUMVERTS*2; i += 2){
		float cosValue = cosf(i/(NUMVERTS/2.0f) * M_PI);
		float sinValue = sinf(i/(NUMVERTS/2.0f) * M_PI);

		rw::V3d up = rw::scale(matrix->up, sinValue*DIAMETER);
		rw::V3d right = rw::scale(matrix->right, cosValue*DIAMETER);
		rw::V3d at = rw::scale(matrix->at, -(CONE_SIZE + 1.0f));

		shape[i].setX(pos->x + at.x + up.x + right.x);
		shape[i].setY(pos->y + at.y + up.y + right.y);
		shape[i].setZ(pos->z + at.z + up.z + right.z);


		at = rw::scale(matrix->at, -(LENGTH + CONE_SIZE));

		shape[i+1].setX(pos->x + at.x + up.x + right.x);
		shape[i+1].setY(pos->y + at.y + up.y + right.y);
		shape[i+1].setZ(pos->z + at.z + up.z + right.z);
	}

	for(i = 0; i < NUMVERTS*2+1; i++)
		shape[i].setColor(LightSolidColor.red, LightSolidColor.green,
			LightSolidColor.blue, 128);

	rw::SetRenderState(rw::VERTEXALPHA, 1);
	rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
	rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);

	rw::im3d::Transform(shape, NUMVERTS*2, nil, 0);
	rw::im3d::RenderPrimitive(rw::PRIMTYPETRISTRIP);
	rw::im3d::RenderTriangle(2*NUMVERTS-2, 2*NUMVERTS-1, 0);
	rw::im3d::RenderTriangle(2*NUMVERTS-1, 1, 0);
	rw::im3d::End();


	// bottom cap
	for(i = 0; i < NUMVERTS*2+1; i++)
		shape[i].setColor(LightSolidColor.red, LightSolidColor.green,
			LightSolidColor.blue, 255);

	rw::V3d at = rw::scale(matrix->at, -(LENGTH + CONE_SIZE));
	shape[NUMVERTS*2].setX(pos->x + at.x);
	shape[NUMVERTS*2].setY(pos->y + at.y);
	shape[NUMVERTS*2].setZ(pos->z + at.z);

	for(i = 0; i < NUMVERTS; i++){
		indices[i*3+0] = NUMVERTS*2;
		indices[i*3+1] = (i+1)*2 + 1;
		indices[i*3+2] = i*2 + 1;
	}
	indices[NUMVERTS*3-2] = 1;

	rw::im3d::Transform(shape, NUMVERTS*2+1, nil, rw::im3d::ALLOPAQUE);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, indices, NUMVERTS*3);
	rw::im3d::End();


	// top cap
	at = rw::scale(matrix->at, -(CONE_SIZE + 1.0f));
	shape[NUMVERTS*2].setX(pos->x + at.x);
	shape[NUMVERTS*2].setY(pos->y + at.y);
	shape[NUMVERTS*2].setZ(pos->z + at.z);

	for(i = 0; i < NUMVERTS; i++){
		indices[i*3+0] = NUMVERTS*2;
		indices[i*3+1] = i*2;
		indices[i*3+2] = (i+1)*2;
	}

	rw::im3d::Transform(shape, NUMVERTS*2+1, nil, rw::im3d::ALLOPAQUE);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, indices, NUMVERTS*3);
	rw::im3d::End();


	// cone
	DrawCone(CONE_ANGLE, CONE_SIZE, -2.0f);
}

void
DrawCurrentLight(void)
{
	rw::SetRenderState(rw::TEXTURERASTER, nil);
	rw::SetRenderState(rw::CULLMODE, rw::CULLBACK);
	rw::SetRenderState(rw::ZTESTENABLE, 1);

	switch(LightTypeIndex){
	case 1: DrawPointLight(); break;
	case 2: DrawDirectLight(); break;
	case 3:
	case 4: DrawCone(LightConeAngle, LightRadius*POINT_LIGHT_RADIUS_FACTOR, 1.0f); break;
	}
}



void
LightRotate(float xAngle, float yAngle)
{
	if(CurrentLight == nil || CurrentLight == AmbientLight || CurrentLight == PointLight)
		return;

	rw::Matrix *cameraMatrix = &Camera->getFrame()->matrix;
	rw::Frame *lightFrame = CurrentLight->getFrame();
	rw::V3d pos = lightFrame->matrix.pos;

	pos = rw::scale(pos, -1.0f);
	lightFrame->translate(&pos, rw::COMBINEPOSTCONCAT);

	lightFrame->rotate(&cameraMatrix->up, xAngle, rw::COMBINEPOSTCONCAT);
	lightFrame->rotate(&cameraMatrix->right, yAngle, rw::COMBINEPOSTCONCAT);

	pos = rw::scale(pos, -1.0f);
	lightFrame->translate(&pos, rw::COMBINEPOSTCONCAT);
}

void
ClampPosition(rw::V3d *pos, rw::V3d *delta, rw::BBox *bbox)
{
	if(pos->x + delta->x < bbox->inf.x)
		delta->x = bbox->inf.x - pos->x;
	else if(pos->x + delta->x > bbox->sup.x)
		delta->x = bbox->sup.x - pos->x;

	if(pos->y + delta->y < bbox->inf.y)
		delta->y = bbox->inf.y - pos->y;
	else if(pos->y + delta->y > bbox->sup.y)
		delta->y = bbox->sup.y - pos->y;

	if(pos->z + delta->z < bbox->inf.z)
		delta->z = bbox->inf.z - pos->z;
	else if(pos->z + delta->z > bbox->sup.z)
		delta->z = bbox->sup.z - pos->z;
}

void
LightTranslateXY(float xDelta, float yDelta)
{
	if(CurrentLight == nil || CurrentLight == AmbientLight || CurrentLight == DirectLight)
		return;

	rw::Matrix *cameraMatrix = &Camera->getFrame()->matrix;
	rw::Frame *lightFrame = CurrentLight->getFrame();
	rw::V3d right = rw::scale(cameraMatrix->right, xDelta);
	rw::V3d up = rw::scale(cameraMatrix->up, yDelta);
	rw::V3d delta = rw::add(right, up);

	ClampPosition(&lightFrame->matrix.pos, &delta, &RoomBBox);

	lightFrame->translate(&delta, rw::COMBINEPOSTCONCAT);
}

void
LightTranslateZ(float zDelta)
{
	if(CurrentLight == nil || CurrentLight == AmbientLight || CurrentLight == DirectLight)
		return;

	rw::Matrix *cameraMatrix = &Camera->getFrame()->matrix;
	rw::Frame *lightFrame = CurrentLight->getFrame();
	rw::V3d delta = rw::scale(cameraMatrix->at, zDelta);

	ClampPosition(&lightFrame->matrix.pos, &delta, &RoomBBox);

	lightFrame->translate(&delta, rw::COMBINEPOSTCONCAT);
}
