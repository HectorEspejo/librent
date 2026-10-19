/*
 * PROJECT:     librent NTVDM test harness
 * LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
 * PURPOSE:     Load a user font with INT 10h AX=1110h and print with it
 * COPYRIGHT:   Copyright 2026 Hector Espejo
 */

#include <asm.inc>

.code16
.text

    /* Get the address of the 8x16 ROM font (ES:BP) */
    mov ax, HEX(1130)
    mov bh, HEX(04)
    int HEX(10)

    /* Copy the glyph of 'X' (char 0x58) into our table */
    push es
    push bp
    pop si
    pop ds

    mov ax, HEX(58) * 16
    add si, ax

    push cs
    pop es
    mov di, offset UserFont
    mov cx, HEX(0010)
    cld
    rep movsb

    /* Turn the glyph into a filled block */
    mov di, offset UserFont
    mov cx, HEX(0010)
FillLoop:
    mov byte ptr es:[di], HEX(FF)
    inc di
    loop FillLoop

    /* Load one user pattern for 'X' (count 1, height 16, first char 0x58) */
    mov ax, HEX(1110)
    mov bh, HEX(00)
    mov bl, HEX(00)
    mov cx, HEX(0110)
    mov dx, HEX(0058)
    push cs
    pop es
    mov bp, offset UserFont
    int HEX(10)

    /* Print the modified character */
    mov al, HEX(58)
    mov ah, HEX(0E)
    mov bx, HEX(0007)
    int HEX(10)

    push cs
    pop ds
    mov si, offset Message
    call PrintString

    /* Wait for a key so the window can be inspected */
    mov ah, HEX(00)
    int HEX(16)

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

UserFont:
    .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

Message:
    .ascii " FONT TEST", CR, LF
    .byte 0

/* EOF */
