#include "DXGICapturer.h"
#include <stdexcept>
#include <cstring>

DXGICapturer::DXGICapturer() 
    : m_screenWidth(0), m_screenHeight(0), m_stride(0) {
}

DXGICapturer::~DXGICapturer() {
    Release();
}

bool DXGICapturer::Initialize(uint32_t displayIndex) {
    try {
        if (!InitializeDirectX()) {
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA("DXGICapturer::Initialize - Exception: ");
        OutputDebugStringA(e.what());
        return false;
    }
}

bool DXGICapturer::InitializeDirectX() {
    // Criar device D3D11
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        featureLevels,
        numFeatureLevels,
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_deviceContext
    );

    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create D3D11 device\n");
        return false;
    }

    // Obter DXGI Adapter
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get DXGI device\n");
        return false;
    }

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get adapter\n");
        return false;
    }

    // Obter DXGI Output (Display)
    ComPtr<IDXGIOutput> output;
    hr = adapter->EnumOutputs(0, &output);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to enumerate outputs\n");
        return false;
    }

    // Obter dimensões da tela
    DXGI_OUTPUT_DESC outputDesc;
    output->GetDesc(&outputDesc);
    m_screenWidth = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
    m_screenHeight = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;
    m_stride = m_screenWidth * 4; // BGRA

    // Obter Desktop Duplication
    ComPtr<IDXGIOutput1> output1;
    hr = output.As(&output1);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to get IDXGIOutput1\n");
        return false;
    }

    hr = output1->DuplicateOutput(m_device.Get(), &m_desktopDuplication);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to duplicate output\n");
        return false;
    }

    // Criar staging texture para CPU read
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = m_screenWidth;
    stagingDesc.Height = m_screenHeight;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // BGRA
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &m_stagingTexture);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create staging texture\n");
        return false;
    }

    // Pré-alocar buffer
    m_frameBuffer.resize(m_stride * m_screenHeight);

    OutputDebugStringA("DXGI Capturer initialized successfully\n");
    return true;
}

bool DXGICapturer::AcquireFrame(FrameData& outFrame) {
    if (!m_desktopDuplication) {
        return false;
    }

    ComPtr<IDXGIResource> frameResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    HRESULT hr = m_desktopDuplication->AcquireNextFrame(
        FRAME_ACQUIRE_TIMEOUT_MS,
        &frameInfo,
        &frameResource
    );

    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // Nenhuma mudança na tela - normal
        outFrame.hasChanged = false;
        return true;
    }

    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            OutputDebugStringA("Desktop Duplication access lost - reinitializing\n");
            Release();
            Initialize();
        }
        return false;
    }

    // Converter recurso para Texture2D
    ComPtr<ID3D11Texture2D> screenTexture;
    hr = frameResource.As(&screenTexture);
    if (FAILED(hr)) {
        m_desktopDuplication->ReleaseFrame();
        return false;
    }

    // Copiar para staging texture
    m_deviceContext->CopyResource(m_stagingTexture.Get(), screenTexture.Get());

    // Mapear para CPU
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_deviceContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        m_desktopDuplication->ReleaseFrame();
        return false;
    }

    // Copiar pixels
    const uint8_t* srcData = static_cast<uint8_t*>(mappedResource.pData);
    std::memcpy(m_frameBuffer.data(), srcData, m_frameBuffer.size());

    m_deviceContext->Unmap(m_stagingTexture.Get(), 0);

    // Preencher saída
    outFrame.pixels = m_frameBuffer;
    outFrame.width = m_screenWidth;
    outFrame.height = m_screenHeight;
    outFrame.stride = m_stride;
    outFrame.hasChanged = true;

    m_desktopDuplication->ReleaseFrame();
    return true;
}

bool DXGICapturer::CopyFrameToBuffer(ID3D11Texture2D* sourceTexture, FrameData& outFrame) {
    if (!sourceTexture || !m_stagingTexture) {
        return false;
    }

    m_deviceContext->CopyResource(m_stagingTexture.Get(), sourceTexture);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_deviceContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr)) {
        return false;
    }

    const uint8_t* srcData = static_cast<uint8_t*>(mappedResource.pData);
    std::memcpy(m_frameBuffer.data(), srcData, m_frameBuffer.size());

    m_deviceContext->Unmap(m_stagingTexture.Get(), 0);

    outFrame.pixels = m_frameBuffer;
    outFrame.width = m_screenWidth;
    outFrame.height = m_screenHeight;
    outFrame.stride = m_stride;
    outFrame.hasChanged = true;

    return true;
}

void DXGICapturer::Release() {
    if (m_desktopDuplication) {
        m_desktopDuplication.Reset();
    }
    if (m_stagingTexture) {
        m_stagingTexture.Reset();
    }
    if (m_deviceContext) {
        m_deviceContext.Reset();
    }
    if (m_device) {
        m_device.Reset();
    }
    m_frameBuffer.clear();
    m_screenWidth = 0;
    m_screenHeight = 0;
}
