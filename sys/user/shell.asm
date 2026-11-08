bits 64
default rel

start:
    mov r12, rdi

.loop:
    mov rax, 4
    int 0x80

    mov rax, 1
    lea rsi, [rel buffer]
    mov rdx, 128
    int 0x80

    test rax, rax
    jz .loop

    mov rax, 2
    lea rsi, [rel buffer]
    int 0x80

    jmp .loop

buffer: times 128 db 0
