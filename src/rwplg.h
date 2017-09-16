namespace rw {

#define PLUGINOFFSET(type, base, offset) \
	((type*)((char*)(base) + (offset)))

typedef void *(*Constructor)(void *object, int32 offset, int32 size);
typedef void *(*Destructor)(void *object, int32 offset, int32 size);
typedef void *(*CopyConstructor)(void *dst, void *src, int32 offset, int32 size);
typedef Stream *(*StreamRead)(Stream *stream, int32 length, void *object, int32 offset, int32 size);
typedef Stream *(*StreamWrite)(Stream *stream, int32 length, void *object, int32 offset, int32 size);
typedef int32 (*StreamGetSize)(void *object, int32 offset, int32 size);
typedef void (*RightsCallback)(void *object, int32 offset, int32 size, uint32 data);

struct Plugin
{
	int32 offset;
	int32 size;
	uint32 id;
	Constructor constructor;
	Destructor destructor;
	CopyConstructor copy;
	StreamRead read;
	StreamWrite write;
	StreamGetSize getSize;
	RightsCallback rightsCallback;
	Plugin *next;
	Plugin *prev;
};

struct PluginList
{
	int32 size;
	int32 defaultSize;
	Plugin *first;
	Plugin *last;

	void construct(void *);
	void destruct(void *);
	void copy(void *dst, void *src);
	bool streamRead(Stream *stream, void *);
	void streamWrite(Stream *stream, void *);
	int streamGetSize(void *);
	void streamSkip(Stream *stream);
	void assertRights(void *, uint32 pluginID, uint32 data);

	int32 registerPlugin(int32 size, uint32 id,
		Constructor, Destructor, CopyConstructor);
	int32 registerStream(uint32 id, StreamRead, StreamWrite, StreamGetSize);
	int32 setStreamRightsCallback(uint32 id, RightsCallback cb);
	int32 getPluginOffset(uint32 id);
};

#define PLUGINBASE \
	static PluginList s_plglist;						    \
	static int32 registerPlugin(int32 size, uint32 id, Constructor ctor, 	    \
			Destructor dtor, CopyConstructor copy){			    \
		return s_plglist.registerPlugin(size, id, ctor, dtor, copy);	    \
	}									    \
	static int32 registerPluginStream(uint32 id, StreamRead read,		    \
			StreamWrite write, StreamGetSize getSize){		    \
		return s_plglist.registerStream(id, read, write, getSize);	    \
	}									    \
	static int32 setStreamRightsCallback(uint32 id, RightsCallback cb){	    \
		return s_plglist.setStreamRightsCallback(id, cb);		    \
	}									    \
	static int32 getPluginOffset(uint32 id){				    \
		return s_plglist.getPluginOffset(id);				    \
	}


}
