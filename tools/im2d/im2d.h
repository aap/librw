extern bool Im2DColored;
extern bool Im2DTextured;

extern rw::int32 Im2DPrimType;

extern rw::V2d ScreenSize;
extern float Scale;

extern rw::RGBA SolidWhite;
extern rw::RGBA SolidBlack;
extern rw::RGBA SolidRed;
extern rw::RGBA SolidGreen;
extern rw::RGBA SolidBlue;
extern rw::RGBA SolidYellow;
extern rw::RGBA SolidPurple;
extern rw::RGBA SolidCyan;


void Im2DInitialize(rw::Camera *camera);
void Im2DTerminate(void);
void Im2DRender(void);
void Im2DSize(rw::Camera *camera, rw::int32 w, rw::int32 h);

void LineListCreate(rw::Camera *camera);
void LineListSetColor(bool white);
void LineListRender(void);

void IndexedLineListCreate(rw::Camera *camera);
void IndexedLineListSetColor(bool white);
void IndexedLineListRender(void);

void PolyLineCreate(rw::Camera *camera);
void PolyLineSetColor(bool white);
void PolyLineRender(void);

void IndexedPolyLineCreate(rw::Camera *camera);
void IndexedPolyLineSetColor(bool white);
void IndexedPolyLineRender(void);

void TriListCreate(rw::Camera *camera);
void TriListSetColor(bool white);
void TriListRender(void);

void IndexedTriListCreate(rw::Camera *camera);
void IndexedTriListSetColor(bool white);
void IndexedTriListRender(void);

void TriStripCreate(rw::Camera *camera);
void TriStripSetColor(bool white);
void TriStripRender(void);

void IndexedTriStripCreate(rw::Camera *camera);
void IndexedTriStripSetColor(bool white);
void IndexedTriStripRender(void);

void TriFanCreate(rw::Camera *camera);
void TriFanSetColor(bool white);
void TriFanRender(void);

void IndexedTriFanCreate(rw::Camera *camera);
void IndexedTriFanSetColor(bool white);
void IndexedTriFanRender(void);
