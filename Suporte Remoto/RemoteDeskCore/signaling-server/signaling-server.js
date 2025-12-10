#!/usr/bin/env node

/**
 * @file signaling-server.js
 * @brief Servidor de Sinalização WebRTC para Remote Desktop Core
 *
 * Funcionalidade:
 * - Gerencia registro de peers (host/guest)
 * - Facilita troca de SDP offers/answers
 * - Relaia ICE candidates
 * - Mantém sessões P2P
 * - Heartbeat/keepalive
 *
 * Instalação:
 * npm install ws express
 *
 * Execução:
 * node signaling-server.js [port]
 *
 * Padrão: porta 8080
 *
 * Exemplo de Deploy:
 * - Heroku: git push heroku main
 * - AWS EC2: npm install pm2 -g && pm2 start signaling-server.js
 * - DigitalOcean: Similar a EC2
 *
 * @author Lucas D.
 * @date 2025-12-09
 */

const WebSocket = require('ws');
const express = require('express');
const http = require('http');
const url = require('url');

const PORT = process.env.PORT || (process.argv[2] || 8080);

// ═══════════════════════════════════════════════════════════════════════════════
// Tipos de Dados
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class Peer
 * Representa um peer conectado
 */
class Peer {
    constructor(ws, peerId, role) {
        this.ws = ws;
        this.peerId = peerId;
        this.role = role;              // "host" ou "guest"
        this.sessionId = null;
        this.remotePeerId = null;
        this.isAlive = true;
        this.lastActivity = Date.now();
    }

    send(message) {
        if (this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify(message));
        }
    }
}

/**
 * @class Session
 * Representa uma sessão P2P entre host e guest
 */
class Session {
    constructor(hostPeerId, sessionId) {
        this.hostPeerId = hostPeerId;
        this.sessionId = sessionId;
        this.guestPeerId = null;
        this.createdAt = Date.now();
        this.isActive = true;
        this.messageLog = [];         // Para debug
    }

    addLog(message) {
        this.messageLog.push({
            timestamp: new Date().toISOString(),
            ...message
        });
        // Manter apenas últimas 100 mensagens
        if (this.messageLog.length > 100) {
            this.messageLog.shift();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Servidor Principal
// ═══════════════════════════════════════════════════════════════════════════════

class SignalingServer {
    constructor(port) {
        this.port = port;
        this.peers = new Map();           // peerId -> Peer
        this.sessions = new Map();        // sessionId -> Session
        this.hostsBySessionId = new Map(); // sessionId -> peerId do host

        this.app = express();
        this.setupRoutes();

        this.server = http.createServer(this.app);
        this.wss = new WebSocket.Server({ server: this.server });

        this.setupWebSocket();
        this.setupHeartbeat();
        this.setupCleanup();
    }

    setupRoutes() {
        // Health check
        this.app.get('/health', (req, res) => {
            res.json({
                status: 'ok',
                uptime: process.uptime(),
                peers: this.peers.size,
                sessions: this.sessions.size
            });
        });

        // Estatísticas do servidor
        this.app.get('/stats', (req, res) => {
            const activeSessions = Array.from(this.sessions.values())
                .filter(s => s.isActive);

            res.json({
                port: this.port,
                peers: this.peers.size,
                activeSessions: activeSessions.length,
                totalSessions: this.sessions.size,
                peers: Array.from(this.peers.values()).map(p => ({
                    peerId: p.peerId,
                    role: p.role,
                    sessionId: p.sessionId,
                    lastActivity: new Date(p.lastActivity).toISOString()
                })),
                sessions: activeSessions.map(s => ({
                    sessionId: s.sessionId,
                    hostPeerId: s.hostPeerId,
                    guestPeerId: s.guestPeerId,
                    createdAt: new Date(s.createdAt).toISOString()
                }))
            });
        });

        // Obter sessão por ID (público)
        this.app.get('/session/:sessionId', (req, res) => {
            const session = this.sessions.get(req.params.sessionId);
            if (!session) {
                return res.status(404).json({ error: 'Session not found' });
            }

            // Retornar informações públicas apenas
            res.json({
                sessionId: session.sessionId,
                hostAvailable: session.hostPeerId !== null,
                guestConnected: session.guestPeerId !== null,
                createdAt: new Date(session.createdAt).toISOString()
            });
        });

        // Página HTML simples para monitoramento
        this.app.get('/', (req, res) => {
            res.send(`
<!DOCTYPE html>
<html>
<head>
    <title>RemoteDeskCore Signaling Server</title>
    <style>
        body { font-family: monospace; margin: 20px; }
        h1 { color: #333; }
        .stats { background: #f0f0f0; padding: 15px; border-radius: 5px; }
        .stat-row { margin: 5px 0; }
        .status-ok { color: green; font-weight: bold; }
    </style>
</head>
<body>
    <h1>🖥️ RemoteDeskCore Signaling Server</h1>
    <div class="stats">
        <div class="stat-row">Status: <span class="status-ok">ONLINE</span></div>
        <div class="stat-row">Porta: ${PORT}</div>
        <div class="stat-row">Endpoint: ws://localhost:${PORT}</div>
        <div class="stat-row"><a href="/stats">Ver Estatísticas</a></div>
    </div>
    <h2>Uso</h2>
    <pre>
Modo Host:
  ws://servidor:${PORT}
  {"type": "register", "peerId": "uuid1", "role": "host"}

Modo Guest:
  ws://servidor:${PORT}
  {"type": "register", "peerId": "uuid2", "role": "guest", "sessionId": "uuid1_xxx"}

Troca de SDP:
  {"type": "offer", "peerId": "uuid1", "remotePeerId": "uuid2", "data": {"sdp": "..."}}
  {"type": "answer", "peerId": "uuid2", "remotePeerId": "uuid1", "data": {"sdp": "..."}}

ICE Candidates:
  {"type": "ice-candidate", "peerId": "uuid1", "remotePeerId": "uuid2", 
   "data": {"candidate": "...", "sdpMLineIndex": "0", "sdpMid": "0"}}
    </pre>
</body>
</html>
            `);
        });
    }

    setupWebSocket() {
        this.wss.on('connection', (ws, req) => {
            console.log(`[WS] Nova conexão: ${req.socket.remoteAddress}`);

            ws.on('message', (data) => {
                try {
                    const message = JSON.parse(data.toString());
                    this.handleMessage(ws, message);
                } catch (e) {
                    console.error('[Error] JSON parse:', e.message);
                    ws.send(JSON.stringify({
                        type: 'error',
                        message: 'Invalid JSON'
                    }));
                }
            });

            ws.on('close', () => {
                this.handleDisconnect(ws);
            });

            ws.on('error', (error) => {
                console.error('[Error] WebSocket:', error.message);
            });

            // Pong para keepalive
            ws.isAlive = true;
            ws.on('pong', () => {
                ws.isAlive = true;
            });
        });
    }

    setupHeartbeat() {
        setInterval(() => {
            this.wss.clients.forEach((ws) => {
                if (ws.isAlive === false) {
                    ws.terminate();
                    return;
                }

                ws.isAlive = false;
                ws.ping();
            });
        }, 30000); // A cada 30 segundos
    }

    setupCleanup() {
        // Limpar sessões inativas a cada 5 minutos
        setInterval(() => {
            const now = Date.now();
            for (const [sessionId, session] of this.sessions.entries()) {
                const age = now - session.createdAt;
                if (age > 24 * 60 * 60 * 1000) { // Mais de 24 horas
                    this.sessions.delete(sessionId);
                    console.log(`[Cleanup] Sessão expirada: ${sessionId}`);
                }
            }
        }, 5 * 60 * 1000); // 5 minutos
    }

    handleMessage(ws, message) {
        const { type, peerId, role, sessionId } = message;

        console.log(`[${type}] de ${peerId}`);

        switch (type) {
            case 'register':
                this.handleRegister(ws, peerId, role, sessionId);
                break;

            case 'offer':
            case 'answer':
            case 'ice-candidate':
                this.handleRelay(message);
                break;

            case 'ping':
                ws.send(JSON.stringify({ type: 'pong' }));
                break;

            default:
                console.warn(`[Warning] Tipo desconhecido: ${type}`);
        }
    }

    handleRegister(ws, peerId, role, sessionId) {
        // Checar se peer já existe
        if (this.peers.has(peerId)) {
            ws.send(JSON.stringify({
                type: 'error',
                message: 'Peer ID já registrado'
            }));
            return;
        }

        let session;

        if (role === 'host') {
            // Host cria nova sessão
            const newSessionId = peerId + '_' + Date.now();
            session = new Session(peerId, newSessionId);
            this.sessions.set(newSessionId, session);
            this.hostsBySessionId.set(newSessionId, peerId);

            const peer = new Peer(ws, peerId, role);
            peer.sessionId = newSessionId;
            this.peers.set(peerId, peer);

            console.log(`[Host Registrado] ID: ${peerId}, Sessão: ${newSessionId}`);

            ws.send(JSON.stringify({
                type: 'register-ack',
                sessionId: newSessionId,
                role: 'host'
            }));

            session.addLog({
                type: 'host_registered',
                peerId: peerId
            });

        } else if (role === 'guest') {
            // Guest conecta a sessão existente
            session = this.sessions.get(sessionId);
            if (!session) {
                ws.send(JSON.stringify({
                    type: 'error',
                    message: 'Sessão não encontrada'
                }));
                return;
            }

            if (session.guestPeerId !== null) {
                ws.send(JSON.stringify({
                    type: 'error',
                    message: 'Sessão já tem guest'
                }));
                return;
            }

            const peer = new Peer(ws, peerId, role);
            peer.sessionId = sessionId;
            this.peers.set(peerId, peer);

            session.guestPeerId = peerId;

            console.log(`[Guest Registrado] ID: ${peerId}, Sessão: ${sessionId}`);

            ws.send(JSON.stringify({
                type: 'register-ack',
                sessionId: sessionId,
                role: 'guest',
                hostPeerId: session.hostPeerId
            }));

            // Notificar host sobre novo guest
            const hostPeer = this.peers.get(session.hostPeerId);
            if (hostPeer) {
                hostPeer.send({
                    type: 'guest-connected',
                    guestPeerId: peerId
                });
            }

            session.addLog({
                type: 'guest_registered',
                peerId: peerId
            });
        }
    }

    handleRelay(message) {
        const { peerId, remotePeerId, sessionId, type } = message;

        // Encontrar peer remoto
        const remotePeer = this.peers.get(remotePeerId);
        if (!remotePeer) {
            console.warn(`[Relay Falhou] Peer remoto não encontrado: ${remotePeerId}`);
            return;
        }

        // Repassar mensagem
        remotePeer.send(message);

        // Log na sessão
        const session = this.sessions.get(sessionId);
        if (session) {
            session.addLog({
                type: type,
                from: peerId,
                to: remotePeerId
            });
        }

        console.log(`[Relay] ${type} de ${peerId} para ${remotePeerId}`);
    }

    handleDisconnect(ws) {
        // Encontrar peer que desconectou
        for (const [peerId, peer] of this.peers.entries()) {
            if (peer.ws === ws) {
                console.log(`[Desconexão] ${peerId} (${peer.role})`);

                // Se era host, encerrar sessão
                if (peer.role === 'host') {
                    const session = this.sessions.get(peer.sessionId);
                    if (session) {
                        if (session.guestPeerId) {
                            const guestPeer = this.peers.get(session.guestPeerId);
                            if (guestPeer) {
                                guestPeer.send({
                                    type: 'host-disconnected'
                                });
                            }
                        }
                        session.isActive = false;
                    }
                }

                this.peers.delete(peerId);
                break;
            }
        }
    }

    start() {
        this.server.listen(this.port, '0.0.0.0', () => {
            console.log(`
╔════════════════════════════════════════════════════════════╗
║   RemoteDeskCore Signaling Server                          ║
╚════════════════════════════════════════════════════════════╝

📡 Servidor Iniciado!

  WebSocket:  ws://0.0.0.0:${this.port}
  HTTP:       http://localhost:${this.port}
  Stats:      http://localhost:${this.port}/stats

🚀 Pronto para aceitar conexões de peers!

Exemplo de conexão:
  Host: ws://seu-servidor:${this.port}
        {"type": "register", "peerId": "host-uuid", "role": "host"}
  
  Guest: ws://seu-servidor:${this.port}
         {"type": "register", "peerId": "guest-uuid", "role": "guest", 
          "sessionId": "host-uuid_timestamp"}

📊 Monitorar: curl http://localhost:${this.port}/stats

`);
        });

        this.server.on('error', (err) => {
            console.error('[Error] Servidor:', err);
        });
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Inicializar e Executar
// ═══════════════════════════════════════════════════════════════════════════════

const server = new SignalingServer(PORT);
server.start();

// Graceful shutdown
process.on('SIGTERM', () => {
    console.log('\n[Info] SIGTERM recebido, encerrando...');
    server.server.close();
    process.exit(0);
});

process.on('SIGINT', () => {
    console.log('\n[Info] Ctrl+C recebido, encerrando...');
    server.server.close();
    process.exit(0);
});
