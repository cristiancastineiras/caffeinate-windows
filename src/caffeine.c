// src/caffeine.c — Utilidad de bandeja para mantener el PC despierto y activo en Teams
// Licencia: CC0-1.0 — dominio público.

#ifndef UNICODE
#  define UNICODE
#  define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#define NTDDI_VERSION NTDDI_VISTA
#define _WIN32_WINNT  _WIN32_WINNT_VISTA

#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
#include "../resources/resource.h"

// ─── Configuración ─────────────────────────────────
#define APP_MUTEX_NAME      L"Global\\CaffeinateWindowsSingleInstance"
#define IDT_KEEPAWAKE       1
#define IDT_COUNTDOWN       2
#define WM_TRAYICON         (WM_USER + 1)

// IDs de menú
#define IDM_TOGGLE          2001
#define IDM_INDEFINIDO      2010
#define IDM_TIMER_30M       2011
#define IDM_TIMER_1H        2012
#define IDM_TIMER_2H        2013
#define IDM_TIMER_4H        2014
#define IDM_TIMER_8H        2015
#define IDM_SALIR           2099

// Teams detecta inactividad cada ~5 min; refrescamos cada 59s
#define KEEPAWAKE_INTERVAL  (59u * 1000u)
#define COUNTDOWN_INTERVAL  (60u * 1000u)

static const wchar_t g_szClassName[] = L"CaffeineTrayWndClass";

// ─── Estado de la Aplicación ───────────────────────
typedef struct {
    HINSTANCE      hInstance;
    HWND           hWnd;
    HANDLE         hMutex;
    NOTIFYICONDATA nid;
    BOOL           bTrayAdded;
    BOOL           bActive;
    UINT           nMinutesLeft;    // 0 = indefinido
    UINT           nTotalMinutes;
} AppState;

static AppState g_app = {0};

// ─── Declaraciones ─────────────────────────────────
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static BOOL    InitSingleInstance(void);
static void    KeepAwake(void);
static void    AllowSleep(void);
static BOOL    TrayAdd(HWND hwnd);
static void    TrayRemove(void);
static void    TrayUpdateTooltip(void);
static void    TrayShowContextMenu(HWND hwnd);
static void    Activate(HWND hwnd, UINT minutes);
static void    Deactivate(HWND hwnd);
static void    Cleanup(void);
static void    SimulateActivity(void);

// ─── Punto de Entrada ──────────────────────────────
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, PWSTR pCmdLine, int nCmdShow)
{
    (void)hPrev; (void)pCmdLine; (void)nCmdShow;

    g_app.hInstance = hInstance;

    if (!InitSingleInstance()) return 0;

    WNDCLASSEXW wcex = {
        .cbSize        = sizeof(wcex),
        .lpfnWndProc   = WndProc,
        .hInstance     = hInstance,
        .lpszClassName = g_szClassName,
    };

    if (!RegisterClassExW(&wcex)) {
        Cleanup();
        return 1;
    }

    g_app.hWnd = CreateWindowExW(
        0, g_szClassName, L"CaffeineTray", 0,
        0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL
    );

    if (!g_app.hWnd) {
        Cleanup();
        return 1;
    }

    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessageW(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Cleanup();
    return (int)msg.wParam;
}

// ─── Instancia Única ───────────────────────────────
static BOOL InitSingleInstance(void)
{
    g_app.hMutex = CreateMutexW(NULL, TRUE, APP_MUTEX_NAME);
    if (!g_app.hMutex) return FALSE;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_app.hMutex);
        g_app.hMutex = NULL;
        return FALSE;
    }
    return TRUE;
}

// ─── Simulación de Actividad (Teams/Slack/etc) ─────
// Teams usa GetLastInputInfo() para detectar inactividad.
// Movemos el mouse 1px ida y vuelta para resetear ese timer
// sin que el usuario lo note.
static void SimulateActivity(void)
{
    INPUT inputs[2] = {0};

    // Movimiento relativo: +1px en X
    inputs[0].type       = INPUT_MOUSE;
    inputs[0].mi.dx      = 1;
    inputs[0].mi.dy      = 0;
    inputs[0].mi.dwFlags = MOUSEEVENTF_MOVE;

    // Movimiento relativo: -1px en X (vuelve al lugar)
    inputs[1].type       = INPUT_MOUSE;
    inputs[1].mi.dx      = -1;
    inputs[1].mi.dy      = 0;
    inputs[1].mi.dwFlags = MOUSEEVENTF_MOVE;

    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
}

// ─── Mantener Despierto ────────────────────────────
static void KeepAwake(void)
{
    // Impedir suspensión del sistema y apagado de pantalla
    SetThreadExecutionState(
        ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED
    );

    // Tecla F15 como respaldo (evita screensavers de terceros)
    INPUT inputs[2] = {0};
    inputs[0].type       = INPUT_KEYBOARD;
    inputs[0].ki.wVk     = VK_F15;
    inputs[1].type       = INPUT_KEYBOARD;
    inputs[1].ki.wVk     = VK_F15;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));

    // Simular actividad de usuario para Teams
    SimulateActivity();
}

static void AllowSleep(void)
{
    SetThreadExecutionState(ES_CONTINUOUS);
}

// ─── Activar / Desactivar ──────────────────────────
static void Activate(HWND hwnd, UINT minutes)
{
    g_app.bActive       = TRUE;
    g_app.nTotalMinutes = minutes;
    g_app.nMinutesLeft  = minutes;

    SetTimer(hwnd, IDT_KEEPAWAKE, KEEPAWAKE_INTERVAL, NULL);
    KeepAwake();

    if (minutes > 0) {
        SetTimer(hwnd, IDT_COUNTDOWN, COUNTDOWN_INTERVAL, NULL);
    } else {
        KillTimer(hwnd, IDT_COUNTDOWN);
    }

    TrayUpdateTooltip();
}

static void Deactivate(HWND hwnd)
{
    g_app.bActive       = FALSE;
    g_app.nMinutesLeft  = 0;
    g_app.nTotalMinutes = 0;

    KillTimer(hwnd, IDT_KEEPAWAKE);
    KillTimer(hwnd, IDT_COUNTDOWN);
    AllowSleep();

    TrayUpdateTooltip();
}

// ─── Icono en Bandeja del Sistema ──────────────────
static BOOL TrayAdd(HWND hwnd)
{
    NOTIFYICONDATA *nid = &g_app.nid;
    ZeroMemory(nid, sizeof(*nid));

    nid->cbSize           = sizeof(*nid);
    nid->hWnd             = hwnd;
    nid->uID              = 1;
    nid->uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid->uCallbackMessage = WM_TRAYICON;
    nid->uVersion         = NOTIFYICON_VERSION_4;

    nid->hIcon = (HICON)LoadImageW(
        g_app.hInstance, MAKEINTRESOURCEW(IDI_ICON1),
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR
    );

    if (!nid->hIcon) return FALSE;

    TrayUpdateTooltip();

    if (!Shell_NotifyIconW(NIM_ADD, nid)) {
        DestroyIcon(nid->hIcon);
        nid->hIcon = NULL;
        return FALSE;
    }

    Shell_NotifyIconW(NIM_SETVERSION, nid);
    g_app.bTrayAdded = TRUE;
    return TRUE;
}

static void TrayRemove(void)
{
    if (!g_app.bTrayAdded) return;

    Shell_NotifyIconW(NIM_DELETE, &g_app.nid);
    g_app.bTrayAdded = FALSE;

    if (g_app.nid.hIcon) {
        DestroyIcon(g_app.nid.hIcon);
        g_app.nid.hIcon = NULL;
    }
}

static void TrayUpdateTooltip(void)
{
    wchar_t tip[128];

    if (!g_app.bActive) {
        StringCchCopyW(tip, ARRAYSIZE(tip), L"Caffeine \u2013 desactivado");
    } else if (g_app.nMinutesLeft == 0) {
        StringCchCopyW(tip, ARRAYSIZE(tip), L"Caffeine \u2013 activo (indefinido)");
    } else {
        UINT h = g_app.nMinutesLeft / 60;
        UINT m = g_app.nMinutesLeft % 60;
        if (h > 0)
            StringCchPrintfW(tip, ARRAYSIZE(tip),
                L"Caffeine \u2013 activo (%uh %umin restantes)", h, m);
        else
            StringCchPrintfW(tip, ARRAYSIZE(tip),
                L"Caffeine \u2013 activo (%umin restantes)", m);
    }

    wcscpy_s(g_app.nid.szTip, ARRAYSIZE(g_app.nid.szTip), tip);

    if (g_app.bTrayAdded) {
        g_app.nid.uFlags = NIF_TIP | NIF_SHOWTIP;
        Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
        g_app.nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    }
}

static void TrayShowContextMenu(HWND hwnd)
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Toggle activar/desactivar
    if (g_app.bActive)
        AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE,
                    L"\u2714 Activado (click para desactivar)");
    else
        AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE,
                    L"\u2716 Desactivado (click para activar)");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Submenú de duración
    HMENU hTimerMenu = CreatePopupMenu();
    if (hTimerMenu) {
        UINT current = g_app.nTotalMinutes;
        AppendMenuW(hTimerMenu,
                    MF_STRING | (g_app.bActive && current == 0 ? MF_CHECKED : 0),
                    IDM_INDEFINIDO, L"Indefinido");
        AppendMenuW(hTimerMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hTimerMenu,
                    MF_STRING | (g_app.bActive && current == 30  ? MF_CHECKED : 0),
                    IDM_TIMER_30M, L"30 minutos");
        AppendMenuW(hTimerMenu,
                    MF_STRING | (g_app.bActive && current == 60  ? MF_CHECKED : 0),
                    IDM_TIMER_1H,  L"1 hora");
        AppendMenuW(hTimerMenu,
                    MF_STRING | (g_app.bActive && current == 120 ? MF_CHECKED : 0),
                    IDM_TIMER_2H,  L"2 horas");
        AppendMenuW(hTimerMenu,
                    MF_STRING | (g_app.bActive && current == 240 ? MF_CHECKED : 0),
                    IDM_TIMER_4H,  L"4 horas");
        AppendMenuW(hTimerMenu,
                    MF_STRING | (g_app.bActive && current == 480 ? MF_CHECKED : 0),
                    IDM_TIMER_8H,  L"8 horas");

        AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTimerMenu,
                    L"Mantener activo por...");
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_SALIR, L"Salir");

    POINT pt;
    GetCursorPos(&pt);

    SetForegroundWindow(hwnd);
    TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN,
                     pt.x, pt.y, hwnd, NULL);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

// ─── Limpieza ──────────────────────────────────────
static void Cleanup(void)
{
    TrayRemove();
    AllowSleep();

    if (g_app.hMutex) {
        ReleaseMutex(g_app.hMutex);
        CloseHandle(g_app.hMutex);
        g_app.hMutex = NULL;
    }
}

// ─── Detección de reinicio del taskbar ─────────────
static UINT g_uTaskbarRestart = 0;

// ─── Procedimiento de Ventana ──────────────────────
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        g_uTaskbarRestart = RegisterWindowMessageW(L"TaskbarCreated");

        if (!TrayAdd(hwnd)) return -1;

        // Iniciar activo por defecto (indefinido)
        Activate(hwnd, 0);
        return 0;

    case WM_TIMER:
        if (wParam == IDT_KEEPAWAKE) {
            if (g_app.bActive) KeepAwake();
        } else if (wParam == IDT_COUNTDOWN) {
            if (g_app.nMinutesLeft > 0) {
                g_app.nMinutesLeft--;
                TrayUpdateTooltip();
                if (g_app.nMinutesLeft == 0) {
                    Deactivate(hwnd);
                }
            }
        }
        return 0;

    case WM_TRAYICON:
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            TrayShowContextMenu(hwnd);
            break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_TOGGLE:
            if (g_app.bActive)
                Deactivate(hwnd);
            else
                Activate(hwnd, 0);
            break;
        case IDM_INDEFINIDO:  Activate(hwnd, 0);   break;
        case IDM_TIMER_30M:   Activate(hwnd, 30);  break;
        case IDM_TIMER_1H:    Activate(hwnd, 60);  break;
        case IDM_TIMER_2H:    Activate(hwnd, 120); break;
        case IDM_TIMER_4H:    Activate(hwnd, 240); break;
        case IDM_TIMER_8H:    Activate(hwnd, 480); break;
        case IDM_SALIR:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, IDT_KEEPAWAKE);
        KillTimer(hwnd, IDT_COUNTDOWN);
        TrayRemove();
        PostQuitMessage(0);
        return 0;
    }

    // Recuperación si explorer.exe se reinicia
    if (msg == g_uTaskbarRestart && g_uTaskbarRestart != 0) {
        g_app.bTrayAdded = FALSE;
        TrayAdd(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
