cmake_minimum_required(VERSION 3.20)
project(RemoteDesktopCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Adicionar diretório de includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Encontrar pacotes
find_package(SDL2 REQUIRED)
find_package(d3d11 REQUIRED)
find_package(dxgi REQUIRED)

# ============== Fonte da aplicação ==============
set(SOURCES
    src/main.cpp
    src/capture/DXGICapturer.cpp
    src/render/Renderer.cpp
    src/network/P2PManager.cpp
    src/network/NVENCEncoder.cpp
    src/network/OptimizationLayer.cpp
    src/network/RemoteDesktopSystem.cpp
    src/input/InputInjector.cpp
)

set(HEADERS
    include/DXGICapturer.h
    include/Renderer.h
    include/P2PManager.h
    include/NVENCEncoder.h
    include/OptimizationLayer.h
    include/InputInjector.h
    include/RemoteDesktopSystem.h
)

# Criar executável
add_executable(remote_desktop_app ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(remote_desktop_app PRIVATE
    SDL2::SDL2
    d3d11
    dxgi
    dxguid
    uuid
    ws2_32
)

# Windows-specific settings
if(MSVC)
    target_compile_options(remote_desktop_app PRIVATE /W4 /permissive- /EHsc)
    # Ativar suporte para RAII e otimizações
    target_compile_options(remote_desktop_app PRIVATE /O2 /Oi /Ot)
else()
    target_compile_options(remote_desktop_app PRIVATE -Wall -Wextra -Wpedantic -O3)
endif()

# Configuração de debug
if(MSVC)
    target_compile_options(remote_desktop_app PRIVATE $<$<CONFIG:Debug>:/Zi>)
    target_link_options(remote_desktop_app PRIVATE $<$<CONFIG:Debug>:/DEBUG>)
endif()

message(STATUS "Remote Desktop Core - Build Configuration")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  SDL2: Found")
message(STATUS "  Direct3D 11: Found")
message(STATUS "  DXGI: Found")
