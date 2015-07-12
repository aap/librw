namespace rw {

struct PipeAttribute
{
	const char *name;
	uint32 attrib;
};

struct Atomic;

struct Pipeline
{
	uint32 pluginID;
	uint32 pluginData;

	uint32 platform;
	PipeAttribute *attribs[10];

	Pipeline(uint32 platform);
	Pipeline(Pipeline *p);
	~Pipeline(void);
	virtual void instance(Atomic *atomic);
	virtual void uninstance(Atomic *atomic);
	virtual void render(Atomic *atomic);
};

}
