
#include "RenderDevice.h"
#include <SDL_syswm.h>
#include <iostream>
#include <SDL_messagebox.h>
#include <fstream>

void Error(std::string&& err)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", err.c_str(), nullptr);
}

struct SimpleVertex
{
	SimpleVertex(float x, float y, float z) : x(x), y(y), z(z) {}

	float x;
	float y;
	float z;
};

RenderDevice::RenderDevice(MainWindow* window) : m_Window(window)
{

}

bool RenderDevice::Initialise()
{
	if (!CreateDevice())
		return false;

	if (!CreateSwapChain())
		return false;

	if (!CreateRenderTargetView())
		return false;

	HRESULT hr;

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)800;
	vp.Height = (FLOAT)600;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_DeviceContext->RSSetViewports(1, &vp);

	// Create vertex shader
	std::ifstream vertexFile("../x64/Debug/VertexShader.cso", std::fstream::in | std::fstream::binary);
	if (!vertexFile.is_open())
	{
		Error("Could not read VertexShader.cso");
		return false;
	}

	vertexFile.seekg(0, vertexFile.end);
	int vertexsize = (int)vertexFile.tellg();
	vertexFile.seekg(0, vertexFile.beg);

	char* vertexbuffer = new char[vertexsize];
	vertexFile.read(vertexbuffer, vertexsize);

	hr = m_Device->CreateVertexShader(
		vertexbuffer,
		vertexsize,
		nullptr,
		&m_VertexShader);

	if (FAILED(hr))
	{
		Error("CreateVertexShader fucking exploded");
		return false;
	}

	// Create pixel shader
	std::ifstream pixelFile("../x64/Debug/PixelShader.cso", std::fstream::in | std::fstream::binary);
	if (!pixelFile.is_open())
	{
		Error("Could not read PixelShader.cso");
		return false;
	}

	pixelFile.seekg(0, pixelFile.end);
	int pixelsize = (int)pixelFile.tellg();
	pixelFile.seekg(0, pixelFile.beg);

	char* pixelbuffer = new char[pixelsize];
	pixelFile.read(pixelbuffer, pixelsize);

	hr = m_Device->CreatePixelShader(
		pixelbuffer,
		pixelsize,
		nullptr,
		&m_PixelShader);

	if (FAILED(hr))
	{
		Error("CreatePixelShader fucking exploded");
		return false;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	ID3D11InputLayout* vertexLayout = nullptr;
	hr = m_Device->CreateInputLayout(layout, numElements, vertexbuffer,
		vertexsize, &vertexLayout);

	if (FAILED(hr))
	{
		Error("CreateInputLayout fucking exploded");
		return false;
	}

	m_DeviceContext->IASetInputLayout(vertexLayout);

	struct SimpleVertex
	{
		SimpleVertex(float x, float y, float z) : x(x), y(y), z(z) {}

		float x;
		float y;
		float z;
	};

	// Create vertex buffer
	SimpleVertex vertices[] =
	{
		SimpleVertex(0.0f, 0.5f, 0.5f),
		SimpleVertex(0.5f, -0.5f, 0.5f),
		SimpleVertex(-0.5f, -0.5f, 0.5f),
	};

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 3;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData = {};
	InitData.pSysMem = vertices;

	ID3D11Buffer* vertexBuffer = nullptr;
	hr = m_Device->CreateBuffer(&bd, &InitData, &vertexBuffer);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	m_DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	// Set primitive topology
	m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return true;
}

void RenderDevice::Render()
{
	static const float Blue[] = { 0.0f, 0.0f, 1.0f, 1.0f };
	m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, reinterpret_cast<const float*>(&Blue));

	// Render bullshit
	m_DeviceContext->VSSetShader(m_VertexShader, nullptr, 0);
	m_DeviceContext->PSSetShader(m_PixelShader, nullptr, 0);
	m_DeviceContext->Draw(3, 0);

	// Update window
	m_SwapChain->Present(0, 0);
}

bool RenderDevice::CreateDevice()
{
	D3D_FEATURE_LEVEL featureLevel;
	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		D3D11_CREATE_DEVICE_DEBUG,
		nullptr,
		NULL,
		D3D11_SDK_VERSION,
		&m_Device,
		&featureLevel,
		&m_DeviceContext);

	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "D3D11CreateDeviceAndSwapChain failed", nullptr);
		return false;
	}

	if (featureLevel != D3D_FEATURE_LEVEL_11_0)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Invalid Direct3D version failed", nullptr);
		return false;
	}

	return true;
}

bool RenderDevice::CreateSwapChain()
{
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(m_Window->m_Window, &wmInfo);
	HWND hwnd = wmInfo.info.win.window;

	// Description
	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = 800;
	sd.BufferDesc.Height = 600;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 1;
	sd.OutputWindow = hwnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = 0;

	// Create swapchain
	IDXGIDevice* dxgiDevice = nullptr;
	m_Device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);

	IDXGIAdapter* dxgiAdapter = nullptr;
	dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);

	IDXGIFactory* dxgiFactory = nullptr;
	dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);

	HRESULT hr = dxgiFactory->CreateSwapChain(m_Device, &sd, &m_SwapChain);
	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Swapchain", nullptr);
		return false;
	}

	dxgiDevice->Release();
	dxgiAdapter->Release();
	dxgiFactory->Release();

	return true;
}

bool RenderDevice::CreateRenderTargetView()
{
	ID3D11Resource* backBuffer = nullptr;
	m_SwapChain->GetBuffer(0, __uuidof(ID3D11Resource), reinterpret_cast<void**>(&backBuffer));
	if (backBuffer == NULL)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "IDXGISwapChain::GetBuffer failed", nullptr);
		return false;
	}

	// Create render target
	HRESULT hr = m_Device->CreateRenderTargetView(backBuffer, nullptr, &m_RenderTargetView);
	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "CreateRenderTargetView failed", nullptr);
		return false;
	}

	m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);
	return true;
}

bool RenderDevice::CreateVertexShader()
{
	std::ifstream vertexFile("../x64/Debug/VertexShader.cso", std::fstream::in | std::fstream::binary);
	if (!vertexFile.is_open())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not read VertexShader.cso", nullptr);
		return false;
	}

	vertexFile.seekg(0, vertexFile.end);
	int vertexsize = (int)vertexFile.tellg();
	vertexFile.seekg(0, vertexFile.beg);

	char* vertexbuffer = new char[vertexsize];
	vertexFile.read(vertexbuffer, vertexsize);

	HRESULT hr = m_Device->CreateVertexShader(
		vertexbuffer,
		vertexsize,
		nullptr,
		&m_VertexShader);

	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "CreateVertexShader failed", nullptr);
		return false;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT numElements = ARRAYSIZE(layout);

	ID3D11InputLayout* vertexLayout = nullptr;
	m_Device->CreateInputLayout(layout, numElements, vertexbuffer, vertexsize, &vertexLayout);

	return true;
}

bool RenderDevice::CreatePixelShader()
{
	std::ifstream pixelFile("../x64/Debug/PixelShader.cso", std::fstream::in | std::fstream::binary);
	if (!pixelFile.is_open())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not read PixelShader.cso", nullptr);
		return false;
	}

	pixelFile.seekg(0, pixelFile.end);
	int pixelsize = (int)pixelFile.tellg();
	pixelFile.seekg(0, pixelFile.beg);

	char* pixelbuffer = new char[pixelsize];
	pixelFile.read(pixelbuffer, pixelsize);

	HRESULT hr = m_Device->CreatePixelShader(
		pixelbuffer,
		pixelsize,
		nullptr,
		&m_PixelShader);

	if (FAILED(hr))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "CreatePixelShader failed", nullptr);
		return false;
	}

	return true;
}