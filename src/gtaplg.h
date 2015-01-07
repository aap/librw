namespace Rw {

enum
{
	ID_EXTRAVERTCOLORS = 0x253f2f9,
	ID_BREAKABLE       = 0x253f2fd,
	ID_NODENAME        = 0x253f2fe
};

void RegisterNodeNamePlugin(void);

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

void RegisterBreakableModelPlugin(void);

struct ExtraVertColors
{
	uint8 *nightColors;
	uint8 *dayColors;
	float balance;
};

void RegisterExtraVertColorPlugin(void);

}
