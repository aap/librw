#ifdef RW_PS2

//#define SKEL_CDROM

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <eekernel.h>
#include <sifdev.h>
#include <sifrpc.h>
#include <libpad.h>
#ifdef SKEL_CDROM
#include <libcdvd.h>
#endif

#include <rw.h>
#include "skeleton.h"

using namespace sk;
using namespace rw;

// timer functions from the ps2 driver
void StartTime(void);
int GetTime(void);
float GetTimeF(void);

/*
 * Pad
 */

struct PadData
{
	union {
		unsigned short bits;
		struct {
			unsigned short l2 : 1;
			unsigned short r2 : 1;
			unsigned short l1 : 1;
			unsigned short r1 : 1;
			unsigned short triangle : 1;
			unsigned short circle : 1;
			unsigned short cross : 1;
			unsigned short square : 1;
			unsigned short select : 1;
			unsigned short l3 : 1;
			unsigned short r3 : 1;
			unsigned short start : 1;
			unsigned short Dup : 1;
			unsigned short Dright : 1;
			unsigned short Ddown : 1;
			unsigned short Dleft : 1;
		};
	};
	/* down/right is positive */
	float rX, rY;
	float lX, lY;
};

struct Pad
{
	PadData prev, now;
	PadData rising, falling;
};

static bool gotPadirx;
static Pad pad;
static u_long128 pad_dma_buf[scePadDmaBufferMax] __attribute__((aligned(64)));

#define EPSILON 0.2f

static float
PadAnalog(int x)
{
	float f = (x/255.0f - 0.5f)*2.0f;
	if(f > EPSILON) return (f-EPSILON)/(1.0f-EPSILON);
	if(f < -EPSILON) return (f+EPSILON)/(1.0f-EPSILON);
	return 0.0f;
}

static void
ReadPad(PadData *pd)
{
	u_char rdata[32];

	if(gotPadirx && scePadRead(0, 0, rdata) > 0){
		pd->bits = 0xFFFF ^ ((rdata[2] << 8) | rdata[3]);
		if(rdata[1] == 0x73){
			pd->rX = PadAnalog(rdata[4]);
			pd->rY = PadAnalog(rdata[5]);
			pd->lX = PadAnalog(rdata[6]);
			pd->lY = PadAnalog(rdata[7]);
		}else{
			pd->rX = pd->rY = 0.0f;
			pd->lX = pd->lY = 0.0f;
		}
	}else
		pd->bits = 0;
}

static void
UpdatePad(Pad *p)
{
	p->prev = p->now;
	ReadPad(&p->now);
	p->rising = p->now;
	p->rising.bits &= ~p->prev.bits;
	p->falling = p->prev;
	p->falling.bits &= ~p->now.bits;
}

static void
InitPad(void)
{
	if(gotPadirx){
		scePadInit(0);
		scePadPortOpen(0, 0, pad_dma_buf);
	}
}

// translate pad buttons to key events so apps
// written against the keyboard interface just work
static const struct {
	unsigned short mask;
	int key;
} padkeymap[] = {
	{ 0x8000, KEY_LEFT },	// Dleft
	{ 0x4000, KEY_DOWN },	// Ddown
	{ 0x2000, KEY_RIGHT },	// Dright
	{ 0x1000, KEY_UP },	// Dup
	{ 0x0040, ' ' },	// cross
	{ 0x0020, KEY_ENTER },	// circle
	{ 0x0080, KEY_TAB },	// square
	{ 0x0010, KEY_ESC },	// triangle
	{ 0x0004, KEY_PGUP },	// l1
	{ 0x0008, KEY_PGDN },	// r1
	{ 0x0800, KEY_ENTER },	// start
};

static void
PadEvents(void)
{
	int i, key;

	UpdatePad(&pad);
	for(i = 0; i < (int)(sizeof(padkeymap)/sizeof(padkeymap[0])); i++){
		key = padkeymap[i].key;
		if(pad.rising.bits & padkeymap[i].mask)
			EventHandler(KEYDOWN, &key);
		if(pad.falling.bits & padkeymap[i].mask)
			EventHandler(KEYUP, &key);
	}
}

/*
 * IOP modules
 */

static char*
GetModulePath(char *dst, const char *module)
{
#ifdef SKEL_CDROM
	char *p;
	sprintf(dst, "cdrom0:\\MODULES\\%s;1", module);
	for(p = dst+7; *p; p++)
		if(islower(*p))
			*p = toupper(*p);
#else
	sprintf(dst, "host0:/usr/local/sce/iop/modules/%s", module);
#endif
	return dst;
}

static void
LoadModules(void)
{
	char buf[128];
	int i;

	gotPadirx = true;
	for(i = 0; i < 10; i++)
		if(sceSifLoadModule(GetModulePath(buf, "sio2man.irx"), 0, NULL) >= 0)
			goto sio2ok;
	printf("can't load module sio2man\n");
	gotPadirx = false;
	return;
sio2ok:
	for(i = 0; i < 10; i++)
		if(sceSifLoadModule(GetModulePath(buf, "padman.irx"), 0, NULL) >= 0)
			return;
	printf("can't load module padman\n");
	gotPadirx = false;
}

/*
 * Main loop
 */

int
main(int argc, char *argv[])
{
	float timeDelta, drawTime, synchTime;
	int frame;

	args.argc = argc;
	args.argv = argv;

	sceSifInitRpc(0);
#ifdef SKEL_CDROM
	while(!sceSifRebootIop("cdrom0:\\MODULES\\" IOP_IMAGE_FILE ";1"));
	while(!sceSifSyncIop());
	sceSifInitRpc(0);
	sceSifLoadFileReset();
	sceCdInit(SCECdINIT);
	sceCdMmode(SCECdCD);
	sceFsReset();
	sceSifInitIopHeap();
#endif
	LoadModules();
	InitPad();

	if(EventHandler(INITIALIZE, nil) == EVENTERROR)
		return 0;

	rw::ps2::adcHack = true;
	if(EventHandler(RWINITIALIZE, nil) == EVENTERROR)
		return 0;

	timeDelta = 1.0f/60.0f;
	frame = 0;
	while(!sk::globals.quit){
		StartTime();

		rw::ps2::beginFrame(frame);

		PadEvents();

		EventHandler(IDLE, &timeDelta);

		rw::ps2::endFrame(&drawTime, &synchTime);

		timeDelta = GetTimeF()*0.001f;
		frame++;
	}

	EventHandler(RWTERMINATE, nil);

	return 0;
}

namespace sk {

// no window to move the pointer in
void
SetMousePosition(int x, int y)
{
}

}

#endif
