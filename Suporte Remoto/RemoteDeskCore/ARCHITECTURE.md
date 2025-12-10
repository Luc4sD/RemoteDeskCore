# Documentação Técnica - Remote Desktop Core

## Arquitetura do Sistema

### Visão Geral

```
┌─────────────────────────────────────────────────────────────┐
│                    Main Loop (main.cpp)                     │
│  • Orquestra captura e renderização                        │
│  • Coleta de performance metrics                           │
│  • Controle de eventos                                      │
└──────────────┬──────────────────────────┬──────────────────┘
               │                          │
        ┌──────▼──────────┐        ┌─────▼──────────┐
        │  DXGICapturer   │        │   Renderer     │
        │  ───────────────│        │  ────────────  │
        │ • GPU Capture   │        │ • SDL2 Window  │
        │ • Non-blocking  │        │ • Stream Tex   │
        │ • BGRA Format   │        │ • Input Events │
        │ • Auto-Retry    │        │ • Scaling      │
        └────────────────┘        └────────────────┘
```

### Fluxo de Dados

```
GPU Display Buffer
       │
       ▼
[DXGI Desktop Duplication]
       │ (IDXGIOutputDuplication::AcquireNextFrame)
       │
       ▼
GPU Texture (D3D11Texture2D)
       │
       ▼
[Staging Texture] ◄─ CPU Read Access
       │ (CopyResource → Map)
       │
       ▼
System Memory (std::vector<uint8_t>)
  BGRA 32-bit format
       │
       ▼
[SDL2 Texture] ◄─ SDL_TEXTUREACCESS_STREAMING
       │ (UpdateFrame)
       │
       ▼
Display Window ◄─ Rendered/Scaled
       │ (RenderCopy → RenderPresent)
```

## Componentes Principais

### 1. DXGICapturer

**Responsabilidade**: Capturar frames da GPU usando DXGI Desktop Duplication

#### Inicialização
```cpp
1. Create D3D11 Device
   └─ D3D11CreateDevice()
   
2. Get DXGI Adapter
   └─ IDXGIDevice::GetAdapter()
   
3. Get Display Output
   └─ IDXGIAdapter::EnumOutputs()
   
4. Create Desktop Duplication
   └─ IDXGIOutput1::DuplicateOutput()
   
5. Create Staging Texture
   └─ D3D11Device::CreateTexture2D()
       • Usage: D3D11_USAGE_STAGING
       • CPUAccess: D3D11_CPU_ACCESS_READ
```

#### Fluxo de Captura
```cpp
AcquireFrame():
  │
  ├─ IDXGIOutputDuplication::AcquireNextFrame(100ms timeout)
  │  ├─ DXGI_ERROR_WAIT_TIMEOUT → hasChanged = false (normal)
  │ └─ DXGI_ERROR_ACCESS_LOST → Reinitialize
  │
  ├─ ID3D11DeviceContext::CopyResource()
  │  └─ GPU Texture → Staging Texture
  │
  ├─ ID3D11DeviceContext::Map()
  │  └─ GPU Memory → CPU Accessible
  │
  ├─ std::memcpy() → FrameData.pixels
  │
  └─ ReleaseFrame()
```

#### Características de Performance

| Aspecto | Valor |
|--------|-------|
| Latência de captura | 1-2ms |
| Memória buffer | ~8.3 MB (1920x1080 BGRA) |
| Timeout inatividade | 100ms |
| Format pixel | BGRA 32-bit |

#### Casos de Erro

| Erro | Ação |
|------|------|
| `DXGI_ERROR_WAIT_TIMEOUT` | Retornar `hasChanged=false` (normal) |
| `DXGI_ERROR_ACCESS_LOST` | Reinitializar desktop duplication |
| `E_FAIL` em CopyResource | Retornar false |
| Device D3D11 inválido | Retornar false |

### 2. Renderer

**Responsabilidade**: Renderizar frames usando SDL2 com performance otimizada

#### Inicialização
```cpp
1. SDL_Init(SDL_INIT_VIDEO)
   └─ Inicializar subsistema de vídeo
   
2. SDL_CreateWindow()
   └─ Flags: SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
   
3. SDL_CreateRenderer()
   └─ Flags: SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
   
4. SDL_CreateTexture()
   └─ Format: SDL_PIXELFORMAT_ARGB8888
   └─ Access: SDL_TEXTUREACCESS_STREAMING
```

#### Pipeline de Renderização
```cpp
RenderFrame():
  │
  ├─ SDL_SetRenderDrawColor() → Black
  ├─ SDL_RenderClear()
  │
  ├─ SDL_RenderCopy()
  │  ├─ Src: Texture (original size)
  │  └─ Dst: Window (scaled)
  │
  └─ SDL_RenderPresent()
     └─ V-Sync (se habilitado)
```

#### Streaming Texture Update
```cpp
UpdateFrame(pixelData, width, height, stride):
  │
  ├─ CreateTextureIfNeeded()
  │  └─ Recreate se tamanho mudou
  │
  ├─ SDL_LockTexture()
  │  └─ Obter acesso para escrita
  │
  ├─ std::memcpy() → BGRA conversion
  │  └─ CPU → GPU texture memory
  │
  └─ SDL_UnlockTexture()
     └─ GPU texture ready para render
```

#### Processamento de Eventos
```cpp
ProcessEvents():
  │
  ├─ SDL_QUIT → m_isRunning = false
  ├─ WINDOWEVENT_RESIZED → Atualizar window size
  ├─ WINDOWEVENT_CLOSE → m_isRunning = false
  └─ KEYDOWN (ESC) → m_isRunning = false
```

### 3. Main Loop

**Responsabilidade**: Orquestrar captura/renderização e coleta de metrics

#### Ciclo Principal
```cpp
while(running):
    │
    ├─ [1] ProcessEvents()
    │      └─ Handle input, resize, quit
    │
    ├─ [2] AcquireFrame()
    │      ├─ Capture from DXGI
    │      └─ Get FrameData
    │
    ├─ [3] IF hasChanged:
    │      │  UpdateFrame()
    │      │  └─ Update SDL2 texture
    │      └─ Else: Sleep 1ms
    │
    ├─ [4] RenderFrame()
    │      └─ Present to display
    │
    ├─ [5] RecordPerformance()
    │      ├─ Calculate delta time
    │      ├─ Update FPS counter
    │      └─ Set window title
    │
    └─ [6] Sync
           └─ V-Sync automatic com SDL2
```

#### Performance Monitoring
```cpp
PerformanceCounter:
  │
  ├─ Janela móvel de 120 frames
  ├─ Calcula FPS a cada 30 frames
  └─ Atualiza título a cada 60 frames
```

## Detalhes de Implementação

### Format de Pixel: BGRA 32-bit

```
Byte Order (Little Endian - Windows):
┌──┬──┬──┬──┐
│ B │ G │ R │ A │
└──┴──┴──┴──┘
 0   1   2   3

Exemplo: Vermelho puro
BGRA: [0x00, 0x00, 0xFF, 0xFF]
```

### Otimizações RAII (Resource Acquisition Is Initialization)

```cpp
// ComPtr (WRL) para COM objects
ComPtr<ID3D11Device> m_device;
// Destrutor automático → Device::Release()

// std::unique_ptr para objetos C++
std::unique_ptr<DXGICapturer> capturer;
// Destrutor automático → ~DXGICapturer()

// std::vector para buffer dinâmico
std::vector<uint8_t> m_frameBuffer;
// Destrutor automático → desalocar memória
```

### Tratamento de Redimensionamento de Janela

```
[User resizes window]
         │
         ▼
ProcessEvents() detects SDL_WINDOWEVENT_RESIZED
         │
         ▼
Store m_windowWidth, m_windowHeight
         │
         ▼
RenderFrame() calcula novo SDL_Rect (dst)
         │
         ▼
SDL_RenderCopy escala texture para novo tamanho
```

### Detecção de Mudanças de Tela

```
DXGI Desktop Duplication usa copy-on-write:
  │
  ├─ Se tela não mudou: AcquireNextFrame() timeout
  │   └─ Retorna DXGI_ERROR_WAIT_TIMEOUT (normal)
  │
  └─ Se tela mudou: AcquireNextFrame() retorna frame
      └─ Retorna S_OK com novos pixels
```

## Métricas de Performance Esperadas

### Latência (End-to-End)
```
Captura DXGI:     2-4ms
Transfer CPU:     0.5-1ms
Atualizar Tex:    0.5-1ms
Render SDL2:      1-2ms
V-Sync:          16-17ms (60 FPS)
─────────────────────────
Total:           20-25ms (típico)
```

### Uso de Recursos
```
CPU:   5-15% (1 core)
GPU:   1-5% (mainly copy ops)
RAM:   ~100 MB (buffer + SDL2)
VRAM:  ~50 MB (textures)
```

### Escalabilidade
```
Resolução | FPS | Latência | CPU
──────────┼─────┼──────────┼─────
1280x720  | 60  | 18ms     | 5%
1920x1080 | 60  | 20ms     | 8%
2560x1440 | 55  | 22ms     | 12%
3840x2160 | 40  | 30ms     | 18%
```

## Próximos Passos - Roadmap

### Fase 2: Rede P2P
```
P2PManager (UDP):
  ├─ Serialize frame (encapsular FrameData)
  ├─ UDP packet transmission
  ├─ Packet loss detection & retry
  └─ Latency measurement
```

### Fase 3: Codec de Vídeo
```
VideoEncoder (NVENC):
  ├─ NVENC H.264 encoding
  ├─ Adaptive bitrate
  ├─ KeyFrame injection
  └─ Packet fragmentation
```

### Fase 4: Input Injection
```
InputInjector:
  ├─ Mouse events (absolute positioning)
  ├─ Keyboard events (key injection)
  └─ Clipboard sync (futuro)
```

## Debugging e Troubleshooting

### Habilitar Debug Output
```cpp
// Em DXGICapturer.cpp
OutputDebugStringA("DXGI Debug: ...");

// Visualizar com DebugView (SysInternals)
// Ou: Output window no Visual Studio (Debug > Windows > Output)
```

### Performance Profiling
```
Windows Performance Analyzer (WPA):
  ├─ CPU Usage
  ├─ Disk I/O
  └─ GPU Activity

Visual Studio Profiler:
  ├─ CPU Sampling
  ├─ Memory Allocation
  └─ Frame Rate Analysis
```

### Common Issues

| Problema | Causa | Solução |
|----------|-------|---------|
| Baixo FPS | GPU congestionada | Reduzir resolução |
| Jitter | V-Sync desabilitado | Ativar V-Sync |
| Crash ao resize | Texture não recriada | CheckFrameSize() |
| Memory leak | COM object não liberado | Usar ComPtr |
| Preto na tela | SDL2 texture vazia | Verificar UpdateFrame |

---

**Autor**: Lucas D. - Engenheiro de Software Sênior
**Versão**: 1.0.0
**Data**: 2025-12-09
