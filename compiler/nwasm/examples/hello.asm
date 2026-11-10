; section of data
section data
    msg: db 'hello world', 10, 0        ; Contains 'hello world', and new line
    msg_len: equ $ - msg                ; lenght of message

section code                            ; code section
    entry _start                        ; 'global _start'

_start:                                 ; Main code
    mov rax, 1                          ; Mov register 'rax' to 1
    mov rdi, 1                          ; Mov register 'rdi' to 1
    mov rsi, msg                        ; Mov register 'rsi' to message (hello world)
    mov rdx, msg_len                    ; Mov register 'rdx' to lenght (msg_len)
    syscall                             ; Call kernel (NwOS)

    mov rax, 60                         ; Exit
    mov rdi, 0                          ; Return 0
    syscall                             ; Call kernel
