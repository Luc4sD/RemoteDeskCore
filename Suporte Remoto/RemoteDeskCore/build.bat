@echo off
REM Build Script para Remote Desktop Core
REM Uso: build.bat [debug|release] [clean]

setlocal enabledelayedexpansion

set BUILD_TYPE=%1
if "!BUILD_TYPE!"=="" set BUILD_TYPE=release

set CLEAN=%2
if "!CLEAN!"=="clean" (
    echo Limpando build anterior...
    rmdir /s /q build 2>nul
)

echo.
echo ===============================================
echo Remote Desktop Core - Build Script
echo ===============================================
echo Build Type: %BUILD_TYPE%
echo.

REM Verificar se CMake está instalado
where cmake >nul 2>&1
if !errorlevel! neq 0 (
    echo ERROR: CMake não encontrado. Instale CMake primeiro.
    exit /b 1
)

REM Criar diretório de build
if not exist build mkdir build
cd build

REM Gerar projeto Visual Studio
echo Gerando arquivos de projeto...
if "%BUILD_TYPE%"=="debug" (
    cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Debug ..
) else (
    cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release ..
)

if !errorlevel! neq 0 (
    echo ERROR: Falha ao gerar arquivos do CMake.
    exit /b 1
)

REM Compilar
echo.
echo Compilando...
if "%BUILD_TYPE%"=="debug" (
    cmake --build . --config Debug
) else (
    cmake --build . --config Release
)

if !errorlevel! neq 0 (
    echo ERROR: Falha na compilação.
    exit /b 1
)

echo.
echo ===============================================
echo Build concluído com sucesso!
echo ===============================================

REM Encontrar o executável
if "%BUILD_TYPE%"=="debug" (
    set EXE_PATH=Debug\remote_desktop_app.exe
) else (
    set EXE_PATH=Release\remote_desktop_app.exe
)

if exist !EXE_PATH! (
    echo Executável: !CD!\!EXE_PATH!
    echo.
    echo Para executar:
    echo   !EXE_PATH!
) else (
    echo AVISO: Executável não encontrado em !EXE_PATH!
)

cd ..
endlocal
