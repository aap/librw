#include <rw.h>
#include <skeleton.h>
#include <assert.h>

#include "imgui/imgui.h"
#include "imgui_impl_rw.h"

using namespace rw::RWDEVICE;

static rw::Texture *g_FontTexture;
static Im2DVertex *g_vertbuf;
static int g_vertbufSize;

void
ImGui_ImplRW_RenderDrawLists(ImDrawData* draw_data)
{
	ImGuiIO &io = ImGui::GetIO();

	// minimized
	if (io.DisplaySize.x <= 0.0f || io.DisplaySize.y <= 0.0f)
		return;

	if(g_vertbuf == nil || g_vertbufSize < draw_data->TotalVtxCount){
		if(g_vertbuf){
			rwFree(g_vertbuf);
			g_vertbuf = nil;
		}
		g_vertbufSize = draw_data->TotalVtxCount + 5000;
		g_vertbuf = rwNewT(Im2DVertex, g_vertbufSize, 0);
	}

	float xoff = 0.0f;
	float yoff = 0.0f;
#ifdef RWHALFPIXEL
	xoff = -0.5;
	yoff = 0.5;
#endif

	rw::Camera *cam = (rw::Camera*)rw::engine->currentCamera;
	Im2DVertex *vtx_dst = g_vertbuf;
	float recipZ = 1.0f/cam->nearPlane;
	for(int n = 0; n < draw_data->CmdListsCount; n++){
		const ImDrawList *cmd_list = draw_data->CmdLists[n];
		const ImDrawVert *vtx_src = cmd_list->VtxBuffer.Data;
		for(int i = 0; i < cmd_list->VtxBuffer.Size; i++){
			vtx_dst[i].setScreenX(vtx_src[i].pos.x + xoff);
			vtx_dst[i].setScreenY(vtx_src[i].pos.y + yoff);
			vtx_dst[i].setScreenZ(rw::im2d::GetNearZ());
			vtx_dst[i].setCameraZ(cam->nearPlane);
			vtx_dst[i].setRecipCameraZ(recipZ);
			vtx_dst[i].setColor(vtx_src[i].col&0xFF, vtx_src[i].col>>8 & 0xFF, vtx_src[i].col>>16 & 0xFF, vtx_src[i].col>>24 & 0xFF);
			vtx_dst[i].setU(vtx_src[i].uv.x, recipZ);
			vtx_dst[i].setV(vtx_src[i].uv.y, recipZ);
		}
		vtx_dst += cmd_list->VtxBuffer.Size;
	}

	int vertexAlpha = rw::GetRenderState(rw::VERTEXALPHA);
	int srcBlend = rw::GetRenderState(rw::SRCBLEND);
	int dstBlend = rw::GetRenderState(rw::DESTBLEND);
	int ztest = rw::GetRenderState(rw::ZTESTENABLE);
	void *tex = rw::GetRenderStatePtr(rw::TEXTURERASTER);
	int addrU = rw::GetRenderState(rw::TEXTUREADDRESSU);
	int addrV = rw::GetRenderState(rw::TEXTUREADDRESSV);
	int filter = rw::GetRenderState(rw::TEXTUREFILTER);
	int cullmode = rw::GetRenderState(rw::CULLMODE);

	rw::SetRenderState(rw::VERTEXALPHA, 1);
	rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
	rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);
	rw::SetRenderState(rw::ZTESTENABLE, 0);
	rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);

	int vtx_offset = 0;
	for(int n = 0; n < draw_data->CmdListsCount; n++){
		const ImDrawList *cmd_list = draw_data->CmdLists[n];
		int idx_offset = 0;
		for(int i = 0; i < cmd_list->CmdBuffer.Size; i++){
			const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[i];
			if(pcmd->UserCallback)
				pcmd->UserCallback(cmd_list, pcmd);
			else{
				rw::Texture *tex = (rw::Texture*)pcmd->GetTexID();
				if(tex && tex->raster){
					rw::SetRenderStatePtr(rw::TEXTURERASTER, tex->raster);
					rw::SetRenderState(rw::TEXTUREADDRESSU, tex->getAddressU());
					rw::SetRenderState(rw::TEXTUREADDRESSV, tex->getAddressV());
					rw::SetRenderState(rw::TEXTUREFILTER, tex->getFilter());
				}else
					rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
				rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST,
					g_vertbuf+vtx_offset, cmd_list->VtxBuffer.Size,
					cmd_list->IdxBuffer.Data+idx_offset, pcmd->ElemCount);
			}
			idx_offset += pcmd->ElemCount;
		}
		vtx_offset += cmd_list->VtxBuffer.Size;
	}

	rw::SetRenderState(rw::VERTEXALPHA,vertexAlpha);
	rw::SetRenderState(rw::SRCBLEND, srcBlend);
	rw::SetRenderState(rw::DESTBLEND, dstBlend);
	rw::SetRenderState(rw::ZTESTENABLE, ztest);
	rw::SetRenderStatePtr(rw::TEXTURERASTER, tex);
	rw::SetRenderState(rw::TEXTUREADDRESSU, addrU);
	rw::SetRenderState(rw::TEXTUREADDRESSV, addrV);
	rw::SetRenderState(rw::TEXTUREFILTER, filter);
	rw::SetRenderState(rw::CULLMODE, cullmode);
}

bool
ImGui_ImplRW_Init(void)
{
	using namespace sk;

	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();

	return true;
}

void
ImGui_ImplRW_Shutdown(void)
{
}

static bool
ImGui_ImplRW_CreateFontsTexture()
{
	// Build texture atlas
	ImGuiIO &io = ImGui::GetIO();
	unsigned char *pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, nil);

	rw::Image *image;
	image = rw::Image::create(width, height, 32);
	image->allocate();
	for(int y = 0; y < height; y++)
		memcpy(image->pixels + image->stride*y, pixels + width*4* y, width*4);
	g_FontTexture = rw::Texture::create(rw::Raster::createFromImage(image));
	g_FontTexture->setFilter(rw::Texture::LINEAR);
	image->destroy();
	
	// Store our identifier
	io.Fonts->TexID = (void*)g_FontTexture;

	return true;
}

bool
ImGui_ImplRW_CreateDeviceObjects()
{
//	if(!g_pd3dDevice)
//		return false;
	if(!ImGui_ImplRW_CreateFontsTexture())
		return false;
	return true;
}

void
ImGui_ImplRW_NewFrame(float timeDelta)
{
	if(!g_FontTexture)
		ImGui_ImplRW_CreateDeviceObjects();

	ImGuiIO &io = ImGui::GetIO();

	io.DisplaySize = ImVec2(sk::globals.width, sk::globals.height);
	io.DeltaTime = timeDelta;

	io.KeyCtrl = ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) || ImGui::IsKeyPressed(ImGuiKey_RightCtrl);
	io.KeyShift = ImGui::IsKeyPressed(ImGuiKey_LeftShift) || ImGui::IsKeyPressed(ImGuiKey_RightShift);
	io.KeyAlt = ImGui::IsKeyPressed(ImGuiKey_LeftAlt) || ImGui::IsKeyPressed(ImGuiKey_RightAlt);
	io.KeySuper = false;

	if(io.WantSetMousePos)
		sk::SetMousePosition(io.MousePos.x, io.MousePos.y);

	ImGui::NewFrame();
}

static ImGuiKey SkKeyToImGuiKey(int keycode)
{
    switch (keycode) {
        case ' ': return ImGuiKey_Space;
        case '\'': return ImGuiKey_Apostrophe;
        case '-': return ImGuiKey_Minus;
        case '.': return ImGuiKey_Period;
        case '/': return ImGuiKey_Slash;
        case '0': return ImGuiKey_0;
        case '1': return ImGuiKey_1;
        case '2': return ImGuiKey_2;
        case '3': return ImGuiKey_3;
        case '4': return ImGuiKey_4;
        case '5': return ImGuiKey_5;
        case '6': return ImGuiKey_6;
        case '7': return ImGuiKey_7;
        case '8': return ImGuiKey_8;
        case '9': return ImGuiKey_9;

        case ';': return ImGuiKey_Semicolon;
        case '=': return ImGuiKey_Equal;

        case 'A': return ImGuiKey_A;
        case 'B': return ImGuiKey_B;
        case 'C': return ImGuiKey_C;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case 'G': return ImGuiKey_G;
        case 'H': return ImGuiKey_H;
        case 'I': return ImGuiKey_I;
        case 'J': return ImGuiKey_J;
        case 'K': return ImGuiKey_K;
        case 'L': return ImGuiKey_L;
        case 'M': return ImGuiKey_M;
        case 'N': return ImGuiKey_N;
        case 'O': return ImGuiKey_O;
        case 'P': return ImGuiKey_P;
        case 'Q': return ImGuiKey_Q;
        case 'R': return ImGuiKey_R;
        case 'S': return ImGuiKey_S;
        case 'T': return ImGuiKey_T;
        case 'U': return ImGuiKey_U;
        case 'V': return ImGuiKey_V;
        case 'W': return ImGuiKey_W;
        case 'X': return ImGuiKey_X;
        case 'Y': return ImGuiKey_Y;
        case 'Z': return ImGuiKey_Z;

        case '[': return ImGuiKey_LeftBracket;
        case '\\': return ImGuiKey_Backslash;
        case ']': return ImGuiKey_RightBracket;
        case '`': return ImGuiKey_GraveAccent;
        case sk::KEY_ESC: return ImGuiKey_Escape;
        case sk::KEY_ENTER: return ImGuiKey_Enter;
        case sk::KEY_TAB: return ImGuiKey_Tab;
        case sk::KEY_BACKSP: return ImGuiKey_Backspace;
        case sk::KEY_INS: return ImGuiKey_Insert;
        case sk::KEY_DEL: return ImGuiKey_Delete;
        case sk::KEY_RIGHT: return ImGuiKey_RightArrow;
        case sk::KEY_LEFT: return ImGuiKey_LeftArrow;
        case sk::KEY_DOWN: return ImGuiKey_DownArrow;
        case sk::KEY_UP: return ImGuiKey_UpArrow;
        case sk::KEY_PGUP: return ImGuiKey_PageUp;
        case sk::KEY_PGDN: return ImGuiKey_PageDown;
        case sk::KEY_HOME: return ImGuiKey_Home;
        case sk::KEY_END: return ImGuiKey_End;
        case sk::KEY_CAPSLK: return ImGuiKey_CapsLock;

        case sk::KEY_F1: return ImGuiKey_F1;
        case sk::KEY_F2: return ImGuiKey_F2;
        case sk::KEY_F3: return ImGuiKey_F3;
        case sk::KEY_F4: return ImGuiKey_F4;
        case sk::KEY_F5: return ImGuiKey_F5;
        case sk::KEY_F6: return ImGuiKey_F6;
        case sk::KEY_F7: return ImGuiKey_F7;
        case sk::KEY_F8: return ImGuiKey_F8;
        case sk::KEY_F9: return ImGuiKey_F9;
        case sk::KEY_F10: return ImGuiKey_F10;
        case sk::KEY_F11: return ImGuiKey_F11;
        case sk::KEY_F12: return ImGuiKey_F12;

        case sk::KEY_LSHIFT: return ImGuiKey_LeftShift;
        case sk::KEY_LCTRL: return ImGuiKey_LeftCtrl;
        case sk::KEY_LALT: return ImGuiKey_LeftAlt;
        case sk::KEY_RSHIFT: return ImGuiKey_RightShift;
        case sk::KEY_RCTRL: return ImGuiKey_RightCtrl;
        case sk::KEY_RALT: return ImGuiKey_RightAlt;

        case sk::KEY_NULL: return ImGuiKey_None;
    }
    return ImGuiKey_None;
}

sk::EventStatus
ImGuiEventHandler(sk::Event e, void *param)
{
	using namespace sk;

	ImGuiIO &io = ImGui::GetIO();
	MouseState *ms;
	uint c;

	switch(e){
	case KEYDOWN:
        io.AddKeyEvent(SkKeyToImGuiKey(*(int*)param), true);
		return EVENTPROCESSED;
	case KEYUP:
        io.AddKeyEvent(SkKeyToImGuiKey(*(int*)param), false);
		return EVENTPROCESSED;
	case CHARINPUT:
		c = (uint)(uintptr)param;
		io.AddInputCharacter((unsigned short)c);
		return EVENTPROCESSED;
	case MOUSEMOVE:
		ms = (MouseState*)param;
		io.MousePos.x = ms->posx;
		io.MousePos.y = ms->posy;
		return EVENTPROCESSED;
	case MOUSEBTN:
		ms = (MouseState*)param;
		io.MouseDown[0] = !!(ms->buttons & 1);
		io.MouseDown[2] = !!(ms->buttons & 2);
		io.MouseDown[1] = !!(ms->buttons & 4);
		return EVENTPROCESSED;
	}
	return EVENTPROCESSED;
}
