#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <stdint.h>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"
#include "rwps2.h"
#include "rwogl.h"

using namespace std;

namespace rw {

//
// HAnim
//

int32 hAnimOffset;

static void*
createHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	hanim->id = -1;
	hanim->hierarchy = NULL;
	return object;
}

static void*
destroyHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	if(hanim->hierarchy){
		HAnimHierarchy *hier = hanim->hierarchy;
		delete[] (uint8*)hier->matricesUnaligned;
		delete[] hier->nodeInfo;
		delete hier;
	}
	hanim->id = -1;
	hanim->hierarchy = NULL;
	return object;
}

static void*
copyHAnim(void *dst, void *src, int32 offset, int32)
{
	HAnimData *dsthanim = PLUGINOFFSET(HAnimData, dst, offset);
	HAnimData *srchanim = PLUGINOFFSET(HAnimData, src, offset);
	dsthanim->id = srchanim->id;
	// TODO
	dsthanim->hierarchy = NULL;
	return dst;
}

static void
readHAnim(Stream *stream, int32, void *object, int32 offset, int32)
{
	int32 cnst, numNodes;
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	cnst = stream->readI32();
	if(cnst != 256){
		printf("hanim const was not 256\n");
		return;
	}
	hanim->id = stream->readI32();
	numNodes = stream->readI32();
	if(numNodes != 0){
		HAnimHierarchy *hier = new HAnimHierarchy;
		hanim->hierarchy = hier;
		hier->numNodes = numNodes;
		hier->flags = stream->readI32();
		hier->maxInterpKeyFrameSize = stream->readI32();
		hier->parentFrame = (Frame*)object;
		hier->parentHierarchy = hier;
		if(hier->flags & 2)
			hier->matrices = hier->matricesUnaligned = NULL;
		else{
			hier->matricesUnaligned =
			  (float*) new uint8[hier->numNodes*64 + 15];
			hier->matrices =
			  (float*)((uintptr)hier->matricesUnaligned & ~0xF);
		}
		hier->nodeInfo = new HAnimNodeInfo[hier->numNodes];
		for(int32 i = 0; i < hier->numNodes; i++){
			hier->nodeInfo[i].id = stream->readI32();
			hier->nodeInfo[i].index = stream->readI32();
			hier->nodeInfo[i].flags = stream->readI32();
			hier->nodeInfo[i].frame = NULL;
		}
	}
}

static void
writeHAnim(Stream *stream, int32, void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	stream->writeI32(256);
	stream->writeI32(hanim->id);
	if(hanim->hierarchy == NULL){
		stream->writeI32(0);
		return;
	}
	HAnimHierarchy *hier = hanim->hierarchy;
	stream->writeI32(hier->numNodes);
	stream->writeI32(hier->flags);
	stream->writeI32(hier->maxInterpKeyFrameSize);
	for(int32 i = 0; i < hier->numNodes; i++){
		stream->writeI32(hier->nodeInfo[i].id);
		stream->writeI32(hier->nodeInfo[i].index);
		stream->writeI32(hier->nodeInfo[i].flags);
	}
}

static int32
getSizeHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	if(hanim->hierarchy)
		return 12 + 8 + hanim->hierarchy->numNodes*12;
	return 12;
}

void
registerHAnimPlugin(void)
{
	hAnimOffset = Frame::registerPlugin(sizeof(HAnimData), ID_HANIMPLUGIN,
	                                    createHAnim,
	                                    destroyHAnim, copyHAnim);
	Frame::registerPluginStream(ID_HANIMPLUGIN,
	                            readHAnim,
	                            writeHAnim,
	                            getSizeHAnim);
}


//
// Geometry
//

// Mesh

static void
readMesh(Stream *stream, int32 len, void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	int32 indbuf[256];
	uint32 buf[3];
	stream->read(buf, 12);
	geo->meshHeader = new MeshHeader;
	geo->meshHeader->flags = buf[0];
	geo->meshHeader->numMeshes = buf[1];
	geo->meshHeader->totalIndices = buf[2];
	geo->meshHeader->mesh = new Mesh[geo->meshHeader->numMeshes];
	Mesh *mesh = geo->meshHeader->mesh;
	bool hasData = len > 12+geo->meshHeader->numMeshes*8;
	for(uint32 i = 0; i < geo->meshHeader->numMeshes; i++){
		stream->read(buf, 8);
		mesh->numIndices = buf[0];
		mesh->material = geo->materialList[buf[1]];
		mesh->indices = NULL;
		if(geo->geoflags & Geometry::NATIVE){
			// OpenGL stores uint16 indices here
			if(hasData){
				mesh->indices = new uint16[mesh->numIndices];
				stream->read(mesh->indices,
				            mesh->numIndices*2);
			}
		}else{
			mesh->indices = new uint16[mesh->numIndices];
			uint16 *ind = mesh->indices;
			int32 numIndices = mesh->numIndices;
			for(; numIndices > 0; numIndices -= 256){
				int32 n = numIndices < 256 ? numIndices : 256;
				stream->read(indbuf, n*4);
				for(int32 j = 0; j < n; j++)
					ind[j] = indbuf[j];
				ind += n;
			}
		}
		mesh++;
	}
}

static void
writeMesh(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	int32 indbuf[256];
	uint32 buf[3];
	buf[0] = geo->meshHeader->flags;
	buf[1] = geo->meshHeader->numMeshes;
	buf[2] = geo->meshHeader->totalIndices;
	stream->write(buf, 12);
	Mesh *mesh = geo->meshHeader->mesh;
	for(uint32 i = 0; i < geo->meshHeader->numMeshes; i++){
		buf[0] = mesh->numIndices;
		buf[1] = findPointer((void*)mesh->material,
		                     (void**)geo->materialList,
		                     geo->numMaterials);
		stream->write(buf, 8);
		if(geo->geoflags & Geometry::NATIVE){
			assert(geo->instData != NULL);
			if(geo->instData->platform == PLATFORM_OGL)
				stream->write(mesh->indices,
				            mesh->numIndices*2);
		}else{
			uint16 *ind = mesh->indices;
			int32 numIndices = mesh->numIndices;
			for(; numIndices > 0; numIndices -= 256){
				int32 n = numIndices < 256 ? numIndices : 256;
				for(int32 j = 0; j < n; j++)
					indbuf[j] = ind[j];
				stream->write(indbuf, n*4);
				ind += n;
			}
		}
		mesh++;
	}
}

static int32
getSizeMesh(void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	if(geo->meshHeader == NULL)
		return -1;
	int32 size = 12 + geo->meshHeader->numMeshes*8;
	if(geo->geoflags & Geometry::NATIVE){
		assert(geo->instData != NULL);
		if(geo->instData->platform == PLATFORM_OGL)
			size += geo->meshHeader->totalIndices*2;
	}else{
		size += geo->meshHeader->totalIndices*4;
	}
	return size;
}

void
registerMeshPlugin(void)
{
	Geometry::registerPlugin(0, 0x50E, NULL, NULL, NULL);
	Geometry::registerPluginStream(0x50E, readMesh, writeMesh, getSizeMesh);
}

// Native Data

static void*
destroyNativeData(void *object, int32 offset, int32 size)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == NULL)
		return object;
	if(geometry->instData->platform == PLATFORM_PS2)
		return ps2::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_OGL)
		return gl::destroyNativeData(object, offset, size);
	return object;
}

static void
readNativeData(Stream *stream, int32 len, void *object, int32 o, int32 s)
{
	ChunkHeaderInfo header;
	uint32 libid;
	uint32 platform;
	// ugly hack to find out platform
	stream->seek(-4);
	libid = stream->readU32();
	readChunkHeaderInfo(stream, &header);
	if(header.type == ID_STRUCT && 
	   libraryIDPack(header.version, header.build) == libid){
		// must be PS2 or Xbox
		platform = stream->readU32();
		stream->seek(-16);
		if(platform == PLATFORM_PS2)
			ps2::readNativeData(stream, len, object, o, s);
		else if(platform == PLATFORM_XBOX)
			stream->seek(len);
	}else{
		stream->seek(-12);
		gl::readNativeData(stream, len, object, o, s);
	}
}

static void
writeNativeData(Stream *stream, int32 len, void *object, int32 o, int32 s)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == NULL)
		return;
	if(geometry->instData->platform == PLATFORM_PS2)
		ps2::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_OGL)
		gl::writeNativeData(stream, len, object, o, s);
}

static int32
getSizeNativeData(void *object, int32 offset, int32 size)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == NULL)
		return -1;
	if(geometry->instData->platform == PLATFORM_PS2)
		return ps2::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_XBOX)
		return -1;
	else if(geometry->instData->platform == PLATFORM_OGL)
		return gl::getSizeNativeData(object, offset, size);
	return -1;
}

void
registerNativeDataPlugin(void)
{
	Geometry::registerPlugin(0, ID_NATIVEDATA,
	                         NULL, destroyNativeData, NULL);
	Geometry::registerPluginStream(ID_NATIVEDATA,
	                               readNativeData,
	                               writeNativeData,
	                               getSizeNativeData);
}

//
// Skin
//

SkinGlobals skinGlobals = { 0, NULL };

static void*
createSkin(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(Skin*, object, offset) = NULL;
	return object;
}

static void*
destroySkin(void *object, int32 offset, int32)
{
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	if(skin)
		delete[] skin->data;
	delete skin;
	return object;
}

static void*
copySkin(void *dst, void *src, int32 offset, int32)
{
	Skin *srcskin = *PLUGINOFFSET(Skin*, src, offset);
	if(srcskin == NULL)
		return dst;
	Geometry *geometry = (Geometry*)src;
	assert(geometry->instData == NULL);
	assert(((Geometry*)src)->numVertices == ((Geometry*)dst)->numVertices);
	Skin *dstskin = new Skin;
	*PLUGINOFFSET(Skin*, dst, offset) = dstskin;
	dstskin->numBones = srcskin->numBones;
	dstskin->numUsedBones = srcskin->numUsedBones;
	dstskin->maxIndex = srcskin->maxIndex;
	uint32 size = srcskin->numUsedBones +
	              srcskin->numBones*64 +
	              geometry->numVertices*(16+4) + 15;
	uint8 *data = new uint8[size];
	dstskin->data = data;
	memcpy(dstskin->data, srcskin->data, size);

	dstskin->usedBones = NULL;
	if(srcskin->usedBones){
		dstskin->usedBones = data;
		data += dstskin->numUsedBones;
	}

	uintptr ptr = (uintptr)data + 15;
	ptr &= ~0xF;
	data = (uint8*)ptr;
	dstskin->inverseMatrices = NULL;
	if(srcskin->inverseMatrices){
		dstskin->inverseMatrices = (float*)data;
		data += 64*dstskin->numBones;
	}

	dstskin->indices = NULL;
	if(srcskin->indices){
		dstskin->indices = data;
		data += 4*geometry->numVertices;
	}

	dstskin->weights = NULL;
	if(srcskin->weights)
		dstskin->weights = (float*)data;

	return dst;
}

static void
readSkin(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	uint8 header[4];
	Geometry *geometry = (Geometry*)object;

	if(geometry->instData){
		if(geometry->instData->platform == PLATFORM_PS2)
			ps2::readNativeSkin(stream, len, object, offset);
		else if(geometry->instData->platform == PLATFORM_OGL)
			gl::readNativeSkin(stream, len, object, offset);
		else
			assert(0 && "unsupported native skin platform");
		return;
	}

	stream->read(header, 4);
	Skin *skin = new Skin;
	*PLUGINOFFSET(Skin*, geometry, offset) = skin;
	skin->numBones = header[0];

	// both values unused in/before 33002, used in/after 34003
	skin->numUsedBones = header[1];
	skin->maxIndex = header[2];

	bool oldFormat = skin->numUsedBones == 0;
	uint32 size = skin->numUsedBones +
	              skin->numBones*64 +
	              geometry->numVertices*(16+4) + 15;
	uint8 *data = new uint8[size];
	skin->data = data;

	skin->usedBones = NULL;
	if(skin->numUsedBones){
		skin->usedBones = data;
		data += skin->numUsedBones;
	}

	uintptr ptr = (uintptr)data + 15;
	ptr &= ~0xF;
	data = (uint8*)ptr;
	skin->inverseMatrices = NULL;
	if(skin->numBones){
		skin->inverseMatrices = (float*)data;
		data += 64*skin->numBones;
	}

	skin->indices = NULL;
	if(geometry->numVertices){
		skin->indices = data;
		data += 4*geometry->numVertices;
	}

	skin->weights = NULL;
	if(geometry->numVertices)
		skin->weights = (float*)data;

	if(skin->usedBones)
		stream->read(skin->usedBones, skin->numUsedBones);
	if(skin->indices)
		stream->read(skin->indices, geometry->numVertices*4);
	if(skin->weights)
		stream->read(skin->weights, geometry->numVertices*16);
	for(int32 i = 0; i < skin->numBones; i++){
		if(oldFormat)
			stream->seek(4);	// skip 0xdeaddead
		stream->read(&skin->inverseMatrices[i*16], 64);
	}
	// TODO: find out what this is (related to skin splitting)
	// always 0 in GTA files
	if(!oldFormat)
		stream->seek(12);
}

static void
writeSkin(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	uint8 header[4];
	Geometry *geometry = (Geometry*)object;

	if(geometry->instData){
		if(geometry->instData->platform == PLATFORM_PS2)
			ps2::writeNativeSkin(stream, len, object, offset);
		else if(geometry->instData->platform == PLATFORM_OGL)
			gl::writeNativeSkin(stream, len, object, offset);
		else
			assert(0 && "unsupported native skin platform");
		return;
	}

	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	bool oldFormat = version < 0x34003;
	header[0] = skin->numBones;
	header[1] = skin->numUsedBones;
	header[2] = skin->maxIndex;
	header[3] = 0;
	if(oldFormat){
		header[1] = 0;
		header[2] = 0;
	}
	stream->write(header, 4);
	if(!oldFormat)
		stream->write(skin->usedBones, skin->numUsedBones);
	stream->write(skin->indices, geometry->numVertices*4);
	stream->write(skin->weights, geometry->numVertices*16);
	for(int32 i = 0; i < skin->numBones; i++){
		if(oldFormat)
			stream->writeU32(0xdeaddead);
		stream->write(&skin->inverseMatrices[i*16], 64);
	}
	if(!oldFormat){
		uint32 buffer[3] = { 0, 0, 0};
		stream->write(buffer, 12);
	}
}

static int32
getSizeSkin(void *object, int32 offset, int32)
{
	Geometry *geometry = (Geometry*)object;

	if(geometry->instData){
		if(geometry->instData->platform == PLATFORM_PS2)
			return ps2::getSizeNativeSkin(object, offset);
		if(geometry->instData->platform == PLATFORM_OGL)
			return gl::getSizeNativeSkin(object, offset);
		assert(0 && "unsupported native skin platform");
	}

	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	if(skin == NULL)
		return -1;

	int32 size = 4 + geometry->numVertices*(16+4) +
	             skin->numBones*64;
	// not sure which version introduced the new format
	if(version < 0x34003)
		size += skin->numBones*4;
	else
		size += skin->numUsedBones + 12;
	return size;
}

static void
skinRights(void *object, int32, int32, uint32 data)
{
	((Atomic*)object)->pipeline = skinGlobals.pipeline;
}

void
registerSkinPlugin(void)
{
	skinGlobals.pipeline = new Pipeline;
	skinGlobals.pipeline->pluginID = ID_SKIN;
	skinGlobals.pipeline->pluginData = 1;

	skinGlobals.offset = Geometry::registerPlugin(sizeof(Skin*), ID_SKIN,
	                                              createSkin,
	                                              destroySkin,
	                                              copySkin);
	Geometry::registerPluginStream(ID_SKIN,
	                               readSkin, writeSkin, getSizeSkin);
	Atomic::registerPlugin(0, ID_SKIN, NULL, NULL, NULL);
	Atomic::setStreamRightsCallback(ID_SKIN, skinRights);
}

//
// MatFX
//

// Atomic

static void*
createAtomicMatFX(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(int32, object, offset) = 0;
	return object;
}

static void*
copyAtomicMatFX(void *dst, void *src, int32 offset, int32)
{
	*PLUGINOFFSET(int32, dst, offset) = *PLUGINOFFSET(int32, src, offset);
	return dst;
}

static void
readAtomicMatFX(Stream *stream, int32, void *object, int32 offset, int32)
{
	int32 flag;
	stream->read(&flag, 4);
	*PLUGINOFFSET(int32, object, offset) = flag;
	if(flag)
		((Atomic*)object)->pipeline = matFXGlobals.pipeline;
}

static void
writeAtomicMatFX(Stream *stream, int32, void *object, int32 offset, int32)
{
	int32 flag;
	flag = *PLUGINOFFSET(int32, object, offset);
	stream->writeI32(flag);
}

static int32
getSizeAtomicMatFX(void *object, int32 offset, int32)
{
	int32 flag;
	flag = *PLUGINOFFSET(int32, object, offset);
	return flag ? 4 : -1;
}

// Material

MatFXGlobals matFXGlobals = { 0, 0, NULL };

// TODO: Frames and Matrices?
static void
clearMatFX(MatFX *matfx)
{
	for(int i = 0; i < 2; i++)
		switch(matfx->fx[i].type){
		case MatFX::BUMPMAP:
			if(matfx->fx[i].bump.bumpedTex)
				matfx->fx[i].bump.bumpedTex->decRef();
			if(matfx->fx[i].bump.tex)
				matfx->fx[i].bump.tex->decRef();
			break;

		case MatFX::ENVMAP:
			if(matfx->fx[i].env.tex)
				matfx->fx[i].env.tex->decRef();
			break;

		case MatFX::DUAL:
			if(matfx->fx[i].dual.tex)
				matfx->fx[i].dual.tex->decRef();
			break;
		}
	memset(matfx, 0, sizeof(MatFX));
}

void
MatFX::setEffects(uint32 flags)
{
	if(this->flags != 0 && this->flags != flags)
		clearMatFX(this);
	this->flags = flags;
	switch(flags){
	case BUMPMAP:
	case ENVMAP:
	case DUAL:
	case UVTRANSFORM:
		this->fx[0].type = flags;
		this->fx[1].type = NOTHING;
		break;

	case BUMPENVMAP:
		this->fx[0].type = BUMPMAP;
		this->fx[1].type = ENVMAP;
		break;

	case DUALUVTRANSFORM:
		this->fx[0].type = UVTRANSFORM;
		this->fx[1].type = DUAL;
		break;
	}
}

int32
MatFX::getEffectIndex(uint32 type)
{
	for(int i = 0; i < 2; i++)
		if(this->fx[i].type == type)
			return i;
	return -1;
}

static void*
createMaterialMatFX(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(MatFX*, object, offset) = NULL;
	return object;
}

static void*
destroyMaterialMatFX(void *object, int32 offset, int32)
{
	MatFX *matfx = *PLUGINOFFSET(MatFX*, object, offset);
	if(matfx){
		clearMatFX(matfx);
		delete matfx;
	}
	return object;
}

static void*
copyMaterialMatFX(void *dst, void *src, int32 offset, int32)
{
	MatFX *srcfx = *PLUGINOFFSET(MatFX*, src, offset);
	if(srcfx == NULL)
		return dst;
	MatFX *dstfx = new MatFX;
	*PLUGINOFFSET(MatFX*, dst, offset) = dstfx;
	memcpy(dstfx, srcfx, sizeof(MatFX));
	for(int i = 0; i < 2; i++)
		switch(dstfx->fx[i].type){
		case MatFX::BUMPMAP:
			if(dstfx->fx[i].bump.bumpedTex)
				dstfx->fx[i].bump.bumpedTex->refCount++;
			if(dstfx->fx[i].bump.tex)
				dstfx->fx[i].bump.tex->refCount++;
			break;

		case MatFX::ENVMAP:
			if(dstfx->fx[i].env.tex)
				dstfx->fx[i].env.tex->refCount++;
			break;

		case MatFX::DUAL:
			if(dstfx->fx[i].dual.tex)
				dstfx->fx[i].dual.tex->refCount++;
			break;
		}
	return dst;
}

static void
readMaterialMatFX(Stream *stream, int32, void *object, int32 offset, int32)
{
	Texture *tex, *bumpedTex;
	float coefficient;
	int32 fbAlpha;
	int32 srcBlend, dstBlend;
	int32 idx;

	MatFX *matfx = new MatFX;
	memset(matfx, 0, sizeof(MatFX));
	*PLUGINOFFSET(MatFX*, object, offset) = matfx;
	matfx->setEffects(stream->readU32());

	for(int i = 0; i < 2; i++){
		uint32 type = stream->readU32();
		switch(type){
		case MatFX::BUMPMAP:
			coefficient = stream->readF32();
			bumpedTex = tex = NULL;
			if(stream->readI32()){
				assert(findChunk(stream, ID_TEXTURE,
				       NULL, NULL));
				bumpedTex = Texture::streamRead(stream);
			}
			if(stream->readI32()){
				assert(findChunk(stream, ID_TEXTURE,
				       NULL, NULL));
				tex = Texture::streamRead(stream);
			}
			idx = matfx->getEffectIndex(type);
			assert(idx >= 0);
			matfx->fx[idx].bump.bumpedTex = bumpedTex;
			matfx->fx[idx].bump.tex = tex;
			matfx->fx[idx].bump.coefficient = coefficient;
			break;

		case MatFX::ENVMAP:
			coefficient = stream->readF32();
			fbAlpha = stream->readI32();
			tex = NULL;
			if(stream->readI32()){
				assert(findChunk(stream, ID_TEXTURE,
				       NULL, NULL));
				tex = Texture::streamRead(stream);
			}
			idx = matfx->getEffectIndex(type);
			assert(idx >= 0);
			matfx->fx[idx].env.tex = tex;
			matfx->fx[idx].env.fbAlpha = fbAlpha;
			matfx->fx[idx].env.coefficient = coefficient;
			break;

		case MatFX::DUAL:
			srcBlend = stream->readI32();
			dstBlend = stream->readI32();
			tex = NULL;
			if(stream->readI32()){
				assert(findChunk(stream, ID_TEXTURE,
				       NULL, NULL));
				tex = Texture::streamRead(stream);
			}
			idx = matfx->getEffectIndex(type);
			assert(idx >= 0);
			matfx->fx[idx].dual.tex = tex;
			matfx->fx[idx].dual.srcBlend = srcBlend;
			matfx->fx[idx].dual.dstBlend = dstBlend;
			break;
		}
	}
}

static void
writeMaterialMatFX(Stream *stream, int32, void *object, int32 offset, int32)
{
	MatFX *matfx = *PLUGINOFFSET(MatFX*, object, offset);

	stream->writeU32(matfx->flags);
	for(int i = 0; i < 2; i++){
		stream->writeU32(matfx->fx[i].type);
		switch(matfx->fx[i].type){
		case MatFX::BUMPMAP:
			stream->writeF32(matfx->fx[i].bump.coefficient);
			stream->writeI32(matfx->fx[i].bump.bumpedTex != NULL);
			if(matfx->fx[i].bump.bumpedTex)
				matfx->fx[i].bump.bumpedTex->streamWrite(stream);
			stream->writeI32(matfx->fx[i].bump.tex != NULL);
			if(matfx->fx[i].bump.tex)
				matfx->fx[i].bump.tex->streamWrite(stream);
			break;

		case MatFX::ENVMAP:
			stream->writeF32(matfx->fx[i].env.coefficient);
			stream->writeI32(matfx->fx[i].env.fbAlpha);
			stream->writeI32(matfx->fx[i].env.tex != NULL);
			if(matfx->fx[i].env.tex)
				matfx->fx[i].env.tex->streamWrite(stream);
			break;

		case MatFX::DUAL:
			stream->writeI32(matfx->fx[i].dual.srcBlend);
			stream->writeI32(matfx->fx[i].dual.dstBlend);
			stream->writeI32(matfx->fx[i].dual.tex != NULL);
			if(matfx->fx[i].dual.tex)
				matfx->fx[i].dual.tex->streamWrite(stream);
			break;
		}
	}
}

static int32
getSizeMaterialMatFX(void *object, int32 offset, int32)
{
	MatFX *matfx = *PLUGINOFFSET(MatFX*, object, offset);
	if(matfx == NULL)
		return -1;
	int32 size = 4 + 4 + 4;

	for(int i = 0; i < 2; i++)
		switch(matfx->fx[i].type){
		case MatFX::BUMPMAP:
			size += 4 + 4 + 4;
			if(matfx->fx[i].bump.bumpedTex)
				size += 12 +
				  matfx->fx[i].bump.bumpedTex->streamGetSize();
			if(matfx->fx[i].bump.tex)
				size += 12 +
				  matfx->fx[i].bump.tex->streamGetSize();
			break;

		case MatFX::ENVMAP:
			size += 4 + 4 + 4;
			if(matfx->fx[i].env.tex)
				size += 12 +
				  matfx->fx[i].env.tex->streamGetSize();
			break;

		case MatFX::DUAL:
			size += 4 + 4 + 4;
			if(matfx->fx[i].dual.tex)
				size += 12 +
				  matfx->fx[i].dual.tex->streamGetSize();
			break;
		}
	return size;
}

void
registerMatFXPlugin(void)
{
	matFXGlobals.pipeline = new Pipeline;
	matFXGlobals.pipeline->pluginID = ID_MATFX;
	matFXGlobals.pipeline->pluginData = 0;

	matFXGlobals.atomicOffset =
	Atomic::registerPlugin(sizeof(int32), ID_MATFX,
	                       createAtomicMatFX, NULL, copyAtomicMatFX);
	Atomic::registerPluginStream(ID_MATFX,
	                             readAtomicMatFX,
	                             writeAtomicMatFX,
	                             getSizeAtomicMatFX);

	matFXGlobals.materialOffset =
	Material::registerPlugin(sizeof(MatFX*), ID_MATFX,
	                         createMaterialMatFX, destroyMaterialMatFX,
	                         copyMaterialMatFX);
	Material::registerPluginStream(ID_MATFX,
	                               readMaterialMatFX,
	                               writeMaterialMatFX,
	                               getSizeMaterialMatFX);
}

}
