extern rw::Camera *Camera;

extern bool Im3DColored;
extern bool Im3DTextured;

extern rw::int32 Im3DPrimType;

extern rw::RGBA SolidWhite;
extern rw::RGBA SolidBlack;
extern rw::RGBA SolidRed;
extern rw::RGBA SolidGreen;
extern rw::RGBA SolidBlue;
extern rw::RGBA SolidYellow;
extern rw::RGBA SolidPurple;
extern rw::RGBA SolidCyan;


void Im3DInitialize(void);
void Im3DTerminate(void);
void Im3DRender(void);
void Im3DRotate(float xAngle, float yAngle);
void Im3DTranslateZ(float zDelta);

void LineListCreate(void);
void LineListSetColor(bool white);
void LineListRender(rw::Matrix *transform, rw::uint32 transformFlags);

void IndexedLineListCreate(void);
void IndexedLineListSetColor(bool white);
void IndexedLineListRender(rw::Matrix *transform, rw::uint32 transformFlags);

void PolyLineCreate(void);
void PolyLineSetColor(bool white);
void PolyLineRender(rw::Matrix *transform, rw::uint32 transformFlags);

void IndexedPolyLineCreate(void);
void IndexedPolyLineSetColor(bool white);
void IndexedPolyLineRender(rw::Matrix *transform, rw::uint32 transformFlags);

void TriListCreate(void);
void TriListSetColor(bool white);
void TriListRender(rw::Matrix *transform, rw::uint32 transformFlags);

void IndexedTriListCreate(void);
void IndexedTriListSetColor(bool white);
void IndexedTriListRender(rw::Matrix *transform, rw::uint32 transformFlags);

void TriStripCreate(void);
void TriStripSetColor(bool white);
void TriStripRender(rw::Matrix *transform, rw::uint32 transformFlags);

void IndexedTriStripCreate(void);
void IndexedTriStripSetColor(bool white);
void IndexedTriStripRender(rw::Matrix *transform, rw::uint32 transformFlags);

void TriFanCreate(void);
void TriFanSetColor(bool white);
void TriFanRender(rw::Matrix *transform, rw::uint32 transformFlags);

void IndexedTriFanCreate(void);
void IndexedTriFanSetColor(bool white);
void IndexedTriFanRender(rw::Matrix *transform, rw::uint32 transformFlags);
