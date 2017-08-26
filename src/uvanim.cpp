#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "rwanim.h"
#include "rwplugins.h"

#define PLUGIN_ID ID_UVANIMATION

namespace rw {

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
	UVAnimDictionary *dict = (UVAnimDictionary*)rwMalloc(sizeof(UVAnimDictionary), MEMDUR_EVENT | ID_UVANIMATION);
	if(dict == nil){
		RWERROR((ERR_ALLOC, sizeof(UVAnimDictionary)));
		return nil;
	}
	dict->animations.init();
	return dict;
}

void
UVAnimDictionary::destroy(void)
{
	FORLIST(lnk, this->animations){
		UVAnimDictEntry *de = UVAnimDictEntry::fromDict(lnk);
		UVAnimCustomData *cust = UVAnimCustomData::get(de->anim);
		cust->destroy(de->anim);
		rwFree(de);
	}
	rwFree(this);
}

void
UVAnimDictionary::add(Animation *anim)
{
	UVAnimDictEntry *de = rwNewT(UVAnimDictEntry, 1, MEMDUR_EVENT | ID_UVANIMDICT);
	de->anim = anim;
	this->animations.append(&de->inDict);
}

UVAnimDictionary*
UVAnimDictionary::streamRead(Stream *stream)
{
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	UVAnimDictionary *dict = UVAnimDictionary::create();
	if(dict == nil)
		return nil;
	int32 numAnims = stream->readI32();
	Animation *anim;
	for(int32 i = 0; i < numAnims; i++){
		if(!findChunk(stream, ID_ANIMANIMATION, nil, nil)){
			RWERROR((ERR_CHUNK, "ANIMANIMATION"));
			goto fail;
		}
		anim = Animation::streamRead(stream);
		if(anim == nil)
			goto fail;
		dict->add(anim);
	}
	return dict;
fail:
	dict->destroy();
	return nil;
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
		UVAnimCustomData *custom = UVAnimCustomData::get(anim);
		if(strncmp_ci(custom->name, name, 32) == 0)
			return anim;
	}
	return nil;
}

static void
uvAnimStreamRead(Stream *stream, Animation *anim)
{
	UVAnimCustomData *custom = UVAnimCustomData::get(anim);
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
	UVAnimCustomData *custom = UVAnimCustomData::get(anim);
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
	AnimInterpolatorInfo *info = rwNewT(AnimInterpolatorInfo, 1, MEMDUR_GLOBAL | ID_UVANIMATION);
	info->id = 0x1C0;
	info->interpKeyFrameSize = sizeof(UVAnimInterpFrame);
	info->animKeyFrameSize = sizeof(UVAnimKeyFrame);
	info->customDataSize = sizeof(UVAnimCustomData);
	info->applyCB = nil;
	info->blendCB = nil;
	info->interpCB = nil;
	info->addCB = nil;
	info->mulRecipCB = nil;
	info->streamRead = uvAnimStreamRead;
	info->streamWrite = uvAnimStreamWrite;
	info->streamGetSize = uvAnimStreamGetSize;
	AnimInterpolatorInfo::registerInterp(info);

	// Param
	info = rwNewT(AnimInterpolatorInfo, 1, MEMDUR_GLOBAL | ID_UVANIMATION);
	info->id = 0x1C1;
	info->interpKeyFrameSize = sizeof(UVAnimInterpFrame);
	info->animKeyFrameSize = sizeof(UVAnimKeyFrame);
	info->customDataSize = sizeof(UVAnimCustomData);
	info->applyCB = nil;
	info->blendCB = nil;
	info->interpCB = nil;
	info->addCB = nil;
	info->mulRecipCB = nil;
	info->streamRead = uvAnimStreamRead;
	info->streamWrite = uvAnimStreamWrite;
	info->streamGetSize = uvAnimStreamGetSize;
	AnimInterpolatorInfo::registerInterp(info);
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
			UVAnimCustomData *custom =
				UVAnimCustomData::get(ip->currentAnim);
			custom->destroy(ip->currentAnim);
			rwFree(ip);
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
			Animation *anim = srcip->currentAnim;
			UVAnimCustomData *custom = UVAnimCustomData::get(anim);
			dstip = AnimInterpolator::create(anim->getNumNodes(),
				anim->interpInfo->interpKeyFrameSize);
			dstip->setCurrentAnim(anim);
			custom->refCount++;
			dstuvanim->interp[i] = dstip;
		}
	}
	return dst;
}

Animation*
makeDummyAnimation(const char *name)
{
	AnimInterpolatorInfo *interpInfo = AnimInterpolatorInfo::find(0x1C0);
	Animation *anim = Animation::create(interpInfo, 2, 0, 1.0f);
	UVAnimCustomData *custom = UVAnimCustomData::get(anim);
	strncpy(custom->name, name, 32);
	memset(custom->nodeToUVChannel, 0, sizeof(custom->nodeToUVChannel));
	custom->refCount = 1;
	// TODO: init the frames
//	UVAnimKeyFrame *frames = (UVAnimKeyFrame*)anim->keyframes;
	return anim;
}

static Stream*
readUVAnim(Stream *stream, int32, void *object, int32 offset, int32)
{
	UVAnim *uvanim = PLUGINOFFSET(UVAnim, object, offset);
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	char name[32];
	uint32 mask = stream->readI32();
	uint32 bit = 1;
	for(int32 i = 0; i < 8; i++){
		if(mask & bit){
			stream->read(name, 32);
			Animation *anim = nil;
			if(currentUVAnimDictionary)
				anim = currentUVAnimDictionary->find(name);
			if(anim == nil){
				anim = makeDummyAnimation(name);
				if(currentUVAnimDictionary)
					currentUVAnimDictionary->add(anim);
			}
			UVAnimCustomData *custom = UVAnimCustomData::get(anim);
			AnimInterpolator *interp;
			interp = AnimInterpolator::create(anim->getNumNodes(),
				anim->interpInfo->interpKeyFrameSize);
			interp->setCurrentAnim(anim);
			custom->refCount++;
			uvanim->interp[i] = interp;
		}
		bit <<= 1;
	}
	return stream;
}

static Stream*
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
			  UVAnimCustomData::get(uvanim->interp[i]->currentAnim);
			stream->write(custom->name, 32);
		}
	}
	return stream;
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

static void*
uvanimOpen(void *object, int32 offset, int32 size)
{
	registerUVAnimInterpolator();
	return object;
}
static void *uvanimClose(void *object, int32 offset, int32 size) { return object; }

void
registerUVAnimPlugin(void)
{
	Engine::registerPlugin(0, ID_UVANIMATION, uvanimOpen, uvanimClose);
	uvAnimOffset = Material::registerPlugin(sizeof(UVAnim), ID_UVANIMATION,
		createUVAnim, destroyUVAnim, copyUVAnim);
	Material::registerPluginStream(ID_UVANIMATION,
		readUVAnim, writeUVAnim, getSizeUVAnim);
}

}
