typedef struct GsDispCtx GsDispCtx;
struct GsDispCtx
{
	// two circuits
	uint64_t pmode;
	uint64_t dispfb1;
	uint64_t dispfb2;
	uint64_t display1;
	uint64_t display2;
	uint64_t bgcolor;
};

typedef struct GsDrawCtx GsDrawCtx;
struct GsDrawCtx
{
	//two contexts
	uint128_t gifTag;
	uint64_t frame1;
	uint64_t ad_frame1;
	uint64_t frame2;
	uint64_t ad_frame2;
	uint64_t zbuf1;
	uint64_t ad_zbuf1;
	uint64_t zbuf2;
	uint64_t ad_zbuf2;
	uint64_t xyoffset1;
	uint64_t ad_xyoffset1;
	uint64_t xyoffset2;
	uint64_t ad_xyoffset2;
	uint64_t scissor1;
	uint64_t ad_scissor1;
	uint64_t scissor2;
	uint64_t ad_scissor2;
};

typedef struct GsCtx GsCtx;
struct GsCtx
{
	// display context; two buffers
	GsDispCtx disp[2];
	// draw context; two buffers
	GsDrawCtx draw[2];
};

typedef struct GsCrtState GsCrtState;
struct GsCrtState
{
	int16_t inter, mode, ff;
};

void GsSetDisp(GsDispCtx *disp);
void GsSetDraw(GsDrawCtx *draw);

