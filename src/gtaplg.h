namespace Rw {

enum
{
	ID_SPECMAT         = 0x253f2f6,
	ID_EXTRAVERTCOLORS = 0x253f2f9,
	ID_ENVMAT          = 0x253f2fc,
	ID_BREAKABLE       = 0x253f2fd,
	ID_NODENAME        = 0x253f2fe
};

// Node name

extern int32 NodeNameOffset;
void RegisterNodeNamePlugin(void);

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

extern int32 BreakableOffset;
void RegisterBreakableModelPlugin(void);

// Extra vert colors

struct ExtraVertColors
{
	uint8 *nightColors;
	uint8 *dayColors;
	float balance;
};

extern int32 ExtraVertColorOffset;
void RegisterExtraVertColorPlugin(void);

// Environment mat

struct EnvMat
{
	int8 scaleX, scaleY;
	int8 transScaleX, transScaleY;
	int8 shininess;
	Texture *texture;
};

extern int32 EnvMatOffset;

// Specular mat

struct SpecMat
{
	float specularity;
	Texture *texture;
};

extern int32 SpecMatOffset;

void RegisterEnvSpecPlugin(void);

}
