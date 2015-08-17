namespace gta {
using namespace rw;

enum
{
	ID_EXTRANORMALS    = 0x253f2f2,
	ID_SPECMAT         = 0x253f2f6,
	ID_EXTRAVERTCOLORS = 0x253f2f9,
	ID_ENVMAT          = 0x253f2fc,
	ID_BREAKABLE       = 0x253f2fd,
	ID_NODENAME        = 0x253f2fe
};

// Node name

extern int32 nodeNameOffset;
void registerNodeNamePlugin(void);

// Breakable model

struct Breakable
{
	uint32 position;
	uint32 numVertices;
	uint32 numFaces;
	uint32 numMaterials;

	float32 *vertices;
	float32 *texCoords;
	uint8   *colors;
	uint16  *faces;
	uint16  *matIDs;
	char    (*texNames)[32];
	char    (*maskNames)[32];
	float32 (*surfaceProps)[3];
};

extern int32 breakableOffset;
void registerBreakableModelPlugin(void);

// Extra normals (only on Xbox)

extern int32 extraNormalsOffset;
void registerExtraNormalsPlugin(void);

// Extra vert colors (not on Xbox)

struct ExtraVertColors
{
	uint8 *nightColors;
	uint8 *dayColors;
	float balance;
};

extern int32 extraVertColorOffset;
void registerExtraVertColorPlugin(void);

// Environment mat

struct EnvMat
{
	int8 scaleX, scaleY;
	int8 transScaleX, transScaleY;
	int8 shininess;
	Texture *texture;
};

extern int32 envMatOffset;

// Specular mat

struct SpecMat
{
	float specularity;
	Texture *texture;
};

extern int32 specMatOffset;

void registerEnvSpecPlugin(void);

}
