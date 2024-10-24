#pragma once
#include <d3d11.h>

enum class ARStudioTheme {
	STYLE_COLOR_LIGHT,
	STYLE_COLOR_DARK,
	STYLE_COLOR_CLASSIC,
};

class MainWindow {
public:
	MainWindow();
	MainWindow(ARStudioTheme theme);
	~MainWindow();
	void render();
private:
	bool initMainWindow();
	bool createDeviceD3D();
	void createRenderTarget();
	void cleanupRenderTarget();
	void cleanupDeviceD3D();

	bool is_init = false;
	ARStudioTheme theme = ARStudioTheme::STYLE_COLOR_LIGHT;

	WNDCLASSEXW wc;
	HWND hwnd;
	ID3D11Device* g_pd3dDevice = nullptr;
	ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
	IDXGISwapChain* g_pSwapChain = nullptr;
	bool g_SwapChainOccluded = false;
	ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
};