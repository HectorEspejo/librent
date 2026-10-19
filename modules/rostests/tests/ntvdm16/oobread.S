/*
 * PROJECT:     librent NTVDM test harness
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Read through DOS INT 21h into a buffer placed at the top of
 *              the addressable guest memory, at the last paragraphs a
 *              real-mode far pointer can reach
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

#include <asm.inc>

.code16
.text

    mov dx, offset TestFileName
    mov ax, HEX(3D00)
    int HEX(21)
    jc OpenFailed

    mov bx, ax

    push ds
    mov ax, HEX(FFFF)
    mov ds, ax
    mov dx, HEX(FFF0)
    mov cx, HEX(0020)
    mov ah, HEX(3F)
    int HEX(21)
    pop ds
    jc ReadRejected

    mov dx, offset BoundsOkMessage
    mov ah, HEX(09)
    int HEX(21)

    mov ax, HEX(4C00)
    int HEX(21)

ReadRejected:
    mov dx, offset BoundsFailedMessage
    mov ah, HEX(09)
    int HEX(21)

    mov ax, HEX(4C01)
    int HEX(21)

OpenFailed:
    mov dx, offset OpenFailedMessage
    mov ah, HEX(09)
    int HEX(21)

    mov ax, HEX(4C02)
    int HEX(21)

TestFileName:
    .asciz "OOBREAD.COM"

BoundsOkMessage:
    .ascii "BOUNDS OK", CR, LF, "$"

BoundsFailedMessage:
    .ascii "BOUNDS FAILED", CR, LF, "$"

OpenFailedMessage:
    .ascii "OPEN FAILED", CR, LF, "$"

/* EOF */
