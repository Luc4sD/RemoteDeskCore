#pragma once

#include <vector>
#include <cstdint>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <memory>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct FrameData {
    std::vector<uint8_t> pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    bool hasChanged;
};

class DXGICapturer {
public:
    DXGICapturer();
    ~DXGICapturer();

    // Inicializa o capturer (D3D11, DXGI, etc)
    bool Initialize(uint32_t displayIndex = 0);

    // Captura um frame - retorna true se bem-sucedido
    bool AcquireFrame(FrameData& outFrame);

    // Libera os recursos
    void Release();

    // Obtém dimensões da tela
    uint32_t GetScreenWidth() const { return m_screenWidth; }
    uint32_t GetScreenHeight() const { return m_screenHeight; }

private:
    // Inicializa o device D3D11 e DXGI
    bool InitializeDirectX();

    // Copia a textura para memória CPU
    bool CopyFrameToBuffer(ID3D11Texture2D* sourceTexture, FrameData& outFrame);

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_deviceContext;
    ComPtr<IDXGIOutputDuplication> m_desktopDuplication;
    ComPtr<ID3D11Texture2D> m_stagingTexture;

    std::vector<uint8_t> m_frameBuffer;
    uint32_t m_screenWidth = 0;
    uint32_t m_screenHeight = 0;
    uint32_t m_stride = 0;

    static const uint32_t FRAME_ACQUIRE_TIMEOUT_MS = 100;
};
