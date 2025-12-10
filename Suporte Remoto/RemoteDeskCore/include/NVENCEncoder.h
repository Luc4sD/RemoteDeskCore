#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <d3d11.h>
#include <nvEncodeAPI.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct EncodedFrame {
    std::vector<uint8_t> data;
    uint32_t width;
    uint32_t height;
    uint32_t bitrate;
    bool isKeyframe;
    uint64_t timestamp;
};

class NVENCEncoder {
public:
    enum class BitRateMode { CONSTANT, VARIABLE };

    NVENCEncoder();
    ~NVENCEncoder();

    // Inicializa o encoder NVENC
    bool Initialize(uint32_t width, uint32_t height, uint32_t targetBitrateMbps = 25);

    // Codifica um frame BGRA em H.264
    bool EncodeFrame(const uint8_t* bgraPixels, uint32_t width, uint32_t height,
                     uint32_t stride, EncodedFrame& outFrame, bool forceKeyframe = false);

    // Finaliza a codificação (obtém frames restantes)
    bool EndEncode(std::vector<EncodedFrame>& outFrames);

    // Configurações
    void SetTargetBitrate(uint32_t mbps) { m_targetBitrateMbps = mbps; }
    void SetBitRateMode(BitRateMode mode) { m_bitrateMode = mode; }
    void SetPreset(uint32_t presetIndex);  // 0=default_preset, 11=lossless

    // Estatísticas
    struct EncoderStats {
        uint64_t totalFramesEncoded = 0;
        uint64_t totalBytesEncoded = 0;
        uint32_t keyframeInterval = 60;
        double averageBitrate = 0.0;
    };

    EncoderStats GetStats() const { return m_stats; }

    // Libera recursos
    void Release();

    // Obtém informações suportadas
    bool GetCapabilities(uint32_t& maxWidth, uint32_t& maxHeight,
                        uint32_t& maxFramerate) const;

private:
    bool InitializeNVENC();
    bool CreateInputBuffer(uint32_t width, uint32_t height);
    bool CreateBitstreamBuffer();
    bool ConfigureEncoder(uint32_t width, uint32_t height);

    // NVENC function pointers
    void* m_nvencModule = nullptr;
    NV_ENCODE_API_FUNCTION_LIST m_nvencFunctions = {};
    void* m_encoder = nullptr;

    // D3D11 resources
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_deviceContext;
    ComPtr<ID3D11Texture2D> m_inputTexture;
    ComPtr<ID3D11Texture2D> m_stagingTexture;

    // NVENC buffers
    void* m_inputBuffer = nullptr;
    void* m_bitstreamBuffer = nullptr;

    // Configuration
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_targetBitrateMbps = 25;
    BitRateMode m_bitrateMode = BitRateMode::VARIABLE;
    uint32_t m_presetIndex = 4; // NVENC_PRESET_DEFAULT

    // Statistics
    EncoderStats m_stats;
    uint32_t m_frameCount = 0;
    uint32_t m_keyframeCounter = 0;
};
