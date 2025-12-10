#pragma once

#include <cstdint>
#include <windows.h>

class InputInjector {
public:
    enum class KeyState { PRESSED, RELEASED };
    enum class MouseButton { LEFT, RIGHT, MIDDLE };

    InputInjector();
    ~InputInjector();

    // Inicializa o injetor de entrada
    bool Initialize();

    // ===== Mouse Input =====

    // Move o mouse para coordenada absoluta na tela
    bool MoveMouseAbsolute(int32_t x, int32_t y);

    // Move o mouse relativamente
    bool MoveMouseRelative(int32_t deltaX, int32_t deltaY);

    // Clica com o botão do mouse
    bool MouseClick(MouseButton button, KeyState state);

    // Scroll do mouse (deltaY positivo = cima)
    bool MouseScroll(int32_t deltaY);

    // ===== Keyboard Input =====

    // Injetar tecla (VK_* constants)
    bool InjectKey(uint8_t virtualKeyCode, KeyState state);

    // Injetar texto (simula digitação)
    bool InjectText(const char* text);

    // ===== Special Keys =====

    bool PressKey(uint8_t virtualKeyCode)   { return InjectKey(virtualKeyCode, KeyState::PRESSED); }
    bool ReleaseKey(uint8_t virtualKeyCode) { return InjectKey(virtualKeyCode, KeyState::RELEASED); }

    bool PressCtrl()  { return PressKey(VK_CONTROL); }
    bool ReleaseCtrl() { return ReleaseKey(VK_CONTROL); }

    bool PressAlt()   { return PressKey(VK_MENU); }
    bool ReleaseAlt() { return ReleaseKey(VK_MENU); }

    bool PressShift() { return PressKey(VK_SHIFT); }
    bool ReleaseShift() { return ReleaseKey(VK_SHIFT); }

    // ===== Clipboard =====

    // Copia texto para clipboard
    bool CopyToClipboard(const char* text);

    // Obtém texto do clipboard
    bool GetFromClipboard(std::string& outText);

    // ===== Configuration =====

    // Define atraso entre inputs (para evitar taxa muito rápida)
    void SetInputDelay(uint32_t delayMs) { m_inputDelayMs = delayMs; }

    // Obtém mouse position (para validação)
    bool GetMousePosition(int32_t& outX, int32_t& outY) const;

    // Libera recursos
    void Release();

private:
    bool SendKeyboardInput(uint8_t virtualKeyCode, bool isKeyDown);
    bool SendMouseInput(int32_t x, int32_t y, uint32_t flags);

    bool m_initialized = false;
    uint32_t m_inputDelayMs = 0;
};
