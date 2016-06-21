#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwplugins.h"
#include "rwps2.h"
#include "rwps2plg.h"
#include "rwxbox.h"
#include "rwd3d8.h"
#include "rwd3d9.h"
#include "rwwdgl.h"

#define PLUGIN_ID 2

namespace rw {

//
// HAnim
//

int32 hAnimOffset;
bool32 hAnimDoStream = 1;

HAnimHierarchy*
HAnimHierarchy::create(int32 numNodes, int32 *nodeFlags, int32 *nodeIDs, int32 flags, int32 maxKeySize)
{
	HAnimHierarchy *hier = (HAnimHierarchy*)malloc(sizeof(*hier));

	hier->numNodes = numNodes;
	hier->flags = flags;
	hier->maxInterpKeyFrameSize = maxKeySize;
	hier->parentFrame = nil;
	hier->parentHierarchy = hier;
	if(hier->flags & 2)
		hier->matrices = hier->matricesUnaligned = nil;
	else{
		hier->matricesUnaligned =
		  (float*) new uint8[hier->numNodes*64 + 15];
		hier->matrices =
		  (float*)((uintptr)hier->matricesUnaligned & ~0xF);
	}
	hier->nodeInfo = new HAnimNodeInfo[hier->numNodes];
	for(int32 i = 0; i < hier->numNodes; i++){
		hier->nodeInfo[i].id = nodeIDs[i];
		hier->nodeInfo[i].index = i;
		hier->nodeInfo[i].flags = nodeFlags[i];
		hier->nodeInfo[i].frame = nil;
	}
	return hier;
}

void
HAnimHierarchy::destroy(void)
{
	delete[] (uint8*)this->matricesUnaligned;
	delete[] this->nodeInfo;
	free(this);
}

static Frame*
findById(Frame *f, int32 id)
{
	if(f == nil) return nil;
	HAnimData *hanim = HAnimData::get(f);
	if(hanim->id >= 0 && hanim->id == id) return f;
	Frame *ff = findById(f->next, id);
	if(ff) return ff;
	return findById(f->child, id);
}

void
HAnimHierarchy::attachByIndex(int32 idx)
{
	int32 id = this->nodeInfo[idx].id;
	Frame *f = findById(this->parentFrame, id);
	this->nodeInfo[idx].frame = f;
}

void
HAnimHierarchy::attach(void)
{
	for(int32 i = 0; i < this->numNodes; i++)
		this->attachByIndex(i);
}

int32
HAnimHierarchy::getIndex(int32 id)
{
	for(int32 i = 0; i < this->numNodes; i++)
		if(this->nodeInfo[i].id == id)
			return i;
	return -1;
}

HAnimHierarchy*
HAnimHierarchy::get(Frame *f)
{
	return HAnimData::get(f)->hierarchy;
}

HAnimHierarchy*
HAnimHierarchy::find(Frame *f)
{
	if(f == nil) return nil;
	HAnimHierarchy *hier = HAnimHierarchy::get(f);
	if(hier) return hier;
	hier = HAnimHierarchy::find(f->next);
	if(hier) return hier;
	return HAnimHierarchy::find(f->child);
}

HAnimData*
HAnimData::get(Frame *f)
{
	return PLUGINOFFSET(HAnimData, f, hAnimOffset);
}

static void*
createHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	hanim->id = -1;
	hanim->hierarchy = nil;
	return object;
}

static void*
destroyHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	if(hanim->hierarchy)
		hanim->hierarchy->destroy();
	hanim->id = -1;
	hanim->hierarchy = nil;
	return object;
}

static void*
copyHAnim(void *dst, void *src, int32 offset, int32)
{
	HAnimData *dsthanim = PLUGINOFFSET(HAnimData, dst, offset);
	HAnimData *srchanim = PLUGINOFFSET(HAnimData, src, offset);
	dsthanim->id = srchanim->id;
	// TODO
	dsthanim->hierarchy = nil;
	return dst;
}

static Stream*
readHAnim(Stream *stream, int32, void *object, int32 offset, int32)
{
	int32 ver, numNodes;
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	ver = stream->readI32();
	assert(ver == 0x100);
	hanim->id = stream->readI32();
	numNodes = stream->readI32();
	if(numNodes != 0){
		int32 flags = stream->readI32();
		int32 maxKeySize = stream->readI32();
		int32 *nodeFlags = new int32[numNodes];
		int32 *nodeIDs = new int32[numNodes];
		for(int32 i = 0; i < numNodes; i++){
			nodeIDs[i] = stream->readI32();
			stream->readI32();	// index...unused
			nodeFlags[i] = stream->readI32();
		}
		hanim->hierarchy = HAnimHierarchy::create(numNodes,
			nodeFlags, nodeIDs, flags, maxKeySize);
		hanim->hierarchy->parentFrame = (Frame*)object;
		delete[] nodeFlags;
		delete[] nodeIDs;
	}
	return stream;
}

static Stream*
writeHAnim(Stream *stream, int32, void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	stream->writeI32(256);
	stream->writeI32(hanim->id);
	if(hanim->hierarchy == nil){
		stream->writeI32(0);
		return stream;
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
	return stream;
}

static int32
getSizeHAnim(void *object, int32 offset, int32)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, object, offset);
	if(!hAnimDoStream ||
	   version >= 0x35000 && hanim->id == -1 && hanim->hierarchy == nil)
		return 0;
	if(hanim->hierarchy)
		return 12 + 8 + hanim->hierarchy->numNodes*12;
	return 12;
}

static void
hAnimFrameRead(Stream *stream, Animation *anim)
{
	HAnimKeyFrame *frames = (HAnimKeyFrame*)anim->keyframes;
	for(int32 i = 0; i < anim->numFrames; i++){
		frames[i].time = stream->readF32();
		stream->read(frames[i].q, 4*4);
		stream->read(frames[i].t, 3*4);
		int32 prev = stream->readI32();
		frames[i].prev = &frames[prev];
	}
}

static void
hAnimFrameWrite(Stream *stream, Animation *anim)
{
	HAnimKeyFrame *frames = (HAnimKeyFrame*)anim->keyframes;
	for(int32 i = 0; i < anim->numFrames; i++){
		stream->writeF32(frames[i].time);
		stream->write(frames[i].q, 4*4);
		stream->write(frames[i].t, 3*4);
		stream->writeI32(frames[i].prev - frames);
	}
}

static uint32
hAnimFrameGetSize(Animation *anim)
{
	return anim->numFrames*(4 + 4*4 + 3*4 + 4);
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

	AnimInterpolatorInfo *info = new AnimInterpolatorInfo;
	info->id = 1;
	info->keyFrameSize = sizeof(HAnimKeyFrame);
	info->customDataSize = sizeof(HAnimKeyFrame);
	info->streamRead = hAnimFrameRead;
	info->streamWrite = hAnimFrameWrite;
	info->streamGetSize = hAnimFrameGetSize;
	registerAnimInterpolatorInfo(info);
}


//
// Geometry
//

// Mesh

static Stream*
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
	uint16 *p = nil;
	if(!(geo->geoflags & Geometry::NATIVE) || hasData)
		p = new uint16[geo->meshHeader->totalIndices];
	for(uint32 i = 0; i < geo->meshHeader->numMeshes; i++){
		stream->read(buf, 8);
		mesh->numIndices = buf[0];
		mesh->material = geo->materialList[buf[1]];
		mesh->indices = nil;
		if(geo->geoflags & Geometry::NATIVE){
			// OpenGL stores uint16 indices here
			if(hasData){
				mesh->indices = p;
				p += mesh->numIndices;
				stream->read(mesh->indices,
				            mesh->numIndices*2);
			}
		}else{
			mesh->indices = p;
			p += mesh->numIndices;
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
	return stream;
}

static Stream*
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
			assert(geo->instData != nil);
			if(geo->instData->platform == PLATFORM_WDGL)
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
	return stream;
}

static int32
getSizeMesh(void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	if(geo->meshHeader == nil)
		return -1;
	int32 size = 12 + geo->meshHeader->numMeshes*8;
	if(geo->geoflags & Geometry::NATIVE){
		assert(geo->instData != nil);
		if(geo->instData->platform == PLATFORM_WDGL)
			size += geo->meshHeader->totalIndices*2;
	}else{
		size += geo->meshHeader->totalIndices*4;
	}
	return size;
}

void
registerMeshPlugin(void)
{
	Geometry::registerPlugin(0, 0x50E, nil, nil, nil);
	Geometry::registerPluginStream(0x50E, readMesh, writeMesh, getSizeMesh);
}

MeshHeader::~MeshHeader(void)
{
	// first mesh holds pointer to all indices
	delete[] this->mesh[0].indices;
	delete[] this->mesh;
}

void
MeshHeader::allocateIndices(void)
{
	uint16 *p = new uint16[this->totalIndices];
	Mesh *mesh = this->mesh;
	for(uint32 i = 0; i < this->numMeshes; i++){
		mesh->indices = p;
		p += mesh->numIndices;
		mesh++;
	}
}

// Native Data

static void*
destroyNativeData(void *object, int32 offset, int32 size)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil)
		return object;
	if(geometry->instData->platform == PLATFORM_PS2)
		return ps2::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_WDGL)
		return wdgl::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_XBOX)
		return xbox::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_D3D8)
		return d3d8::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_D3D9)
		return d3d9::destroyNativeData(object, offset, size);
	return object;
}

static Stream*
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
		platform = stream->readU32();
		stream->seek(-16);
		if(platform == PLATFORM_PS2)
			return ps2::readNativeData(stream, len, object, o, s);
		else if(platform == PLATFORM_XBOX)
			return xbox::readNativeData(stream, len, object, o, s);
		else if(platform == PLATFORM_D3D8)
			return d3d8::readNativeData(stream, len, object, o, s);
		else if(platform == PLATFORM_D3D9)
			return d3d9::readNativeData(stream, len, object, o, s);
		else{
			fprintf(stderr, "unknown platform %d\n", platform);
			stream->seek(len);
		}
	}else{
		stream->seek(-12);
		wdgl::readNativeData(stream, len, object, o, s);
	}
	return stream;
}

static Stream*
writeNativeData(Stream *stream, int32 len, void *object, int32 o, int32 s)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil)
		return stream;
	if(geometry->instData->platform == PLATFORM_PS2)
		return ps2::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_WDGL)
		return wdgl::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_XBOX)
		return xbox::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_D3D8)
		return d3d8::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_D3D9)
		return d3d9::writeNativeData(stream, len, object, o, s);
	return stream;
}

static int32
getSizeNativeData(void *object, int32 offset, int32 size)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil)
		return 0;
	if(geometry->instData->platform == PLATFORM_PS2)
		return ps2::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_WDGL)
		return wdgl::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_XBOX)
		return xbox::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_D3D8)
		return d3d8::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_D3D9)
		return d3d9::getSizeNativeData(object, offset, size);
	return 0;
}

void
registerNativeDataPlugin(void)
{
	Geometry::registerPlugin(0, ID_NATIVEDATA,
	                         nil, destroyNativeData, nil);
	Geometry::registerPluginStream(ID_NATIVEDATA,
	                               readNativeData,
	                               writeNativeData,
	                               getSizeNativeData);
}

//
// Skin
//

SkinGlobals skinGlobals = { 0, { nil } };

static void*
createSkin(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(Skin*, object, offset) = nil;
	return object;
}

static void*
destroySkin(void *object, int32 offset, int32)
{
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	if(skin){
		delete[] skin->data;
//		delete[] skin->platformData;
	}
	delete skin;
	return object;
}

static void*
copySkin(void *dst, void *src, int32 offset, int32)
{
	Skin *srcskin = *PLUGINOFFSET(Skin*, src, offset);
	if(srcskin == nil)
		return dst;
	Geometry *geometry = (Geometry*)src;
	assert(geometry->instData == nil);
	assert(((Geometry*)src)->numVertices == ((Geometry*)dst)->numVertices);
	Skin *dstskin = new Skin;
	*PLUGINOFFSET(Skin*, dst, offset) = dstskin;
	dstskin->numBones = srcskin->numBones;
	dstskin->numUsedBones = srcskin->numUsedBones;
	dstskin->numWeights = srcskin->numWeights;

	assert(0 && "can't copy skin yet");
	dstskin->init(srcskin->numBones, srcskin->numUsedBones, geometry->numVertices);
	memcpy(dstskin->usedBones, srcskin->usedBones, srcskin->numUsedBones);
	memcpy(dstskin->inverseMatrices, srcskin->inverseMatrices,
	       srcskin->numBones*64);
	memcpy(dstskin->indices, srcskin->indices, geometry->numVertices*4);
	memcpy(dstskin->weights, srcskin->weights, geometry->numVertices*16);
	return dst;
}

static Stream*
readSkin(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	uint8 header[4];
	Geometry *geometry = (Geometry*)object;

	if(geometry->instData){
		// TODO: function pointers
		if(geometry->instData->platform == PLATFORM_PS2)
			return ps2::readNativeSkin(stream, len, object, offset);
		else if(geometry->instData->platform == PLATFORM_WDGL)
			return wdgl::readNativeSkin(stream, len, object, offset);
		else if(geometry->instData->platform == PLATFORM_XBOX)
			return xbox::readNativeSkin(stream, len, object, offset);
		else{
			assert(0 && "unsupported native skin platform");
			return nil;
		}
	}

	stream->read(header, 4);	// numBones, numUsedBones, numWeights, unused
	Skin *skin = new Skin;
	*PLUGINOFFSET(Skin*, geometry, offset) = skin;

	// numUsedBones and numWeights appear in/after 34003 but not in/before 33002
	// (probably rw::version >= 0x34000)
	bool oldFormat = header[1] == 0;

	// Use numBones for numUsedBones to allocate data, find out the correct value later
	if(oldFormat)
		skin->init(header[0], header[0], geometry->numVertices);
	else
		skin->init(header[0], header[1], geometry->numVertices);
	skin->numWeights = header[2];

	if(!oldFormat)
		stream->read(skin->usedBones, skin->numUsedBones);
	if(skin->indices)
		stream->read(skin->indices, geometry->numVertices*4);
	if(skin->weights)
		stream->read(skin->weights, geometry->numVertices*16);
	for(int32 i = 0; i < skin->numBones; i++){
		if(oldFormat)
			stream->seek(4);	// skip 0xdeaddead
		stream->read(&skin->inverseMatrices[i*16], 64);

		//{
		//float *mat = &skin->inverseMatrices[i*16];
		//printf("[ [ %8.4f, %8.4f, %8.4f, %8.4f ]\n"
		//       "  [ %8.4f, %8.4f, %8.4f, %8.4f ]\n"
		//       "  [ %8.4f, %8.4f, %8.4f, %8.4f ]\n"
		//       "  [ %8.4f, %8.4f, %8.4f, %8.4f ] ]\n",
		//	mat[0], mat[4], mat[8], mat[12],
		//	mat[1], mat[5], mat[9], mat[13],
		//	mat[2], mat[6], mat[10], mat[14],
		//	mat[3], mat[7], mat[11], mat[15]);
		//}
	}

	if(oldFormat){
		skin->findNumWeights(geometry->numVertices);
		skin->findUsedBones(geometry->numVertices);
	}

	// no split skins in GTA
	if(!oldFormat)
		stream->seek(12);
	return stream;
}

static Stream*
writeSkin(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	uint8 header[4];
	Geometry *geometry = (Geometry*)object;

	if(geometry->instData){
		if(geometry->instData->platform == PLATFORM_PS2)
			ps2::writeNativeSkin(stream, len, object, offset);
		else if(geometry->instData->platform == PLATFORM_WDGL)
			wdgl::writeNativeSkin(stream, len, object, offset);
		else if(geometry->instData->platform == PLATFORM_XBOX)
			xbox::writeNativeSkin(stream, len, object, offset);
		else{
			assert(0 && "unsupported native skin platform");
			return nil;
		}
	}

	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	// not sure which version introduced the new format
	bool oldFormat = version < 0x34000;
	header[0] = skin->numBones;
	if(oldFormat){
		header[1] = 0;
		header[2] = 0;
	}else{
		header[1] = skin->numUsedBones;
		header[2] = skin->numWeights;
	}
	header[3] = 0;
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

	// no split skins in GTA
	if(!oldFormat){
		uint32 buffer[3] = { 0, 0, 0};
		stream->write(buffer, 12);
	}
	return stream;
}

static int32
getSizeSkin(void *object, int32 offset, int32)
{
	Geometry *geometry = (Geometry*)object;

	if(geometry->instData){
		if(geometry->instData->platform == PLATFORM_PS2)
			return ps2::getSizeNativeSkin(object, offset);
		if(geometry->instData->platform == PLATFORM_WDGL)
			return wdgl::getSizeNativeSkin(object, offset);
		if(geometry->instData->platform == PLATFORM_XBOX)
			return xbox::getSizeNativeSkin(object, offset);
		if(geometry->instData->platform == PLATFORM_D3D8)
			return -1;
		if(geometry->instData->platform == PLATFORM_D3D9)
			return -1;
		assert(0 && "unsupported native skin platform");
	}

	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	if(skin == nil)
		return -1;

	int32 size = 4 + geometry->numVertices*(16+4) +
	             skin->numBones*64;
	// not sure which version introduced the new format
	if(version < 0x34000)
		size += skin->numBones*4;
	else
		size += skin->numUsedBones + 12;
	return size;
}

static void
skinRights(void *object, int32, int32, uint32)
{
	Skin::setPipeline((Atomic*)object, 1);
}

void
registerSkinPlugin(void)
{
	ObjPipeline *defpipe = new ObjPipeline(PLATFORM_nil);
	defpipe->pluginID = ID_SKIN;
	defpipe->pluginData = 1;
	for(uint i = 0; i < nelem(skinGlobals.pipelines); i++)
		skinGlobals.pipelines[i] = defpipe;
	skinGlobals.pipelines[PLATFORM_PS2] =
		ps2::makeSkinPipeline();
	skinGlobals.pipelines[PLATFORM_WDGL] =
		wdgl::makeSkinPipeline();
	skinGlobals.pipelines[PLATFORM_XBOX] =
		xbox::makeSkinPipeline();
	skinGlobals.pipelines[PLATFORM_D3D8] =
		d3d8::makeSkinPipeline();
	skinGlobals.pipelines[PLATFORM_D3D9] =
		d3d9::makeSkinPipeline();

	skinGlobals.offset = Geometry::registerPlugin(sizeof(Skin*), ID_SKIN,
	                                              createSkin,
	                                              destroySkin,
	                                              copySkin);
	Geometry::registerPluginStream(ID_SKIN,
	                               readSkin, writeSkin, getSizeSkin);
	Atomic::registerPlugin(0, ID_SKIN, nil, nil, nil);
	Atomic::setStreamRightsCallback(ID_SKIN, skinRights);
}

void
Skin::init(int32 numBones, int32 numUsedBones, int32 numVertices)
{
	this->numBones = numBones;
	this->numUsedBones = numUsedBones;
	uint32 size = this->numUsedBones +
	              this->numBones*64 +
	              numVertices*(16+4) + 0xF;
	this->data = new uint8[size];
	uint8 *p = this->data;

	this->usedBones = nil;
	if(this->numUsedBones){
		this->usedBones = p;
		p += this->numUsedBones;
	}

	p = (uint8*)(((uintptr)p + 0xF) & ~0xF);
	this->inverseMatrices = nil;
	if(this->numBones){
		this->inverseMatrices = (float*)p;
		p += 64*this->numBones;
	}

	this->indices = nil;
	if(numVertices){
		this->indices = p;
		p += 4*numVertices;
	}

	this->weights = nil;
	if(numVertices)
		this->weights = (float*)p;

	this->platformData = nil;
}

void
Skin::findNumWeights(int32 numVertices)
{
	this->numWeights = 1;
	float *w = this->weights;
	while(numVertices--){
		while(w[this->numWeights] != 0.0f){
			this->numWeights++;
			if(this->numWeights == 4)
				return;
		}
		w += 4;
	}
}

void
Skin::findUsedBones(int32 numVertices)
{
	uint8 usedTab[256];
	uint8 *indices = this->indices;
	float *weights = this->weights;
	memset(usedTab, 0, 256);
	while(numVertices--){
		for(int32 i = 0; i < this->numWeights; i++){
			if(weights[i] == 0.0f)
				continue;	// TODO: this could probably be optimized
			if(usedTab[indices[i]] == 0)
				usedTab[indices[i]]++;
		}
		indices += 4;
		weights += 4;
	}
	this->numUsedBones = 0;
	for(int32 i = 0; i < 256; i++)
		if(usedTab[i])
			this->usedBones[this->numUsedBones++] = i;
}

void
Skin::setPipeline(Atomic *a, int32 type)
{
	(void)type;
	a->pipeline = skinGlobals.pipelines[rw::platform];
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
	if(*PLUGINOFFSET(int32, src, offset))
		MatFX::enableEffects((Atomic*)dst);
	return dst;
}

static Stream*
readAtomicMatFX(Stream *stream, int32, void *object, int32, int32)
{
	if(stream->readI32())
		MatFX::enableEffects((Atomic*)object);
	return stream;
}

static Stream*
writeAtomicMatFX(Stream *stream, int32, void *object, int32 offset, int32)
{
	stream->writeI32(*PLUGINOFFSET(int32, object, offset));
	return stream;
}

static int32
getSizeAtomicMatFX(void *object, int32 offset, int32)
{
	int32 flag = *PLUGINOFFSET(int32, object, offset);
	// TODO: not sure which version
	return flag || rw::version < 0x34000 ? 4 : 0;
}

// Material

MatFXGlobals matFXGlobals = { 0, 0, { nil } };

// TODO: Frames and Matrices?
static void
clearMatFX(MatFX *matfx)
{
	for(int i = 0; i < 2; i++)
		switch(matfx->fx[i].type){
		case MatFX::BUMPMAP:
			if(matfx->fx[i].bump.bumpedTex)
				matfx->fx[i].bump.bumpedTex->destroy();
			if(matfx->fx[i].bump.tex)
				matfx->fx[i].bump.tex->destroy();
			break;

		case MatFX::ENVMAP:
			if(matfx->fx[i].env.tex)
				matfx->fx[i].env.tex->destroy();
			break;

		case MatFX::DUAL:
			if(matfx->fx[i].dual.tex)
				matfx->fx[i].dual.tex->destroy();
			break;
		}
	memset(matfx, 0, sizeof(MatFX));
}

void
MatFX::setEffects(uint32 type)
{
	if(this->type != 0 && this->type != type)
		clearMatFX(this);
	this->type = type;
	switch(type){
	case BUMPMAP:
	case ENVMAP:
	case DUAL:
	case UVTRANSFORM:
		this->fx[0].type = type;
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

uint32
MatFX::getEffects(Material *m)
{
	MatFX *fx = *PLUGINOFFSET(MatFX*, m, matFXGlobals.materialOffset);
	if(fx)
		return fx->type;
	return 0;
}

uint32
MatFX::getEffectIndex(uint32 type)
{
	for(int i = 0; i < 2; i++)
		if(this->fx[i].type == type)
			return i;
	return -1;
}

void
MatFX::setBumpTexture(Texture *t)
{
	int32 i = this->getEffectIndex(BUMPMAP);
	if(i >= 0)
		this->fx[i].bump.tex = t;
}

void
MatFX::setBumpCoefficient(float32 coef)
{
	int32 i = this->getEffectIndex(BUMPMAP);
	if(i >= 0)
		this->fx[i].bump.coefficient = coef;
}

void
MatFX::setEnvTexture(Texture *t)
{
	int32 i = this->getEffectIndex(ENVMAP);
	if(i >= 0)
		this->fx[i].env.tex = t;
}

void
MatFX::setEnvCoefficient(float32 coef)
{
	int32 i = this->getEffectIndex(ENVMAP);
	if(i >= 0)
		this->fx[i].env.coefficient = coef;
}

void
MatFX::setDualTexture(Texture *t)
{
	int32 i = this->getEffectIndex(DUAL);
	if(i >= 0)
		this->fx[i].dual.tex = t;
}

void
MatFX::setDualSrcBlend(int32 blend)
{
	int32 i = this->getEffectIndex(DUAL);
	if(i >= 0)
		this->fx[i].dual.srcBlend = blend;
}

void
MatFX::setDualDestBlend(int32 blend)
{
	int32 i = this->getEffectIndex(DUAL);
	if(i >= 0)
		this->fx[i].dual.dstBlend = blend;
}

static void*
createMaterialMatFX(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(MatFX*, object, offset) = nil;
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
	if(srcfx == nil)
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

static Stream*
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
			bumpedTex = tex = nil;
			if(stream->readI32()){
				if(!findChunk(stream, ID_TEXTURE,
				              nil, nil)){
					RWERROR((ERR_CHUNK, "TEXTURE"));
					return nil;
				}
				bumpedTex = Texture::streamRead(stream);
			}
			if(stream->readI32()){
				if(!findChunk(stream, ID_TEXTURE,
				              nil, nil)){
					RWERROR((ERR_CHUNK, "TEXTURE"));
					return nil;
				}
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
			tex = nil;
			if(stream->readI32()){
				if(!findChunk(stream, ID_TEXTURE,
				              nil, nil)){
					RWERROR((ERR_CHUNK, "TEXTURE"));
					return nil;
				}
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
			tex = nil;
			if(stream->readI32()){
				if(!findChunk(stream, ID_TEXTURE,
				              nil, nil)){
					RWERROR((ERR_CHUNK, "TEXTURE"));
					return nil;
				}
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
	return stream;
}

static Stream*
writeMaterialMatFX(Stream *stream, int32, void *object, int32 offset, int32)
{
	MatFX *matfx = *PLUGINOFFSET(MatFX*, object, offset);

	stream->writeU32(matfx->type);
	for(int i = 0; i < 2; i++){
		stream->writeU32(matfx->fx[i].type);
		switch(matfx->fx[i].type){
		case MatFX::BUMPMAP:
			stream->writeF32(matfx->fx[i].bump.coefficient);
			stream->writeI32(matfx->fx[i].bump.bumpedTex != nil);
			if(matfx->fx[i].bump.bumpedTex)
				matfx->fx[i].bump.bumpedTex->streamWrite(stream);
			stream->writeI32(matfx->fx[i].bump.tex != nil);
			if(matfx->fx[i].bump.tex)
				matfx->fx[i].bump.tex->streamWrite(stream);
			break;

		case MatFX::ENVMAP:
			stream->writeF32(matfx->fx[i].env.coefficient);
			stream->writeI32(matfx->fx[i].env.fbAlpha);
			stream->writeI32(matfx->fx[i].env.tex != nil);
			if(matfx->fx[i].env.tex)
				matfx->fx[i].env.tex->streamWrite(stream);
			break;

		case MatFX::DUAL:
			stream->writeI32(matfx->fx[i].dual.srcBlend);
			stream->writeI32(matfx->fx[i].dual.dstBlend);
			stream->writeI32(matfx->fx[i].dual.tex != nil);
			if(matfx->fx[i].dual.tex)
				matfx->fx[i].dual.tex->streamWrite(stream);
			break;
		}
	}
	return stream;
}

static int32
getSizeMaterialMatFX(void *object, int32 offset, int32)
{
	MatFX *matfx = *PLUGINOFFSET(MatFX*, object, offset);
	if(matfx == nil)
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
MatFX::enableEffects(Atomic *atomic)
{
	*PLUGINOFFSET(int32, atomic, matFXGlobals.atomicOffset) = 1;
	atomic->pipeline = matFXGlobals.pipelines[rw::platform];
}

void
registerMatFXPlugin(void)
{
	ObjPipeline *defpipe = new ObjPipeline(PLATFORM_nil);
	defpipe->pluginID = 0; //ID_MATFX;
	defpipe->pluginData = 0;
	for(uint i = 0; i < nelem(matFXGlobals.pipelines); i++)
		matFXGlobals.pipelines[i] = defpipe;
	matFXGlobals.pipelines[PLATFORM_PS2] =
		ps2::makeMatFXPipeline();
	matFXGlobals.pipelines[PLATFORM_WDGL] =
		wdgl::makeMatFXPipeline();
	matFXGlobals.pipelines[PLATFORM_XBOX] =
		xbox::makeMatFXPipeline();
	matFXGlobals.pipelines[PLATFORM_D3D8] =
		d3d8::makeMatFXPipeline();
	matFXGlobals.pipelines[PLATFORM_D3D9] =
		d3d9::makeMatFXPipeline();

	matFXGlobals.atomicOffset =
	Atomic::registerPlugin(sizeof(int32), ID_MATFX,
	                       createAtomicMatFX, nil, copyAtomicMatFX);
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
