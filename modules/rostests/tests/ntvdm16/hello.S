/*
 * PROJECT:     librent NTVDM test harness
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Print a greeting through DOS INT 21h and exit with 0
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

#include <asm.inc>

.code16
.text

    mov dx, offset HelloMessage
    mov ah, HEX(09)
    int HEX(21)

    mov ax, HEX(4C00)
    int HEX(21)

HelloMessage:
    .ascii "HOLA DESDE DOS", CR, LF, "$"

/* EOF */
