#ifdef LIBRW_SDL3

#include <rw.h>
#include "skeleton.h"

using namespace sk;
using namespace rw;

#ifdef RW_OPENGL

SDL_Window *window;

static int keyCodeToSkKey(SDL_Keycode keycode) {
	switch (keycode) {
	case SDLK_SPACE: return ' ';
	case SDLK_APOSTROPHE: return '\'';
	case SDLK_COMMA: return ',';
	case SDLK_MINUS: return '-';
	case SDLK_PERIOD: return '.';
	case SDLK_SLASH: return '/';

	case SDLK_0: return '0';
	case SDLK_1: return '1';
	case SDLK_2: return '2';
	case SDLK_3: return '3';
	case SDLK_4: return '4';
	case SDLK_5: return '5';
	case SDLK_6: return '6';
	case SDLK_7: return '7';
	case SDLK_8: return '8';
	case SDLK_9: return '9';

	case SDLK_SEMICOLON: return ';';
	case SDLK_EQUALS: return '=';

	case SDLK_A: return 'A';
	case SDLK_B: return 'B';
	case SDLK_C: return 'C';
	case SDLK_D: return 'D';
	case SDLK_E: return 'E';
	case SDLK_F: return 'F';
	case SDLK_G: return 'G';
	case SDLK_H: return 'H';
	case SDLK_I: return 'I';
	case SDLK_J: return 'J';
	case SDLK_K: return 'K';
	case SDLK_L: return 'L';
	case SDLK_M: return 'M';
	case SDLK_N: return 'N';
	case SDLK_O: return 'O';
	case SDLK_P: return 'P';
	case SDLK_Q: return 'Q';
	case SDLK_R: return 'R';
	case SDLK_S: return 'S';
	case SDLK_T: return 'T';
	case SDLK_U: return 'U';
	case SDLK_V: return 'V';
	case SDLK_W: return 'W';
	case SDLK_X: return 'X';
	case SDLK_Y: return 'Y';
	case SDLK_Z: return 'Z';

	case SDLK_LEFTBRACKET: return '[';
	case SDLK_BACKSLASH: return '\\';
	case SDLK_RIGHTBRACKET: return ']';
	case SDLK_GRAVE: return '`';
	case SDLK_ESCAPE: return KEY_ESC;
	case SDLK_RETURN: return KEY_ENTER;
	case SDLK_TAB: return KEY_TAB;
	case SDLK_BACKSPACE: return KEY_BACKSP;
	case SDLK_INSERT: return KEY_INS;
	case SDLK_DELETE: return KEY_DEL;
	case SDLK_RIGHT: return KEY_RIGHT;
	case SDLK_LEFT: return KEY_LEFT;
	case SDLK_DOWN: return KEY_DOWN;
	case SDLK_UP: return KEY_UP;
	case SDLK_PAGEUP: return KEY_PGUP;
	case SDLK_PAGEDOWN: return KEY_PGDN;
	case SDLK_HOME: return KEY_HOME;
	case SDLK_END: return KEY_END;
	case SDLK_CAPSLOCK: return KEY_CAPSLK;
	case SDLK_SCROLLLOCK: return KEY_NULL;
	case SDLK_NUMLOCKCLEAR: return KEY_NULL;
	case SDLK_PRINTSCREEN: return KEY_NULL;
	case SDLK_PAUSE: return KEY_NULL;

	case SDLK_F1: return KEY_F1;
	case SDLK_F2: return KEY_F2;
	case SDLK_F3: return KEY_F3;
	case SDLK_F4: return KEY_F4;
	case SDLK_F5: return KEY_F5;
	case SDLK_F6: return KEY_F6;
	case SDLK_F7: return KEY_F7;
	case SDLK_F8: return KEY_F8;
	case SDLK_F9: return KEY_F9;
	case SDLK_F10: return KEY_F10;
	case SDLK_F11: return KEY_F11;
	case SDLK_F12: return KEY_F12;
	case SDLK_F13: return KEY_NULL;
	case SDLK_F14: return KEY_NULL;
	case SDLK_F15: return KEY_NULL;
	case SDLK_F16: return KEY_NULL;
	case SDLK_F17: return KEY_NULL;
	case SDLK_F18: return KEY_NULL;
	case SDLK_F19: return KEY_NULL;
	case SDLK_F20: return KEY_NULL;
	case SDLK_F21: return KEY_NULL;
	case SDLK_F22: return KEY_NULL;
	case SDLK_F23: return KEY_NULL;
	case SDLK_F24: return KEY_NULL;

	case SDLK_KP_0: return KEY_NULL;
	case SDLK_KP_1: return KEY_NULL;
	case SDLK_KP_2: return KEY_NULL;
	case SDLK_KP_3: return KEY_NULL;
	case SDLK_KP_4: return KEY_NULL;
	case SDLK_KP_5: return KEY_NULL;
	case SDLK_KP_6: return KEY_NULL;
	case SDLK_KP_7: return KEY_NULL;
	case SDLK_KP_8: return KEY_NULL;
	case SDLK_KP_9: return KEY_NULL;
	case SDLK_KP_DECIMAL: return KEY_NULL;
	case SDLK_KP_DIVIDE: return KEY_NULL;
	case SDLK_KP_MULTIPLY: return KEY_NULL;
	case SDLK_KP_MINUS: return KEY_NULL;
	case SDLK_KP_PLUS: return KEY_NULL;
	case SDLK_KP_ENTER: return KEY_NULL;
	case SDLK_KP_EQUALS: return KEY_NULL;

	case SDLK_LSHIFT: return KEY_LSHIFT;
	case SDLK_LCTRL: return KEY_LCTRL;
	case SDLK_LALT: return KEY_LALT;
	case SDLK_LGUI: return KEY_NULL;
	case SDLK_RSHIFT: return KEY_RSHIFT;
	case SDLK_RCTRL: return KEY_RCTRL;
	case SDLK_RALT: return KEY_RALT;
	case SDLK_RGUI: return KEY_NULL;
	case SDLK_MENU: return KEY_NULL;
	}
	return KEY_NULL;
}

#if 0
static void
keypress(SDL_Window *window, int key, int scancode, int action, int mods)
{
	if(key >= 0 && key <= GLFW_KEY_LAST){
		if(action == GLFW_RELEASE) KeyUp(keymap[key]);
		else if(action == GLFW_PRESS)   KeyDown(keymap[key]);
		else if(action == GLFW_REPEAT)  KeyDown(keymap[key]);
	}
}

static void
charinput(GLFWwindow *window, unsigned int c)
{
	EventHandler(CHARINPUT, (void*)(uintptr)c);
}

static void
resize(GLFWwindow *window, int w, int h)
{
	rw::Rect r;
	r.x = 0;
	r.y = 0;
	r.w = w;
	r.h = h;
	EventHandler(RESIZE, &r);
}

static void
mousebtn(GLFWwindow *window, int button, int action, int mods)
{
	static int buttons = 0;
	sk::MouseState ms;

	switch(button){
	case GLFW_MOUSE_BUTTON_LEFT:
		if(action == GLFW_PRESS)
			buttons |= 1;
		else
			buttons &= ~1;
		break;
	case GLFW_MOUSE_BUTTON_MIDDLE:
		if(action == GLFW_PRESS)
			buttons |= 2;
		else
			buttons &= ~2;
		break;
	case GLFW_MOUSE_BUTTON_RIGHT:
		if(action == GLFW_PRESS)
			buttons |= 4;
		else
			buttons &= ~4;
		break;
	}

	sk::MouseState ms;
	ms.buttons = buttons;
	EventHandler(MOUSEBTN, &ms);
}
#endif

enum mousebutton {
BUTTON_LEFT = 0x1,
BUTTON_MIDDLE = 0x2,
BUTTON_RIGHT = 0x4,
};

int
main(int argc, char *argv[])
{
	args.argc = argc;
	args.argv = argv;

	if(EventHandler(INITIALIZE, nil) == EVENTERROR)
		return 0;

	engineOpenParams.width = sk::globals.width;
	engineOpenParams.height = sk::globals.height;
	engineOpenParams.windowtitle = sk::globals.windowtitle;
	engineOpenParams.window = &window;

	if(EventHandler(RWINITIALIZE, nil) == EVENTERROR)
		return 0;

	Uint64 lastTicks = SDL_GetPerformanceCounter();
	const float tickPeriod = 1.f / SDL_GetPerformanceFrequency();
	SDL_Event event;
	int mouseButtons = 0;

	SDL_StartTextInput(window);

	while(!sk::globals.quit){
		while(SDL_PollEvent(&event)){
			switch(event.type){
			case SDL_EVENT_QUIT:
				sk::globals.quit = true;
				break;
			case SDL_EVENT_WINDOW_RESIZED: {
				rw::Rect r;
				SDL_GetWindowPosition(window, &r.x, &r.y);
				r.w = event.window.data1;
				r.h = event.window.data2;
				EventHandler(RESIZE, &r);
				break;
			}
			case SDL_EVENT_KEY_UP: {
				int c = keyCodeToSkKey(event.key.key);
				EventHandler(KEYUP, &c);
				break;
			}
			case SDL_EVENT_KEY_DOWN: {
				int c = keyCodeToSkKey(event.key.key);
				EventHandler(KEYDOWN, &c);
				break;
			}
			case SDL_EVENT_TEXT_INPUT: {
				const char *c = event.text.text;
				while (int ci = *c) {
					EventHandler(CHARINPUT, (void*)(uintptr)ci);
					++c;
				}
				break;
			}
			case SDL_EVENT_MOUSE_MOTION: {
				sk::MouseState ms;
				ms.posx = event.motion.x;
				ms.posy = event.motion.y;
				EventHandler(MOUSEMOVE, &ms);
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_DOWN: {
				switch (event.button.button) {
				case SDL_BUTTON_LEFT: mouseButtons |= BUTTON_LEFT; break;
				case SDL_BUTTON_MIDDLE: mouseButtons |= BUTTON_MIDDLE; break;
				case SDL_BUTTON_RIGHT: mouseButtons |= BUTTON_RIGHT; break;
				}
				sk::MouseState ms;
				ms.buttons = mouseButtons;
				EventHandler(MOUSEBTN, &ms);
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_UP: {
				switch (event.button.button) {
				case SDL_BUTTON_LEFT: mouseButtons &= ~BUTTON_LEFT; break;
				case SDL_BUTTON_MIDDLE: mouseButtons &= ~BUTTON_MIDDLE; break;
				case SDL_BUTTON_RIGHT: mouseButtons &= ~BUTTON_RIGHT; break;
				}
				sk::MouseState ms;
				ms.buttons = mouseButtons;
				EventHandler(MOUSEBTN, &ms);
				break;
			}
			}
		}
		Uint64 currTicks = SDL_GetPerformanceCounter();
		float timeDelta = (currTicks - lastTicks) * tickPeriod;

		EventHandler(IDLE, &timeDelta);

		lastTicks = currTicks;
	}

	SDL_StopTextInput(window);

	EventHandler(RWTERMINATE, nil);

	return 0;
}

namespace sk {

void
SetMousePosition(int x, int y)
{
	SDL_WarpMouseInWindow(*engineOpenParams.window, x, y);
}

}

#endif
#endif
