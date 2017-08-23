#include <Windows.h>
#include <rw.h>
#include "skeleton.h"

using namespace sk;
using namespace rw;

#ifdef RW_D3D9

static int keymap[256];
static void
initkeymap(void)
{
	int i;
	for(i = 0; i < 256; i++)
		keymap[i] = KEY_NULL;
	keymap[VK_SPACE] = ' ';
	keymap[VK_OEM_7] = '\'';
	keymap[VK_OEM_COMMA] = ',';
	keymap[VK_OEM_MINUS] = '-';
	keymap[VK_OEM_PERIOD] = '.';
	keymap[VK_OEM_2] = '/';
	for(i = '0'; i <= '9'; i++)
		keymap[i] = i;
	keymap[VK_OEM_1] = ';';
	keymap[VK_OEM_NEC_EQUAL] = '=';
	for(i = 'A'; i <= 'Z'; i++)
		keymap[i] = i;
	keymap[VK_OEM_4] = '[';
	keymap[VK_OEM_5] = '\\';
	keymap[VK_OEM_6] = ']';
	keymap[VK_OEM_3] = '`';
	keymap[VK_ESCAPE] = KEY_ESC;
	keymap[VK_RETURN] = KEY_ENTER;
	keymap[VK_TAB] = KEY_TAB;
	keymap[VK_BACK] = KEY_BACKSP;
	keymap[VK_INSERT] = KEY_INS;
	keymap[VK_DELETE] = KEY_DEL;
	keymap[VK_RIGHT] = KEY_RIGHT;
	keymap[VK_LEFT] = KEY_LEFT;
	keymap[VK_DOWN] = KEY_DOWN;
	keymap[VK_UP] = KEY_UP;
	keymap[VK_PRIOR] = KEY_PGUP;
	keymap[VK_NEXT] = KEY_PGDN;
	keymap[VK_HOME] = KEY_HOME;
	keymap[VK_END] = KEY_END;
	keymap[VK_MODECHANGE] = KEY_CAPSLK;
	for(i = VK_F1; i <= VK_F24; i++)
		keymap[i] = i-VK_F1+KEY_F1;
	keymap[VK_LSHIFT] = KEY_LSHIFT;
	keymap[VK_LCONTROL] = KEY_LCTRL;
	keymap[VK_LMENU] = KEY_LALT;
	keymap[VK_RSHIFT] = KEY_RSHIFT;
	keymap[VK_RCONTROL] = KEY_RCTRL;
	keymap[VK_RMENU] = KEY_RALT;
}
bool running;

static void KeyUp(int key) { EventHandler(KEYUP, &key); }
static void KeyDown(int key) { EventHandler(KEYDOWN, &key); }

LRESULT CALLBACK
WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg){
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
		if(wParam == VK_MENU){
			if(GetKeyState(VK_LMENU) & 0x8000) KeyDown(keymap[VK_LMENU]);
			if(GetKeyState(VK_RMENU) & 0x8000) KeyDown(keymap[VK_RMENU]);
		}else if(wParam == VK_CONTROL){
			if(GetKeyState(VK_LCONTROL) & 0x8000) KeyDown(keymap[VK_LCONTROL]);
			if(GetKeyState(VK_RCONTROL) & 0x8000) KeyDown(keymap[VK_RCONTROL]);
		}else if(wParam == VK_SHIFT){
			if(GetKeyState(VK_LSHIFT) & 0x8000) KeyDown(keymap[VK_LSHIFT]);
			if(GetKeyState(VK_RSHIFT) & 0x8000) KeyDown(keymap[VK_RSHIFT]);
		}else
			KeyDown(keymap[wParam]);
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		if(wParam == VK_MENU){
			if((GetKeyState(VK_LMENU) & 0x8000) == 0) KeyUp(keymap[VK_LMENU]);
			if((GetKeyState(VK_RMENU) & 0x8000) == 0) KeyUp(keymap[VK_RMENU]);
		}else if(wParam == VK_CONTROL){
			if((GetKeyState(VK_LCONTROL) & 0x8000) == 0) KeyUp(keymap[VK_LCONTROL]);
			if((GetKeyState(VK_RCONTROL) & 0x8000) == 0) KeyUp(keymap[VK_RCONTROL]);
		}else if(wParam == VK_SHIFT){
			if((GetKeyState(VK_LSHIFT) & 0x8000) == 0) KeyUp(keymap[VK_LSHIFT]);
			if((GetKeyState(VK_RSHIFT) & 0x8000) == 0) KeyUp(keymap[VK_RSHIFT]);
		}else
			KeyUp(keymap[wParam]);
		break;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

	case WM_QUIT:
		running = false;
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND
MakeWindow(HINSTANCE instance, int width, int height, const char *title)
{
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = instance;
	wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "librwD3D9";
	if(!RegisterClass(&wc)){
		MessageBox(0, "RegisterClass() - FAILED", 0, 0);
		return 0;
	}

	HWND win;
	win = CreateWindow("librwD3D9", title,
		WS_BORDER | WS_CAPTION | WS_SYSMENU |
		            WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		0, 0, width, height, 0, 0, instance, 0);
	if(!win){
		MessageBox(0, "CreateWindow() - FAILED", 0, 0);
		return 0;
	}
	ShowWindow(win, SW_SHOW);
	UpdateWindow(win);
	return win;
}

void
pollEvents(void)
{
	MSG msg;
	while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)){
		if(msg.message == WM_QUIT){
			running = false;
			break;
		}else{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE,
        PSTR cmdLine, int showCmd)
{
	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	EventHandler(INITIALIZE, nil);

	HWND win = MakeWindow(instance,
		sk::globals.width, sk::globals.height,
		sk::globals.windowtitle);
	if(win == 0){
		MessageBox(0, "MakeWindow() - FAILED", 0, 0);
		return 0;
	}
	engineStartParams.window = win;
	initkeymap();

	if(EventHandler(RWINITIALIZE, nil) == EVENTERROR)
		return 0;

	float lastTime = (float)GetTickCount();
	running = true;
	while(pollEvents(), !globals.quit){
		float currTime  = (float)GetTickCount();
		float timeDelta = (currTime - lastTime)*0.001f;

		EventHandler(IDLE, &timeDelta);

		lastTime = currTime;
	}

	EventHandler(RWTERMINATE, nil);

	return 0;
}
#endif

#ifdef RW_OPENGL
int main(int argc, char *argv[]);

int WINAPI
WinMain(HINSTANCE instance, HINSTANCE,
        PSTR cmdLine, int showCmd)
{
	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	return main(__argc, __argv);
}
#endif
