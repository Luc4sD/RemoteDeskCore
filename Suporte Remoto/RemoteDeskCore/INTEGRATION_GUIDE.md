# Guia de Integração: WebRTC com libdatachannel

## Pré-requisitos

- Visual Studio 2019+ ou Clang
- vcpkg instalado
- C++17/20 compilador
- CMake 3.20+

## Passo 1: Instalar Dependências via vcpkg

```bash
# Clone vcpkg se ainda não tiver
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Instalar libdatachannel (inclui todas as dependências)
.\vcpkg install libdatachannel:x64-windows

# Instalar websocketpp para sinalização
.\vcpkg install websocketpp:x64-windows

# Instalar nlohmann-json para parsing de mensagens
.\vcpkg install nlohmann-json:x64-windows

# Integrar com CMake
.\vcpkg integrate install
```

## Passo 2: Atualizar CMakeLists.txt

Adicione estas linhas ao seu `CMakeLists.txt`:

```cmake
# Find packages
find_package(libdatachannel REQUIRED)
find_package(websocketpp REQUIRED)
find_package(nlohmann_json REQUIRED)

# Link libraries
target_link_libraries(remote_desktop_app PRIVATE
    libdatachannel::libdatachannel
    websocketpp::websocketpp
    nlohmann_json::nlohmann_json
)
```

## Passo 3: Descomentar Código Real em WebRTCDataChannel.cpp

Substitua a seção STUB com a implementação real:

```cpp
// Procure por este comentário:
// Implementação real usando libdatachannel:

// Descomente todo o bloco try/catch abaixo
// Remova o bloco STUB (// STUB: ...)
```

### Exemplo Completo Descomentado:

```cpp
bool WebRTCDataChannel::Initialize(const std::vector<std::string>& stunServers,
                                  const std::vector<std::string>& turnServers) {
    if (m_initialized) {
        return true;
    }

    try {
        rtc::Configuration config;
        
        // Adicionar STUN servers
        for (const auto& stun : stunServers) {
            rtc::IceServer iceServer;
            iceServer.urls.push_back(stun);
            config.iceServers.push_back(iceServer);
        }
        
        // Adicionar TURN servers
        for (const auto& turn : turnServers) {
            rtc::IceServer iceServer;
            iceServer.urls.push_back(turn);
            config.iceServers.push_back(iceServer);
        }
        
        m_pImpl->peerConnection = std::make_shared<rtc::PeerConnection>(config);
        
        // ... resto do código original descomentado ...
        
        m_initialized = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[WebRTC Error] " << e.what() << std::endl;
        return false;
    }
}
```

## Passo 4: Configurar Servidor de Sinalização

### Instalação Rápida (Windows)

```bash
# Instalar Node.js de https://nodejs.org (LTS recomendado)

# Navegue para pasta signaling-server
cd RemoteDeskCore\signaling-server

# Instalar dependências
npm install

# Testar localmente
npm start
# Acesse http://localhost:8080
```

### Instalação em Produção (Linux)

```bash
# SSH no servidor
ssh ubuntu@seu-servidor.com

# Instalar Node.js
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt-get install -y nodejs

# Clonar repo
git clone seu-repo
cd RemoteDeskCore/signaling-server
npm install

# Instalar PM2 (process manager)
sudo npm install -g pm2

# Iniciar com PM2
pm2 start signaling-server.js --name "signal"
pm2 startup    # Autostart em reboot
pm2 save

# Firewall
sudo ufw allow 8080/tcp
sudo ufw enable

# (Opcional) Nginx reverse proxy com SSL
sudo apt install nginx certbot python3-certbot-nginx
# Configurar certificado SSL...
```

## Passo 5: Testar Localmente

### Terminal 1: Inicie o Servidor de Sinalização

```bash
cd RemoteDeskCore\signaling-server
npm start
# Output: Servidor Iniciado! WebSocket: ws://0.0.0.0:8080
```

### Terminal 2: Compile o Cliente

```bash
cd RemoteDeskCore
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE="[vcpkg-path]/scripts/buildsystems/vcpkg.cmake" ..
cmake --build . --config Release
```

### Terminal 3: Execute o HOST

```bash
cd RemoteDeskCore\build\Release
remote_desktop_app.exe host
# Output: Iniciando em modo Host...
#         Session ID: uuid-1234_1702144802000
#         Share this ID with guest
```

### Terminal 4: Execute o GUEST

```bash
cd RemoteDeskCore\build\Release
remote_desktop_app.exe join uuid-1234_1702144802000
# Output: Iniciando em modo Guest...
#         Conectando ao host...
```

## Passo 6: Verificar Conexão P2P

Durante a execução, você deve ver:

```
[WebSocket] Conectado ao servidor de sinalização
[WebRTC] Inicializado com 4 STUN e 0 TURN servers
[WebRTC] Oferta SDP criada
[WebRTC] Resposta SDP recebida
[WebRTC] ICE candidate #1 processado
[WebRTC] ICE candidate #2 processado
[WebRTC] P2P Data Channel opened
[Network] Conexão P2P estabelecida!
```

## Passo 7: Monitorar com curl

Em outro terminal:

```bash
# Verificar saúde do servidor
curl http://localhost:8080/health

# Ver estatísticas
curl http://localhost:8080/stats

# Esperado:
# {
#   "peers": 2,
#   "activeSessions": 1,
#   "sessions": [
#     {
#       "sessionId": "uuid-1234_...",
#       "hostPeerId": "uuid-1234",
#       "guestPeerId": "uuid-5678",
#       "createdAt": "2024-12-09T..."
#     }
#   ]
# }
```

## Passo 8: Integração no main.cpp

Atualize o main.cpp para suportar WebRTC:

```cpp
#include "RemoteDesktopSystem.h"
#include "WebRTCDataChannel.h"
#include "WebSocketSignalingClient.h"

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv, argv + argc);
    RemoteDesktopSystem system;

    if (args.size() >= 2 && args[1] == "host") {
        std::cout << "🖥️ Modo WebRTC HOST" << std::endl;
        
        std::string signalingServer = "ws://localhost:8080";
        if (args.size() >= 3) {
            signalingServer = "ws://" + args[2];
        }
        
        system.SetUseWebRTC(true);
        system.SetSignalingServerUrl(signalingServer);
        system.SetUseMultiThreading(true);
        system.SetUseEncoding(true);
        system.SetInputEnabled(true);
        
        if (system.InitializeAsWebRTCHost(
            signalingServer,
            {"stun:stun.l.google.com:19302",
             "stun:stun.services.mozilla.com:3478"})) {
            
            std::string sessionId = system.GetWebRTCSessionId();
            std::cout << "\n📱 Session ID (compartilhe com guest):" << std::endl;
            std::cout << "   " << sessionId << std::endl;
            std::cout << "\nAguardando guest conectar..." << std::endl;
            
            system.Run();
        }
    }
    else if (args.size() >= 3 && args[1] == "join") {
        std::string sessionId = args[2];
        std::string signalingServer = "ws://localhost:8080";
        if (args.size() >= 4) {
            signalingServer = "ws://" + args[3];
        }
        
        std::cout << "👁️ Modo WebRTC GUEST" << std::endl;
        std::cout << "   Sessão: " << sessionId << std::endl;
        
        system.SetUseWebRTC(true);
        system.SetSignalingServerUrl(signalingServer);
        system.SetInputEnabled(true);
        
        if (system.InitializeAsWebRTCGuest(signalingServer, sessionId)) {
            system.Run();
        } else {
            std::cerr << "❌ Falha ao conectar" << std::endl;
            return 1;
        }
    }
    else if (args.size() == 1 || args[1] == "loopback") {
        std::cout << "🔄 Modo Loopback (teste local)" << std::endl;
        
        if (system.InitializeLoopback(1920, 1080)) {
            system.Run();
        } else {
            return 1;
        }
    }
    else {
        std::cout << "Uso:" << std::endl;
        std::cout << "  remote_desktop_app host [servidor]     - Iniciar como host" << std::endl;
        std::cout << "  remote_desktop_app join <id> [servidor] - Conectar como guest" << std::endl;
        std::cout << "  remote_desktop_app loopback             - Teste local" << std::endl;
        std::cout << "\nExemplos:" << std::endl;
        std::cout << "  remote_desktop_app host" << std::endl;
        std::cout << "  remote_desktop_app host seu-servidor.com:8080" << std::endl;
        std::cout << "  remote_desktop_app join abc-123-def seu-servidor.com:8080" << std::endl;
    }

    return 0;
}
```

## Passo 9: Teste End-to-End

### Teste Local

```bash
# Terminal 1
npm start

# Terminal 2  
remote_desktop_app host

# Terminal 3
# Copiar o Session ID do Terminal 2
remote_desktop_app join <SESSION_ID>

# Esperado:
# - Terminal 2: "Guest conectou!"
# - Terminal 3: "Conectado ao host!"
# - Vídeo flui via WebRTC P2P
```

### Teste Internet

```bash
# Servidor remoto (AWS/DigitalOcean)
ssh seu-servidor
cd signaling-server && npm start

# Desktop #1 (Host em casa)
remote_desktop_app host seu-servidor.com:8080

# Desktop #2 (Guest em outro lugar)
remote_desktop_app join <SESSION_ID> seu-servidor.com:8080

# Verificar latência:
curl seu-servidor.com:8080/stats
```

## Troubleshooting

### Erro: "libdatachannel.dll not found"

```
Solução:
1. Verificar PATH do Windows inclui pasta bin de libdatachannel
2. Copiar DLL para pasta do executable:
   copy vcpkg/installed/x64-windows/bin/*.dll build/Release/
```

### Erro: "Failed to resolve STUN"

```
Causa: Firewall bloqueando UDP
Solução:
1. Windows Firewall: 
   netsh advfirewall firewall add rule name="WebRTC" \
     dir=out action=allow protocol=UDP

2. Router: Habilitar UPnP ou abrir porta UDP
```

### Conexão muito lenta (usando TURN)

```
Sintoma: RTT > 100ms, logs mostram "TURN relay active"
Solução:
1. Tentar outro STUN server
2. Verificar firewall ISP bloqueando UDP
3. Usar TURN próprio mais próximo geograficamente
4. Testar com https://webrtc.github.io/samples/src/content/peerconnection/trickle-ice/
```

## Performance Esperada com WebRTC

| Métrica | Esperado |
|---------|----------|
| **Latência P2P (STUN)** | 10-25ms |
| **Latência Relay (TURN)** | 30-80ms |
| **Jitter** | <10ms |
| **Packet Loss** | <1% |
| **FPS** | 55-60 |
| **Bandwidth** | 3-5 Mbps (H.264) |

## Checklista de Produção

- [ ] Servidor de sinalização em domínio com SSL/TLS
- [ ] TURN server próprio ou Metered configurado
- [ ] Monitoring e alertas (PM2 Plus ou New Relic)
- [ ] Logs rotacionados (PM2 logs ou ELK Stack)
- [ ] Rate limiting no servidor (max 100 reqs/min)
- [ ] Timeout de inatividade (30 min)
- [ ] Cleanup de sessões antigas (24h)
- [ ] Testes com diferentes ISPs
- [ ] Documentação para deploy
- [ ] Script de rollback rápido

## Próximos Aprimoramentos

1. **Múltiplas sessões simultaneamente**
   - Host com múltiplos guests

2. **Adaptive Quality**
   - Reduzir resolução se latência > 100ms
   - Aumentar quality se RTT < 20ms

3. **Backup UDP direto**
   - Se WebRTC falhar, voltar para UDP LAN
   - Fallback transparente

4. **Analytics**
   - Rastrear taxa de sucesso
   - Monitorar latência média
   - Alertas automáticos

5. **WebRTC Simulcast**
   - Múltiplas bitrates simultaneamente
   - Cliente escolhe melhor adaptação

---

**Tempo Estimado de Implementação**: 4-6 horas  
**Dificuldade**: Médio  
**Recompensa**: Funcionalidade internet verdadeira com NAT traversal! 🌐
