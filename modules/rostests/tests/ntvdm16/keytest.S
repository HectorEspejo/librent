/*
 * PROJECT:     librent NTVDM test harness
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Print a message on the VGA screen, wait for a key and report
 *              its scan code and character through the BIOS
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

#include <asm.inc>

.code16
.text

    mov si, offset WelcomeMessage
    call PrintString

    mov ah, HEX(00)
    int HEX(16)

    mov byte ptr [ScanCode], ah
    mov byte ptr [AsciiChar], al

    mov si, offset ScanPrefix
    call PrintString
    mov al, byte ptr [ScanCode]
    call PrintHex

    mov si, offset CharPrefix
    call PrintString
    mov al, byte ptr [AsciiChar]
    call PrintHex

    mov si, offset CrLf
    call PrintString

    mov ax, HEX(4C00)
    int HEX(21)

    hlt

PrintString:
    lodsb
    test al, al
    jz PrintStringDone
    mov ah, HEX(0E)
    mov bx, HEX(0007)
    int HEX(10)
    jmp PrintString
PrintStringDone:
    ret

PrintHex:
    push ax
    mov cl, al
    shr al, 4
    call PrintHexDigit
    mov al, cl
    and al, HEX(0F)
    call PrintHexDigit
    pop ax
    ret

PrintHexDigit:
    and al, HEX(0F)
    add al, HEX(30)
    cmp al, HEX(39)
    jbe PrintHexDigitOk
    add al, 7
PrintHexDigitOk:
    mov ah, HEX(0E)
    mov bx, HEX(0007)
    int HEX(10)
    ret

ScanCode:
    .byte 0

AsciiChar:
    .byte 0

WelcomeMessage:
    .ascii "KEYTEST - PRESS A KEY", CR, LF
    .byte 0

ScanPrefix:
    .ascii "SCAN="
    .byte 0

CharPrefix:
    .ascii " CHAR="
    .byte 0

CrLf:
    .ascii CR, LF
    .byte 0

/* EOF */
