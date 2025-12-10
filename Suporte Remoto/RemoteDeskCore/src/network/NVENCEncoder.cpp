#include "NVENCEncoder.h"
#include <iostream>
#include <cstring>

NVENCEncoder::NVENCEncoder() {
}

NVENCEncoder::~NVENCEncoder() {
    Release();
}

bool NVENCEncoder::InitializeNVENC() {
    // Carregar módulo NVENC
    m_nvencModule = LoadLibraryA("nvEncodeAPI64.dll");
    
    if (!m_nvencModule) {
        OutputDebugStringA("Failed to load nvEncodeAPI64.dll\n");
        return false;
    }

    // Obter função de criação
    typedef NVENCSTATUS(APIENTRY * PNVENCODEAPICREATEINSTANCE)(NV_ENCODE_API_FUNCTION_LIST*);
    PNVENCODEAPICREATEINSTANCE nvEncodeAPICreateInstance =
        (PNVENCODEAPICREATEINSTANCE)GetProcAddress((HMODULE)m_nvencModule, "NvEncodeAPICreateInstance");

    if (!nvEncodeAPICreateInstance) {
        OutputDebugStringA("Failed to get NvEncodeAPICreateInstance\n");
        return false;
    }

    // Inicializar função list
    m_nvencFunctions.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    NVENCSTATUS status = nvEncodeAPICreateInstance(&m_nvencFunctions);

    if (status != NV_ENC_SUCCESS) {
        OutputDebugStringA("NvEncodeAPICreateInstance failed\n");
        return false;
    }

    return true;
}

bool NVENCEncoder::Initialize(uint32_t width, uint32_t height, uint32_t targetBitrateMbps) {
    m_width = width;
    m_height = height;
    m_targetBitrateMbps = targetBitrateMbps;

    if (!InitializeNVENC()) {
        return false;
    }

    if (!CreateInputBuffer(width, height)) {
        return false;
    }

    if (!CreateBitstreamBuffer()) {
        return false;
    }

    if (!ConfigureEncoder(width, height)) {
        return false;
    }

    OutputDebugStringA("NVENCEncoder initialized successfully\n");
    return true;
}

bool NVENCEncoder::CreateInputBuffer(uint32_t width, uint32_t height) {
    // Criar D3D11 device se não existir
    if (!m_device) {
        D3D_FEATURE_LEVEL featureLevel;
        if (FAILED(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &m_device,
            &featureLevel,
            &m_deviceContext
        ))) {
            OutputDebugStringA("Failed to create D3D11 device for NVENC\n");
            return false;
        }
    }

    // Criar textura de entrada BGRA
    D3D11_TEXTURE2D_DESC inputDesc = {};
    inputDesc.Width = width;
    inputDesc.Height = height;
    inputDesc.MipLevels = 1;
    inputDesc.ArraySize = 1;
    inputDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // BGRA
    inputDesc.SampleDesc.Count = 1;
    inputDesc.Usage = D3D11_USAGE_DEFAULT;
    inputDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(m_device->CreateTexture2D(&inputDesc, nullptr, &m_inputTexture))) {
        OutputDebugStringA("Failed to create input texture\n");
        return false;
    }

    // Criar staging texture para CPU to GPU copy
    D3D11_TEXTURE2D_DESC stagingDesc = inputDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(m_device->CreateTexture2D(&stagingDesc, nullptr, &m_stagingTexture))) {
        OutputDebugStringA("Failed to create staging texture\n");
        return false;
    }

    return true;
}

bool NVENCEncoder::CreateBitstreamBuffer() {
    // Alocação simplificada - em produção, usar NVENC API corretamente
    // Este é um stub que demonstra a estrutura
    return true;
}

bool NVENCEncoder::ConfigureEncoder(uint32_t width, uint32_t height) {
    // Configuração básica do encoder NVENC
    // Em produção, configurar com parâmetros reais de NVENC
    m_stats.keyframeInterval = 60;
    return true;
}

bool NVENCEncoder::EncodeFrame(const uint8_t* bgraPixels, uint32_t width, uint32_t height,
                               uint32_t stride, EncodedFrame& outFrame, bool forceKeyframe) {
    if (!m_device || !m_inputTexture || !bgraPixels) {
        return false;
    }

    // Copiar pixels CPU → Staging texture
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(m_deviceContext->Map(m_stagingTexture.Get(), 0, D3D11_MAP_WRITE, 0, &mapped))) {
        OutputDebugStringA("Failed to map staging texture\n");
        return false;
    }

    uint8_t* destData = static_cast<uint8_t*>(mapped.pData);
    for (uint32_t y = 0; y < height; ++y) {
        std::memcpy(
            destData + (y * mapped.RowPitch),
            bgraPixels + (y * stride),
            width * 4
        );
    }

    m_deviceContext->Unmap(m_stagingTexture.Get(), 0);

    // Copiar Staging → Input texture
    m_deviceContext->CopyResource(m_inputTexture.Get(), m_stagingTexture.Get());

    // Preparar frame codificado
    outFrame.width = width;
    outFrame.height = height;
    outFrame.bitrate = m_targetBitrateMbps;
    outFrame.isKeyframe = (m_keyframeCounter == 0) || forceKeyframe;
    outFrame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();

    // Simular dados comprimidos (em produção, usar NVENC real)
    // Compressão estimada: ~1/10 do tamanho original para H.264
    uint32_t estimatedCompressedSize = (width * height * 4) / 10;
    outFrame.data.resize(estimatedCompressedSize);

    // Preencher com dados de exemplo (em produção, usar NVENC)
    std::memset(outFrame.data.data(), 0, estimatedCompressedSize);

    m_stats.totalFramesEncoded++;
    m_stats.totalBytesEncoded += estimatedCompressedSize;
    m_frameCount++;
    m_keyframeCounter++;

    if (m_keyframeCounter >= m_stats.keyframeInterval) {
        m_keyframeCounter = 0;
    }

    // Calcular bitrate médio
    m_stats.averageBitrate = (m_stats.totalBytesEncoded * 8.0) / (m_frameCount / 60.0) / 1000000.0;

    return true;
}

bool NVENCEncoder::EndEncode(std::vector<EncodedFrame>& outFrames) {
    // Flush remaining frames
    outFrames.clear();
    return true;
}

void NVENCEncoder::SetPreset(uint32_t presetIndex) {
    m_presetIndex = presetIndex;
}

bool NVENCEncoder::GetCapabilities(uint32_t& maxWidth, uint32_t& maxHeight,
                                   uint32_t& maxFramerate) const {
    // Retornar capacidades típicas de NVIDIA GPUs modernas
    maxWidth = 7680;   // 8K
    maxHeight = 4320;  // 8K
    maxFramerate = 120; // 120 FPS
    return true;
}

void NVENCEncoder::Release() {
    if (m_inputTexture) {
        m_inputTexture.Reset();
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
    if (m_nvencModule) {
        FreeLibrary((HMODULE)m_nvencModule);
        m_nvencModule = nullptr;
    }
}
