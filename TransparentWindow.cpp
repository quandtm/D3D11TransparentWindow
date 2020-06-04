#define WIN32_LEAN_AND_MEAN
#define NO_MINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <stdint.h>

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(hwnd, msg, wParam, lParam);
}

HWND SetupWindow(HINSTANCE hInst, int width, int height)
{
	WNDCLASSEX wndcls = {};
	wndcls.cbSize = sizeof(WNDCLASSEX);
	wndcls.hCursor = LoadCursor(0, IDC_ARROW);
	wndcls.lpszClassName = "frametest";
	wndcls.hInstance = hInst;
	wndcls.lpfnWndProc = WndProc;
	wndcls.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH); // Need this unless you draw something...
	if (RegisterClassExA(&wndcls) == 0)
	{
		return nullptr;
	}
	// Doesn't matter what style is used here, it will be overwritten later
	// WS_EX_NOREDIRECTIONBITMAP removes any default client area rendering
	HWND hwnd = CreateWindowExA(WS_EX_NOREDIRECTIONBITMAP, "frametest", "Frame Test", 0, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, hInst, nullptr);
	if (hwnd == nullptr)
	{
		return nullptr;
	}

	// This removes the default titlebar/border
	SetWindowLongA(hwnd, GWL_STYLE, 0);

	RECT rct;
	GetWindowRect(hwnd, &rct);
	// This clears the border that still exists after SetWindowLongA
	SetWindowPos(hwnd, nullptr, rct.left, rct.top, rct.right - rct.left, rct.bottom - rct.top, SWP_FRAMECHANGED);
	return hwnd;
}

struct D3D
{
	IDCompositionDevice* m_dcDevice;
	IDCompositionTarget* m_dcTarget;
	IDCompositionVisual* m_dcVisual;

	ID3D11Device* m_device;
	ID3D11DeviceContext* m_context;
	IDXGISwapChain1* m_swapChain;

	ID3D11RenderTargetView* m_screenRtv;

	bool isValid;
};

D3D SetupD3D(HWND hwnd, int width, int height)
{
	D3D d3d = {};
	d3d.isValid = false;

	D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
	if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, &fl, 1, D3D11_SDK_VERSION, &d3d.m_device, nullptr, &d3d.m_context)))
	{
		return d3d;
	}

	IDXGIDevice* dxgiDev;
	d3d.m_device->QueryInterface(&dxgiDev);
	IDXGIAdapter* adapter;
	dxgiDev->GetParent(__uuidof(IDXGIAdapter), (void**)&adapter);
	IDXGIFactory3* factory;
	adapter->GetParent(__uuidof(IDXGIFactory3), (void**)&factory);
	adapter->Release();

	DXGI_SWAP_CHAIN_DESC1 swd = {};
	swd.Width = (uint32_t)width;
	swd.Height = (uint32_t)height;
	swd.BufferCount = 2;
	swd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swd.SampleDesc.Count = 1;
	swd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
	swd.Scaling = DXGI_SCALING_STRETCH;
	// Use DirectComposition to get alpha support
	bool failed = FAILED(factory->CreateSwapChainForComposition(d3d.m_device, &swd, nullptr, &d3d.m_swapChain));
	failed |= FAILED(DCompositionCreateDevice(dxgiDev, __uuidof(IDCompositionDevice), (void**)&d3d.m_dcDevice));
	factory->Release();
	dxgiDev->Release();
	if (failed)
	{
		return d3d;
	}
	if (FAILED(d3d.m_dcDevice->CreateTargetForHwnd(hwnd, true, &d3d.m_dcTarget)))
	{
		return d3d;
	}
	if (FAILED(d3d.m_dcDevice->CreateVisual(&d3d.m_dcVisual)))
	{
		return d3d;
	}
	// Bind to dcomp
	if (
		FAILED(d3d.m_dcVisual->SetContent(d3d.m_swapChain)) ||
		FAILED(d3d.m_dcTarget->SetRoot(d3d.m_dcVisual))
		)
	{
		return d3d;
	}
	d3d.m_dcDevice->Commit();

	ID3D11Resource* bbRes;
	HRESULT result = d3d.m_swapChain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&bbRes);
	if (FAILED(result))
	{
		return d3d;
	}
	D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
	rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	result = d3d.m_device->CreateRenderTargetView(bbRes, &rtvd, &d3d.m_screenRtv);
	bbRes->Release();
	if (FAILED(result))
	{
		return d3d;
	}

	d3d.isValid = true;
	return d3d;
}

void CleanupD3D(D3D& d3d)
{
#define SafeRelease(itm) if (itm != nullptr){ itm->Release(); }
	SafeRelease(d3d.m_screenRtv);
	SafeRelease(d3d.m_swapChain);
	SafeRelease(d3d.m_dcVisual);
	SafeRelease(d3d.m_dcTarget);
	SafeRelease(d3d.m_dcDevice);
	SafeRelease(d3d.m_context);
	SafeRelease(d3d.m_device);
#undef SafeRelease
}

void Draw(D3D& d3d)
{
	// Uses premultiplied alpha
	// 50% occlusion with Green=0.8 (multiplied down to 0.4)
	constexpr float const ClearCol[4] = { 0.0f, 0.4f, 0.0f, 0.5f };
	d3d.m_context->ClearRenderTargetView(d3d.m_screenRtv, ClearCol);
	d3d.m_context->OMSetRenderTargets(1, &d3d.m_screenRtv, nullptr);

	DXGI_PRESENT_PARAMETERS pp = {};
	d3d.m_swapChain->Present1(1, 0, &pp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
{
	int const Width = 500;
	int const Height = 500;
	HWND hwnd = SetupWindow(hInst, Width, Height);
	if (hwnd == nullptr)
	{
		return -1;
	}

	D3D d3d = SetupD3D(hwnd, Width, Height);
	if (!d3d.isValid)
	{
		CleanupD3D(d3d);
		return -2;
	}
	// Show/redraw window
	ShowWindow(hwnd, SW_SHOW);

	bool running = true;
	while (running)
	{
		MSG msg;
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE) > 0)
		{
			switch (msg.message)
			{
			case WM_QUIT:
				running = false;
				break;

			case WM_KEYDOWN:
				if (msg.wParam == VK_ESCAPE)
				{
					DestroyWindow(hwnd);
				}
				break;

			default:
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
				break;
			}
		}

		Draw(d3d);
	}

	CleanupD3D(d3d);
	return 0;
}