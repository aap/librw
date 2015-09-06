#include "d3dUtility.h"

bool
d3d::InitD3D(HINSTANCE hInstance,
             int width, int height,
             bool windowed,
             D3DDEVTYPE deviceType,
             IDirect3DDevice9 **device)
{
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = (WNDPROC)d3d::WndProc; 
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName  = 0;
	wc.lpszClassName = "Direct3D9App";
	if(!RegisterClass(&wc)){
		MessageBox(0, "RegisterClass() - FAILED", 0, 0);
		return false;
	}
		
	HWND hwnd = 0;
	hwnd = CreateWindow("Direct3D9App", "Direct3D9App", 
		WS_BORDER | WS_CAPTION | WS_SYSMENU |
		            WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		0, 0, width, height, 0, 0, hInstance, 0); 
	if(!hwnd){
		MessageBox(0, "CreateWindow() - FAILED", 0, 0);
		return false;
	}
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	HRESULT hr = 0;
	IDirect3D9 *d3d9 = 0;
	d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if(!d3d9){
		MessageBox(0, "Direct3DCreate9() - FAILED", 0, 0);
		return false;
	}

	D3DCAPS9 caps;
	d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, deviceType, &caps);
	int vp = 0;
	if(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		vp = D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		vp = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	D3DPRESENT_PARAMETERS d3dpp;
	d3dpp.BackBufferWidth            = width;
	d3dpp.BackBufferHeight           = height;
	d3dpp.BackBufferFormat           = D3DFMT_A8R8G8B8;
	d3dpp.BackBufferCount            = 1;
	d3dpp.MultiSampleType            = D3DMULTISAMPLE_NONE;
	d3dpp.MultiSampleQuality         = 0;
	d3dpp.SwapEffect                 = D3DSWAPEFFECT_DISCARD; 
	d3dpp.hDeviceWindow              = hwnd;
	d3dpp.Windowed                   = windowed;
	d3dpp.EnableAutoDepthStencil     = true; 
	d3dpp.AutoDepthStencilFormat     = D3DFMT_D24S8;
	d3dpp.Flags                      = 0;
	d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	d3dpp.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;

	hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, deviceType, hwnd,
	                        vp, &d3dpp, device);
	if(FAILED(hr)){
		// try again using a 16-bit depth buffer
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
		
		hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, deviceType,
		                        hwnd, vp, &d3dpp, device);

		if(FAILED(hr)){
			d3d9->Release();
			MessageBox(0, "CreateDevice() - FAILED", 0, 0);
			return false;
		}
	}
	d3d9->Release();
	return true;
}

int
d3d::EnterMsgLoop(bool (*ptr_display)(float timeDelta))
{
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	static float lastTime = (float)timeGetTime(); 

	while(msg.message != WM_QUIT){
		if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}else{	
			float currTime  = (float)timeGetTime();
			float timeDelta = (currTime - lastTime)*0.001f;

			ptr_display(timeDelta);

			lastTime = currTime;
		}
	}
	return msg.wParam;
}