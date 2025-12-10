#include <windows.h>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "DXGICapturer.h"
#include "Renderer.h"

class PerformanceCounter {
public:
    PerformanceCounter(size_t windowSize = 60)
        : m_windowSize(windowSize), m_frameCount(0), m_fps(0) {
    }

    void RecordFrame(double deltaMs) {
        m_frameTimes.push_back(deltaMs);
        if (m_frameTimes.size() > m_windowSize) {
            m_frameTimes.pop_front();
        }

        m_frameCount++;

        // Calcular FPS a cada 30 frames
        if (m_frameCount % 30 == 0) {
            CalculateFPS();
        }
    }

    double GetFPS() const { return m_fps; }

    void CalculateFPS() {
        if (m_frameTimes.empty()) {
            m_fps = 0.0;
            return;
        }

        double totalMs = 0.0;
        for (double dt : m_frameTimes) {
            totalMs += dt;
        }

        double avgFrameMs = totalMs / m_frameTimes.size();
        m_fps = avgFrameMs > 0.0 ? 1000.0 / avgFrameMs : 0.0;
    }

private:
    std::deque<double> m_frameTimes;
    size_t m_windowSize;
    size_t m_frameCount;
    double m_fps;
};

int main(int argc, char* argv[]) {
    std::cout << "=== Remote Desktop - Local Loopback Test ===" << std::endl;
    std::cout << "Initializing DXGI Capturer and SDL2 Renderer..." << std::endl;

    // Criar capturer DXGI
    auto capturer = std::make_unique<DXGICapturer>();
    if (!capturer->Initialize()) {
        std::cerr << "ERROR: Failed to initialize DXGI Capturer" << std::endl;
        return -1;
    }

    uint32_t screenWidth = capturer->GetScreenWidth();
    uint32_t screenHeight = capturer->GetScreenHeight();

    std::cout << "Screen size: " << screenWidth << "x" << screenHeight << std::endl;

    // Criar renderer SDL2
    auto renderer = std::make_unique<Renderer>();
    if (!renderer->Initialize(screenWidth, screenHeight, "Remote Desktop - Loopback Test")) {
        std::cerr << "ERROR: Failed to initialize Renderer" << std::endl;
        return -1;
    }

    std::cout << "Renderer initialized successfully" << std::endl;
    std::cout << "Starting capture loop... (Press ESC to exit)" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    PerformanceCounter perfCounter(120);
    FrameData frameData;

    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    uint64_t totalFrames = 0;
    uint64_t changedFrames = 0;
    uint64_t skippedFrames = 0;

    // Main loop
    while (renderer->IsRunning()) {
        auto frameStartTime = std::chrono::high_resolution_clock::now();

        // Processar eventos da janela
        if (!renderer->ProcessEvents()) {
            break;
        }

        // Capturar frame
        if (!capturer->AcquireFrame(frameData)) {
            skippedFrames++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        totalFrames++;

        // Se a tela mudou, atualizar texture e renderizar
        if (frameData.hasChanged) {
            changedFrames++;

            // Atualizar texture SDL2
            if (!renderer->UpdateFrame(frameData.pixels.data(), frameData.width, 
                                       frameData.height, frameData.stride)) {
                std::cerr << "ERROR: Failed to update frame" << std::endl;
                break;
            }
        }

        // Renderizar
        if (!renderer->RenderFrame()) {
            std::cerr << "ERROR: Failed to render frame" << std::endl;
            break;
        }

        // Calcular timing
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        double frameDeltaMs = std::chrono::duration<double, std::milli>(
            frameEndTime - frameStartTime
        ).count();

        perfCounter.RecordFrame(frameDeltaMs);

        // Atualizar título com FPS a cada 60 frames
        if (totalFrames % 60 == 0) {
            std::ostringstream titleStream;
            titleStream << "Remote Desktop - Loopback Test | FPS: " 
                       << std::fixed << std::setprecision(1) << perfCounter.GetFPS()
                       << " | Frame: " << totalFrames
                       << " | Changed: " << changedFrames
                       << " | Screen: " << screenWidth << "x" << screenHeight;
            
            renderer->SetWindowTitle(titleStream.str());

            // Imprimir stats no console a cada 300 frames
            if (totalFrames % 300 == 0) {
                std::cout << titleStream.str() << std::endl;
            }
        }

        // Controle de CPU - se não há mudanças na tela, reduzir frequência de captura
        if (!frameData.hasChanged) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Cleanup
    std::cout << std::string(60, '-') << std::endl;
    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Total frames captured: " << totalFrames << std::endl;
    std::cout << "Changed frames: " << changedFrames << std::endl;
    std::cout << "Skipped frames: " << skippedFrames << std::endl;
    std::cout << "Average FPS: " << std::fixed << std::setprecision(1) << perfCounter.GetFPS() << std::endl;
    std::cout << "Shutting down..." << std::endl;

    renderer->Release();
    capturer->Release();

    std::cout << "Done!" << std::endl;
    return 0;
}
