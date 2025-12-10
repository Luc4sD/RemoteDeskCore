# WebRTC + NAT Traversal Architecture

## Visão Geral

Este documento descreve a implementação de WebRTC com NAT Traversal para o RemoteDeskCore, permitindo conexões P2P através da internet com segurança e confiabilidade.

## Problema: NAT (Network Address Translation)

### Por que é um problema?

Quando você acessa a internet em casa, seu roteador realiza NAT:
- Você tem IP privado: `192.168.1.100`
- Internet vê apenas IP público: `203.0.113.42` (seu ISP)
- Roteador não sabe como rotear pacotes de volta para sua máquina

```
┌─────────────────────────────────────────────────────────────┐
│                      INTERNET                               │
│                   203.0.113.42 (ISP-1)  203.0.113.99 (ISP-2)│
└────────────────┬──────────────────────┬─────────────────────┘
                 │                      │
        ┌────────▼────────┐   ┌────────▼────────┐
        │   Roteador-1    │   │   Roteador-2    │
        │   192.168.1.1   │   │   192.168.1.1   │
        └────────┬────────┘   └────────┬────────┘
                 │                     │
        ┌────────▼────────┐   ┌────────▼────────┐
        │  Host (Sender)  │   │ Guest (Viewer)  │
        │  192.168.1.100  │   │  192.168.1.101  │
        └─────────────────┘   └─────────────────┘

Problema: Host e Guest não conseguem se conectar diretamente!
Solução: WebRTC + STUN/TURN
```

## Solução: WebRTC com STUN/TURN

### Como Funciona

#### 1. STUN (Session Traversal Utilities for NAT)

STUN é um protocolo simples que ajuda a descobrir seu IP público:

```
Host (192.168.1.100)
    │
    ├─ "Qual é meu IP?" (STUN Request)
    │
    └──► STUN Server (stun.l.google.com:19302)
            │
            └─► "Seu IP é 203.0.113.42:54321" (STUN Response)

Resultado: Host sabe que é alcançável em 203.0.113.42:54321
```

**Casos de Sucesso (70-80% das conexões):**
- NAT Cone: Roteador permite qualquer peer se conectar
- NAT Simétrico Parcial: Funciona com STUN

#### 2. TURN (Traversal Using Relays around NAT)

TURN é um servidor relay para casos onde STUN não funciona:

```
Host (192.168.1.100) ──► TURN Server ◄── Guest (192.168.1.101)
                          (IP Público)
                         
Dados fluem através do TURN server (fallback de último recurso)
```

**Casos de Uso:**
- NAT Restritivo Simétrico (20-30%)
- Corporate Firewall bloqueando UDP
- IPv4/IPv6 NAT64

### Arquitetura WebRTC para RemoteDeskCore

```
┌──────────────────────────────────────────────────────────────────┐
│                    INTERNET (IP Público)                         │
├──────────────────┬────────────────────┬────────────────────────────┤
│                  │                    │                            │
│          ┌───────▼────────┐    ┌──────▼──────┐     ┌──────────────▼───┐
│          │  Signaling      │    │ STUN Server │     │   TURN Server     │
│          │  Server         │    │ (Google)    │     │  (fallback)       │
│          │  (WebSocket)    │    │             │     │                   │
│          │ PORT 8080       │    │ UDP 19302   │     │  UDP 3478         │
│          └───────▲────────┘    └──────▲──────┘     └──────────────▲───┘
│                  │                    │                            │
│        ┌─────────┴─────────┐          │                            │
│        │ Apenas para setup  │          └────────────────────────────┘
│        └────────────────────┘                      Apenas se necessário
│
└────────────────┬────────────────────┬────────────────────┬───────────────┘
                 │                    │                    │
        ┌────────▼──────┐    ┌────────▼──────┐    ┌────────▼──────┐
        │  HOST (Sender) │    │ GUEST (Viewer)│    │ OTHER GUEST   │
        │                │    │                │    │               │
        │ RemoteDeskCore │    │ RemoteDeskCore │    │ RemoteDeskCore│
        │ (P2P Direct)   │◄───┤(P2P Direct)    │    │ (P2P Direct)  │
        │                │    │                │    │               │
        │ libdatachannel │    │ libdatachannel │    │ libdatachannel│
        └────────────────┘    └────────────────┘    └────────────────┘
         
         UDP P2P Streams (low latency)
         sem intermediários após handshake
```

## Fluxo de Conexão (Passo a Passo)

### FASE 1: Registro no Servidor de Sinalização

```
┌──────────────────────────────────────────────────────────────────┐
│ ETAPA 1: HOST INICIA                                             │
└──────────────────────────────────────────────────────────────────┘

1. Host abre conexão WebSocket ao servidor de sinalização
   ws://seu-servidor.com:8080

2. Host envia:
   {
     "type": "register",
     "peerId": "uuid-1234",        // ID único para o host
     "role": "host"
   }

3. Servidor responde:
   {
     "type": "register-ack",
     "sessionId": "uuid-1234_1702144802000",  // ID da sessão
     "role": "host"
   }

4. Host armazena sessionId para compartilhar com guest


┌──────────────────────────────────────────────────────────────────┐
│ ETAPA 2: GUEST CONECTA                                           │
└──────────────────────────────────────────────────────────────────┘

1. Guest recebe sessionId do host (via QR code, link, etc)

2. Guest abre WebSocket e envia:
   {
     "type": "register",
     "peerId": "uuid-5678",        // ID único para o guest
     "role": "guest",
     "sessionId": "uuid-1234_1702144802000"
   }

3. Servidor responde:
   {
     "type": "register-ack",
     "sessionId": "uuid-1234_1702144802000",
     "role": "guest",
     "hostPeerId": "uuid-1234"
   }

4. Servidor notifica host:
   {
     "type": "guest-connected",
     "guestPeerId": "uuid-5678"
   }
```

### FASE 2: Troca de Ofertas SDP (Session Description Protocol)

```
┌──────────────────────────────────────────────────────────────────┐
│ ETAPA 3: HOST CRIA OFERTA                                        │
└──────────────────────────────────────────────────────────────────┘

Host (Initiator) cria uma oferta usando WebRTC:

1. Host chama: WebRTCDataChannel::CreateOffer(sdpOffer)
   
2. LibDataChannel gera SDP com:
   - Codecs suportados
   - Modos de transporte (DTLs, SRTP)
   - Candidates ICE locais iniciais

3. Host envia para Guest via servidor:
   {
     "type": "offer",
     "peerId": "uuid-1234",
     "remotePeerId": "uuid-5678",
     "data": {
       "sdp": "v=0\no=- 123456... (muitas linhas de SDP)"
     }
   }


┌──────────────────────────────────────────────────────────────────┐
│ ETAPA 4: GUEST RECEBE OFERTA E CRIA RESPOSTA                    │
└──────────────────────────────────────────────────────────────────┘

1. Guest recebe oferta do servidor

2. Guest chama: WebRTCDataChannel::SetRemoteOffer(sdpOffer)

3. Guest cria resposta: WebRTCDataChannel::CreateAnswer(sdpAnswer)

4. Guest envia resposta para Host:
   {
     "type": "answer",
     "peerId": "uuid-5678",
     "remotePeerId": "uuid-1234",
     "data": {
       "sdp": "v=0\no=- 987654... (resposta SDP)"
     }
   }


┌──────────────────────────────────────────────────────────────────┐
│ ETAPA 5: HOST RECEBE RESPOSTA                                    │
└──────────────────────────────────────────────────────────────────┘

1. Host recebe resposta do servidor

2. Host chama: WebRTCDataChannel::SetRemoteAnswer(sdpAnswer)

3. WebRTC agora sabe como se conectar!
```

### FASE 3: Troca de ICE Candidates (Descoberta de Caminhos)

```
┌──────────────────────────────────────────────────────────────────┐
│ DURANTE E APÓS: ICE CANDIDATE GATHERING                          │
└──────────────────────────────────────────────────────────────────┘

Tanto Host quanto Guest coletam possíveis formas de se conectar:

1. Candidates Locais (sempre):
   candidate:1 192.168.1.100:54321    (IP local privado)

2. Candidates STUN (geralmente):
   candidate:2 203.0.113.42:54321     (IP público via STUN)

3. Candidates TURN (se necessário):
   candidate:3 198.51.100.1:3478      (IP do TURN server relay)

HOST ENVIA CANDIDATES:
  Host descobre candidates e envia para Guest:
  {
    "type": "ice-candidate",
    "peerId": "uuid-1234",
    "remotePeerId": "uuid-5678",
    "data": {
      "candidate": "candidate:1 1 udp 2130706431 192.168.1.100 54321...",
      "sdpMLineIndex": "0",
      "sdpMid": "0"
    }
  }

GUEST RECEBE E TESTA:
  Guest testa cada candidate:
  
  ✓ "Consigo alcançar 203.0.113.42:54321" (STUN candidate)
    → SUCESSO! Conexão direta P2P
  
  ou
  
  ✗ "Não consigo alcançar o IP privado" (candidate local)
  ✗ "Não consigo alcançar o STUN" (firewall)
  ✓ "Consigo alcançar o TURN server"
    → OK! Usa relay (mais lento mas funciona)
```

### FASE 4: Conexão P2P Estabelecida

```
┌──────────────────────────────────────────────────────────────────┐
│ ETAPA 6: P2P CONECTADO - SERVIDOR NÃO MAIS NECESSÁRIO           │
└──────────────────────────────────────────────────────────────────┘

Uma vez conectados:

HOST                          GUEST
  │                             │
  │ WebRTC Data Channel         │
  │ ◄────────────────────────────│
  │ (UDP direto com DTLS/SRTP)  │
  │ ──────────────────────────── │
  │                              │
  ├─ Envia frames comprimidos    │
  ├─ Recebe input (mouse/teclado)│
  │                              │
  │                         Recebe frames
  │                         Envia input
  │
  
Servidor de Sinalização agora PODE DESCONECTAR
(foi útil apenas para coordenação inicial)
```

## Implementação no RemoteDeskCore

### 1. Instalar Dependências

#### No Cliente C++:

```bash
cd RemoteDeskCore

# Instalar via vcpkg
vcpkg install libdatachannel:x64-windows
vcpkg install nlohmann-json:x64-windows

# Atualizar CMakeLists.txt para incluir libdatachannel
```

#### No Servidor de Sinalização:

```bash
cd signaling-server

# Instalar dependências Node.js
npm install

# Executar servidor
npm start

# Ou especificar porta
node signaling-server.js 8080
```

### 2. Modos de Operação

#### MODO HOST

```cpp
#include "RemoteDesktopSystem.h"
#include "WebRTCDataChannel.h"
#include "WebSocketSignalingClient.h"

int main() {
    RemoteDesktopSystem system;
    
    // Usar modo WebRTC em vez de UDP direto
    system.SetUseWebRTC(true);
    system.SetSignalingServerUrl("ws://seu-servidor.com:8080");
    
    // Inicializar como host
    if (system.InitializeAsWebRTCHost(
        "seu-servidor.com:8080",      // Servidor de sinalização
        std::vector<std::string>{      // STUN servers
            "stun:stun.l.google.com:19302",
            "stun:stun.services.mozilla.com:3478"
        })) {
        
        // Obter session ID para compartilhar com guest
        std::string sessionId = system.GetWebRTCSessionId();
        std::cout << "Share this ID with guest: " << sessionId << std::endl;
        
        // Run - aguarda guest conectar
        system.Run();
    }
    
    return 0;
}
```

#### MODO GUEST

```cpp
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Uso: app.exe <session-id>" << std::endl;
        return 1;
    }
    
    std::string sessionId = argv[1];
    
    RemoteDesktopSystem system;
    system.SetUseWebRTC(true);
    system.SetSignalingServerUrl("ws://seu-servidor.com:8080");
    
    // Conectar como guest
    if (system.InitializeAsWebRTCGuest(
        "seu-servidor.com:8080",
        sessionId)) {
        
        system.Run();
    }
    
    return 0;
}
```

### 3. Fluxo de Frames com WebRTC

```cpp
// Em RemoteDesktopSystem::MainLoopServer() com WebRTC:

while (running) {
    // 1. Capturar frame
    FrameBuffer frame = capturer->CaptureFrame();
    
    // 2. Codificar (H.264 com NVENC)
    EncodedFrame encoded = encoder->EncodeFrame(frame);
    
    // 3. Criar header customizado
    NetworkFrameHeader header = {
        .magic = 0xDEADBEEF,
        .width = 1920,
        .height = 1080,
        .stride = 1920 * 4,
        .pixelDataSize = encoded.size,
        .timestamp = GetCurrentTimestamp()
    };
    
    // 4. Enviar via WebRTC (não UDP)
    webrtc->SendData((uint8_t*)&header, sizeof(header));
    webrtc->SendData(encoded.data, encoded.size);
    
    // 5. Receber input
    uint8_t inputBuffer[256];
    size_t inputSize;
    if (webrtc->ReceiveData(inputBuffer, inputSize)) {
        // Processar input remoto
        ProcessRemoteInput(inputBuffer, inputSize);
    }
    
    // 6. Processar eventos WebRTC
    webrtc->ProcessMessages();
}
```

## Servidores STUN/TURN Públicos

### STUN Gratuitos (Sempre Use)

```
stun:stun.l.google.com:19302
stun:stun.services.mozilla.com:3478
stun:stun1.l.google.com:19302
stun:stun2.l.google.com:19302
stun:stun3.l.google.com:19302
stun:stun4.l.google.com:19302
stun:stun.twilio.com:3478
```

### TURN Gratuitos (Limitados)

```
# Metered (1GB/mês gratuito)
turn:global.metered.ca:80
  username: d2f3932155e505d46db37cd6
  credential: +EsxZ7dWXgkqNWAJ

# Xirsys (25GB/mês, requer API key)
turn:ap1.xirsys.com:80
  username: seu-username
  credential: seu-password
```

### Hostedar Próprio TURN (Recomendado para Produção)

```bash
# Instalar coturn
apt-get install coturn

# /etc/coturn/turnserver.conf
listening-port=3478
listening-ip=0.0.0.0
external-ip=203.0.113.42

# Iniciar
systemctl start coturn
```

## Segurança

### DTLS (Datagram TLS) - Automático com libdatachannel

```
WebRTC Data Channel
    ↓
SRTP (Secure RTP) - Encripta áudio/vídeo
    ↓
DTLS 1.2+ - Encripta handshake
    ↓
UDP Criptografado
```

### Recomendações

1. **Sempre usar HTTPS/WSS para sinalização**:
   ```
   wss://seu-servidor.com:8443  (WebSocket Secure)
   ```

2. **Validar certificados SDP**:
   - Verificar fingerprint DTLS
   - Confirmar identidade do peer

3. **Rate limit no servidor de sinalização**:
   - Max 100 registros/min por IP
   - Timeout após 30 min inativo

## Deployment

### Heroku (Gratuito com limitações)

```bash
# Criar app
heroku create remotedeskcore-signaling

# Deploy
git push heroku main

# Ver logs
heroku logs --tail
```

### AWS EC2 (Production)

```bash
# Instalar Node.js 20+
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs

# Clonar repo
git clone seu-repo
cd RemoteDeskCore/signaling-server

# Instalar PM2 (process manager)
sudo npm install -g pm2

# Iniciar servidor
pm2 start signaling-server.js --name "signaling"
pm2 startup
pm2 save

# Firewall
sudo ufw allow 8080/tcp
```

### Docker

```dockerfile
FROM node:20-alpine
WORKDIR /app
COPY signaling-server .
RUN npm install
EXPOSE 8080
CMD ["node", "signaling-server.js"]
```

```bash
docker build -t remotedeskcore-signaling .
docker run -p 8080:8080 remotedeskcore-signaling
```

## Monitoramento

### Verificar Saúde do Servidor

```bash
curl http://seu-servidor.com:8080/stats
```

Resposta:
```json
{
  "port": 8080,
  "peers": 5,
  "activeSessions": 2,
  "sessions": [
    {
      "sessionId": "uuid-1234_1702144802000",
      "hostPeerId": "uuid-1234",
      "guestPeerId": "uuid-5678",
      "createdAt": "2024-12-09T10:00:00Z"
    }
  ]
}
```

### Latência de Conexão

```cpp
// Em RemoteDeskCore após P2P conectado:
WebRTCStats stats = webrtc->GetStats();

std::cout << "RTT: " << stats.currentRoundTripTime << "ms" << std::endl;
std::cout << "Bytes enviados: " << stats.bytesSent << std::endl;
std::cout << "Bytes recebidos: " << stats.bytesReceived << std::endl;
```

## Troubleshooting

### Problema: Conexão Falha

```
1. Verificar servidor de sinalização
   curl http://seu-servidor.com/health
   
2. Verificar STUN/TURN (pode testar online)
   https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/
   
3. Verificar firewall UDP
   # Windows
   netsh advfirewall firewall add rule name="WebRTC" \
     dir=out action=allow protocol=UDP localport=any
   
4. Logs de debug
   Ativar logging em WebRTCDataChannel:
   #define DEBUG_WEBRTC 1
```

### Problema: Latência Alta

```
1. Se usando TURN relay:
   - Escolher TURN mais próximo geograficamente
   - Considerar hospedar próprio TURN
   
2. Se usando P2P direto:
   - Pode ser MTU (path fragmentation)
   - Testar com ping para diagnosticar
   
3. Verificar statsC:
   if (stats.currentRoundTripTime > 100) {
       // Adaptar bitrate para baixo
       adaptiveController.SetMode(ABR_CONSERVATIVE);
   }
```

## Comparação: UDP Direto vs WebRTC

| Aspecto | UDP Direto | WebRTC |
|---------|-----------|--------|
| **NAT Traversal** | ❌ Não | ✅ Sim (STUN/TURN) |
| **Segurança** | ❌ Nenhuma | ✅ DTLS 1.2+ |
| **Latência** | ⚡ Ultra-baixa (<5ms) | ⚡ Baixa (<15ms) |
| **Internet** | ❌ LAN apenas | ✅ Qualquer lugar |
| **Confiabilidade** | ❌ Sem tratamento erro | ✅ Retry automático |
| **Fallback** | ❌ Sem opção | ✅ TURN relay |
| **Setup Simples** | ✅ Sim | ⚠️ Precisa sinal server |

## Próximos Passos

1. **Integração Real com libdatachannel**
   - Descomentadar código em WebRTCDataChannel.cpp
   - Linkar libdatachannel em CMakeLists.txt
   - Testar com STUN Google

2. **Servidor de Sinalização em Produção**
   - Deploy em AWS/DigitalOcean/Heroku
   - Obter domínio + SSL
   - Configurar TURN próprio para fallback

3. **Client-side QR Code**
   - Gerar QR com session ID
   - Permitir compartilhar via link: `remotedeskcore://sessionId`

4. **Analítica**
   - Rastrear taxa de sucesso de conexão
   - Monitorar latência média
   - Alertar se TURN relay necessário

## Referências

- [WebRTC API Spec](https://www.w3.org/TR/webrtc/)
- [libdatachannel](https://github.com/paullouisageneau/libdatachannel)
- [STUN RFC 3489](https://tools.ietf.org/html/rfc3489)
- [TURN RFC 5766](https://tools.ietf.org/html/rfc5766)
- [ICE RFC 8445](https://tools.ietf.org/html/rfc8445)

---

**Documento Versão**: 1.0  
**Data**: 2025-12-09  
**Autor**: Lucas D.
