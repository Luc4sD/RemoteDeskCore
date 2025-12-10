#include "P2PManager.h"
#include <iostream>
#include <cstring>
#include <chrono>

P2PManager::P2PManager() {
    m_sendBuffer.reserve(m_sendBufferSize);
    m_receiveBuffer.reserve(m_recvBufferSize);
}

P2PManager::~P2PManager() {
    Disconnect();
}

bool P2PManager::InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    if (result != 0) {
        OutputDebugStringA("WSAStartup failed\n");
        return false;
    }

    m_wsaInitialized = true;
    return true;
}

bool P2PManager::CreateUDPSocket() {
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    if (m_socket == INVALID_SOCKET) {
        OutputDebugStringA("socket() failed\n");
        return false;
    }

    // Configurar socket para não-bloqueante
    u_long mode = 1;
    if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
        OutputDebugStringA("ioctlsocket() failed\n");
        closesocket(m_socket);
        return false;
    }

    // Configurar tamanho dos buffers
    int sendBufSize = m_sendBufferSize;
    int recvBufSize = m_recvBufferSize;

    setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufSize, sizeof(sendBufSize));
    setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufSize, sizeof(recvBufSize));

    return true;
}

bool P2PManager::BindSocket(uint16_t port) {
    sockaddr_in sockAddr = {};
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockAddr.sin_port = htons(port);

    if (bind(m_socket, (sockaddr*)&sockAddr, sizeof(sockAddr)) == SOCKET_ERROR) {
        OutputDebugStringA("bind() failed\n");
        return false;
    }

    return true;
}

bool P2PManager::InitializeAsServer(uint16_t listenPort) {
    if (!InitializeWinsock()) {
        return false;
    }

    if (!CreateUDPSocket()) {
        return false;
    }

    if (!BindSocket(listenPort)) {
        return false;
    }

    m_role = Role::SERVER;
    m_isConnected = true;

    std::string msg = "P2P Server initialized on port " + std::to_string(listenPort) + "\n";
    OutputDebugStringA(msg.c_str());

    return true;
}

bool P2PManager::InitializeAsClient(const std::string& serverIP, uint16_t serverPort) {
    if (!InitializeWinsock()) {
        return false;
    }

    if (!CreateUDPSocket()) {
        return false;
    }

    if (!ConnectToServer(serverIP, serverPort)) {
        return false;
    }

    m_role = Role::CLIENT;
    m_isConnected = true;

    std::string msg = "P2P Client connected to " + serverIP + ":" + std::to_string(serverPort) + "\n";
    OutputDebugStringA(msg.c_str());

    return true;
}

bool P2PManager::ConnectToServer(const std::string& ip, uint16_t port) {
    m_peerAddr.sin_family = AF_INET;
    m_peerAddr.sin_port = htons(port);

    int result = inet_pton(AF_INET, ip.c_str(), &m_peerAddr.sin_addr);
    if (result != 1) {
        OutputDebugStringA("inet_pton() failed\n");
        return false;
    }

    return true;
}

bool P2PManager::SendPacket(const NetworkPacket& packet) {
    if (!m_isConnected || m_socket == INVALID_SOCKET) {
        return false;
    }

    // Serializar header + dados
    m_sendBuffer.clear();
    m_sendBuffer.reserve(sizeof(NetworkFrameHeader) + packet.pixelData.size());

    // Copiar header
    const uint8_t* headerPtr = reinterpret_cast<const uint8_t*>(&packet.header);
    m_sendBuffer.insert(m_sendBuffer.end(), headerPtr, headerPtr + sizeof(NetworkFrameHeader));

    // Copiar dados dos pixels
    m_sendBuffer.insert(m_sendBuffer.end(), 
                       packet.pixelData.begin(), 
                       packet.pixelData.end());

    // Enviar packet
    int sentBytes = sendto(
        m_socket,
        (char*)m_sendBuffer.data(),
        (int)m_sendBuffer.size(),
        0,
        (sockaddr*)&m_peerAddr,
        sizeof(m_peerAddr)
    );

    if (sentBytes == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            OutputDebugStringA("sendto() failed\n");
            return false;
        }
    } else {
        m_stats.totalBytesSent += sentBytes;
        m_stats.totalFramesSent++;
    }

    return true;
}

bool P2PManager::ReceivePacket(NetworkPacket& outPacket) {
    if (!m_isConnected || m_socket == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in fromAddr = {};
    int fromAddrLen = sizeof(fromAddr);

    // Receber dados
    int receivedBytes = recvfrom(
        m_socket,
        (char*)m_receiveBuffer.data(),
        (int)m_receiveBuffer.capacity(),
        0,
        (sockaddr*)&fromAddr,
        &fromAddrLen
    );

    if (receivedBytes == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            OutputDebugStringA("recvfrom() failed\n");
            return false;
        }
        return false; // Nenhum dado disponível
    }

    if (receivedBytes < (int)sizeof(NetworkFrameHeader)) {
        OutputDebugStringA("Packet too small\n");
        return false;
    }

    // Desserializar header
    std::memcpy(&outPacket.header, m_receiveBuffer.data(), sizeof(NetworkFrameHeader));

    // Validar magic number
    if (outPacket.header.magic != NetworkFrameHeader::MAGIC) {
        OutputDebugStringA("Invalid magic number\n");
        return false;
    }

    // Extrair dados dos pixels
    uint32_t pixelDataSize = receivedBytes - sizeof(NetworkFrameHeader);
    outPacket.pixelData.resize(pixelDataSize);
    std::memcpy(outPacket.pixelData.data(), 
                m_receiveBuffer.data() + sizeof(NetworkFrameHeader),
                pixelDataSize);

    m_stats.totalBytesReceived += receivedBytes;
    m_stats.totalFramesReceived++;

    // Atualizar peer address para servidor modo
    if (m_role == Role::SERVER) {
        m_peerAddr = fromAddr;
    }

    // Calcular latência
    m_lastFrameTime = std::chrono::high_resolution_clock::now();

    return true;
}

bool P2PManager::SendFrame(const uint8_t* pixelData, uint32_t width, uint32_t height,
                           uint32_t stride, uint16_t frameSequence) {
    if (!pixelData) {
        return false;
    }

    NetworkPacket packet;
    packet.header.magic = NetworkFrameHeader::MAGIC;
    packet.header.version = NetworkFrameHeader::VERSION;
    packet.header.frameSequence = frameSequence;
    packet.header.frameWidth = width;
    packet.header.frameHeight = height;
    packet.header.frameStride = stride;
    packet.header.pixelDataSize = stride * height;
    packet.header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    packet.header.flags = 0;

    // Copiar dados de pixels
    packet.pixelData.resize(packet.header.pixelDataSize);
    std::memcpy(packet.pixelData.data(), pixelData, packet.header.pixelDataSize);

    return SendPacket(packet);
}

bool P2PManager::ReceiveFrame(std::vector<uint8_t>& outPixelData,
                             uint32_t& outWidth, uint32_t& outHeight,
                             uint32_t& outStride, uint16_t& outFrameSequence) {
    NetworkPacket packet;
    
    if (!ReceivePacket(packet)) {
        return false;
    }

    outPixelData = std::move(packet.pixelData);
    outWidth = packet.header.frameWidth;
    outHeight = packet.header.frameHeight;
    outStride = packet.header.frameStride;
    outFrameSequence = packet.header.frameSequence;

    return true;
}

bool P2PManager::IsDataAvailable(int timeoutMs) {
    if (!m_isConnected || m_socket == INVALID_SOCKET) {
        return false;
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(m_socket, &readSet);

    timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

    int result = select(0, &readSet, nullptr, nullptr, &timeout);
    return result > 0;
}

void P2PManager::Disconnect() {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    if (m_wsaInitialized) {
        WSACleanup();
        m_wsaInitialized = false;
    }

    m_isConnected = false;
}
