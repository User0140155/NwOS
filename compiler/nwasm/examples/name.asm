; section of data (vars, etc)
section data
    msg: db 'What your name?: ', 0              ; Message that contains 'wht your name?:'
    len: equ $ - msg                            ; lenght of previous message

section bss                                     ; Data of buffer for input
    name: bytes 64                              ; Buffer in 64 bytes

section code                                    ; section of code (code, start, etc)
    entry _start                                ; global _start (like in nasm), just uses entry

_start:                                         ; the start (entry)
    call print_what_your_name                   ; calling function
    call input_name                             ; calling function
    jmp end                                     ; jumping to end

print_what_your_name:                           ; The function
    mov rax, 1                                  ; Mov register 'rax' to 1 (output)
    mov rdi, 1                                  ; Mov register 'rdi' to 1 (output)
    mov rsi, msg                                ; Mov register 'rsi' to message
    mov rdx, len                                ; Mov register 'rdx' to length (len equ..)
    syscall                                     ; Calling NwOS kernel
    ret                                         ; Returning from function

input_name:                                     ; The function
    mov rax, 0                                  ; Mov register 'rax' to 0 (input)
    mov rdi, 0                                  ; Mov register 'rdi' to 0 (input)
    mov rsi, name                               ; Mov register 'rsi' to name (buffer of bytes, 64)
    mov rdx, 64                                 ; Mov register 'rdx' to 64 (also buffer)
    syscall                                     ; Calling NwOS kernel
    ret                                         ; Returning from function

end:                                            ; The end
    mov rax, 60                                 ; Mov register 'rax' to 60 (exit)
    mov rdi, 0                                  ; Mov register 'rdi' to 0 (exit)
    syscall                                     ; Calling NwOS kernel
