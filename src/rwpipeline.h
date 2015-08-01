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
	// TODO: this is bad, maybe split obj and mat pipelines?
	virtual void instance(Atomic *atomic);
	virtual void uninstance(Atomic *atomic);
	virtual void render(Atomic *atomic);
};

}
