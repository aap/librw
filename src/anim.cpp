#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

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

Animation*
Animation::create(AnimInterpolatorInfo *interpInfo, int32 numFrames, int32 flags, float duration)
{
	Animation *anim = (Animation*)malloc(sizeof(*anim));
	anim->interpInfo = interpInfo;
	anim->numFrames = numFrames;
	anim->flags = flags;
	anim->duration = duration;
	uint8 *data = new uint8[anim->numFrames*interpInfo->keyFrameSize + interpInfo->customDataSize];
	anim->keyframes = data;
	data += anim->numFrames*interpInfo->keyFrameSize;
	anim->customData = data;
	return anim;
}

void
Animation::destroy(void)
{
	uint8 *c = (uint8*)this->keyframes;
	delete[] c;
	free(this);	
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
	anim = Animation::create(interpInfo, numFrames, flags, duration);
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
	anim = Animation::create(interpInfo, numFrames, flags, duration);
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

//
// UVAnim
//

void
UVAnimCustomData::destroy(Animation *anim)
{
	this->refCount--;
	if(this->refCount <= 0)
		anim->destroy();
}

UVAnimDictionary *currentUVAnimDictionary;

UVAnimDictionary*
UVAnimDictionary::create(void)
{
	UVAnimDictionary *dict = (UVAnimDictionary*)malloc(sizeof(*dict));
	dict->animations.init();
	return dict;
}

void
UVAnimDictionary::destroy(void)
{
	FORLIST(lnk, this->animations){
		UVAnimDictEntry *de = UVAnimDictEntry::fromDict(lnk);
		UVAnimCustomData *cust = (UVAnimCustomData*)de->anim->customData;
		cust->destroy(de->anim);
		delete de;
	}
	free(this);
}

void
UVAnimDictionary::add(Animation *anim)
{
	UVAnimDictEntry *de = new UVAnimDictEntry;
	UVAnimCustomData *custom = (UVAnimCustomData*)anim->customData;
	de->anim = anim;
	this->animations.append(&de->inDict);
}

UVAnimDictionary*
UVAnimDictionary::streamRead(Stream *stream)
{
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	UVAnimDictionary *dict = UVAnimDictionary::create();
	int32 numAnims = stream->readI32();
	for(int32 i = 0; i < numAnims; i++){
		assert(findChunk(stream, ID_ANIMANIMATION, NULL, NULL));
		dict->add(Animation::streamRead(stream));
	}
	return dict;
}

bool
UVAnimDictionary::streamWrite(Stream *stream)
{
	uint32 size = this->streamGetSize();
	writeChunkHeader(stream, ID_UVANIMDICT, size);
	writeChunkHeader(stream, ID_STRUCT, 4);
	int32 numAnims = this->count();
	stream->writeI32(numAnims);
	FORLIST(lnk, this->animations){
		UVAnimDictEntry *de = UVAnimDictEntry::fromDict(lnk);
		de->anim->streamWrite(stream);
	}
	return true;
}

uint32
UVAnimDictionary::streamGetSize(void)
{
	uint32 size = 12 + 4;
	int32 numAnims = this->count();
	FORLIST(lnk, this->animations){
		UVAnimDictEntry *de = UVAnimDictEntry::fromDict(lnk);
		size += 12 + de->anim->streamGetSize();
	}
	return size;
}

Animation*
UVAnimDictionary::find(const char *name)
{
	FORLIST(lnk, this->animations){
		Animation *anim = UVAnimDictEntry::fromDict(lnk)->anim;
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
	custom->refCount = 1;

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
	for(int32 i = 0; i < 8; i++){
		AnimInterpolator *ip = uvanim->interp[i];
		if(ip){
			UVAnimCustomData *custom = (UVAnimCustomData*)ip->anim->customData;
			custom->destroy(ip->anim);
			delete ip;
		}
	}
	return object;
}

static void*
copyUVAnim(void *dst, void *src, int32 offset, int32)
{
	UVAnim *srcuvanim, *dstuvanim;
	dstuvanim = PLUGINOFFSET(UVAnim, dst, offset);
	srcuvanim = PLUGINOFFSET(UVAnim, src, offset);
	for(int32 i = 0; i < 8; i++){
		AnimInterpolator *srcip = srcuvanim->interp[i];
		AnimInterpolator *dstip;
		if(srcip){
			UVAnimCustomData *custom = (UVAnimCustomData*)srcip->anim->customData;
			dstip = new AnimInterpolator(srcip->anim);
			custom->refCount++;
			dstuvanim->interp[i] = dstip;
		}
	}
	return dst;
}

Animation*
makeDummyAnimation(const char *name)
{
	AnimInterpolatorInfo *interpInfo = findAnimInterpolatorInfo(0x1C0);
	Animation *anim = Animation::create(interpInfo, 2, 0, 1.0f);
	UVAnimCustomData *custom = (UVAnimCustomData*)anim->customData;
	strncpy(custom->name, name, 32);
	memset(custom->nodeToUVChannel, 0, sizeof(custom->nodeToUVChannel));
	custom->refCount = 1;
	// TODO: init the frames
//	UVAnimKeyFrame *frames = (UVAnimKeyFrame*)anim->keyframes;
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
			if(anim == NULL){
				anim = makeDummyAnimation(name);
				if(currentUVAnimDictionary)
					currentUVAnimDictionary->add(anim);
			}
			UVAnimCustomData *custom = (UVAnimCustomData*)anim->customData;
			AnimInterpolator *interp = new AnimInterpolator(anim);
			custom->refCount++;
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
