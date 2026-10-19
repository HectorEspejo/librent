/*
 * PROJECT:     librent NTVDM
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     VDM window, message loop, menus, settings and console handling
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

#include "../ntvdm.h"

#define NDEBUG
#include <debug.h>

#include "../emulator.h"
#include "frontend.h"
#include "../resource.h"

#include <reactos/version.h>
#include <ndk/rtlfuncs.h>

#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)

#define FRONTEND_WINDOW_CLASS   L"LibrentVdmWindow"
#define FRONTEND_TIMER_ID       1
#define FRONTEND_TIMER_MS       266
#define FRONTEND_RESIZE_MESSAGE (WM_APP + 1)
#define FRONTEND_SETTINGS_KEY   L"Software\\librent\\NTVDM"
#define FRONTEND_EXIT_STATUS    ((ULONG)0xC000013A)

static VDM_FRONTEND Frontend;
static BOOLEAN FrontendShuttingDown = FALSE;
static BOOLEAN FrontendDialogs = TRUE;
static BOOLEAN FrontendCursorPhase = TRUE;
static BOOLEAN FrontendBlinkPhase = TRUE;
static ULONG FrontendBlinkTick = 0;
static RECT FrontendTargetRect;
static HWND FrontendConsoleWindow = NULL;
static RECT FrontendSavedWindowRect;
static LONG_PTR FrontendSavedStyle = 0;
static LONG_PTR FrontendSavedExStyle = 0;
static BOOLEAN FrontendSavedPlacement = FALSE;

PVDM_FRONTEND
VgaFrontendGetState(VOID)
{
    return &Frontend;
}

HWND
VgaConsoleGetWindow(VOID)
{
    return Frontend.Window;
}

BOOLEAN
VgaFrontendDialogsAllowed(VOID)
{
    WCHAR Buffer[8];

    if (GetEnvironmentVariableW(L"NTVDM_NO_DIALOGS", Buffer, ARRAYSIZE(Buffer)) != 0)
    {
        if (Buffer[0] == L'1') return FALSE;
    }

    return TRUE;
}

VOID
VgaFrontendSetExitCode(ULONG ExitCode)
{
    Frontend.ExitCode = ExitCode;
}

ULONG
VgaFrontendGetExitCode(VOID)
{
    return Frontend.ExitCode;
}

VOID
VgaFrontendSetMouseCursor(BOOLEAN Visible)
{
    Frontend.DosMouseCursorVisible = Visible;
}

BOOLEAN
VgaFrontendIsMouseCursorVisible(VOID)
{
    return Frontend.DosMouseCursorVisible;
}

VOID
VgaFrontendInvalidate(VOID)
{
    if (Frontend.Window != NULL) InvalidateRect(Frontend.Window, NULL, FALSE);
}

VOID
VgaFrontendNotifyNativeSize(ULONG Width, ULONG Height)
{
    Frontend.NativeWidth = Width;
    Frontend.NativeHeight = Height;

    if (Frontend.Window != NULL) PostMessageW(Frontend.Window, FRONTEND_RESIZE_MESSAGE, 0, 0);
}

static ULONG
FrontendReadSetting(PCWSTR Name, ULONG Default)
{
    HKEY Key;
    DWORD Type, Value = Default, Size = sizeof(Value);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, FRONTEND_SETTINGS_KEY, 0, KEY_READ, &Key) != ERROR_SUCCESS)
        return Default;

    if (RegQueryValueExW(Key, Name, NULL, &Type, (PBYTE)&Value, &Size) != ERROR_SUCCESS ||
        Type != REG_DWORD)
    {
        Value = Default;
    }

    RegCloseKey(Key);
    return Value;
}

static VOID
FrontendWriteSetting(PCWSTR Name, ULONG Value)
{
    HKEY Key;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, FRONTEND_SETTINGS_KEY, 0, NULL,
                        0, KEY_WRITE, NULL, &Key, NULL) != ERROR_SUCCESS)
    {
        return;
    }

    RegSetValueExW(Key, Name, 0, REG_DWORD, (PBYTE)&Value, sizeof(Value));
    RegCloseKey(Key);
}

static VOID
FrontendLoadSettings(VOID)
{
    Frontend.Scale = min(FrontendReadSetting(L"WindowScale", 0), 4);
    Frontend.Presentation = FrontendReadSetting(L"StartFullScreen", 0)
        ? VdmPresentationFullScreen : VdmPresentationWindow;
}

static VOID
FrontendLoadWindowPosition(VOID)
{
    ULONG X = FrontendReadSetting(L"WindowX", 0xFFFFFFFF);
    ULONG Y = FrontendReadSetting(L"WindowY", 0xFFFFFFFF);

    if (X == 0xFFFFFFFF || Y == 0xFFFFFFFF) return;

    FrontendSavedWindowRect.left = (LONG)X;
    FrontendSavedWindowRect.top = (LONG)Y;
    FrontendSavedPlacement = TRUE;
}

static ULONG
FrontendComputeScale(ULONG NativeWidth, ULONG NativeHeight)
{
    ULONG Scale, MaxWidth, MaxHeight;

    if (Frontend.Presentation == VdmPresentationFullScreen)
    {
        MONITORINFO MonitorInfo;
        RECT MonitorRect;

        MonitorInfo.cbSize = sizeof(MonitorInfo);
        GetMonitorInfoW(MonitorFromWindow(Frontend.Window, MONITOR_DEFAULTTONEAREST), &MonitorInfo);
        MonitorRect = MonitorInfo.rcMonitor;
        MaxWidth = MonitorRect.right - MonitorRect.left;
        MaxHeight = MonitorRect.bottom - MonitorRect.top;
    }
    else
    {
        RECT WorkArea;

        SystemParametersInfoW(SPI_GETWORKAREA, 0, &WorkArea, 0);
        MaxWidth = WorkArea.right - WorkArea.left;
        MaxHeight = WorkArea.bottom - WorkArea.top;
    }

    if (Frontend.Scale > 0) return Frontend.Scale;

    Scale = min(MaxWidth / NativeWidth, MaxHeight / NativeHeight);
    Scale = min(Scale, 4);
    return max(Scale, 1);
}

static VOID
FrontendCenterRect(IN PRECT Container, IN ULONG Width, IN ULONG Height, OUT PRECT Target)
{
    ULONG ContainerWidth = Container->right - Container->left;
    ULONG ContainerHeight = Container->bottom - Container->top;

    Target->left = Container->left + (ContainerWidth - min(Width, ContainerWidth)) / 2;
    Target->top = Container->top + (ContainerHeight - min(Height, ContainerHeight)) / 2;
    Target->right = Target->left + min(Width, ContainerWidth);
    Target->bottom = Target->top + min(Height, ContainerHeight);
}

static VOID
FrontendUpdateLayout(BOOLEAN ResizeWindow)
{
    ULONG NativeWidth, NativeHeight, Scale, Width, Height;
    RECT ClientRect, WindowRect, Container;

    VgaFrontendGetNativeSize(&NativeWidth, &NativeHeight);
    if (NativeWidth == 0 || NativeHeight == 0) return;

    Scale = FrontendComputeScale(NativeWidth, NativeHeight);
    Width = NativeWidth * Scale;
    Height = NativeHeight * Scale;

    if (Frontend.Presentation == VdmPresentationFullScreen)
    {
        MONITORINFO MonitorInfo;

        MonitorInfo.cbSize = sizeof(MonitorInfo);
        GetMonitorInfoW(MonitorFromWindow(Frontend.Window, MONITOR_DEFAULTTONEAREST), &MonitorInfo);

        if (ResizeWindow)
        {
            SetWindowPos(Frontend.Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }

        Container = MonitorInfo.rcMonitor;
    }
    else
    {
        if (ResizeWindow)
        {
            GetWindowRect(Frontend.Window, &WindowRect);
            WindowRect.right = WindowRect.left + Width;
            WindowRect.bottom = WindowRect.top + Height;
            AdjustWindowRectEx(&WindowRect, (DWORD)GetWindowLongPtrW(Frontend.Window, GWL_STYLE),
                               FALSE, (DWORD)GetWindowLongPtrW(Frontend.Window, GWL_EXSTYLE));
            SetWindowPos(Frontend.Window, NULL, 0, 0,
                         WindowRect.right - WindowRect.left,
                         WindowRect.bottom - WindowRect.top,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        GetClientRect(Frontend.Window, &ClientRect);
        Container = ClientRect;
    }

    FrontendCenterRect(&Container, Width, Height, &FrontendTargetRect);
}

VOID
VgaFrontendClientToCell(IN POINT Point, OUT PCOORD Cell)
{
    BOOLEAN TextMode, DoubleX, DoubleY;
    COORD Resolution;
    ULONG Width = FrontendTargetRect.right - FrontendTargetRect.left;
    ULONG Height = FrontendTargetRect.bottom - FrontendTargetRect.top;

    VgaFrontendGetMouseMetrics(&TextMode, &DoubleX, &DoubleY, &Resolution);

    Cell->X = 0;
    Cell->Y = 0;
    if (Width == 0 || Height == 0 || Resolution.X == 0 || Resolution.Y == 0) return;

    Cell->X = (WORD)min((ULONG)max(Point.x - FrontendTargetRect.left, 0) * Resolution.X / Width,
                        (ULONG)Resolution.X - 1);
    Cell->Y = (WORD)min((ULONG)max(Point.y - FrontendTargetRect.top, 0) * Resolution.Y / Height,
                        (ULONG)Resolution.Y - 1);
}

static VOID
FrontendSetScale(ULONG Scale)
{
    Frontend.Scale = min(Scale, 4);
    FrontendWriteSetting(L"WindowScale", Frontend.Scale);
    FrontendUpdateLayout(TRUE);
    VgaFrontendInvalidate();
}

static VOID
FrontendEnterFullScreen(VOID)
{
    GetWindowRect(Frontend.Window, &FrontendSavedWindowRect);
    FrontendSavedStyle = GetWindowLongPtrW(Frontend.Window, GWL_STYLE);
    FrontendSavedExStyle = GetWindowLongPtrW(Frontend.Window, GWL_EXSTYLE);

    SetWindowLongPtrW(Frontend.Window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongPtrW(Frontend.Window, GWL_EXSTYLE, FrontendSavedExStyle & ~(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME));

    Frontend.Presentation = VdmPresentationFullScreen;
    FrontendWriteSetting(L"StartFullScreen", 1);
    FrontendUpdateLayout(TRUE);
    VgaFrontendInvalidate();
}

static VOID
FrontendLeaveFullScreen(VOID)
{
    SetWindowLongPtrW(Frontend.Window, GWL_STYLE, (LONG_PTR)FrontendSavedStyle);
    SetWindowLongPtrW(Frontend.Window, GWL_EXSTYLE, (LONG_PTR)FrontendSavedExStyle);

    Frontend.Presentation = VdmPresentationWindow;
    FrontendWriteSetting(L"StartFullScreen", 0);

    SetWindowPos(Frontend.Window, NULL,
                 FrontendSavedWindowRect.left, FrontendSavedWindowRect.top,
                 FrontendSavedWindowRect.right - FrontendSavedWindowRect.left,
                 FrontendSavedWindowRect.bottom - FrontendSavedWindowRect.top,
                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);

    FrontendUpdateLayout(TRUE);
    VgaFrontendInvalidate();
}

static VOID
FrontendToggleFullScreen(VOID)
{
    if (Frontend.Presentation == VdmPresentationFullScreen)
        FrontendLeaveFullScreen();
    else
        FrontendEnterFullScreen();
}

static VOID
FrontendUpdateMenuState(HMENU Menu)
{
    CheckMenuItem(Menu, ID_VDM_FULLSCREEN, MF_BYCOMMAND |
                  ((Frontend.Presentation == VdmPresentationFullScreen) ? MF_CHECKED : MF_UNCHECKED));

    CheckMenuRadioItem(Menu, ID_VDM_SCALE_AUTO, ID_VDM_SCALE_4X,
                       ID_VDM_SCALE_AUTO + Frontend.Scale, MF_BYCOMMAND);
}

static VOID
FrontendShowAbout(VOID)
{
    WCHAR Format[512], Text[768], Title[128];
    WCHAR LibrentVersion[64];

    if (!VgaFrontendDialogsAllowed()) return;

    LoadStringW(GetModuleHandleW(NULL), IDS_VDM_ABOUT_TEXT, Format, ARRAYSIZE(Format));
    LoadStringW(GetModuleHandleW(NULL), IDS_VDM_ABOUT_TITLE, Title, ARRAYSIZE(Title));

    RtlStringCchCopyW(LibrentVersion, ARRAYSIZE(LibrentVersion), L"0.0.0");
#ifdef LIBRENT_VERSION_STRING
    RtlStringCchCopyW(LibrentVersion, ARRAYSIZE(LibrentVersion), WIDEN(LIBRENT_VERSION_STRING));
#endif

    RtlStringCchPrintfW(Text, ARRAYSIZE(Text), Format, LibrentVersion,
                        KERNEL_VERSION_MAJOR, KERNEL_VERSION_MINOR, KERNEL_VERSION_PATCH_LEVEL);

    MessageBoxW(Frontend.Window, Text, Title, MB_OK | MB_ICONINFORMATION);
}

static VOID
FrontendShowSystemMenu(HWND hWnd)
{
    HMENU Menu = GetSystemMenu(hWnd, FALSE);
    RECT Rect;
    POINT Point;

    FrontendUpdateMenuState(Menu);

    GetCursorPos(&Point);
    GetWindowRect(hWnd, &Rect);
    if (!PtInRect(&Rect, Point))
    {
        Point.x = Rect.left + 8;
        Point.y = Rect.top + 20;
    }

    TrackPopupMenu(Menu, TPM_LEFTALIGN | TPM_TOPALIGN, Point.x, Point.y, 0, hWnd, NULL);
}

static BOOLEAN
FrontendConfirmTerminate(VOID)
{
    WCHAR Title[128], Text[512];

    if (!VgaFrontendDialogsAllowed()) return TRUE;
    if (!VdmRunning) return TRUE;

    LoadStringW(GetModuleHandleW(NULL), IDS_VDM_CLOSE_TITLE, Title, ARRAYSIZE(Title));
    if (Frontend.ProgramName[0] != UNICODE_NULL)
    {
        WCHAR Format[512];

        LoadStringW(GetModuleHandleW(NULL), IDS_VDM_CLOSE_TEXT, Format, ARRAYSIZE(Format));
        RtlStringCchPrintfW(Text, ARRAYSIZE(Text), Format, Frontend.ProgramName);
    }
    else
    {
        LoadStringW(GetModuleHandleW(NULL), IDS_VDM_CLOSE_TEXT_DEFAULT, Text, ARRAYSIZE(Text));
    }

    return (MessageBoxW(Frontend.Window, Text, Title,
                        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES);
}

static VOID
FrontendTerminateVDM(VOID)
{
    if (!FrontendConfirmTerminate()) return;

    FrontendShuttingDown = TRUE;
    VgaFrontendSetExitCode(FRONTEND_EXIT_STATUS);
    VdmShutdown(TRUE);
}

VOID
VgaConsoleSetTitle(PCSTR ProgramName)
{
    WCHAR Title[MAX_PATH];
    WCHAR Format[128];
    WCHAR Name[MAX_PATH] = L"";

    if (ProgramName != NULL && ProgramName[0] != '\0')
    {
        MultiByteToWideChar(437, 0, ProgramName, -1, Name, ARRAYSIZE(Name));

        RtlStringCchCopyW(Frontend.ProgramName, ARRAYSIZE(Frontend.ProgramName), Name);

        LoadStringW(GetModuleHandleW(NULL), IDS_VDM_TITLE_FORMAT, Format, ARRAYSIZE(Format));
        RtlStringCchPrintfW(Title, ARRAYSIZE(Title), Format, Name);
    }
    else
    {
        LoadStringW(GetModuleHandleW(NULL), IDS_VDM_TITLE, Title, ARRAYSIZE(Title));
    }

    if (Frontend.Window != NULL) SetWindowTextW(Frontend.Window, Title);
}

static BOOL
WINAPI
FrontendCtrlHandler(DWORD ControlType)
{
    switch (ControlType)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        {
            VgaFrontendInjectCtrlBreak();
            return TRUE;
        }

        default:
        {
            VdmShutdown(TRUE);
            return TRUE;
        }
    }
}

BOOL
ConsoleInit(VOID)
{
    DWORD Pids[4];

    FrontendDialogs = VgaFrontendDialogsAllowed();

    SetConsoleCtrlHandler(FrontendCtrlHandler, TRUE);

    FrontendConsoleWindow = GetConsoleWindow();
    if (FrontendConsoleWindow != NULL)
    {
        Frontend.ConsoleOwner = (GetConsoleProcessList(Pids, ARRAYSIZE(Pids)) == 1);
    }

    return TRUE;
}

BOOL
ConsoleAttach(VOID)
{
    if (Frontend.ConsoleOwner && !Frontend.ConsoleHidden && FrontendConsoleWindow != NULL)
    {
        ShowWindow(FrontendConsoleWindow, SW_HIDE);
        Frontend.ConsoleHidden = TRUE;
    }

    return TRUE;
}

VOID
ConsoleDetach(VOID)
{
    if (Frontend.ConsoleOwner && Frontend.ConsoleHidden && FrontendConsoleWindow != NULL)
    {
        ShowWindow(FrontendConsoleWindow, SW_SHOW);
        Frontend.ConsoleHidden = FALSE;
    }
}

BOOL
IsConsoleHandle(HANDLE hHandle)
{
    DWORD Mode;

    if ((GetFileType(hHandle) & ~FILE_TYPE_REMOTE) != FILE_TYPE_CHAR) return FALSE;

    return GetConsoleMode(hHandle, &Mode);
}

VOID
UpdateVdmMenuDisks(VOID)
{
}

VOID
ConsoleCleanup(VOID)
{
}

static LRESULT
WINAPI
FrontendWindowProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch (Message)
    {
        case WM_CREATE:
        {
            HMENU Menu = GetSystemMenu(hWnd, FALSE);
            HMENU ScaleMenu = CreatePopupMenu();
            HMENU DumpMenu = CreatePopupMenu();
            WCHAR Text[128];
            HICON Icon;
            UINT i;

            Frontend.Window = hWnd;
            FrontendLoadSettings();

            Icon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(11));
            SetClassLongPtrW(hWnd, GCLP_HICON, (LONG_PTR)Icon);
            SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)Icon);
            SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)Icon);

            AppendMenuW(Menu, MF_SEPARATOR, 0, NULL);

            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_FULLSCREEN, Text, ARRAYSIZE(Text));
            AppendMenuW(Menu, MF_STRING, ID_VDM_FULLSCREEN, Text);

            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_SCALE, Text, ARRAYSIZE(Text));
            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_SCALE_AUTO, Text, ARRAYSIZE(Text));
            AppendMenuW(ScaleMenu, MF_STRING, ID_VDM_SCALE_AUTO, Text);
            for (i = 1; i <= 4; i++)
            {
                WCHAR Item[128], Format[32];

                LoadStringW(GetModuleHandleW(NULL), IDS_VDM_SCALE_FORMAT, Format, ARRAYSIZE(Format));
                RtlStringCchPrintfW(Item, ARRAYSIZE(Item), Format, i);
                AppendMenuW(ScaleMenu, MF_STRING, ID_VDM_SCALE_AUTO + i, Item);
            }

            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_SCALE, Text, ARRAYSIZE(Text));
            AppendMenuW(Menu, MF_POPUP, (UINT_PTR)ScaleMenu, Text);

            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_COPY_TEXT, Text, ARRAYSIZE(Text));
            AppendMenuW(Menu, MF_STRING, ID_VDM_COPY_TEXT, Text);

            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_PASTE, Text, ARRAYSIZE(Text));
            AppendMenuW(Menu, MF_STRING, ID_VDM_PASTE, Text);

            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_MEMORY_DUMP, Text, ARRAYSIZE(Text));
            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_DUMPMEM_TXT, Text, ARRAYSIZE(Text));
            AppendMenuW(DumpMenu, MF_STRING, ID_VDM_DUMPMEM_TXT, Text);
            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_DUMPMEM_BIN, Text, ARRAYSIZE(Text));
            AppendMenuW(DumpMenu, MF_STRING, ID_VDM_DUMPMEM_BIN, Text);
            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_MEMORY_DUMP, Text, ARRAYSIZE(Text));
            AppendMenuW(Menu, MF_POPUP, (UINT_PTR)DumpMenu, Text);

            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_TERMINATE, Text, ARRAYSIZE(Text));
            AppendMenuW(Menu, MF_STRING, ID_VDM_TERMINATE, Text);

            AppendMenuW(Menu, MF_SEPARATOR, 0, NULL);

            LoadStringW(GetModuleHandleW(NULL), IDS_VDM_ABOUT, Text, ARRAYSIZE(Text));
            AppendMenuW(Menu, MF_STRING, ID_VDM_ABOUT, Text);

            FrontendUpdateMenuState(Menu);

            FrontendLoadWindowPosition();
            SetTimer(hWnd, FRONTEND_TIMER_ID, FRONTEND_TIMER_MS, NULL);

            Frontend.State = VdmFrontendCreated;
            return 0;
        }

        case FRONTEND_RESIZE_MESSAGE:
        {
            FrontendUpdateLayout(TRUE);
            if (Frontend.State == VdmFrontendCreated && Frontend.NativeWidth != 0)
            {
                Frontend.State = VdmFrontendVisible;
                ShowWindow(hWnd, SW_SHOW);
                SetForegroundWindow(hWnd);
                ConsoleAttach();
            }
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_SIZE:
        {
            FrontendUpdateLayout(FALSE);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            RECT ClientRect;
            HDC Dc = BeginPaint(hWnd, &Paint);

            GetClientRect(hWnd, &ClientRect);
            if (FrontendTargetRect.right == FrontendTargetRect.left) FrontendUpdateLayout(FALSE);

            FillRect(Dc, &ClientRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
            VgaFrontendPaint(Dc, &FrontendTargetRect, FrontendCursorPhase, FrontendBlinkPhase);
            EndPaint(hWnd, &Paint);
            return 0;
        }

        case WM_TIMER:
        {
            FrontendCursorPhase = !FrontendCursorPhase;
            if (++FrontendBlinkTick >= 2)
            {
                FrontendBlinkTick = 0;
                FrontendBlinkPhase = !FrontendBlinkPhase;
            }
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            VgaFrontendOnKeyEvent(Message, wParam, lParam);
            return 0;
        }

        case WM_SYSKEYDOWN:
        {
            if (wParam == VK_RETURN)
            {
                FrontendToggleFullScreen();
                return 0;
            }

            if (wParam == VK_SPACE && (lParam & (1 << 29)))
            {
                FrontendShowSystemMenu(hWnd);
                return 0;
            }

            VgaFrontendOnKeyEvent(Message, wParam, lParam);

            if (wParam == VK_MENU || wParam == VK_F4 || wParam == VK_F10)
            {
                return DefWindowProcW(hWnd, Message, wParam, lParam);
            }

            return 0;
        }

        case WM_SYSKEYUP:
        {
            VgaFrontendOnKeyEvent(Message, wParam, lParam);

            if (wParam == VK_MENU || wParam == VK_F10)
            {
                return DefWindowProcW(hWnd, Message, wParam, lParam);
            }

            return 0;
        }

        case WM_CHAR:
        case WM_SYSCHAR:
            return 0;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        {
            POINT Point;

            Point.x = (SHORT)LOWORD(lParam);
            Point.y = (SHORT)HIWORD(lParam);
            if (Message == WM_MOUSEWHEEL) ScreenToClient(hWnd, &Point);

            VgaFrontendOnMouseEvent(Message, wParam, lParam, Point);
            return 0;
        }

        case WM_SETCURSOR:
        {
            if (VgaFrontendIsMouseCursorVisible())
            {
                SetCursor(NULL);
                return TRUE;
            }
            break;
        }

        case WM_SETFOCUS:
            VgaFrontendOnFocus(TRUE);
            return 0;

        case WM_KILLFOCUS:
            VgaFrontendOnFocus(FALSE);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_EXITSIZEMOVE:
        {
            if (Frontend.Presentation == VdmPresentationWindow)
            {
                RECT WindowRect;

                GetWindowRect(hWnd, &WindowRect);
                FrontendWriteSetting(L"WindowX", (ULONG)WindowRect.left);
                FrontendWriteSetting(L"WindowY", (ULONG)WindowRect.top);
            }
            return 0;
        }

        case WM_SYSCOMMAND:
        {
            switch (LOWORD(wParam))
            {
                case ID_VDM_FULLSCREEN:
                    FrontendToggleFullScreen();
                    FrontendUpdateMenuState(GetSystemMenu(hWnd, FALSE));
                    return 0;

                case ID_VDM_COPY_TEXT:
                    VgaFrontendCopyScreenToClipboard();
                    return 0;

                case ID_VDM_PASTE:
                    VgaFrontendQueuePaste();
                    return 0;

                case ID_VDM_DUMPMEM_TXT:
                    DumpMemory(TRUE);
                    return 0;

                case ID_VDM_DUMPMEM_BIN:
                    DumpMemory(FALSE);
                    return 0;

                case ID_VDM_TERMINATE:
                    FrontendTerminateVDM();
                    return 0;

                case ID_VDM_ABOUT:
                    FrontendShowAbout();
                    return 0;

                default:
                    break;
            }

            if (LOWORD(wParam) >= ID_VDM_SCALE_AUTO && LOWORD(wParam) <= ID_VDM_SCALE_4X)
            {
                FrontendSetScale((ULONG)LOWORD(wParam) - ID_VDM_SCALE_AUTO);
                FrontendUpdateMenuState(GetSystemMenu(hWnd, FALSE));
                return 0;
            }

            break;
        }

        case WM_CLOSE:
        {
            if (!FrontendShuttingDown && !FrontendConfirmTerminate()) return 0;

            FrontendShuttingDown = TRUE;
            VgaFrontendSetExitCode(FRONTEND_EXIT_STATUS);
            VdmShutdown(TRUE);
            return 0;
        }

        case WM_DESTROY:
        {
            KillTimer(hWnd, FRONTEND_TIMER_ID);
            Frontend.State = VdmFrontendDestroyed;
            PostQuitMessage(0);
            return 0;
        }

        default:
            break;
    }

    return DefWindowProcW(hWnd, Message, wParam, lParam);
}

static DWORD
WINAPI
FrontendThreadProc(LPVOID Parameter)
{
    WNDCLASSEXW WindowClass;
    MSG Message;
    ULONG Width, Height;

    UNREFERENCED_PARAMETER(Parameter);

    RtlZeroMemory(&WindowClass, sizeof(WindowClass));
    WindowClass.cbSize = sizeof(WindowClass);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = FrontendWindowProc;
    WindowClass.hInstance = GetModuleHandleW(NULL);
    WindowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    WindowClass.hbrBackground = NULL;
    WindowClass.lpszClassName = FRONTEND_WINDOW_CLASS;

    if (!RegisterClassExW(&WindowClass))
    {
        SetEvent(Frontend.ReadyEvent);
        return 1;
    }

    VgaFrontendGetNativeSize(&Width, &Height);
    if (Width == 0 || Height == 0)
    {
        Width = 720;
        Height = 400;
    }

    Frontend.Window = CreateWindowExW(WS_EX_APPWINDOW,
                                      FRONTEND_WINDOW_CLASS,
                                      L"",
                                      WS_OVERLAPPEDWINDOW,
                                      FrontendSavedPlacement ? FrontendSavedWindowRect.left : CW_USEDEFAULT,
                                      FrontendSavedPlacement ? FrontendSavedWindowRect.top : CW_USEDEFAULT,
                                      (INT)Width, (INT)Height,
                                      NULL, NULL, GetModuleHandleW(NULL), NULL);

    SetEvent(Frontend.ReadyEvent);
    if (Frontend.Window == NULL) return 1;

    Frontend.UiThreadId = GetCurrentThreadId();

    while (GetMessageW(&Message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Message);
        DispatchMessageW(&Message);
    }

    UnregisterClassW(FRONTEND_WINDOW_CLASS, GetModuleHandleW(NULL));
    Frontend.State = VdmFrontendDestroyed;
    return 0;
}

BOOLEAN
VgaFrontendInitialize(VOID)
{
    RtlZeroMemory(&Frontend, sizeof(Frontend));
    Frontend.State = VdmFrontendCreated;
    Frontend.Scale = 0;

    InitializeCriticalSection(&Frontend.PasteLock);

    Frontend.ReadyEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (Frontend.ReadyEvent == NULL) return FALSE;

    Frontend.UiThread = CreateThread(NULL, 0, FrontendThreadProc, NULL, 0, NULL);
    if (Frontend.UiThread == NULL)
    {
        CloseHandle(Frontend.ReadyEvent);
        Frontend.ReadyEvent = NULL;
        return FALSE;
    }

    WaitForSingleObject(Frontend.ReadyEvent, INFINITE);

    if (Frontend.Window == NULL) return FALSE;

    return TRUE;
}

VOID
VgaFrontendCleanup(VOID)
{
    if (Frontend.Window != NULL && !FrontendShuttingDown)
    {
        FrontendShuttingDown = TRUE;
        PostMessageW(Frontend.Window, WM_CLOSE, 0, 0);
    }

    if (Frontend.UiThread != NULL)
    {
        WaitForSingleObject(Frontend.UiThread, 5000);
        CloseHandle(Frontend.UiThread);
        Frontend.UiThread = NULL;
    }

    if (Frontend.ReadyEvent != NULL)
    {
        CloseHandle(Frontend.ReadyEvent);
        Frontend.ReadyEvent = NULL;
    }

    DeleteCriticalSection(&Frontend.PasteLock);
    Frontend.Window = NULL;
}

/* EOF */
