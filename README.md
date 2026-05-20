<div align="center">
<img src="resources/img/logo.ico" alt="Caffeine for Windows" width="120" height="120">
  <h1>Caffeine for Windows</h1>
  <sub>Evita que tu PC se duerma — utilidad ligera en la bandeja del sistema</sub>
</div>

---

## Qué es

Caffeine es un ejecutable portátil (~47 KB) que se queda en tu bandeja del sistema y **impide que Windows entre en suspensión o apague la pantalla**. Sin instalación, sin dependencias, sin consola.

---

## Características

- Corre como icono en la bandeja (system tray), sin ventana visible
- **Activar / Desactivar** con un click derecho
- **Temporizador**: mantener activo por 30 min, 1h, 2h, 4h u 8h (se apaga solo)
- Modo indefinido (por defecto)
- Instancia única: si ya hay uno corriendo, no abre otro
- Se recupera automáticamente si explorer.exe se reinicia
- Tooltip dinámico que muestra el estado y tiempo restante
- Funciona en Windows 7, 8, 10 y 11

---

## Uso

1. Ejecutá `caffeine.exe`
2. Aparece un icono de café en la bandeja del sistema
3. **Click derecho** sobre el icono para ver el menú:

```
✔ Activated (click to disable)
─────────────────────────────
Keep awake for...  ►  Indefinite ✓
                      30 minutes
                      1 hour
                      2 hours
                      4 hours
                      8 hours
─────────────────────────────
Exit
```

4. Seleccioná una duración o activá/desactivá manualmente
5. Cuando el tiempo se agota, se desactiva solo y Windows puede dormir

---

## Inicio automático con Windows

1. Presioná `Win + R`, escribí `shell:startup` y dale Enter
2. Copiá `caffeine.exe` (o un acceso directo) a esa carpeta
3. Listo — se inicia solo cada vez que prendés la PC

---

## Compilación

### Requisitos

| Plataforma | Herramienta |
|---|---|
| **Windows** | [MinGW-w64](https://www.mingw-w64.org/) (gcc + windres) |
| **macOS** | `brew install mingw-w64` |
| **Linux** | `sudo apt install mingw-w64` |

### Compilar con Make (macOS/Linux cross-compile)

```bash
make          # genera build/win/caffeine.exe
make clean    # borra artefactos
```

### Compilar en Windows (MinGW)

```powershell
# 1. Compilar recursos
windres -i resources/caffeine.rc -O coff -o build/win/caffeine.res

# 2. Compilar ejecutable
gcc -O2 -s -municode -mwindows -Wall -Wextra -Iresources src/caffeine.c build/win/caffeine.res -luser32 -lshell32 -o build/win/caffeine.exe
```

### Compilar en Windows (MSVC / Developer Command Prompt)

```cmd
rc /nologo /fo build\win\caffeine.res resources\caffeine.rc

cl /O2 /W4 /nologo /Iresources src\caffeine.c build\win\caffeine.res ^
   user32.lib shell32.lib ^
   /link /SUBSYSTEM:WINDOWS /OUT:build\win\caffeine.exe
```

---

## Cómo funciona

Caffeine usa dos métodos combinados cada 59 segundos:

1. **`SetThreadExecutionState()`** — API de Windows que le dice al sistema "no te duermas, no apagues la pantalla". Es silencioso y oficial.

2. **Tecla F15 sintética** — Simula presionar F15 (una tecla que nadie usa) como respaldo para evitar screensavers de terceros.

Cuando se desactiva (manual o por temporizador), restaura el estado normal y Windows puede dormir libremente.

---

## Estructura del proyecto

```
caffeinate-windows/
├── src/
│   └── caffeine.c          # Código fuente principal
├── resources/
│   ├── caffeine.rc          # Script de recursos (icono, versión)
│   ├── resource.h           # Definición de IDs
│   └── img/
│       └── logo.ico         # Icono de la app
├── build/win/
│   ├── caffeine.exe         # Ejecutable compilado
│   └── caffeine.res         # Recursos compilados
├── Makefile                 # Build script (cross-compile)
├── startup.bat              # Helper para inicio automático
├── LICENSE                  # CC0-1.0 (dominio público)
└── README.md
```

---

## Detalles técnicos

| | |
|---|---|
| Lenguaje | C (Win32 API) |
| Compilador | MinGW-w64 GCC 16 / MSVC |
| Tamaño | ~47 KB |
| Subsistema | Windows GUI (sin consola) |
| Dependencias | Ninguna |
| Licencia | CC0-1.0 — dominio público |

---

<div align="center">
<sub>Hecho con ☕ para los que necesitan que su PC se quede despierta</sub>
</div>
