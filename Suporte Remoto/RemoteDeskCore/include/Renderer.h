#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

struct FrameData;

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Inicializa o renderer SDL2 com a janela
    bool Initialize(uint32_t width, uint32_t height, const std::string& title = "Remote Desktop");

    // Atualiza a textura com novos dados de pixels (BGRA)
    bool UpdateFrame(const uint8_t* pixelData, uint32_t width, uint32_t height, uint32_t stride);

    // Renderiza o frame na tela
    bool RenderFrame();

    // Processa eventos de janela (redimensionamento, fechamento, etc)
    bool ProcessEvents();

    // Define o título da janela (útil para mostrar FPS)
    void SetWindowTitle(const std::string& title);

    // Obtém o status da janela
    bool IsRunning() const { return m_isRunning; }

    // Dimensões atuais da janela
    uint32_t GetWindowWidth() const { return m_windowWidth; }
    uint32_t GetWindowHeight() const { return m_windowHeight; }

    // Libera recursos
    void Release();

private:
    bool CreateTextureIfNeeded(uint32_t width, uint32_t height);

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;

    uint32_t m_windowWidth = 0;
    uint32_t m_windowHeight = 0;
    uint32_t m_textureWidth = 0;
    uint32_t m_textureHeight = 0;

    bool m_isRunning = false;
    bool m_vsyncEnabled = true;
};
