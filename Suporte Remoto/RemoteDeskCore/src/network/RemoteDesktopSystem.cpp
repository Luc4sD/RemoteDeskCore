#include "RemoteDesktopSystem.h"
#include <iostream>
#include <iomanip>
#include <chrono>

RemoteDesktopSystem::RemoteDesktopSystem() {
}

RemoteDesktopSystem::~RemoteDesktopSystem() {
    Stop();
}

bool RemoteDesktopSystem::InitializeLoopback(uint32_t width, uint32_t height) {
    m_mode = Mode::LOOPBACK;

    // Criar componentes Fase 1
    m_capturer = std::make_unique<DXGICapturer>();
    if (!m_capturer->Initialize()) {
        std::cerr << "ERROR: Failed to initialize capturer\n";
        return false;
    }

    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->Initialize(m_capturer->GetScreenWidth(), 
                                m_capturer->GetScreenHeight())) {
        std::cerr << "ERROR: Failed to initialize renderer\n";
        return false;
    }

    std::cout << "Loopback mode initialized\n";
    return true;
}

bool RemoteDesktopSystem::InitializeAsServer(uint16_t networkPort, 
                                              uint32_t targetBitrateMbps) {
    m_mode = Mode::SERVER;

    // Fase 1: Captura
    m_capturer = std::make_unique<DXGICapturer>();
    if (!m_capturer->Initialize()) {
        std::cerr << "ERROR: Failed to initialize capturer\n";
        return false;
    }

    // Fase 2: Network
    m_network = std::make_unique<P2PManager>();
    if (!m_network->InitializeAsServer(networkPort)) {
        std::cerr << "ERROR: Failed to initialize network server\n";
        return false;
    }

    // Fase 3: Encoding (opcional)
    if (m_useEncoding) {
        m_encoder = std::make_unique<NVENCEncoder>();
        if (!m_encoder->Initialize(m_capturer->GetScreenWidth(),
                                   m_capturer->GetScreenHeight(),
                                   targetBitrateMbps)) {
            std::cerr << "WARNING: NVENC encoder not available\n";
            m_useEncoding = false;
        }
    }

    // Fase 4: Input (para receber input remoto)
    if (m_inputEnabled) {
        m_inputInjector = std::make_unique<InputInjector>();
        if (!m_inputInjector->Initialize()) {
            std::cerr << "WARNING: Input injector failed\n";
            m_inputEnabled = false;
        }
    }

    // Fase 5: Multi-threading (opcional)
    if (m_useMultiThreading) {
        m_threadedCapture = std::make_unique<MultiThreadedCapture>();
        if (!m_threadedCapture->StartCaptureThread()) {
            std::cerr << "WARNING: Multi-threaded capture failed\n";
            m_useMultiThreading = false;
        }

        m_abrController = std::make_unique<AdaptiveBitRateController>(5, 100);
        m_abrController->SetAdaptationMode(m_abrMode);
    }

    std::cout << "Server mode initialized\n";
    return true;
}

bool RemoteDesktopSystem::InitializeAsClient(const std::string& serverIP,
                                              uint16_t networkPort) {
    m_mode = Mode::CLIENT;

    // Fase 2: Network
    m_network = std::make_unique<P2PManager>();
    if (!m_network->InitializeAsClient(serverIP, networkPort)) {
        std::cerr << "ERROR: Failed to initialize network client\n";
        return false;
    }

    // Fase 1: Renderer
    m_renderer = std::make_unique<Renderer>();
    if (!m_renderer->Initialize(1920, 1080, "Remote Desktop - Client")) {
        std::cerr << "ERROR: Failed to initialize renderer\n";
        return false;
    }

    // Fase 4: Input (para enviar input remoto)
    if (m_inputEnabled) {
        m_inputInjector = std::make_unique<InputInjector>();
        if (!m_inputInjector->Initialize()) {
            std::cerr << "WARNING: Input injector failed\n";
            m_inputEnabled = false;
        }
    }

    // Fase 5: Multi-threading (opcional)
    if (m_useMultiThreading) {
        m_threadedRenderer = std::make_unique<MultiThreadedRenderer>();
        if (!m_threadedRenderer->StartRenderThread()) {
            std::cerr << "WARNING: Multi-threaded render failed\n";
            m_useMultiThreading = false;
        }
    }

    std::cout << "Client mode initialized\n";
    return true;
}

void RemoteDesktopSystem::Run() {
    if (m_isRunning) {
        return;
    }

    m_isRunning = true;

    std::cout << "\n=== Starting Remote Desktop System ===\n";
    std::cout << "Mode: " << (m_mode == Mode::LOOPBACK ? "Loopback" :
                              m_mode == Mode::SERVER ? "Server" : "Client") << "\n";
    std::cout << "Multi-threading: " << (m_useMultiThreading ? "ON" : "OFF") << "\n";
    std::cout << "Encoding: " << (m_useEncoding ? "ON" : "OFF") << "\n";
    std::cout << "Networking: " << (m_useNetworking ? "ON" : "OFF") << "\n";
    std::cout << "Input: " << (m_inputEnabled ? "ON" : "OFF") << "\n";
    std::cout << "=========================================\n\n";

    switch (m_mode) {
    case Mode::LOOPBACK:
        MainLoopLoopback();
        break;
    case Mode::SERVER:
        MainLoopServer();
        break;
    case Mode::CLIENT:
        MainLoopClient();
        break;
    }

    PrintStats();
}

void RemoteDesktopSystem::MainLoopLoopback() {
    auto startTime = std::chrono::high_resolution_clock::now();
    FrameData frameData;
    uint16_t frameSequence = 0;

    while (m_isRunning && m_renderer && m_renderer->IsRunning()) {
        auto loopStart = std::chrono::high_resolution_clock::now();

        // Processar eventos
        if (!m_renderer->ProcessEvents()) {
            break;
        }

        // Capturar
        auto captureStart = std::chrono::high_resolution_clock::now();
        if (!m_capturer->AcquireFrame(frameData)) {
            continue;
        }
        auto captureEnd = std::chrono::high_resolution_clock::now();
        m_stats.captureTimeMs = 
            std::chrono::duration<double, std::milli>(captureEnd - captureStart).count();

        // Atualizar e renderizar
        if (frameData.hasChanged) {
            auto renderStart = std::chrono::high_resolution_clock::now();
            m_renderer->UpdateFrame(frameData.pixels.data(), frameData.width,
                                   frameData.height, frameData.stride);
            m_renderer->RenderFrame();
            auto renderEnd = std::chrono::high_resolution_clock::now();
            m_stats.renderTimeMs = 
                std::chrono::duration<double, std::milli>(renderEnd - renderStart).count();
        }

        m_stats.totalFramesProcessed++;
        frameSequence++;

        // Atualizar estatísticas
        auto loopEnd = std::chrono::high_resolution_clock::now();
        double frameTimeMs = 
            std::chrono::duration<double, std::milli>(loopEnd - loopStart).count();
        m_stats.totalLatencyMs = (m_stats.totalLatencyMs * 0.9) + (frameTimeMs * 0.1);
        m_stats.averageFPS = 1000.0 / m_stats.totalLatencyMs;

        // Atualizar título
        if (m_stats.totalFramesProcessed % 60 == 0) {
            std::ostringstream title;
            title << "Remote Desktop - Loopback | FPS: " 
                  << std::fixed << std::setprecision(1) << m_stats.averageFPS
                  << " | Latency: " << m_stats.totalLatencyMs << "ms";
            m_renderer->SetWindowTitle(title.str());
        }
    }
}

void RemoteDesktopSystem::MainLoopServer() {
    std::cout << "Server running. Press Ctrl+C to stop.\n";

    FrameData frameData;
    uint16_t frameSequence = 0;

    while (m_isRunning) {
        // Capturar frame
        if (!m_capturer->AcquireFrame(frameData) || !frameData.hasChanged) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Codificar (opcional)
        if (m_useEncoding && m_encoder) {
            EncodedFrame encoded;
            m_encoder->EncodeFrame(frameData.pixels.data(), frameData.width,
                                  frameData.height, frameData.stride, encoded);
            m_stats.totalBytesSent += encoded.data.size();
            m_stats.compressionRatio = (frameData.stride * frameData.height) / 
                                       std::max(1UL, encoded.data.size());
        }

        // Enviar via rede (opcional)
        if (m_useNetworking && m_network) {
            m_network->SendFrame(frameData.pixels.data(), frameData.width,
                                frameData.height, frameData.stride, frameSequence);
        }

        m_stats.totalFramesProcessed++;
        frameSequence++;

        if (m_stats.totalFramesProcessed % 300 == 0) {
            std::cout << "Server: " << m_stats.totalFramesProcessed 
                      << " frames sent\n";
        }
    }
}

void RemoteDesktopSystem::MainLoopClient() {
    std::cout << "Client connected. Press ESC to disconnect.\n";

    std::vector<uint8_t> pixelData;
    uint32_t width, height, stride;
    uint16_t frameSequence;

    while (m_isRunning && m_renderer && m_renderer->IsRunning()) {
        // Processar eventos
        if (!m_renderer->ProcessEvents()) {
            break;
        }

        // Receber frame
        if (!m_network->ReceiveFrame(pixelData, width, height, stride, frameSequence)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Renderizar
        if (!pixelData.empty()) {
            m_renderer->UpdateFrame(pixelData.data(), width, height, stride);
            m_renderer->RenderFrame();
        }

        m_stats.totalFramesProcessed++;
        m_stats.totalBytesReceived += pixelData.size();

        if (m_stats.totalFramesProcessed % 60 == 0) {
            m_stats.averageFPS = 60.0; // Aproximado
            std::ostringstream title;
            title << "Remote Desktop - Client | FPS: " 
                  << std::fixed << std::setprecision(1) << m_stats.averageFPS;
            m_renderer->SetWindowTitle(title.str());
        }
    }
}

void RemoteDesktopSystem::Stop() {
    m_isRunning = false;

    if (m_threadedCapture) {
        m_threadedCapture->StopCaptureThread();
    }
    if (m_threadedRenderer) {
        m_threadedRenderer->StopRenderThread();
    }
    if (m_network) {
        m_network->Disconnect();
    }
    if (m_renderer) {
        m_renderer->Release();
    }
    if (m_capturer) {
        m_capturer->Release();
    }
}

void RemoteDesktopSystem::PrintStats() const {
    std::cout << "\n=== System Statistics ===\n";
    std::cout << "Total Frames Processed: " << m_stats.totalFramesProcessed << "\n";
    std::cout << "Average FPS: " << std::fixed << std::setprecision(1) 
              << m_stats.averageFPS << "\n";
    std::cout << "Total Latency: " << m_stats.totalLatencyMs << " ms\n";
    std::cout << "\nTiming Breakdown:\n";
    std::cout << "  Capture: " << std::setprecision(2) 
              << m_stats.captureTimeMs << " ms\n";
    std::cout << "  Encode: " << m_stats.encodeTimeMs << " ms\n";
    std::cout << "  Network: " << m_stats.networkTimeMs << " ms\n";
    std::cout << "  Render: " << m_stats.renderTimeMs << " ms\n";

    if (m_stats.totalBytesSent > 0) {
        std::cout << "\nNetwork (Server):\n";
        std::cout << "  Total Bytes Sent: " << (m_stats.totalBytesSent / 1024 / 1024) 
                  << " MB\n";
        std::cout << "  Compression Ratio: 1:" << m_stats.compressionRatio << "\n";
    }

    if (m_stats.totalBytesReceived > 0) {
        std::cout << "\nNetwork (Client):\n";
        std::cout << "  Total Bytes Received: " 
                  << (m_stats.totalBytesReceived / 1024 / 1024) << " MB\n";
    }

    std::cout << "========================\n";
}
