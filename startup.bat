@echo off
REM ========================================
REM Inicio automático de Caffeine (versión español)
REM ========================================
REM Este archivo inicia Caffeine minimizado al arrancar Windows.
REM 
REM Uso:
REM 1. Copia este archivo a tu carpeta de inicio automático:
REM    - Presiona Win+R, escribe shell:startup y pulsa Enter
REM    - Copia startup.bat a esa carpeta
REM 2. Listo: Caffeine se iniciará solo al prender la PC
REM ========================================

REM Ruta al ejecutable (ajusta si lo mueves de carpeta)
set CAFFEINE_PATH=%~dp0build\win\caffeine.exe

REM Verifica que el ejecutable existe
if not exist "%CAFFEINE_PATH%" (
    echo ERROR: No se encontró Caffeine en: %CAFFEINE_PATH%
    echo Edita este archivo si cambiaste la ubicación de caffeine.exe
    pause
    exit /b 1
)

REM Inicia Caffeine minimizado
start /min "" "%CAFFEINE_PATH%"

REM (Opcional) Mensaje breve
REM echo Caffeine iniciado en segundo plano. Tu PC se mantendrá activa.
REM timeout /t 2 /nobreak >nul
