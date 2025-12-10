#pragma once

/**
 * @file WebRTCDataChannel.h
 * @brief WebRTC Data Channel Manager com NAT Traversal e STUN/TURN
 *
 * Implementação de um gerenciador de canais de dados WebRTC usando libdatachannel.
 * Suporta:
 * - Troca de ofertas/respostas SDP (Session Description Protocol)
 * - ICE candidates (STUN/TURN)
 * - Transmissão P2P de frames com NAT traversal automático
 * - Fallback para TURN se NAT bloqueado
 *
 * Arquitetura:
 * ┌─────────────────┐      Signaling Server      ┌─────────────────┐
 * │  Host (Sender)  │◄────────(WebSocket)────────┤ Guest (Receiver)│
 * │                 │                             │                 │
 * │ WebRTCDataChannel├─────────────────────────────┤WebRTCDataChannel
 * │ (STUN/TURN)     │    P2P Data Channel (UDP)   │ (STUN/TURN)     │
 * └─────────────────┘                             └─────────────────┘
 *
 * @author Lucas D. (Senior Software Engineer)
 * @date 2025-12-09
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

// Forward declarations (evitar circular dependencies)
class WebSocketSignalingClient;

/**
 * @struct ICECandidate
 * @brief Representa um candidato ICE (Interactive Connectivity Establishment)
 */
struct ICECandidate {
    std::string candidate;      ///< Descrição completa do candidato
    std::string sdpMLineIndex;  ///< Índice da linha media no SDP
    std::string sdpMid;         ///< Media stream ID
};

/**
 * @struct DataChannelMessage
 * @brief Estrutura para enviar/receber mensagens pelo data channel
 */
struct DataChannelMessage {
    uint8_t* data;              ///< Ponteiro para dados
    size_t size;                ///< Tamanho dos dados em bytes
    bool isBinary;              ///< true para dados binários, false para texto

    DataChannelMessage() : data(nullptr), size(0), isBinary(true) {}
    DataChannelMessage(uint8_t* d, size_t s, bool bin = true)
        : data(d), size(s), isBinary(bin) {}
};

/**
 * @struct WebRTCStats
 * @brief Estatísticas da conexão WebRTC
 */
struct WebRTCStats {
    bool connected;             ///< true se conectado
    uint64_t bytesSent;        ///< Bytes enviados
    uint64_t bytesReceived;    ///< Bytes recebidos
    double currentRoundTripTime; ///< RTT em ms
    std::string currentConnectionState; ///< Estado (connected, disconnected, etc)
    int candidatesSent;        ///< Número de ICE candidates enviados
    int candidatesReceived;    ///< Número de ICE candidates recebidos
};

/**
 * @class WebRTCDataChannel
 * @brief Gerenciador de Data Channels WebRTC com suporte a STUN/TURN
 *
 * Responsabilidades:
 * - Criar e gerenciar conexões WebRTC
 * - Trocar SDP offers/answers
 * - Coletar e processar ICE candidates
 * - Transmitir/receber dados em tempo real
 * - Fallback automático para TURN servers
 * - Callbacks para eventos de conexão/dados
 */
class WebRTCDataChannel {
public:
    /// Tipo de callback para dados recebidos
    using DataReceivedCallback = std::function<void(const uint8_t*, size_t)>;
    
    /// Tipo de callback para mudanças de estado
    using StateChangedCallback = std::function<void(const std::string&)>;
    
    /// Tipo de callback para ICE candidates
    using IceCandidateCallback = std::function<void(const ICECandidate&)>;

    /**
     * @brief Construtor
     * @param isInitiator true se este peer criará a oferta (host), false se responder (guest)
     */
    explicit WebRTCDataChannel(bool isInitiator = true);

    /**
     * @brief Destrutor
     */
    ~WebRTCDataChannel();

    /**
     * @brief Inicializa o WebRTC com servidores STUN/TURN
     * @param stunServers Lista de servidores STUN (ex: "stun:stun.l.google.com:19302")
     * @param turnServers Lista de servidores TURN (optional, para NAT restritivo)
     * @return true se inicializado com sucesso
     *
     * Exemplo:
     * ```cpp
     * std::vector<std::string> stun = {
     *     "stun:stun.l.google.com:19302",
     *     "stun:stun.services.mozilla.com"
     * };
     * channel.Initialize(stun);
     * ```
     */
    bool Initialize(const std::vector<std::string>& stunServers,
                    const std::vector<std::string>& turnServers = {});

    /**
     * @brief Cria uma oferta SDP (para o initiator/host)
     * @param[out] sdpOffer String contendo a oferta SDP completa
     * @return true se oferta criada com sucesso
     */
    bool CreateOffer(std::string& sdpOffer);

    /**
     * @brief Define uma oferta SDP recebida (para o non-initiator/guest)
     * @param sdpOffer String contendo a oferta SDP do peer
     * @return true se oferta processada com sucesso
     */
    bool SetRemoteOffer(const std::string& sdpOffer);

    /**
     * @brief Cria uma resposta SDP (para o non-initiator/guest)
     * @param[out] sdpAnswer String contendo a resposta SDP
     * @return true se resposta criada com sucesso
     */
    bool CreateAnswer(std::string& sdpAnswer);

    /**
     * @brief Define uma resposta SDP recebida (para o initiator/host)
     * @param sdpAnswer String contendo a resposta SDP do peer
     * @return true se resposta processada com sucesso
     */
    bool SetRemoteAnswer(const std::string& sdpAnswer);

    /**
     * @brief Adiciona um ICE candidate recebido
     * @param candidate Estrutura ICECandidate com informações do candidato
     * @return true se candidate processado com sucesso
     */
    bool AddIceCandidate(const ICECandidate& candidate);

    /**
     * @brief Envia dados pelo data channel
     * @param data Ponteiro para dados
     * @param size Tamanho em bytes
     * @return true se enviado com sucesso (dados ficarão em buffer se necessário)
     */
    bool SendData(const uint8_t* data, size_t size);

    /**
     * @brief Conecta callbacks de evento
     * @param onDataReceived Chamado quando dados chegam
     * @param onStateChanged Chamado quando estado muda
     * @param onIceCandidate Chamado quando novo ICE candidate é gerado
     */
    void SetCallbacks(DataReceivedCallback onDataReceived,
                      StateChangedCallback onStateChanged,
                      IceCandidateCallback onIceCandidate);

    /**
     * @brief Processa mensagens pendentes (polls para non-blocking)
     * Deve ser chamado periodicamente na main loop
     */
    void ProcessMessages();

    /**
     * @brief Obtém estatísticas atuais da conexão
     * @return Estrutura com estatísticas
     */
    WebRTCStats GetStats() const;

    /**
     * @brief Verifica se está conectado
     * @return true se data channel está aberto e pronto para comunicação
     */
    bool IsConnected() const;

    /**
     * @brief Fecha a conexão gracefully
     */
    void Close();

    /**
     * @brief Obtém o estado atual (debug/logging)
     * @return String descrevendo o estado atual
     */
    std::string GetConnectionState() const;

private:
    // Implementação privada (pimpl pattern para esconder libdatachannel)
    class Impl;
    std::unique_ptr<Impl> m_pImpl;

    bool m_isInitiator;
    bool m_initialized;
};
