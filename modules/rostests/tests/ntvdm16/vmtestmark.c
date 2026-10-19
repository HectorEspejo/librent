/*
 * PROJECT:     librent NTVDM test harness
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Emit librent-vmtest marks through the debug output so that the
 *              harness can watch them in the serial log
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

#include <windows.h>
#include <stdio.h>

int
wmain(int argc,
      WCHAR* argv[])
{
    CHAR Line[512] = "";

    if (argc >= 3 && _wcsicmp(argv[1], L"keys") == 0)
    {
        _snprintf(Line, sizeof(Line), "LIBRENT-VMTEST-KEYS %ls", argv[2]);
    }
    else if (argc >= 2 && _wcsicmp(argv[1], L"done") == 0)
    {
        strcpy(Line, "LIBRENT-VMTEST-DONE");
    }
    else if (argc >= 2)
    {
        WideCharToMultiByte(CP_ACP, 0, argv[1], -1, Line, sizeof(Line), NULL, NULL);
    }

    if (Line[0] != ANSI_NULL)
    {
        OutputDebugStringA(Line);
    }

    return 0;
}

/* EOF */
