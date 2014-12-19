namespace Rw {

#define PLUGINOFFSET(type, base, offset) \
	((type*)((char*)(base) + (offset)))

typedef void *(*Constructor)(void *object, int32 offset, int32 size);
typedef void *(*Destructor)(void *object, int32 offset, int32 size);
typedef void *(*CopyConstructor)(void *dst, void *src, int32 offset, int32 size);
typedef void (*StreamRead)(std::istream &stream, int32 length, void *object, int32 offset, int32 size);
typedef void (*StreamWrite)(std::ostream &stream, int32 length, void *object, int32 offset, int32 size);
typedef int32 (*StreamGetSize)(void *object, int32 offset, int32 size);

struct Plugin
{
	int offset;
	int size;
	uint id;
	Constructor constructor;
	Destructor destructor;
	CopyConstructor copy;
	StreamRead read;
	StreamWrite write;
	StreamGetSize getSize;
	Plugin *next;
};

template <typename T>
struct PluginBase
{
	static int s_defaultSize;
	static int s_size;
	static Plugin *s_plugins;

	void constructPlugins(void);
	void destructPlugins(void);
	void copyPlugins(T *t);
	void streamReadPlugins(std::istream &stream);
	void streamWritePlugins(std::ostream &stream);
	int streamGetPluginSize(void);

	static int registerPlugin(int size, uint id,
	                          Constructor, Destructor, CopyConstructor);
	static int registerPluginStream(uint id,
	                                StreamRead, StreamWrite, StreamGetSize);
	static int getPluginOffset(uint id);
	static void *operator new(size_t size);
	static void operator delete(void *p);
};
template <typename T>
int PluginBase<T>::s_defaultSize = sizeof(T);
template <typename T>
int PluginBase<T>::s_size = sizeof(T);
template <typename T>
Plugin *PluginBase<T>::s_plugins = 0;

template <typename T> void
PluginBase<T>::constructPlugins(void)
{
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->constructor)
			p->constructor((void*)this, p->offset, p->size);
}

template <typename T> void
PluginBase<T>::destructPlugins(void)
{
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->destructor)
			p->destructor((void*)this, p->offset, p->size);
}

template <typename T> void
PluginBase<T>::copyPlugins(T *t)
{
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->copy)
			p->copy((void*)this, (void*)t, p->offset, p->size);
}

template <typename T> void
PluginBase<T>::streamReadPlugins(std::istream &stream)
{
	uint32 length;
	Rw::ChunkHeaderInfo header;
	if(!Rw::FindChunk(stream, Rw::ID_EXTENSION, &length, NULL))
		return;
	while(length){
		Rw::ReadChunkHeaderInfo(stream, &header);
		length -= 12;
		for(Plugin *p = this->s_plugins; p; p = p->next)
			if(p->id == header.type){
				p->read(stream, header.length,
				        (void*)this, p->offset, p->size);
				goto cont;
			}
		stream.seekg(header.length, std::ios::cur);
cont:
		length -= header.length;
	}
}

template <typename T> void
PluginBase<T>::streamWritePlugins(std::ostream &stream)
{
	int size = this->streamGetPluginSize();
	Rw::WriteChunkHeader(stream, Rw::ID_EXTENSION, size);
	for(Plugin *p = this->s_plugins; p; p = p->next){
		if((size = p->getSize(this, p->offset, p->size)) < 0)
			continue;
		Rw::WriteChunkHeader(stream, p->id, size);
		p->write(stream, size, this, p->offset, p->size);
	}
}

template <typename T> int
PluginBase<T>::streamGetPluginSize(void)
{
	int size = 0;
	int plgsize;
	for(Plugin *p = this->s_plugins; p; p = p->next)
		if(p->getSize &&
		   (plgsize = p->getSize(this, p->offset, p->size)) >= 0)
			size += 12 + plgsize;
	return size;
}

template <typename T> int
PluginBase<T>::registerPlugin(int size, uint id,
	Constructor ctor, Destructor dtor, CopyConstructor cctor)
{
	Plugin *p = new Plugin;
	p->offset = s_size;
	s_size += size;

	p->size = size;
	p->id = id;
	p->constructor = ctor;
	p->copy = cctor;
	p->destructor = dtor;
	p->read = NULL;
	p->write = NULL;
	p->getSize = NULL;

	p->next = s_plugins;
	s_plugins = p;
	return p->offset;
}

template <typename T> int
PluginBase<T>::registerPluginStream(uint id,
	StreamRead read, StreamWrite write, StreamGetSize getSize)
{
	for(Plugin *p = PluginBase<T>::s_plugins; p; p = p->next)
		if(p->id == id){
			p->read = read;
			p->write = write;
			p->getSize = getSize;
			return p->offset;
		}
	return -1;
}

template <typename T> int
PluginBase<T>::getPluginOffset(uint id)
{
	for(Plugin *p = PluginBase<T>::s_plugins; p; p = p->next)
		if(p->id == id)
			return p->offset;
	return -1;
}

template <typename T> void*
PluginBase<T>::operator new(size_t)
{
	void *m = malloc(T::s_size);
	if(!m)
		throw std::bad_alloc();
	return m;
}

template <typename T> void
PluginBase<T>::operator delete(void *p)
{
	free(p);
}
}
