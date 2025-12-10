# Build Script para Remote Desktop Core (PowerShell)
# Uso: .\build.ps1 -BuildType release -Clean

param(
    [ValidateSet("debug", "release")]
    [string]$BuildType = "release",
    
    [switch]$Clean,
    
    [switch]$Run
)

$ErrorActionPreference = "Stop"

Write-Host "`n===============================================" -ForegroundColor Cyan
Write-Host "Remote Desktop Core - Build Script (PowerShell)" -ForegroundColor Cyan
Write-Host "===============================================`n" -ForegroundColor Cyan

Write-Host "Build Type: $BuildType"
if ($Clean) { Write-Host "Clean Build: Habilitado" }
if ($Run) { Write-Host "Run After Build: Habilitado" }
Write-Host ""

# Verificar CMake
try {
    $cmakeVersion = cmake --version | Select-Object -First 1
    Write-Host "✓ CMake encontrado: $cmakeVersion" -ForegroundColor Green
} catch {
    Write-Host "✗ ERROR: CMake não encontrado. Instale CMake primeiro." -ForegroundColor Red
    exit 1
}

# Verificar se está no diretório correto
if (-not (Test-Path "src\main.cpp")) {
    Write-Host "✗ ERROR: Não encontrado src\main.cpp. Execute do diretório raiz do projeto." -ForegroundColor Red
    exit 1
}

# Limpar se solicitado
if ($Clean) {
    Write-Host "Limpando build anterior..." -ForegroundColor Yellow
    if (Test-Path "build") {
        Remove-Item -Recurse -Force "build" | Out-Null
        Write-Host "✓ Diretório build removido" -ForegroundColor Green
    }
}

# Criar diretório de build
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location "build"

# Gerar projeto
Write-Host "Gerando arquivos de projeto CMake..." -ForegroundColor Yellow
$configType = if ($BuildType -eq "debug") { "Debug" } else { "Release" }

& cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=$configType ".." 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ ERROR: Falha ao gerar arquivos CMake." -ForegroundColor Red
    Set-Location ".."
    exit 1
}

Write-Host "✓ Projeto gerado com sucesso" -ForegroundColor Green

# Compilar
Write-Host "`nCompilando ($configType)..." -ForegroundColor Yellow
& cmake --build . --config $configType 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ ERROR: Falha na compilação." -ForegroundColor Red
    Set-Location ".."
    exit 1
}

Write-Host "✓ Compilação concluída com sucesso" -ForegroundColor Green

# Encontrar executável
$exePath = if ($BuildType -eq "debug") { 
    "Debug\remote_desktop_app.exe" 
} else { 
    "Release\remote_desktop_app.exe" 
}

Set-Location ".."

Write-Host "`n===============================================" -ForegroundColor Cyan
Write-Host "Build Completo!" -ForegroundColor Green
Write-Host "===============================================`n" -ForegroundColor Cyan

if (Test-Path $exePath) {
    $fullPath = (Get-Item $exePath).FullName
    Write-Host "✓ Executável: $fullPath" -ForegroundColor Green
    
    if ($Run) {
        Write-Host "`nExecutando aplicação..." -ForegroundColor Cyan
        & $exePath
    } else {
        Write-Host "`nPara executar: .\build\$exePath`n"
    }
} else {
    Write-Host "⚠ Executável não encontrado em: $exePath" -ForegroundColor Yellow
}
