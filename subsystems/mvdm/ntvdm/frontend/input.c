/*
 * PROJECT:     librent NTVDM
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Keyboard, mouse and clipboard input for the VDM window
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

#include "../ntvdm.h"

#define NDEBUG
#include <debug.h>

#include "../emulator.h"
#include "../hardware/keyboard.h"
#include "../hardware/mouse.h"
#include "../bios/bios32/kbdbios32.h"
#include "frontend.h"

static DWORD
VgaFrontendGetControlKeyState(VOID)
{
    DWORD State = 0;

    if (GetKeyState(VK_SHIFT) & 0x8000) State |= SHIFT_PRESSED;
    if (GetKeyState(VK_LSHIFT) & 0x8000) State |= SHIFT_PRESSED;
    if (GetKeyState(VK_RSHIFT) & 0x8000) State |= SHIFT_PRESSED;
    if (GetKeyState(VK_LCONTROL) & 0x8000) State |= LEFT_CTRL_PRESSED;
    if (GetKeyState(VK_RCONTROL) & 0x8000) State |= RIGHT_CTRL_PRESSED;
    if (GetKeyState(VK_LMENU) & 0x8000) State |= LEFT_ALT_PRESSED;
    if (GetKeyState(VK_RMENU) & 0x8000) State |= RIGHT_ALT_PRESSED;
    if (GetKeyState(VK_CAPITAL) & 0x0001) State |= CAPSLOCK_ON;
    if (GetKeyState(VK_NUMLOCK) & 0x0001) State |= NUMLOCK_ON;
    if (GetKeyState(VK_SCROLL) & 0x0001) State |= SCROLLLOCK_ON;

    return State;
}

BYTE
VgaFrontendUnicodeToCp437(WCHAR Character)
{
    CHAR Buffer[2];
    BOOL UsedDefault = FALSE;
    int Count;

    Count = WideCharToMultiByte(437, 0, &Character, 1, Buffer, sizeof(Buffer),
                                "?", &UsedDefault);
    if (Count != 1 || UsedDefault) return 0;

    return (BYTE)Buffer[0];
}

static BYTE
VgaFrontendTranslateKey(UINT VirtualKey, UINT ScanCode)
{
    BYTE KeyState[256];
    WCHAR Buffer[4];
    int Count;

    if (!GetKeyboardState(KeyState)) return 0;

    Count = ToUnicode(VirtualKey, ScanCode, KeyState, Buffer, ARRAYSIZE(Buffer), 0);
    if (Count <= 0) return 0;

    return VgaFrontendUnicodeToCp437(Buffer[0]);
}

VOID
VgaFrontendOnKeyEvent(IN UINT Message, IN WPARAM wParam, IN LPARAM lParam)
{
    KEY_EVENT_RECORD KeyEvent;
    BOOLEAN KeyDown = (Message == WM_KEYDOWN) || (Message == WM_SYSKEYDOWN);

    RtlZeroMemory(&KeyEvent, sizeof(KeyEvent));

    KeyEvent.bKeyDown = KeyDown;
    KeyEvent.wRepeatCount = (WORD)(lParam & 0xFFFF);
    KeyEvent.wVirtualKeyCode = (WORD)wParam;
    KeyEvent.wVirtualScanCode = (WORD)((lParam >> 16) & 0xFF);
    KeyEvent.uChar.AsciiChar = KeyDown
        ? VgaFrontendTranslateKey((UINT)wParam, KeyEvent.wVirtualScanCode)
        : 0;
    KeyEvent.dwControlKeyState = VgaFrontendGetControlKeyState();
    if (lParam & (1 << 24)) KeyEvent.dwControlKeyState |= ENHANCED_KEY;

    KeyboardEventHandler(&KeyEvent);
}

VOID
VgaFrontendOnMouseEvent(IN UINT Message, IN WPARAM wParam, IN LPARAM lParam, IN POINT Point)
{
    MOUSE_EVENT_RECORD MouseEvent;
    COORD Cell;

    VgaFrontendClientToCell(Point, &Cell);

    RtlZeroMemory(&MouseEvent, sizeof(MouseEvent));
    MouseEvent.dwMousePosition = Cell;

    if (wParam & MK_LBUTTON) MouseEvent.dwButtonState |= FROM_LEFT_1ST_BUTTON_PRESSED;
    if (wParam & MK_MBUTTON) MouseEvent.dwButtonState |= FROM_LEFT_2ND_BUTTON_PRESSED;
    if (wParam & MK_RBUTTON) MouseEvent.dwButtonState |= RIGHTMOST_BUTTON_PRESSED;

    MouseEvent.dwControlKeyState = VgaFrontendGetControlKeyState();

    if (Message == WM_MOUSEMOVE) MouseEvent.dwEventFlags = MOUSE_MOVED;
    if (Message == WM_MOUSEWHEEL)
    {
        MouseEvent.dwEventFlags = MOUSE_WHEELED;
        MouseEvent.dwButtonState |= (DWORD)(SHORT)HIWORD(wParam) << 16;
    }

    MouseEventHandler(&MouseEvent);
}

VOID
FocusEventHandler(PFOCUS_EVENT_RECORD FocusEvent)
{
    UNREFERENCED_PARAMETER(FocusEvent);
}

VOID
VgaFrontendOnFocus(BOOLEAN SetFocus)
{
    FOCUS_EVENT_RECORD FocusEvent;

    FocusEvent.bSetFocus = SetFocus;
    FocusEventHandler(&FocusEvent);
}

VOID
VgaFrontendInjectCtrlBreak(VOID)
{
    KeyboardInjectScanCode(0x1D);
    KeyboardInjectScanCode(0x2E);
    KeyboardInjectScanCode(0xAE);
    KeyboardInjectScanCode(0x9D);
}

static VOID
VgaFrontendPasteCharacter(BYTE Character)
{
    PVDM_FRONTEND Frontend = VgaFrontendGetState();
    ULONG Next;

    EnterCriticalSection(&Frontend->PasteLock);

    Next = (Frontend->PasteTail + 1) % VDM_PASTE_QUEUE_SIZE;
    if (Next != Frontend->PasteHead)
    {
        Frontend->PasteQueue[Frontend->PasteTail] = Character;
        Frontend->PasteTail = Next;
    }

    LeaveCriticalSection(&Frontend->PasteLock);
}

VOID
VgaFrontendQueuePaste(VOID)
{
    PVDM_FRONTEND Frontend = VgaFrontendGetState();
    HANDLE Data;
    PWCHAR Text;
    SIZE_T Length, i;

    if (!OpenClipboard(Frontend->Window)) return;

    Data = GetClipboardData(CF_UNICODETEXT);
    if (Data == NULL)
    {
        CloseClipboard();
        return;
    }

    Text = GlobalLock(Data);
    if (Text != NULL)
    {
        Length = wcslen(Text);

        for (i = 0; i < Length; i++)
        {
            WCHAR Character = Text[i];

            if (Character == L'\r' || Character == L'\n')
            {
                if (Character == L'\n' && i > 0 && Text[i - 1] == L'\r') continue;
                VgaFrontendPasteCharacter(0x0D);
                continue;
            }

            Character = (WCHAR)VgaFrontendUnicodeToCp437(Character);
            if (Character != 0) VgaFrontendPasteCharacter((BYTE)Character);
        }

        GlobalUnlock(Data);
    }

    CloseClipboard();
}

VOID
VgaFrontendPumpPaste(VOID)
{
    PVDM_FRONTEND Frontend = VgaFrontendGetState();

    while (Frontend->PasteHead != Frontend->PasteTail)
    {
        BYTE Character;

        if (!BiosKeyboardBufferHasRoom()) break;

        EnterCriticalSection(&Frontend->PasteLock);
        Character = Frontend->PasteQueue[Frontend->PasteHead];
        Frontend->PasteHead = (Frontend->PasteHead + 1) % VDM_PASTE_QUEUE_SIZE;
        LeaveCriticalSection(&Frontend->PasteLock);

        BiosKeyboardPushKey(MAKEWORD(Character, 0));
    }
}

/* EOF */
