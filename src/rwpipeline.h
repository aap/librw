namespace rw {

struct PipeAttribute
{
	const char *name;
	uint32 attrib;
};

struct Atomic;

class Pipeline
{
public:
	uint32 pluginID;
	uint32 pluginData;
	uint32 platform;

	Pipeline(uint32 platform);
	Pipeline(Pipeline *p);
	~Pipeline(void);
	virtual void dump(void);
};

class ObjPipeline : public Pipeline
{
public:
	ObjPipeline(uint32 platform) : Pipeline(platform) {}
	virtual void instance(Atomic *atomic);
	virtual void uninstance(Atomic *atomic);
	virtual void render(Atomic *atomic);
};

}
