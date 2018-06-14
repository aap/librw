#include <rw.h>
#include <skeleton.h>

using namespace rw;

//
// This is a test to implement T&L in software and render with Im2D
//

#define MAX_LIGHTS 8

struct Directional {
	V3d at;
	RGBAf color;
};
static Directional directionals[MAX_LIGHTS];
static int32 numDirectionals;
static RGBAf ambLight;

static void
enumLights(Matrix *lightmat)
{
	int32 n;
	World *world;

	world = (World*)engine->currentWorld;
	ambLight.red = 0.0;
	ambLight.green = 0.0;
	ambLight.blue = 0.0;
	ambLight.alpha = 0.0;
	numDirectionals = 0;
	// only unpositioned lights right now
	FORLIST(lnk, world->directionalLights){
		Light *l = Light::fromWorld(lnk);
		if(l->getType() == Light::DIRECTIONAL){
			if(numDirectionals >= MAX_LIGHTS)
				continue;
			n = numDirectionals++;
			V3d::transformVectors(&directionals[n].at, &l->getFrame()->getLTM()->at, 1, lightmat);
			directionals[n].color = l->color;
			directionals[n].color.alpha = 0.0f;
		}else if(l->getType() == Light::AMBIENT){
			ambLight.red   += l->color.red;
			ambLight.green += l->color.green;
			ambLight.blue  += l->color.blue;
		}
	}
}

static void
drawAtomic(Atomic *a)
{
	using namespace RWDEVICE;
	Im2DVertex *im2dverts;
	V3d *xvert;
	Matrix xform;
	Matrix lightmat;
	Camera *cam = (Camera*)engine->currentCamera;
	Geometry *g = a->geometry;
	MeshHeader *mh = g->meshHeader;
	Mesh *m = mh->getMeshes();
	int32 width = cam->frameBuffer->width;
	int32 height = cam->frameBuffer->height;
	RGBA *prelight;
	V3d *normals;
	TexCoords *texcoords;

	Matrix::mult(&xform, a->getFrame()->getLTM(), &cam->viewMatrix);
	Matrix::invert(&lightmat, a->getFrame()->getLTM());

	enumLights(&lightmat);

	xvert = rwNewT(V3d, g->numVertices, MEMDUR_FUNCTION);
	im2dverts = rwNewT(Im2DVertex, g->numVertices, MEMDUR_FUNCTION);

	prelight = g->colors;
	normals = g->morphTargets[0].normals;
	texcoords = g->texCoords[0];

	V3d::transformPoints(xvert, g->morphTargets[0].vertices, g->numVertices, &xform);
	for(int32 i = 0; i < g->numVertices; i++){
		float32 recipZ = 1.0f/xvert[i].z;

		im2dverts[i].setScreenX(xvert[i].x * recipZ * width);
		im2dverts[i].setScreenY((xvert[i].y * recipZ * height));
		im2dverts[i].setScreenZ(recipZ * cam->zScale + cam->zShift);
		im2dverts[i].setCameraZ(xvert[i].z);
		im2dverts[i].setRecipCameraZ(recipZ);
		im2dverts[i].setColor(255, 0, 0, 255);
		im2dverts[i].setU(texcoords[i].u, recipZ);
		im2dverts[i].setV(texcoords[i].v, recipZ);
	}
	for(int32 i = 0; i < mh->numMeshes; i++){
		for(uint32 j = 0; j < m[i].numIndices; j++){
			int32 idx = m[i].indices[j];
			RGBA col;
			RGBAf colf, color;
			if(prelight)
				convColor(&color, &prelight[idx]);
			else{
				color.red = color.green = color.blue = 0.0f;
				color.alpha = 1.0f;
			}
			color = add(color, ambLight);
			if(normals)
				for(int32 k = 0; k < numDirectionals; k++){
					float32 f = dot(normals[idx], neg(directionals[k].at));
					if(f <= 0.0f) continue;
					colf = scale(directionals[k].color, f);
					color = add(color, colf);
				}
			convColor(&colf, &m[i].material->color);
			color = modulate(color, colf);
			clamp(&color);
			convColor(&col, &color);
			im2dverts[idx].setColor(col.red, col.green, col.blue, col.alpha);
		}

		engine->imtexture = m[i].material->texture;
		im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST,
			im2dverts, g->numVertices, m[i].indices, m[i].numIndices);
	}

	rwFree(xvert);
	rwFree(im2dverts);
}

void
tlTest(Clump *clump)
{
	FORLIST(lnk, clump->atomics){
		Atomic *a = Atomic::fromClump(lnk);
		drawAtomic(a);
	}
}

static RWDEVICE::Im2DVertex *clipverts;
static int32 numClipverts;

void
genIm3DTransform(void *vertices, int32 numVertices, Matrix *world)
{
	using namespace RWDEVICE;
	Im3DVertex *objverts;
	V3d pos;
	Matrix xform;
	Camera *cam;
	int32 i;
	objverts = (Im3DVertex*)vertices;

	cam = (Camera*)engine->currentCamera;
	int32 width = cam->frameBuffer->width;
	int32 height = cam->frameBuffer->height;


	xform = cam->viewMatrix;
	if(world)
		xform.transform(world, COMBINEPRECONCAT);

	clipverts = rwNewT(Im2DVertex, numVertices, MEMDUR_EVENT);
	numClipverts = numVertices;

	for(i = 0; i < numVertices; i++){
		V3d::transformPoints(&pos, &objverts[i].position, 1, &xform);

		float32 recipZ = 1.0f/pos.z;
		RGBA c = objverts[i].getColor();

		clipverts[i].setScreenX(pos.x * recipZ * width);
		clipverts[i].setScreenY((pos.y * recipZ * height));
		clipverts[i].setScreenZ(recipZ * cam->zScale + cam->zShift);
		clipverts[i].setCameraZ(pos.z);
		clipverts[i].setRecipCameraZ(recipZ);
		clipverts[i].setColor(c.red, c.green, c.blue, c.alpha);
		clipverts[i].setU(objverts[i].u, recipZ);
		clipverts[i].setV(objverts[i].v, recipZ);
	}
}

void
genIm3DRenderIndexed(PrimitiveType prim, void *indices, int32 numIndices)
{
	im2d::RenderIndexedPrimitive(prim, clipverts, numClipverts, indices, numIndices);
}

void
genIm3DEnd(void)
{
	rwFree(clipverts);
	clipverts = nil;
	numClipverts = 0;
}
