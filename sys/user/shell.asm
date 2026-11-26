bits 64
default rel

; Syscalls (int 0x80):
;   rax=0 write    (rsi=buf, rdx=len)
;   rax=1 readline (rsi=buf, rdx=cap) -> rax=len
;   rax=2 exec     (rsi=cmd string)
;   rax=4 prompt

start:
.loop:
    mov rax, 4
    int 0x80

    mov rax, 1
    lea rsi, [rel buffer]
    mov rdx, 128
    int 0x80

    test rax, rax
    jz .loop

    ; skip if first byte is NUL (empty line)
    cmp byte [rel buffer], 0
    je .loop

    mov rax, 2
    lea rsi, [rel buffer]
    int 0x80

    jmp .loop


buffer: times 128 db 0
