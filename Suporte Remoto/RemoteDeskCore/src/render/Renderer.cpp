#include "Renderer.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <stdexcept>

Renderer::Renderer() 
    : m_window(nullptr), m_renderer(nullptr), m_texture(nullptr), 
      m_windowWidth(0), m_windowHeight(0), m_isRunning(false) {
}

Renderer::~Renderer() {
    Release();
}

bool Renderer::Initialize(uint32_t width, uint32_t height, const std::string& title) {
    try {
        // Inicializar SDL2
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }

        // Criar janela
        m_window = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width,
            height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );

        if (!m_window) {
            throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        }

        // Criar renderer com aceleração de hardware
        m_renderer = SDL_CreateRenderer(
            m_window,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );

        if (!m_renderer) {
            throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        }

        m_windowWidth = width;
        m_windowHeight = height;
        m_isRunning = true;
        m_vsyncEnabled = true;

        // Criar texture inicial
        if (!CreateTextureIfNeeded(width, height)) {
            throw std::runtime_error("Failed to create initial texture");
        }

        SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
        SDL_RenderClear(m_renderer);
        SDL_RenderPresent(m_renderer);

        return true;
    } catch (const std::exception& e) {
        OutputDebugStringA("Renderer::Initialize - Exception: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        Release();
        return false;
    }
}

bool Renderer::CreateTextureIfNeeded(uint32_t width, uint32_t height) {
    // Se a texture já existe com o tamanho certo, não fazer nada
    if (m_texture && m_textureWidth == width && m_textureHeight == height) {
        return true;
    }

    // Destruir texture anterior se existir
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }

    // Criar nova texture com acesso streaming (para atualizações rápidas)
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_ARGB8888,  // BGRA (32 bits por pixel)
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );

    if (!m_texture) {
        OutputDebugStringA("SDL_CreateTexture failed: ");
        OutputDebugStringA(SDL_GetError());
        OutputDebugStringA("\n");
        return false;
    }

    m_textureWidth = width;
    m_textureHeight = height;

    return true;
}

bool Renderer::UpdateFrame(const uint8_t* pixelData, uint32_t width, uint32_t height, uint32_t stride) {
    if (!m_renderer || !pixelData) {
        return false;
    }

    // Recriar texture se o tamanho mudou
    if (!CreateTextureIfNeeded(width, height)) {
        return false;
    }

    // Atualizar a texture com novos dados
    // SDL2 usa ARGB8888, e os dados chegam em BGRA, então precisamos fazer a conversão
    void* pixels = nullptr;
    int pitch = 0;

    if (SDL_LockTexture(m_texture, nullptr, &pixels, &pitch) != 0) {
        OutputDebugStringA("SDL_LockTexture failed: ");
        OutputDebugStringA(SDL_GetError());
        OutputDebugStringA("\n");
        return false;
    }

    // Copiar dados de pixels
    // Como ambos são BGRA de 32 bits, podemos copiar diretamente
    uint8_t* dstData = static_cast<uint8_t*>(pixels);
    for (uint32_t y = 0; y < height; ++y) {
        std::memcpy(
            dstData + (y * pitch),
            pixelData + (y * stride),
            width * 4  // 4 bytes por pixel (BGRA)
        );
    }

    SDL_UnlockTexture(m_texture);

    return true;
}

bool Renderer::RenderFrame() {
    if (!m_renderer || !m_texture) {
        return false;
    }

    // Limpar a tela com preto
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);

    // Renderizar texture escalada para caber na janela
    SDL_Rect src = { 0, 0, static_cast<int>(m_textureWidth), static_cast<int>(m_textureHeight) };
    SDL_Rect dst = { 0, 0, static_cast<int>(m_windowWidth), static_cast<int>(m_windowHeight) };

    if (SDL_RenderCopy(m_renderer, m_texture, &src, &dst) != 0) {
        OutputDebugStringA("SDL_RenderCopy failed: ");
        OutputDebugStringA(SDL_GetError());
        OutputDebugStringA("\n");
        return false;
    }

    // Apresentar frame na tela
    SDL_RenderPresent(m_renderer);

    return true;
}

bool Renderer::ProcessEvents() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            m_isRunning = false;
            return false;

        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_SIZE_CHANGED:
                m_windowWidth = event.window.data1;
                m_windowHeight = event.window.data2;
                break;

            case SDL_WINDOWEVENT_CLOSE:
                m_isRunning = false;
                return false;

            default:
                break;
            }
            break;

        case SDL_KEYDOWN:
            // ESC para fechar
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                m_isRunning = false;
                return false;
            }
            break;

        default:
            break;
        }
    }

    return true;
}

void Renderer::SetWindowTitle(const std::string& title) {
    if (m_window) {
        SDL_SetWindowTitle(m_window, title.c_str());
    }
}

void Renderer::Release() {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }

    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }

    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }

    SDL_Quit();

    m_isRunning = false;
}
