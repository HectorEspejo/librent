/*
 * PROJECT:     librent NTVDM
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     VGA frontend: text and graphics rendering into the VDM window
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

static CRITICAL_SECTION FrontendScreenLock;
static BOOLEAN FrontendScreenLockReady = FALSE;

typedef struct _FRONTEND_SCREEN
{
    BOOLEAN Valid;
    BOOLEAN TextMode;
    COORD Resolution;
    BYTE CellWidth;
    BYTE CellHeight;
    CHAR_CELL Cells[VDM_MAX_TEXT_COLUMNS * VDM_MAX_TEXT_ROWS];
    BYTE Font[VGA_FONT_CHARACTERS * VGA_MAX_FONT_HEIGHT];
    BOOLEAN BlinkEnabled;
    BOOLEAN LineGraphics;
    RGBQUAD Palette[VGA_MAX_COLORS];
    BOOLEAN CursorVisible;
    BYTE CursorStart;
    BYTE CursorEnd;
    WORD CursorScanlineSize;
    WORD CursorLocation;
    BOOLEAN DoubleWidth;
    BOOLEAN DoubleHeight;
    ULONG NativeWidth;
    ULONG NativeHeight;
} FRONTEND_SCREEN, *PFRONTEND_SCREEN;

static FRONTEND_SCREEN FrontendScreen;

static PULONG FrontendNativeBuffer = NULL;
static ULONG FrontendNativeBufferSize = 0;

static BOOLEAN DoubleWidth = FALSE;
static BOOLEAN DoubleHeight = FALSE;
static HANDLE ConsoleMutex = NULL;
static HBITMAP FrontendGraphicsBitmap = NULL;

static VOID
FrontendCopyPalette(VOID)
{
    ULONG i;

    for (i = 0; i < VGA_MAX_COLORS; i++)
    {
        FrontendScreen.Palette[i].rgbRed      = VGA_DAC_TO_COLOR(VgaDacRegisters[i * 3]);
        FrontendScreen.Palette[i].rgbGreen    = VGA_DAC_TO_COLOR(VgaDacRegisters[i * 3 + 1]);
        FrontendScreen.Palette[i].rgbBlue     = VGA_DAC_TO_COLOR(VgaDacRegisters[i * 3 + 2]);
        FrontendScreen.Palette[i].rgbReserved = 0;
    }
}

static VOID
FrontendCopyFont(VOID)
{
    ULONG i, j;

    for (i = 0; i < VGA_FONT_CHARACTERS; i++)
    {
        for (j = 0; j < VGA_MAX_FONT_HEIGHT; j++)
        {
            FrontendScreen.Font[i * VGA_MAX_FONT_HEIGHT + j] =
                VgaMemory[(i * VGA_MAX_FONT_HEIGHT + j) * VGA_NUM_BANKS + VGA_FONT_BANK];
        }
    }
}

static ULONG
FrontendComputeNativeWidth(VOID)
{
    if (FrontendScreen.TextMode)
        return (ULONG)FrontendScreen.Resolution.X * FrontendScreen.CellWidth;

    return (ULONG)FrontendScreen.Resolution.X << (FrontendScreen.DoubleWidth ? 1 : 0);
}

static ULONG
FrontendComputeNativeHeight(VOID)
{
    if (FrontendScreen.TextMode)
        return (ULONG)FrontendScreen.Resolution.Y * FrontendScreen.CellHeight;

    return (ULONG)FrontendScreen.Resolution.Y << (FrontendScreen.DoubleHeight ? 1 : 0);
}

static VOID
FrontendEnsureNativeBuffer(ULONG Width, ULONG Height)
{
    ULONG Size = Width * Height * sizeof(ULONG);

    if (FrontendNativeBuffer != NULL && FrontendNativeBufferSize >= Size) return;

    if (FrontendNativeBuffer != NULL) RtlFreeHeap(RtlGetProcessHeap(), 0, FrontendNativeBuffer);

    FrontendNativeBuffer = RtlAllocateHeap(RtlGetProcessHeap(), HEAP_ZERO_MEMORY, Size);
    ASSERT(FrontendNativeBuffer != NULL);
    FrontendNativeBufferSize = Size;
}

static inline ULONG
FrontendCellColor(BYTE Fg, BYTE Bg, BOOLEAN Foreground)
{
    ULONG Color = Foreground ? Fg : Bg;

    return (ULONG)FrontendScreen.Palette[Color].rgbRed |
           ((ULONG)FrontendScreen.Palette[Color].rgbGreen << 8) |
           ((ULONG)FrontendScreen.Palette[Color].rgbBlue << 16);
}

static VOID
FrontendRenderTextCells(BOOLEAN CursorPhase, BOOLEAN BlinkPhase)
{
    ULONG Col, Row, GlyphRow, Pixel;
    ULONG Width = FrontendScreen.NativeWidth;
    ULONG Columns = FrontendScreen.Resolution.X;
    ULONG Rows = FrontendScreen.Resolution.Y;
    BYTE CellWidth = FrontendScreen.CellWidth;
    BYTE CellHeight = FrontendScreen.CellHeight;
    BOOLEAN CursorHere;
    BOOLEAN BlinkOn;
    PCHAR_CELL Cell;
    PBYTE Glyph;
    PULONG Line;
    BYTE Fg, Bg;

    for (Row = 0; Row < Rows; Row++)
    {
        for (Col = 0; Col < Columns; Col++)
        {
            Cell = &FrontendScreen.Cells[Row * VDM_MAX_TEXT_COLUMNS + Col];
            Glyph = &FrontendScreen.Font[(BYTE)Cell->Char * VGA_MAX_FONT_HEIGHT];

            Fg = Cell->Attributes & 0x0F;
            Bg = (Cell->Attributes >> 4) & 0x07;
            BlinkOn = FrontendScreen.BlinkEnabled && (Cell->Attributes & 0x80);

            if (BlinkOn && !BlinkPhase) Fg = Bg;

            CursorHere = FrontendScreen.CursorVisible && CursorPhase &&
                         (FrontendScreen.CursorScanlineSize != 0) &&
                         ((FrontendScreen.CursorLocation % FrontendScreen.CursorScanlineSize) == Col) &&
                         ((FrontendScreen.CursorLocation / FrontendScreen.CursorScanlineSize) == Row);

            for (GlyphRow = 0; GlyphRow < CellHeight; GlyphRow++)
            {
                BOOLEAN CursorLine = CursorHere &&
                                     (GlyphRow >= FrontendScreen.CursorStart) &&
                                     (GlyphRow <= FrontendScreen.CursorEnd);
                BYTE Bits = (GlyphRow < VGA_MAX_FONT_HEIGHT) ? Glyph[GlyphRow] : 0;

                Line = FrontendNativeBuffer + (Row * CellHeight + GlyphRow) * Width + Col * CellWidth;

                for (Pixel = 0; Pixel < CellWidth; Pixel++)
                {
                    BOOLEAN Foreground;

                    if (Pixel < 8)
                    {
                        Foreground = (Bits & (0x80 >> Pixel)) != 0;
                    }
                    else
                    {
                        Foreground = FrontendScreen.LineGraphics &&
                                     ((BYTE)Cell->Char >= 0xC0) && ((BYTE)Cell->Char <= 0xDF) &&
                                     (GlyphRow & 0x01) &&
                                     (Bits & 0x01) != 0;
                    }

                    if (CursorLine) Foreground = !Foreground;

                    *Line++ = FrontendCellColor(Fg, Bg, Foreground);
                }
            }
        }
    }
}

static VOID
FrontendRenderGraphics(BOOLEAN CursorPhase, BOOLEAN BlinkPhase)
{
    ULONG X, Y, Dx, Dy;
    ULONG SrcWidth = FrontendScreen.Resolution.X;
    ULONG SrcHeight = FrontendScreen.Resolution.Y;
    ULONG DstWidth = FrontendScreen.NativeWidth;
    ULONG XStep = FrontendScreen.DoubleWidth ? 2 : 1;
    ULONG YStep = FrontendScreen.DoubleHeight ? 2 : 1;
    PULONG Line;
    PBYTE Src;

    UNREFERENCED_PARAMETER(CursorPhase);
    UNREFERENCED_PARAMETER(BlinkPhase);

    for (Y = 0; Y < SrcHeight; Y++)
    {
        for (Dy = 0; Dy < YStep; Dy++)
        {
            Line = FrontendNativeBuffer + (Y * YStep + Dy) * DstWidth;

            for (X = 0; X < SrcWidth; X++)
            {
                Src = GraphicsFramebuffer + Y * SrcWidth + X;

                for (Dx = 0; Dx < XStep; Dx++)
                {
                    *Line++ = (ULONG)FrontendScreen.Palette[*Src].rgbRed |
                              ((ULONG)FrontendScreen.Palette[*Src].rgbGreen << 8) |
                              ((ULONG)FrontendScreen.Palette[*Src].rgbBlue << 16);
                }
            }
        }
    }
}

VOID
VgaFrontendPaint(IN HDC Dc, IN PRECT Target, BOOLEAN CursorPhase, BOOLEAN BlinkPhase)
{
    BITMAPINFO BitmapInfo;
    ULONG Width, Height;
    INT OldMode;

    if (!FrontendScreenLockReady) return;

    EnterCriticalSection(&FrontendScreenLock);

    Width = FrontendComputeNativeWidth();
    Height = FrontendComputeNativeHeight();
    FrontendScreen.NativeWidth = Width;
    FrontendScreen.NativeHeight = Height;

    FrontendEnsureNativeBuffer(Width, Height);
    RtlZeroMemory(FrontendNativeBuffer, Width * Height * sizeof(ULONG));

    if (FrontendScreen.Valid)
    {
        if (FrontendScreen.TextMode)
            FrontendRenderTextCells(CursorPhase, BlinkPhase);
        else if (GraphicsFramebuffer != NULL)
            FrontendRenderGraphics(CursorPhase, BlinkPhase);
    }

    LeaveCriticalSection(&FrontendScreenLock);

    RtlZeroMemory(&BitmapInfo, sizeof(BitmapInfo));
    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = Width;
    BitmapInfo.bmiHeader.biHeight = -(LONG)Height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    OldMode = SetStretchBltMode(Dc, COLORONCOLOR);
    StretchDIBits(Dc,
                  Target->left,
                  Target->top,
                  Target->right - Target->left,
                  Target->bottom - Target->top,
                  0,
                  0,
                  Width,
                  Height,
                  FrontendNativeBuffer,
                  &BitmapInfo,
                  DIB_RGB_COLORS,
                  SRCCOPY);
    SetStretchBltMode(Dc, OldMode);
}

VOID
VgaFrontendGetNativeSize(OUT PULONG Width, OUT PULONG Height)
{
    if (!FrontendScreenLockReady)
    {
        *Width = 0;
        *Height = 0;
        return;
    }

    EnterCriticalSection(&FrontendScreenLock);
    *Width = FrontendComputeNativeWidth();
    *Height = FrontendComputeNativeHeight();
    LeaveCriticalSection(&FrontendScreenLock);
}

BOOLEAN
VgaFrontendIsTextMode(VOID)
{
    return FrontendScreen.TextMode;
}

static VOID
VgaFrontendPublish(VOID)
{
    ULONG OldWidth, OldHeight;

    if (!FrontendScreenLockReady) return;

    OldWidth = FrontendScreen.NativeWidth;
    OldHeight = FrontendScreen.NativeHeight;

    EnterCriticalSection(&FrontendScreenLock);

    FrontendScreen.Valid = (TextFramebuffer != NULL) || (GraphicsFramebuffer != NULL);
    FrontendScreen.TextMode = (ScreenMode == TEXT_MODE);
    FrontendScreen.Resolution = CurrResolution;

    if (ScreenMode == TEXT_MODE)
    {
        FrontendScreen.CellWidth = (VgaSeqRegisters[VGA_SEQ_CLOCK_REG] & VGA_SEQ_CLOCK_98DM) ? 8 : 9;
        FrontendScreen.CellHeight = 1 + (VgaCrtcRegisters[VGA_CRTC_MAX_SCAN_LINE_REG] & 0x1F);

        if (TextFramebuffer != NULL)
        {
            RtlCopyMemory(FrontendScreen.Cells,
                          TextFramebuffer,
                          min(sizeof(FrontendScreen.Cells),
                              (SIZE_T)CurrResolution.X * CurrResolution.Y * sizeof(CHAR_CELL)));
        }
    }

    FrontendScreen.BlinkEnabled = !!(VgaAcRegisters[VGA_AC_CONTROL_REG] & VGA_AC_CONTROL_BLINK);
    FrontendScreen.LineGraphics = !!(VgaAcRegisters[VGA_AC_CONTROL_REG] & VGA_AC_CONTROL_LGE);

    FrontendCopyFont();
    FrontendCopyPalette();

    FrontendScreen.NativeWidth = FrontendComputeNativeWidth();
    FrontendScreen.NativeHeight = FrontendComputeNativeHeight();

    LeaveCriticalSection(&FrontendScreenLock);

    if (FrontendScreen.NativeWidth != OldWidth || FrontendScreen.NativeHeight != OldHeight)
        VgaFrontendNotifyNativeSize(FrontendScreen.NativeWidth, FrontendScreen.NativeHeight);

    VgaFrontendInvalidate();
}

BOOLEAN
VgaConsoleInitialize(VOID)
{
    if (!FrontendScreenLockReady)
    {
        InitializeCriticalSection(&FrontendScreenLock);
        FrontendScreenLockReady = TRUE;
    }

    if (ConsoleMutex == NULL) ConsoleMutex = CreateMutexW(NULL, FALSE, NULL);

    return VgaFrontendInitialize();
}

VOID
VgaConsoleCleanup(VOID)
{
    VgaFrontendCleanup();

    if (FrontendNativeBuffer != NULL)
    {
        RtlFreeHeap(RtlGetProcessHeap(), 0, FrontendNativeBuffer);
        FrontendNativeBuffer = NULL;
        FrontendNativeBufferSize = 0;
    }

    if (ConsoleMutex != NULL)
    {
        CloseHandle(ConsoleMutex);
        ConsoleMutex = NULL;
    }

    if (FrontendScreenLockReady)
    {
        DeleteCriticalSection(&FrontendScreenLock);
        FrontendScreenLockReady = FALSE;
    }
}

BOOL
VgaConsoleCreateTextScreen(IN PCOORD Resolution, IN HANDLE PaletteHandle)
{
    SIZE_T Size;

    UNREFERENCED_PARAMETER(PaletteHandle);

    Size = (SIZE_T)Resolution->X * Resolution->Y * sizeof(CHAR_CELL);
    TextFramebuffer = RtlAllocateHeap(RtlGetProcessHeap(), HEAP_ZERO_MEMORY, Size);
    if (TextFramebuffer == NULL) return FALSE;

    VgaFrontendPublish();
    return TRUE;
}

VOID
VgaConsoleDestroyTextScreen(VOID)
{
    if (TextFramebuffer != NULL)
    {
        RtlFreeHeap(RtlGetProcessHeap(), 0, TextFramebuffer);
        TextFramebuffer = NULL;
    }
}

BOOL
VgaConsoleCreateGraphicsScreen(IN PCOORD Resolution, IN HANDLE PaletteHandle)
{
    BITMAPINFO BitmapInfo;
    PVOID Bits = NULL;
    ULONG i;

    UNREFERENCED_PARAMETER(PaletteHandle);

    DoubleWidth  = (Resolution->X < VGA_MINIMUM_WIDTH);
    DoubleHeight = (Resolution->Y < VGA_MINIMUM_HEIGHT);

    RtlZeroMemory(&BitmapInfo, sizeof(BitmapInfo));
    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = Resolution->X;
    BitmapInfo.bmiHeader.biHeight = -(LONG)Resolution->Y;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 8;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;
    BitmapInfo.bmiHeader.biClrUsed = VGA_MAX_COLORS;

    for (i = 0; i < VGA_MAX_COLORS; i++)
    {
        BitmapInfo.bmiColors[i].rgbRed      = VGA_DAC_TO_COLOR(VgaDacRegisters[i * 3]);
        BitmapInfo.bmiColors[i].rgbGreen    = VGA_DAC_TO_COLOR(VgaDacRegisters[i * 3 + 1]);
        BitmapInfo.bmiColors[i].rgbBlue     = VGA_DAC_TO_COLOR(VgaDacRegisters[i * 3 + 2]);
        BitmapInfo.bmiColors[i].rgbReserved = 0;
    }

    FrontendGraphicsBitmap = CreateDIBSection(NULL,
                                              &BitmapInfo,
                                              DIB_RGB_COLORS,
                                              &Bits,
                                              NULL,
                                              0);
    if (FrontendGraphicsBitmap == NULL) return FALSE;

    GraphicsFramebuffer = (PBYTE)Bits;
    RtlZeroMemory(Bits, (SIZE_T)Resolution->X * Resolution->Y);

    VgaFrontendPublish();
    return TRUE;
}

VOID
VgaConsoleDestroyGraphicsScreen(VOID)
{
    if (FrontendGraphicsBitmap != NULL)
    {
        DeleteObject(FrontendGraphicsBitmap);
        FrontendGraphicsBitmap = NULL;
    }

    GraphicsFramebuffer = NULL;

    DoubleWidth  = FALSE;
    DoubleHeight = FALSE;
}

VOID
VgaConsoleRepaintScreen(PSMALL_RECT Rect)
{
    UNREFERENCED_PARAMETER(Rect);

    VgaFrontendPublish();
}

VOID
VgaConsoleUpdateTextCursor(BOOL CursorVisible, BYTE CursorStart, BYTE CursorEnd,
                           BYTE TextSize, DWORD ScanlineSize, WORD Location)
{
    if (!FrontendScreenLockReady) return;

    EnterCriticalSection(&FrontendScreenLock);

    FrontendScreen.CursorVisible = (CursorStart < CursorEnd) ? CursorVisible : FALSE;
    FrontendScreen.CursorStart = min(CursorStart, TextSize ? TextSize - 1 : CursorStart);
    FrontendScreen.CursorEnd   = min(CursorEnd,   TextSize ? TextSize - 1 : CursorEnd);
    FrontendScreen.CursorScanlineSize = (WORD)ScanlineSize;
    FrontendScreen.CursorLocation = Location;

    LeaveCriticalSection(&FrontendScreenLock);

    VgaFrontendInvalidate();
}

BOOL
VgaAttachToConsole(VOID)
{
    ConsoleAttach();
    VgaFrontendPublish();
    return TRUE;
}

VOID
VgaDetachFromConsole(VOID)
{
    ConsoleDetach();
}

BOOLEAN
VgaGetDoubleVisionState(PBOOLEAN Horizontal, PBOOLEAN Vertical)
{
    if (GraphicsFramebuffer == NULL) return FALSE;
    if (Horizontal) *Horizontal = DoubleWidth;
    if (Vertical)   *Vertical   = DoubleHeight;
    return TRUE;
}

VOID
VgaFrontendGetMouseMetrics(OUT PBOOLEAN TextMode, OUT PBOOLEAN DoubleX,
                           OUT PBOOLEAN DoubleY, OUT PCOORD Resolution)
{
    *TextMode = FrontendScreen.TextMode;
    *DoubleX = FrontendScreen.DoubleWidth;
    *DoubleY = FrontendScreen.DoubleHeight;
    *Resolution = FrontendScreen.Resolution;
}

BOOLEAN
VgaFrontendCopyScreenToClipboard(VOID)
{
    PVDM_FRONTEND Frontend = VgaFrontendGetState();
    ULONG Columns, Rows, Col, Row, Position = 0;
    WCHAR *Buffer;
    HGLOBAL Data;
    BOOLEAN Success = FALSE;

    EnterCriticalSection(&FrontendScreenLock);

    if (!FrontendScreen.TextMode || !FrontendScreen.Valid)
    {
        LeaveCriticalSection(&FrontendScreenLock);
        return FALSE;
    }

    Columns = FrontendScreen.Resolution.X;
    Rows = FrontendScreen.Resolution.Y;

    Buffer = RtlAllocateHeap(RtlGetProcessHeap(), 0, (Columns + 2) * Rows * sizeof(WCHAR) + sizeof(WCHAR));
    if (Buffer == NULL)
    {
        LeaveCriticalSection(&FrontendScreenLock);
        return FALSE;
    }

    for (Row = 0; Row < Rows; Row++)
    {
        CHAR Line[VDM_MAX_TEXT_COLUMNS];
        ULONG LineLength = Columns;

        for (Col = 0; Col < Columns; Col++)
        {
            Line[Col] = (CHAR)FrontendScreen.Cells[Row * VDM_MAX_TEXT_COLUMNS + Col].Char;
        }

        while (LineLength > 0 && Line[LineLength - 1] == ' ') LineLength--;

        MultiByteToWideChar(437, 0, Line, LineLength, Buffer + Position, (INT)(Columns + 2));
        Position += LineLength;
        Buffer[Position++] = L'\r';
        Buffer[Position++] = L'\n';
    }

    Buffer[Position] = UNICODE_NULL;

    LeaveCriticalSection(&FrontendScreenLock);

    if (OpenClipboard(Frontend->Window))
    {
        Data = GlobalAlloc(GMEM_MOVEABLE, (Position + 1) * sizeof(WCHAR));
        if (Data != NULL)
        {
            PWCHAR Destination = GlobalLock(Data);

            if (Destination != NULL)
            {
                RtlCopyMemory(Destination, Buffer, (Position + 1) * sizeof(WCHAR));
                GlobalUnlock(Data);

                if (EmptyClipboard()) Success = (SetClipboardData(CF_UNICODETEXT, Data) != NULL);
            }

            if (!Success) GlobalFree(Data);
        }

        CloseClipboard();
    }

    RtlFreeHeap(RtlGetProcessHeap(), 0, Buffer);
    return Success;
}

/* EOF */
