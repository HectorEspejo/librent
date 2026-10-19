/*
 * PROJECT:     librent NTVDM
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Window frontend of the virtual DOS machine
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

#pragma once

#define VDM_MAX_TEXT_COLUMNS 80
#define VDM_MAX_TEXT_ROWS    50
#define VDM_PASTE_QUEUE_SIZE 8192

typedef enum _VDM_FRONTEND_STATE
{
    VdmFrontendCreated,
    VdmFrontendVisible,
    VdmFrontendClosing,
    VdmFrontendDestroyed
} VDM_FRONTEND_STATE;

typedef enum _VDM_PRESENTATION
{
    VdmPresentationWindow,
    VdmPresentationFullScreen
} VDM_PRESENTATION;

typedef struct _VDM_FRONTEND
{
    HWND Window;
    HANDLE UiThread;
    DWORD UiThreadId;
    HANDLE ReadyEvent;
    VDM_FRONTEND_STATE State;
    VDM_PRESENTATION Presentation;
    BOOLEAN ConsoleOwner;
    BOOLEAN ConsoleHidden;
    BOOLEAN DosMouseCursorVisible;
    ULONG Scale;
    ULONG NativeWidth;
    ULONG NativeHeight;
    ULONG ExitCode;
    WCHAR ProgramName[MAX_PATH];
    CRITICAL_SECTION PasteLock;
    BYTE PasteQueue[VDM_PASTE_QUEUE_SIZE];
    ULONG PasteHead;
    ULONG PasteTail;
} VDM_FRONTEND, *PVDM_FRONTEND;

BOOLEAN VgaFrontendInitialize(VOID);
VOID VgaFrontendCleanup(VOID);
PVDM_FRONTEND VgaFrontendGetState(VOID);
HWND VgaConsoleGetWindow(VOID);
BOOLEAN VgaFrontendDialogsAllowed(VOID);
VOID VgaFrontendSetExitCode(ULONG ExitCode);
ULONG VgaFrontendGetExitCode(VOID);
VOID VgaFrontendSetMouseCursor(BOOLEAN Visible);
BOOLEAN VgaFrontendIsMouseCursorVisible(VOID);
VOID VgaFrontendInvalidate(VOID);
VOID VgaFrontendNotifyNativeSize(ULONG Width, ULONG Height);
VOID VgaFrontendClientToCell(IN POINT Point, OUT PCOORD Cell);
BOOLEAN VgaFrontendCopyScreenToClipboard(VOID);
VOID FocusEventHandler(PFOCUS_EVENT_RECORD FocusEvent);

BOOLEAN VgaConsoleInitialize(VOID);
VOID VgaConsoleCleanup(VOID);
VOID VgaConsoleSetTitle(PCSTR ProgramName);
BOOL VgaConsoleCreateTextScreen(IN PCOORD Resolution, IN HANDLE PaletteHandle);
VOID VgaConsoleDestroyTextScreen(VOID);
BOOL VgaConsoleCreateGraphicsScreen(IN PCOORD Resolution, IN HANDLE PaletteHandle);
VOID VgaConsoleDestroyGraphicsScreen(VOID);
VOID VgaConsoleRepaintScreen(PSMALL_RECT Rect);
VOID VgaConsoleUpdateTextCursor(BOOL CursorVisible, BYTE CursorStart, BYTE CursorEnd,
                                BYTE TextSize, DWORD ScanlineSize, WORD Location);
BOOL VgaAttachToConsole(VOID);
VOID VgaDetachFromConsole(VOID);
BOOLEAN VgaGetDoubleVisionState(PBOOLEAN Horizontal, PBOOLEAN Vertical);
VOID VgaFrontendPaint(IN HDC Dc, IN PRECT Target, BOOLEAN CursorPhase, BOOLEAN BlinkPhase);
VOID VgaFrontendGetNativeSize(OUT PULONG Width, OUT PULONG Height);
BOOLEAN VgaFrontendIsTextMode(VOID);
VOID VgaFrontendGetMouseMetrics(OUT PBOOLEAN TextMode, OUT PBOOLEAN DoubleX,
                                OUT PBOOLEAN DoubleY, OUT PCOORD Resolution);

VOID VgaFrontendOnKeyEvent(IN UINT Message, IN WPARAM wParam, IN LPARAM lParam);
VOID VgaFrontendOnMouseEvent(IN UINT Message, IN WPARAM wParam, IN LPARAM lParam, IN POINT Point);
VOID VgaFrontendOnFocus(BOOLEAN SetFocus);
VOID VgaFrontendQueuePaste(VOID);
VOID VgaFrontendPumpPaste(VOID);
VOID VgaFrontendInjectCtrlBreak(VOID);
BYTE VgaFrontendUnicodeToCp437(WCHAR Character);

/* EOF */
