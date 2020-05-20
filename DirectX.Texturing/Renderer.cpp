#include "Renderer.h"
#include <SDL_syswm.h>
#include "Application.h"
#include <iostream>

namespace
{
	std::unique_ptr<MainWindow>& GetWindow() 
	{
		return Engine::Get()->GetWindow();
	}
}

DX::Renderer::~Renderer()
{
	DX::Release(m_DepthStencilView);
	DX::Release(m_RenderTargetView);
	DX::Release(m_DepthStencil);
	DX::Release(m_SwapChain);
	DX::Release(m_DeviceContext);
	DX::Release(m_Device);

	DX::Release(m_RasterStateSolid);
	DX::Release(m_RasterStateWireframe);
}

bool DX::Renderer::Initialise()
{
	if (!CreateDevice())
		return false;

	if (!CreateSwapChain())
		return false;

	if (!CreateRenderTargetView())
		return false;

	SetViewport();

	CreateRasterStateSolid();
	CreateRasterStateWireframe();

	return true;
}

void DX::Renderer::Clear()
{
	static const float blue[] = { 0.3f, 0.5f, 0.7f, 1.0f };
	m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, reinterpret_cast<const float*>(&blue));
	m_DeviceContext->ClearDepthStencilView(m_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void DX::Renderer::Draw()
{
	m_SwapChain->Present(0, 0);
}

void DX::Renderer::Resize(int width, int height)
{
	DX::Release(m_RenderTargetView);
	DX::Release(m_DepthStencilView);

	DX::ThrowIfFailed(m_SwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
	CreateRenderTargetView();
	SetViewport();
}

void DX::Renderer::SetWireframe(bool wireframe)
{
	if (wireframe)
	{
		DeviceContext()->RSSetState(m_RasterStateWireframe);
	}
	else
	{
		DeviceContext()->RSSetState(m_RasterStateSolid);
	}
}

bool DX::Renderer::CreateDevice()
{
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	D3D_FEATURE_LEVEL featureLevel;
	DX::ThrowIfFailed(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &m_Device, &featureLevel, &m_DeviceContext));

	if (featureLevel != D3D_FEATURE_LEVEL_11_1)
	{
		return false;
	}

	DX::ThrowIfFailed(m_Device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &m_4xMsaaQuality));
	return true;
}

bool DX::Renderer::CreateSwapChain()
{
	IDXGIFactory1* dxgiFactory1 = GetDXGIFactory();
	IDXGIFactory2* dxgiFactory2 = nullptr;
	DX::ThrowIfFailed(dxgiFactory1->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2)));
	if (dxgiFactory2 != nullptr)
	{
		ID3D11Device1* device1 = nullptr;
		DX::ThrowIfFailed(m_Device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&device1)));

		ID3D11DeviceContext1* deviceContext1 = nullptr;
		(void)m_DeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&deviceContext1));

		DXGI_SWAP_CHAIN_DESC1 sd = {};
		sd.Width = GetWindow()->GetWidth();
		sd.Height = GetWindow()->GetHeight();
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 4;
		sd.SampleDesc.Quality = m_4xMsaaQuality - 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		IDXGISwapChain1* swapChain1 = nullptr;
		DX::ThrowIfFailed(dxgiFactory2->CreateSwapChainForHwnd(m_Device, GetHwnd(), &sd, nullptr, nullptr, &swapChain1));

		DX::ThrowIfFailed(swapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&m_SwapChain)));

		dxgiFactory2->Release();
	}
	else
	{
		DXGI_SWAP_CHAIN_DESC sd = {};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = GetWindow()->GetWidth();
		sd.BufferDesc.Height = GetWindow()->GetHeight();
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = GetHwnd();
		sd.SampleDesc.Count = 4;
		sd.SampleDesc.Quality = m_4xMsaaQuality - 1;
		sd.Windowed = TRUE;

		DX::ThrowIfFailed(dxgiFactory1->CreateSwapChain(m_Device, &sd, &m_SwapChain));
	}

	dxgiFactory1->MakeWindowAssociation(GetHwnd(), DXGI_MWA_NO_ALT_ENTER);

	// Get GPU Name
	IDXGIAdapter1* pAdapter;
	dxgiFactory2->EnumAdapters1(0, &pAdapter);

	DXGI_ADAPTER_DESC1 adapterDescription;
	pAdapter->GetDesc1(&adapterDescription);
	std::wcout << adapterDescription.Description << '\n';

	pAdapter->Release();
	dxgiFactory1->Release();
	return true;
}

bool DX::Renderer::CreateRenderTargetView()
{
	// Render target view
	ID3D11Resource* backBuffer = nullptr;
	DX::ThrowIfFailed(m_SwapChain->GetBuffer(0, __uuidof(ID3D11Resource), reinterpret_cast<void**>(&backBuffer)));
	if (backBuffer == nullptr)
	{
		return false;
	}

	DX::ThrowIfFailed(m_Device->CreateRenderTargetView(backBuffer, nullptr, &m_RenderTargetView));
	backBuffer->Release();

	// Depth stencil view
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = GetWindow()->GetWidth();
	descDepth.Height = GetWindow()->GetHeight();
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 4;
	descDepth.SampleDesc.Quality = m_4xMsaaQuality - 1;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	DX::ThrowIfFailed(m_Device->CreateTexture2D(&descDepth, nullptr, &m_DepthStencil));
	if (m_DepthStencil == nullptr)
	{
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	descDSV.Texture2D.MipSlice = 0;
	DX::ThrowIfFailed(m_Device->CreateDepthStencilView(m_DepthStencil, &descDSV, &m_DepthStencilView));

	m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, m_DepthStencilView);
	return true;
}

void DX::Renderer::SetViewport()
{
	D3D11_VIEWPORT vp;
	vp.Width = static_cast<FLOAT>(GetWindow()->GetWidth());
	vp.Height = static_cast<FLOAT>(GetWindow()->GetHeight());
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	m_DeviceContext->RSSetViewports(1, &vp);
}

void DX::Renderer::CreateRasterStateSolid()
{
	D3D11_RASTERIZER_DESC rasterizerState;
	ZeroMemory(&rasterizerState, sizeof(D3D11_RASTERIZER_DESC));

	rasterizerState.AntialiasedLineEnable = true;
	rasterizerState.CullMode = D3D11_CULL_FRONT;
	rasterizerState.FillMode = D3D11_FILL_SOLID;
	rasterizerState.DepthClipEnable = true;
	rasterizerState.FrontCounterClockwise = true;
	rasterizerState.MultisampleEnable = true;

	DX::ThrowIfFailed(Device()->CreateRasterizerState(&rasterizerState, &m_RasterStateSolid));
}

void DX::Renderer::CreateRasterStateWireframe()
{
	D3D11_RASTERIZER_DESC rasterizerState;
	ZeroMemory(&rasterizerState, sizeof(D3D11_RASTERIZER_DESC));

	rasterizerState.AntialiasedLineEnable = true;
	rasterizerState.CullMode = D3D11_CULL_NONE;
	rasterizerState.FillMode = D3D11_FILL_WIREFRAME;
	rasterizerState.DepthClipEnable = true;
	rasterizerState.FrontCounterClockwise = true;
	rasterizerState.MultisampleEnable = true;

	DX::ThrowIfFailed(Device()->CreateRasterizerState(&rasterizerState, &m_RasterStateWireframe));
}

IDXGIFactory1* DX::Renderer::GetDXGIFactory()
{
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		DX::ThrowIfFailed(m_Device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice)));

		IDXGIAdapter* adapter = nullptr;
		DX::ThrowIfFailed(dxgiDevice->GetAdapter(&adapter));

		DX::ThrowIfFailed(adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory)));

		adapter->Release();
		dxgiDevice->Release();
	}

	return dxgiFactory;
}

HWND DX::Renderer::GetHwnd() const
{
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);

	SDL_Window* window = reinterpret_cast<SDL_Window*>(GetWindow()->GetWindow());
	SDL_GetWindowWMInfo(window, &wmInfo);
	return wmInfo.info.win.window;
}