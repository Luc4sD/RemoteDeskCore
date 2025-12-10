#pragma once

/**
 * @file WebSocketSignalingClient.h
 * @brief Cliente WebSocket para comunicação de sinalização com servidor remoto
 *
 * Implementação de um cliente WebSocket que:
 * - Conecta a servidor de sinalização remoto
 * - Registra o peer (host/guest)
 * - Troca SDP offers/answers
 * - Troca ICE candidates
 * - Gerencia reconexão automática
 *
 * Protocolo de Sinalização (JSON):
 * {
 *   "type": "register" | "offer" | "answer" | "ice-candidate" | "ping",
 *   "peerId": "unique-peer-id",
 *   "role": "host" | "guest",
 *   "sessionId": "session-id",
 *   "data": { ... }
 * }
 *
 * @author Lucas D.
 * @date 2025-12-09
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>

/**
 * @struct SignalingMessage
 * @brief Mensagem de sinalização
 */
struct SignalingMessage {
    enum Type {
        REGISTER,           // Registra novo peer
        OFFER,              // Oferta SDP
        ANSWER,             // Resposta SDP
        ICE_CANDIDATE,      // Candidato ICE
        PING,               // Keep-alive
        PONG,               // Keep-alive response
        ERROR               // Erro
    };

    Type type;
    std::string peerId;         // ID único do peer
    std::string role;           // "host" ou "guest"
    std::string sessionId;      // ID da sessão
    std::string sdpOffer;       // Para tipo OFFER
    std::string sdpAnswer;      // Para tipo ANSWER
    std::string iceCandidate;   // Para tipo ICE_CANDIDATE
    std::string sdpMLineIndex;  // Para ICE_CANDIDATE
    std::string sdpMid;         // Para ICE_CANDIDATE
    std::string errorMessage;   // Para tipo ERROR
};

/**
 * @class WebSocketSignalingClient
 * @brief Cliente de sinalização WebSocket para NAT traversal
 *
 * Fluxo de Conexão:
 * 1. Connect() ao servidor de sinalização
 * 2. SendRegister() para se registrar (host gera sessionId)
 * 3. Host cria offer SDP → SendOffer()
 * 4. Guest recebe offer → CreateAnswer() em WebRTCDataChannel
 * 5. Guest envia answer → SendAnswer()
 * 6. Ambos coletam ICE candidates → SendIceCandidate()
 * 7. WebRTC P2P conecta
 * 8. Dados fluem direto P2P (sinalização não é mais usada)
 */
class WebSocketSignalingClient {
public:
    /// Callback para mensagens recebidas
    using MessageReceivedCallback = std::function<void(const SignalingMessage&)>;

    /**
     * @brief Construtor
     * @param signalingServerUrl URL do servidor de sinalização (ex: "ws://example.com:8080")
     */
    explicit WebSocketSignalingClient(const std::string& signalingServerUrl);

    /**
     * @brief Destrutor
     */
    ~WebSocketSignalingClient();

    /**
     * @brief Conecta ao servidor de sinalização
     * @return true se conectado com sucesso
     *
     * A conexão é feita de forma assíncrona em thread separada.
     * Use IsConnected() para verificar quando está pronto.
     */
    bool Connect();

    /**
     * @brief Verifica se está conectado ao servidor
     * @return true se conectado e pronto para enviar mensagens
     */
    bool IsConnected() const;

    /**
     * @brief Registra este peer no servidor (primeira mensagem)
     * @param peerId ID único deste peer (ex: UUID)
     * @param role "host" ou "guest"
     * @param sessionId Para guest: ID da sessão do host. Para host: deixar vazio
     * @return true se enviado com sucesso
     */
    bool SendRegister(const std::string& peerId,
                      const std::string& role,
                      const std::string& sessionId = "");

    /**
     * @brief Envia uma oferta SDP ao peer remoto
     * @param peerId ID do peer remoto
     * @param sdpOffer String SDP completa
     * @return true se enviado com sucesso
     */
    bool SendOffer(const std::string& peerId, const std::string& sdpOffer);

    /**
     * @brief Envia uma resposta SDP ao peer remoto
     * @param peerId ID do peer remoto
     * @param sdpAnswer String SDP completa
     * @return true se enviado com sucesso
     */
    bool SendAnswer(const std::string& peerId, const std::string& sdpAnswer);

    /**
     * @brief Envia um ICE candidate ao peer remoto
     * @param peerId ID do peer remoto
     * @param candidate String do candidato
     * @param sdpMLineIndex Índice da mídia
     * @param sdpMid ID da mídia
     * @return true se enviado com sucesso
     */
    bool SendIceCandidate(const std::string& peerId,
                          const std::string& candidate,
                          const std::string& sdpMLineIndex,
                          const std::string& sdpMid);

    /**
     * @brief Define callback para mensagens recebidas
     * @param callback Função chamada quando mensagem chegar
     */
    void SetMessageReceivedCallback(MessageReceivedCallback callback);

    /**
     * @brief Processa fila de mensagens recebidas
     * Deve ser chamado regularmente na main loop
     */
    void ProcessMessages();

    /**
     * @brief Desconecta do servidor de sinalização
     */
    void Disconnect();

    /**
     * @brief Obtém sessão ID (preenchido após SendRegister como host)
     * @return ID da sessão
     */
    std::string GetSessionId() const;

    /**
     * @brief Obtém ID do peer remoto (preenchido após receber register)
     * @return ID do peer remoto
     */
    std::string GetRemotePeerId() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_pImpl;

    std::string m_signalingServerUrl;
    std::queue<SignalingMessage> m_messageQueue;
    mutable std::mutex m_queueMutex;
};
