# Remote Desktop Core - Alta Performance P2P

Software de acesso remoto de baixa latência focado em performance ultra-rápida (<20ms).

## Stack Tecnológico

- **Linguagem**: C++17/20 (Modern C++)
- **SO Alvo**: Windows 10/11
- **Captura de Tela**: DXGI Desktop Duplication (GPU direto)
- **Renderização**: SDL2
- **Rede**: UDP Sockets (futuro WebRTC/QUIC)
- **Codec**: H.264 NVENC (futuro)

## Estrutura do Projeto

```
RemoteDeskCore/
├── CMakeLists.txt
├── include/
│   ├── DXGICapturer.h      # Interface de captura DXGI
│   └── Renderer.h          # Interface de renderização SDL2
├── src/
│   ├── main.cpp            # Loop principal e testes
│   ├── capture/
│   │   └── DXGICapturer.cpp    # Implementação DXGI
│   ├── render/
│   │   └── Renderer.cpp        # Implementação SDL2
│   ├── network/            # (Futuro) P2PManager
│   └── input/              # (Futuro) InputInjector
└── README.md
```

## Pré-requisitos

### Windows 10/11
- Visual Studio 2019+ ou MinGW com suporte C++17/20
- CMake 3.20+
- SDK do Windows (para DirectX)
- SDL2 development libraries

### Instalação de Dependências

#### Via vcpkg (Recomendado)
```bash
# Clone e instale vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Instale SDL2
.\vcpkg install sdl2:x64-windows

# Configure CMake para usar vcpkg
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake ..
```

#### Via Chocolatey
```bash
choco install sdl2 cmake
```

#### Manual (SDL2)
1. Download de https://www.libsdl.org/download-2.0.php
2. Extrair em `C:\SDL2`
3. Configurar CMake com `-DSDL2_DIR=C:\SDL2`

## Compilação

### Windows (Visual Studio)

```bash
cd RemoteDeskCore
mkdir build
cd build

# Gerar arquivos do Visual Studio
cmake -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ..

# Compilar
cmake --build . --config Release

# Executável gerado
# build/Release/remote_desktop_app.exe
```

### Windows (Command Line com MSVC)

```bash
cd RemoteDeskCore
mkdir build
cd build

cmake -G "Ninja" ..
ninja

# Ou
cmake --build . --config Release
```

### Windows (MinGW)

```bash
cd RemoteDeskCore
mkdir build
cd build

cmake -G "MinGW Makefiles" ..
mingw32-make
```

## Execução

### Teste de Loopback Local

O `main.cpp` implementa um teste de loopback que captura a tela e a renderiza em tempo real:

```bash
./build/Release/remote_desktop_app.exe
```

**Controles:**
- ESC: Sair
- Redimensionar janela: Suportado automaticamente

**Monitoramento:**
- Título mostra FPS em tempo real
- Console mostra estatísticas de performance
- Rastreia frames capturados, alterados e ignorados

### Output Esperado

```
=== Remote Desktop - Local Loopback Test ===
Initializing DXGI Capturer and SDL2 Renderer...
Screen size: 1920x1080
Renderer initialized successfully
Starting capture loop... (Press ESC to exit)
------------------------------------------------------------
Remote Desktop - Loopback Test | FPS: 59.8 | Frame: 60 | Changed: 18 | Screen: 1920x1080
Remote Desktop - Loopback Test | FPS: 59.9 | Frame: 300 | Changed: 87 | Screen: 1920x1080
...
------------------------------------------------------------

=== Final Statistics ===
Total frames captured: 1500
Changed frames: 425
Skipped frames: 0
Average FPS: 59.9
Shutting down...
Done!
```

## Detalhes de Implementação

### DXGICapturer

Implementa captura de tela via DXGI Desktop Duplication:

- **Initialize()**: Configura device D3D11, DXGI Output e staging texture
- **AcquireFrame()**: Captura frame não-bloqueante com timeout
- **Formato**: BGRA 32-bit (4 bytes por pixel)
- **Detecção de mudanças**: Usando DXGI timeout (100ms) para detectar quando a tela não mudou
- **Retry automático**: Se a sessão for perdida, reinitializa automaticamente

### Renderer

Implementa renderização via SDL2 com streaming texture:

- **Initialize()**: Cria janela SDL2 redimensionável com renderer acelerado
- **UpdateFrame()**: Atualiza texture streaming (SDL_TEXTUREACCESS_STREAMING) para máxima performance
- **RenderFrame()**: Renderiza com scale automático para o tamanho da janela
- **V-Sync**: Ativado por padrão para evitar tearing
- **ProcessEvents()**: Processa redimensionamentos e fechamento de janela

### Main Loop

```cpp
while(running) {
    // 1. Processar eventos de janela
    renderer->ProcessEvents();
    
    // 2. Capturar frame DXGI
    capturer->AcquireFrame(frameData);
    
    // 3. Atualizar texture SDL2 (se houver mudança)
    renderer->UpdateFrame(frameData.pixels, ...);
    
    // 4. Renderizar frame
    renderer->RenderFrame();
    
    // 5. Registrar performance
    perfCounter.RecordFrame(deltaTime);
}
```

**Otimizações implementadas:**
- Captura não-bloqueante (timeout 100ms)
- Skips automáticos quando tela não muda
- Texture streaming para update rápido
- Buffer pré-alocado para evitar realocações
- RAII para gerenciamento automático de recursos

## Performance

### Benchmarks Esperados

| Métrica | Esperado |
|---------|----------|
| FPS | 55-60 (V-Sync on) |
| Latência por frame | 16-18ms |
| Uso de CPU | 5-15% |
| Uso de memória | 50-100MB |
| Tempo de captura | 2-4ms |
| Tempo de render | 1-2ms |

### Dicas de Performance

1. **V-Sync**: Desabilitar em `Renderer::Initialize()` se quiser >60FPS
2. **Resolução**: Reduzir resolução de captura (crop) para casos de menor bandwidth
3. **Codec**: Implementar H.264 com NVENC para compressão
4. **Rede**: Usar UDP com packet loss recovery

## Próximos Passos

1. **Módulo de Rede** (`/src/network/P2PManager.cpp`)
   - UDP socket communication
   - Serialização de frames
   - Packet loss handling

2. **Módulo de Input** (`/src/input/InputInjector.cpp`)
   - SendInput para mouse/teclado
   - Injeção remota de eventos

3. **Codec de Vídeo**
   - NVENC H.264 encoding
   - Adaptive bitrate control

4. **Otimizações Avançadas**
   - Multi-threading para captura/render separado
   - WebRTC para NAT traversal
   - QUIC para latência ultra-baixa

## Troubleshooting

### "DXGI Capturer initialization failed"
- Verificar se GPU suporta Desktop Duplication (usualmente todas as GPU modernas)
- Verificar DirectX 11 installation

### "SDL_CreateWindow failed"
- Verificar se SDL2 está instalada corretamente
- Verificar CMAKE_TOOLCHAIN_FILE se usando vcpkg

### Baixo FPS
- Verificar resolução da tela (1920x1080 é baseline)
- Verificar se V-Sync está interferindo
- Monitorar CPU/GPU com perfmon

### Latência alta
- Latência DXGI é tipicamente 16ms (1 frame)
- Latência SDL2 render é <2ms
- Latência de rede (futuro) será o principal gargalo

## Arquitetura de Segurança (Futuro)

- Criptografia TLS para comunicação
- Autenticação via certificados
- Rate limiting de input injection
- Audit logging de conexões

## Licença

Proprietary - Desenvolvimento interno

## Contato / Suporte

Lucas D. - Engenheiro de Software Sênior
"# RemoteDeskCore" 
