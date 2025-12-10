#pragma once

#include <cstdint>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

struct FrameBuffer {
    std::vector<uint8_t> pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint64_t timestamp;
    uint16_t frameSequence;
};

class ThreadSafeQueue {
public:
    template<typename T>
    class Queue {
    public:
        void Push(const T& item) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(item);
            m_condition.notify_one();
        }

        bool TryPop(T& outItem, uint32_t timeoutMs = 0) {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (timeoutMs == 0) {
                if (m_queue.empty()) return false;
            } else {
                if (!m_condition.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                    [this] { return !m_queue.empty(); })) {
                    return false;
                }
            }

            if (m_queue.empty()) return false;

            outItem = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        size_t Size() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.size();
        }

        bool Empty() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        void Clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            while (!m_queue.empty()) {
                m_queue.pop();
            }
        }

    private:
        std::queue<T> m_queue;
        mutable std::mutex m_mutex;
        std::condition_variable m_condition;
    };
};

class MultiThreadedCapture {
public:
    MultiThreadedCapture();
    ~MultiThreadedCapture();

    // Inicia thread de captura
    bool StartCaptureThread();

    // Para thread de captura
    void StopCaptureThread();

    // Obtém frame capturado (não-bloqueante)
    bool GetCapturedFrame(FrameBuffer& outFrame, uint32_t timeoutMs = 0);

    // Verifica se há frames disponíveis
    size_t GetPendingFrameCount() const { return m_captureQueue.Size(); }

    // Configurações
    void SetMaxQueueSize(size_t size) { m_maxQueueSize = size; }

    // Estatísticas
    struct CaptureStats {
        uint64_t totalFramesCaptured = 0;
        uint64_t totalFramesDropped = 0;
        double averageCaptureTimeMs = 0.0;
    };

    CaptureStats GetStats() const { return m_stats; }

private:
    void CaptureThreadMain();

    ThreadSafeQueue::Queue<FrameBuffer> m_captureQueue;
    std::thread m_captureThread;
    std::atomic<bool> m_isRunning{ false };
    std::atomic<bool> m_shouldStop{ false };

    size_t m_maxQueueSize = 10;
    CaptureStats m_stats;
};

class MultiThreadedRenderer {
public:
    MultiThreadedRenderer();
    ~MultiThreadedRenderer();

    // Inicia thread de renderização
    bool StartRenderThread();

    // Para thread de renderização
    void StopRenderThread();

    // Enfileira frame para renderização
    bool QueueFrameForRender(const FrameBuffer& frame);

    // Verifica se há frames pendentes
    size_t GetPendingFrameCount() const { return m_renderQueue.Size(); }

    // Configurações
    void SetMaxQueueSize(size_t size) { m_maxQueueSize = size; }
    void SetTargetFPS(uint32_t fps) { m_targetFPS = fps; }

    // Estatísticas
    struct RenderStats {
        uint64_t totalFramesRendered = 0;
        uint64_t totalFramesDropped = 0;
        double averageRenderTimeMs = 0.0;
        double actualFPS = 0.0;
    };

    RenderStats GetStats() const { return m_stats; }

private:
    void RenderThreadMain();

    ThreadSafeQueue::Queue<FrameBuffer> m_renderQueue;
    std::thread m_renderThread;
    std::atomic<bool> m_isRunning{ false };
    std::atomic<bool> m_shouldStop{ false };

    size_t m_maxQueueSize = 10;
    uint32_t m_targetFPS = 60;
    RenderStats m_stats;
};

class AdaptiveBitRateController {
public:
    // Modo de adaptação de bitrate
    enum class AdaptationMode { CONSERVATIVE, BALANCED, AGGRESSIVE };

    AdaptiveBitRateController(uint32_t minBitrateMbps = 5, 
                              uint32_t maxBitrateMbps = 100);

    // Atualiza controller com métricas de rede/performance
    void UpdateMetrics(double networkLatencyMs, double packetLossPercent,
                       double decoderBufferMs);

    // Obtém bitrate recomendado
    uint32_t GetTargetBitrate() const { return m_currentBitrateMbps; }

    // Define modo de adaptação
    void SetAdaptationMode(AdaptationMode mode) { m_mode = mode; }

    // Força bitrate mínimo/máximo
    void SetBitRateRange(uint32_t minMbps, uint32_t maxMbps) {
        m_minBitrateMbps = minMbps;
        m_maxBitrateMbps = maxMbps;
    }

    // Estatísticas
    struct ABRStats {
        uint32_t currentBitrateMbps = 0;
        double currentLatencyMs = 0.0;
        double currentPacketLossPercent = 0.0;
        uint32_t bitrateChangeCount = 0;
    };

    ABRStats GetStats() const;

private:
    void CalculateTargetBitrate();

    uint32_t m_minBitrateMbps;
    uint32_t m_maxBitrateMbps;
    uint32_t m_currentBitrateMbps;
    uint32_t m_previousBitrateMbps;

    double m_networkLatencyMs = 0.0;
    double m_packetLossPercent = 0.0;
    double m_decoderBufferMs = 0.0;

    AdaptationMode m_mode = AdaptationMode::BALANCED;
    uint32_t m_bitrateChangeCount = 0;
};

// Placeholder para futuro WebRTC
class WebRTCPeerConnection {
public:
    WebRTCPeerConnection();
    ~WebRTCPeerConnection();

    // Conecta a peer remoto (futuro com libwebrtc)
    bool Connect(const std::string& peerId);

    // Envia frame codificado
    bool SendEncodedFrame(const std::vector<uint8_t>& encodedData);

    // Recebe frame codificado
    bool ReceiveEncodedFrame(std::vector<uint8_t>& outData);

    bool IsConnected() const { return m_isConnected; }

private:
    bool m_isConnected = false;
    // Futuramente: rtc::PeerConnection*, etc
};

// Placeholder para futuro QUIC
class QUICTransport {
public:
    QUICTransport();
    ~QUICTransport();

    // Inicializa transportador QUIC (futuro com Quinn/MASQUE)
    bool Initialize(const std::string& remoteAddr, uint16_t remotePort);

    // Envia dados
    bool Send(const uint8_t* data, uint32_t size);

    // Recebe dados
    bool Receive(std::vector<uint8_t>& outData);

    // Obtém latência estimada
    double GetLatencyMs() const { return m_estimatedLatencyMs; }

    bool IsConnected() const { return m_isConnected; }

private:
    bool m_isConnected = false;
    double m_estimatedLatencyMs = 0.0;
    // Futuramente: quic::Connection*, etc
};
