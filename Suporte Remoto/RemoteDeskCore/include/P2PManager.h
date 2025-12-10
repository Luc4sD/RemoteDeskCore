#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

struct NetworkFrameHeader {
    static constexpr uint32_t MAGIC = 0xDEADBEEF;
    static constexpr uint16_t VERSION = 1;

    uint32_t magic;              // Validação
    uint16_t version;            // Versão do protocolo
    uint16_t frameSequence;      // Número sequencial do frame
    uint32_t frameWidth;         // Largura da imagem
    uint32_t frameHeight;        // Altura da imagem
    uint32_t frameStride;        // Stride (bytes por linha)
    uint32_t pixelDataSize;      // Tamanho dos pixels
    uint64_t timestamp;          // Timestamp do frame
    uint8_t flags;               // Flags (keyframe, etc)
    uint8_t reserved[7];         // Padding para alinhamento
};

static_assert(sizeof(NetworkFrameHeader) == 48, "NetworkFrameHeader must be 48 bytes");

struct NetworkPacket {
    NetworkFrameHeader header;
    std::vector<uint8_t> pixelData;
};

class P2PManager {
public:
    enum class Role { CLIENT, SERVER };

    P2PManager();
    ~P2PManager();

    // Inicializa como servidor (aguarda conexão)
    bool InitializeAsServer(uint16_t listenPort = 12345);

    // Inicializa como cliente (conecta a servidor)
    bool InitializeAsClient(const std::string& serverIP, uint16_t serverPort = 12345);

    // Envia frame para o peer
    bool SendFrame(const uint8_t* pixelData, uint32_t width, uint32_t height,
                   uint32_t stride, uint16_t frameSequence = 0);

    // Recebe frame do peer (não-bloqueante)
    bool ReceiveFrame(std::vector<uint8_t>& outPixelData, 
                      uint32_t& outWidth, uint32_t& outHeight,
                      uint32_t& outStride, uint16_t& outFrameSequence);

    // Verifica se há dados disponíveis para leitura
    bool IsDataAvailable(int timeoutMs = 0);

    // Obtém estatísticas da conexão
    struct ConnectionStats {
        uint64_t totalBytesSent = 0;
        uint64_t totalBytesReceived = 0;
        uint32_t totalFramesSent = 0;
        uint32_t totalFramesReceived = 0;
        double latencyMs = 0.0;
        double bandwidthMbps = 0.0;
    };

    ConnectionStats GetStats() const { return m_stats; }

    // Verifica status da conexão
    bool IsConnected() const { return m_isConnected; }

    // Libera recursos
    void Disconnect();

    // Configurações de performance
    void SetMaxPacketSize(uint32_t size) { m_maxPacketSize = size; }
    void SetSendBufferSize(uint32_t size) { m_sendBufferSize = size; }
    void SetRecvBufferSize(uint32_t size) { m_recvBufferSize = size; }

private:
    bool InitializeWinsock();
    bool CreateUDPSocket();
    bool BindSocket(uint16_t port);
    bool ConnectToServer(const std::string& ip, uint16_t port);
    bool SendPacket(const NetworkPacket& packet);
    bool ReceivePacket(NetworkPacket& outPacket);

    SOCKET m_socket = INVALID_SOCKET;
    sockaddr_in m_peerAddr = {};
    
    Role m_role = Role::CLIENT;
    bool m_isConnected = false;
    bool m_wsaInitialized = false;

    // Buffers
    std::vector<uint8_t> m_sendBuffer;
    std::vector<uint8_t> m_receiveBuffer;
    uint32_t m_maxPacketSize = 65536;      // 64KB max UDP packet
    uint32_t m_sendBufferSize = 2097152;   // 2MB send buffer
    uint32_t m_recvBufferSize = 2097152;   // 2MB receive buffer

    // Estatísticas
    ConnectionStats m_stats;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
};
