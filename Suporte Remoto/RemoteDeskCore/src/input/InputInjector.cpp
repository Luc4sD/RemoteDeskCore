#include "InputInjector.h"
#include <thread>
#include <cstring>

InputInjector::InputInjector() {
}

InputInjector::~InputInjector() {
    Release();
}

bool InputInjector::Initialize() {
    // Verificar se estamos rodando com privilégios administrativos
    BOOL isAdmin = FALSE;
    HANDLE tokenHandle = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)) {
        TOKEN_ELEVATION elevation;
        DWORD elevationSize = sizeof(TOKEN_ELEVATION);

        if (GetTokenInformation(tokenHandle, TokenElevation, &elevation, elevationSize, &elevationSize)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(tokenHandle);
    }

    if (!isAdmin) {
        OutputDebugStringA("Warning: InputInjector works best with administrator privileges\n");
    }

    m_initialized = true;
    return true;
}

bool InputInjector::SendKeyboardInput(uint8_t virtualKeyCode, bool isKeyDown) {
    if (!m_initialized) {
        return false;
    }

    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtualKeyCode;
    input.ki.dwFlags = isKeyDown ? 0 : KEYEVENTF_KEYUP;
    input.ki.time = 0;

    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
        OutputDebugStringA("SendInput (keyboard) failed\n");
        return false;
    }

    if (m_inputDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_inputDelayMs));
    }

    return true;
}

bool InputInjector::SendMouseInput(int32_t x, int32_t y, uint32_t flags) {
    if (!m_initialized) {
        return false;
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = x;
    input.mi.dy = y;
    input.mi.dwFlags = flags | MOUSEEVENTF_ABSOLUTE;
    input.mi.time = 0;

    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
        OutputDebugStringA("SendInput (mouse) failed\n");
        return false;
    }

    if (m_inputDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_inputDelayMs));
    }

    return true;
}

bool InputInjector::InjectKey(uint8_t virtualKeyCode, KeyState state) {
    bool isKeyDown = (state == KeyState::PRESSED);
    return SendKeyboardInput(virtualKeyCode, isKeyDown);
}

bool InputInjector::InjectText(const char* text) {
    if (!text || !m_initialized) {
        return false;
    }

    for (const char* p = text; *p != '\0'; ++p) {
        char c = *p;

        // Suportar caracteres ASCII básicos
        // Para caracteres especiais, usar ShiftState appropriadamente
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[1].type = INPUT_KEYBOARD;

        // Determinar se precisa de Shift
        bool needsShift = isupper(c) || strchr("!@#$%^&*()", c) != nullptr;

        if (needsShift) {
            inputs[0].ki.wVk = VK_SHIFT;
            inputs[0].ki.dwFlags = 0;

            inputs[1].ki.wVk = VkKeyScanA(c) & 0xFF;
            inputs[1].ki.dwFlags = 0;

            if (SendInput(2, inputs, sizeof(INPUT)) == 0) {
                return false;
            }

            // Release shift
            inputs[0].ki.dwFlags = KEYEVENTF_KEYUP;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(2, inputs, sizeof(INPUT));
        } else {
            inputs[0].ki.wVk = VkKeyScanA(c) & 0xFF;
            inputs[0].ki.dwFlags = 0;

            inputs[1].ki.wVk = VkKeyScanA(c) & 0xFF;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

            if (SendInput(2, inputs, sizeof(INPUT)) == 0) {
                return false;
            }
        }

        if (m_inputDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_inputDelayMs));
        }
    }

    return true;
}

bool InputInjector::MoveMouseAbsolute(int32_t x, int32_t y) {
    // Converter para coordenadas normalizadas (0-65535)
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    uint32_t normalizedX = (x * 65535) / screenWidth;
    uint32_t normalizedY = (y * 65535) / screenHeight;

    return SendMouseInput(normalizedX, normalizedY, MOUSEEVENTF_MOVE);
}

bool InputInjector::MoveMouseRelative(int32_t deltaX, int32_t deltaY) {
    if (!m_initialized) {
        return false;
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = deltaX;
    input.mi.dy = deltaY;
    input.mi.dwFlags = MOUSEEVENTF_MOVE; // Sem MOUSEEVENTF_ABSOLUTE

    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
        return false;
    }

    if (m_inputDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_inputDelayMs));
    }

    return true;
}

bool InputInjector::MouseClick(MouseButton button, KeyState state) {
    if (!m_initialized) {
        return false;
    }

    uint32_t flags = 0;
    bool isDown = (state == KeyState::PRESSED);

    switch (button) {
    case MouseButton::LEFT:
        flags = isDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        break;
    case MouseButton::RIGHT:
        flags = isDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        break;
    case MouseButton::MIDDLE:
        flags = isDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        break;
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;

    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
        return false;
    }

    if (m_inputDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_inputDelayMs));
    }

    return true;
}

bool InputInjector::MouseScroll(int32_t deltaY) {
    if (!m_initialized) {
        return false;
    }

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.mouseData = static_cast<DWORD>(deltaY);
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;

    if (SendInput(1, &input, sizeof(INPUT)) == 0) {
        return false;
    }

    if (m_inputDelayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_inputDelayMs));
    }

    return true;
}

bool InputInjector::GetMousePosition(int32_t& outX, int32_t& outY) const {
    POINT point;
    if (!GetCursorPos(&point)) {
        return false;
    }

    outX = point.x;
    outY = point.y;
    return true;
}

bool InputInjector::CopyToClipboard(const char* text) {
    if (!text) {
        return false;
    }

    size_t len = strlen(text) + 1;
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, len);

    if (!hGlobal) {
        return false;
    }

    void* pData = GlobalLock(hGlobal);
    if (!pData) {
        GlobalFree(hGlobal);
        return false;
    }

    std::memcpy(pData, text, len);
    GlobalUnlock(hGlobal);

    if (!OpenClipboard(nullptr)) {
        GlobalFree(hGlobal);
        return false;
    }

    EmptyClipboard();
    SetClipboardData(CF_TEXT, hGlobal);
    CloseClipboard();

    return true;
}

bool InputInjector::GetFromClipboard(std::string& outText) {
    if (!OpenClipboard(nullptr)) {
        return false;
    }

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData) {
        CloseClipboard();
        return false;
    }

    const char* text = static_cast<const char*>(GlobalLock(hData));
    if (text) {
        outText = text;
        GlobalUnlock(hData);
    }

    CloseClipboard();
    return true;
}

void InputInjector::Release() {
    m_initialized = false;
}
