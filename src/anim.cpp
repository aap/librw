#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"

using namespace std;

namespace rw {

#define MAXINTERPINFO 10

static AnimInterpolatorInfo *interpInfoList[MAXINTERPINFO];

void
registerAnimInterpolatorInfo(AnimInterpolatorInfo *interpInfo)
{
	for(int32 i = 0; i < MAXINTERPINFO; i++)
		if(interpInfoList[i] == NULL){
			interpInfoList[i] = interpInfo;
			return;
		}
	assert(0 && "no room for interpolatorInfo");
}

AnimInterpolatorInfo*
findAnimInterpolatorInfo(int32 id)
{
	for(int32 i = 0; i < MAXINTERPINFO; i++){
		if(interpInfoList[i] && interpInfoList[i]->id == id)
			return interpInfoList[i];
	}
	return NULL;
}

Animation::Animation(AnimInterpolatorInfo *interpInfo, int32 numFrames, int32 flags, float duration)
{
	this->interpInfo = interpInfo;
	this->numFrames = numFrames;
	this->flags = flags;
	this->duration = duration;
	uint8 *data = new uint8[this->numFrames*interpInfo->keyFrameSize + interpInfo->customDataSize];
	this->keyframes = data;
	data += this->numFrames*interpInfo->keyFrameSize;
	this->customData = data;
}

Animation*
Animation::streamRead(Stream *stream)
{
	Animation *anim;
	assert(stream->readI32() == 0x100);
	int32 typeID = stream->readI32();
	AnimInterpolatorInfo *interpInfo = findAnimInterpolatorInfo(typeID);
	int32 numFrames = stream->readI32();
	int32 flags = stream->readI32();
	float duration = stream->readF32();
	anim = new Animation(interpInfo, numFrames, flags, duration);
	interpInfo->streamRead(stream, anim);
	return anim;
}

Animation*
Animation::streamReadLegacy(Stream *stream)
{
	Animation *anim;
	AnimInterpolatorInfo *interpInfo = findAnimInterpolatorInfo(1);
	int32 numFrames = stream->readI32();
	int32 flags = stream->readI32();
	float duration = stream->readF32();
	anim = new Animation(interpInfo, numFrames, flags, duration);
	HAnimKeyFrame *frames = (HAnimKeyFrame*)anim->keyframes;
	for(int32 i = 0; i < anim->numFrames; i++){
		stream->read(frames[i].q, 4*4);
		stream->read(frames[i].t, 3*4);
		frames[i].time = stream->readF32();
		int32 prev = stream->readI32();
		frames[i].prev = &frames[prev];
	}
	return anim;
}

bool
Animation::streamWrite(Stream *stream)
{
	writeChunkHeader(stream, ID_ANIMANIMATION, this->streamGetSize());
	stream->writeI32(0x100);
	stream->writeI32(this->interpInfo->id);
	stream->writeI32(this->numFrames);
	stream->writeI32(this->flags);
	stream->writeF32(this->duration);
	this->interpInfo->streamWrite(stream, this);
	return true;
}

bool
Animation::streamWriteLegacy(Stream *stream)
{
	stream->writeI32(this->numFrames);
	stream->writeI32(this->flags);
	stream->writeF32(this->duration);
	assert(interpInfo->id == 1);
	HAnimKeyFrame *frames = (HAnimKeyFrame*)this->keyframes;
	for(int32 i = 0; i < this->numFrames; i++){
		stream->write(frames[i].q, 4*4);
		stream->write(frames[i].t, 3*4);
		stream->writeF32(frames[i].time);
		stream->writeI32(frames[i].prev - frames);
	}
	return true;
}

uint32
Animation::streamGetSize(void)
{
	uint32 size = 4 + 4 + 4 + 4 + 4;
	size += this->interpInfo->streamGetSize(this);
	return size;
}

AnimInterpolator::AnimInterpolator(Animation *anim)
{
	this->anim = anim;
}

UVAnimDictionary *currentUVAnimDictionary;

UVAnimDictionary*
UVAnimDictionary::streamRead(Stream *stream)
{
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	UVAnimDictionary *dict = new UVAnimDictionary;
	dict->numAnims = stream->readI32();
	dict->anims = new Animation*[dict->numAnims];
	for(int32 i = 0; i < dict->numAnims; i++){
		assert(findChunk(stream, ID_ANIMANIMATION, NULL, NULL));
		dict->anims[i] = Animation::streamRead(stream);
	}
	return dict;
}

bool
UVAnimDictionary::streamWrite(Stream *stream)
{
	uint32 size = this->streamGetSize();
	writeChunkHeader(stream, ID_UVANIMDICT, size);
	writeChunkHeader(stream, ID_STRUCT, 4);
	stream->writeI32(this->numAnims);
	for(int32 i = 0; i < this->numAnims; i++)
		this->anims[i]->streamWrite(stream);
	return true;
}

uint32
UVAnimDictionary::streamGetSize(void)
{
	uint32 size = 12 + 4;
	for(int32 i = 0; i < this->numAnims; i++)
		size += 12 + this->anims[i]->streamGetSize();
	return size;
}

Animation*
UVAnimDictionary::find(const char *name)
{
	for(int32 i = 0; i < this->numAnims; i++){
		Animation *anim = this->anims[i];
		UVAnimCustomData *custom = (UVAnimCustomData*)anim->customData;
		if(strncmp(custom->name, name, 32) == 0)	// strncmp correct?
			return anim;
	}
	return NULL;
}

static void
uvAnimStreamRead(Stream *stream, Animation *anim)
{
	UVAnimCustomData *custom = (UVAnimCustomData*)anim->customData;
	UVAnimKeyFrame *frames = (UVAnimKeyFrame*)anim->keyframes;
	stream->readI32();
	stream->read(custom->name, 32);
	stream->read(custom->nodeToUVChannel, 8*4);

	for(int32 i = 0; i < anim->numFrames; i++){
		frames[i].time = stream->readF32();
		stream->read(frames[i].uv, 6*4);
		int32 prev = stream->readI32();
		frames[i].prev = &frames[prev];
	}
}

static void
uvAnimStreamWrite(Stream *stream, Animation *anim)
{
	UVAnimCustomData *custom = (UVAnimCustomData*)anim->customData;
	UVAnimKeyFrame *frames = (UVAnimKeyFrame*)anim->keyframes;
	stream->writeI32(0);
	stream->write(custom->name, 32);
	stream->write(custom->nodeToUVChannel, 8*4);

	for(int32 i = 0; i < anim->numFrames; i++){
		stream->writeF32(frames[i].time);
		stream->write(frames[i].uv, 6*4);
		stream->writeI32(frames[i].prev - frames);
	}
}

static uint32
uvAnimStreamGetSize(Animation *anim)
{
	return 4 + 32 + 8*4 + anim->numFrames*(4 + 6*4 + 4);
}

static void
registerUVAnimInterpolator(void)
{
	// Linear
	AnimInterpolatorInfo *info = new AnimInterpolatorInfo;
	info->id = 0x1C0;
	info->keyFrameSize = sizeof(UVAnimKeyFrame);
	info->customDataSize = sizeof(UVAnimCustomData);
	info->streamRead = uvAnimStreamRead;
	info->streamWrite = uvAnimStreamWrite;
	info->streamGetSize = uvAnimStreamGetSize;
	registerAnimInterpolatorInfo(info);

	// Param
	info = new AnimInterpolatorInfo;
	info->id = 0x1C1;
	info->keyFrameSize = sizeof(UVAnimKeyFrame);
	info->customDataSize = sizeof(UVAnimCustomData);
	info->streamRead = uvAnimStreamRead;
	info->streamWrite = uvAnimStreamWrite;
	info->streamGetSize = uvAnimStreamGetSize;
	registerAnimInterpolatorInfo(info);
}

int32 uvAnimOffset;

struct UVAnim
{
	AnimInterpolator *interp[8];
};

static void*
createUVAnim(void *object, int32 offset, int32)
{
	UVAnim *uvanim;
	uvanim = PLUGINOFFSET(UVAnim, object, offset);
	memset(uvanim, 0, sizeof(*uvanim));
	return object;
}

static void*
destroyUVAnim(void *object, int32 offset, int32)
{
	UVAnim *uvanim;
	uvanim = PLUGINOFFSET(UVAnim, object, offset);
	// TODO: ref counts &c.
	(void)uvanim;
	return object;
}

static void*
copyUVAnim(void *dst, void *src, int32 offset, int32)
{
	UVAnim *srcuvanim, *dstuvanim;
	dstuvanim = PLUGINOFFSET(UVAnim, dst, offset);
	srcuvanim = PLUGINOFFSET(UVAnim, src, offset);
	memcpy(dstuvanim, srcuvanim, sizeof(*srcuvanim));
	// TODO: ref counts &c.
	return dst;
}

Animation*
makeDummyAnimation(const char *name)
{
	AnimInterpolatorInfo *interpInfo = findAnimInterpolatorInfo(0x1C0);
	Animation *anim = new Animation(interpInfo, 2, 0, 1.0f);
	UVAnimCustomData *custom = (UVAnimCustomData*)anim->customData;
//	UVAnimKeyFrame *frames = (UVAnimKeyFrame*)anim->keyframes;
	strncpy(custom->name, name, 32);
	memset(custom->nodeToUVChannel, 0, sizeof(custom->nodeToUVChannel));
	// TODO: init the frames
	return anim;
}

static void
readUVAnim(Stream *stream, int32, void *object, int32 offset, int32)
{
	UVAnim *uvanim = PLUGINOFFSET(UVAnim, object, offset);	
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	char name[32];
	uint32 mask = stream->readI32();
	uint32 bit = 1;
	for(int32 i = 0; i < 8; i++){
		if(mask & bit){
			stream->read(name, 32);
			Animation *anim = NULL;
			if(currentUVAnimDictionary)
				anim = currentUVAnimDictionary->find(name);
			if(anim == NULL)
				anim = makeDummyAnimation(name);
			AnimInterpolator *interp = new AnimInterpolator(anim);
			uvanim->interp[i] = interp;
		}
		bit <<= 1;
	}
}

static void
writeUVAnim(Stream *stream, int32 size, void *object, int32 offset, int32)
{
	UVAnim *uvanim = PLUGINOFFSET(UVAnim, object, offset);
	writeChunkHeader(stream, ID_STRUCT, size-12);
	uint32 mask = 0;
	uint32 bit = 1;
	for(int32 i = 0; i < 8; i++){
		if(uvanim->interp[i])
			mask |= bit;
		bit <<= 1;
	}
	stream->writeI32(mask);
	for(int32 i = 0; i < 8; i++){
		if(uvanim->interp[i]){
			UVAnimCustomData *custom =
			  (UVAnimCustomData*)uvanim->interp[i]->anim->customData;
			stream->write(custom->name, 32);
		}
	}
}

static int32
getSizeUVAnim(void *object, int32 offset, int32)
{
	UVAnim *uvanim = PLUGINOFFSET(UVAnim, object, offset);
	int32 size = 0;
	for(int32 i = 0; i < 8; i++)
		if(uvanim->interp[i])
			size += 32;
	return size ? size + 12 + 4 : 0;
}


void
registerUVAnimPlugin(void)
{
	registerUVAnimInterpolator();
	uvAnimOffset = Material::registerPlugin(sizeof(UVAnim), ID_UVANIMATION,
	                                        createUVAnim, destroyUVAnim, copyUVAnim);
	Material::registerPluginStream(ID_UVANIMATION, readUVAnim, writeUVAnim, getSizeUVAnim);
}

}
