#pragma once

/**
 * @file RemoteDesktopSystem.h
 * @brief Sistema integrado de acesso remoto com todas as 5 fases
 * 
 * Exemplo de uso completo do sistema com:
 * - Fase 1: Captura DXGI + Renderização SDL2
 * - Fase 2: Networking P2P UDP
 * - Fase 3: Codec NVENC H.264
 * - Fase 4: Input injection
 * - Fase 5: Multi-threading + ABR
 */

#include "DXGICapturer.h"
#include "Renderer.h"
#include "P2PManager.h"
#include "NVENCEncoder.h"
#include "InputInjector.h"
#include "OptimizationLayer.h"

#include <memory>
#include <atomic>

class RemoteDesktopSystem {
public:
    enum class Mode { LOOPBACK, SERVER, CLIENT };

    RemoteDesktopSystem();
    ~RemoteDesktopSystem();

    // ===== Inicialização =====

    // Modo 1: Loopback local (Fase 1 apenas)
    bool InitializeLoopback(uint32_t width, uint32_t height);

    // Modo 2: Servidor (Captura + envia)
    bool InitializeAsServer(uint16_t networkPort = 12345, 
                            uint32_t targetBitrateMbps = 25);

    // Modo 3: Cliente (Recebe + renderiza)
    bool InitializeAsClient(const std::string& serverIP, 
                            uint16_t networkPort = 12345);

    // ===== Execução =====

    // Run main loop
    void Run();

    // Stop execution
    void Stop();

    bool IsRunning() const { return m_isRunning; }

    // ===== Configuration =====

    void SetUseMultiThreading(bool useThreads) { m_useMultiThreading = useThreads; }
    void SetUseEncoding(bool useEncoding) { m_useEncoding = useEncoding; }
    void SetUseNetworking(bool useNetworking) { m_useNetworking = useNetworking; }
    void SetInputEnabled(bool enabled) { m_inputEnabled = enabled; }

    // ABR settings
    void SetAdaptiveMode(AdaptiveBitRateController::AdaptationMode mode) { 
        m_abrMode = mode; 
    }

    // ===== Statistics =====

    struct SystemStats {
        uint64_t totalFramesProcessed = 0;
        double averageFrameTimeMs = 0.0;
        double averageFPS = 0.0;
        double totalLatencyMs = 0.0;

        // Breakdown
        double captureTimeMs = 0.0;
        double encodeTimeMs = 0.0;
        double networkTimeMs = 0.0;
        double renderTimeMs = 0.0;

        // Network
        uint64_t totalBytesSent = 0;
        uint64_t totalBytesReceived = 0;
        uint32_t compressionRatio = 0;
    };

    SystemStats GetStats() const { return m_stats; }

    void PrintStats() const;

private:
    // ===== Main Loop Components =====

    void MainLoopServer();
    void MainLoopClient();
    void MainLoopLoopback();

    // Phase 1: Capture & Render
    std::unique_ptr<DXGICapturer> m_capturer;
    std::unique_ptr<Renderer> m_renderer;

    // Phase 2: Networking
    std::unique_ptr<P2PManager> m_network;

    // Phase 3: Encoding
    std::unique_ptr<NVENCEncoder> m_encoder;

    // Phase 4: Input
    std::unique_ptr<InputInjector> m_inputInjector;

    // Phase 5: Optimization
    std::unique_ptr<MultiThreadedCapture> m_threadedCapture;
    std::unique_ptr<MultiThreadedRenderer> m_threadedRenderer;
    std::unique_ptr<AdaptiveBitRateController> m_abrController;

    // Configuration
    Mode m_mode = Mode::LOOPBACK;
    bool m_useMultiThreading = false;
    bool m_useEncoding = false;
    bool m_useNetworking = false;
    bool m_inputEnabled = false;

    AdaptiveBitRateController::AdaptationMode m_abrMode = 
        AdaptiveBitRateController::AdaptationMode::BALANCED;

    // State
    std::atomic<bool> m_isRunning{ false };
    SystemStats m_stats;
};
