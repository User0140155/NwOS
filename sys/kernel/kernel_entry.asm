section .multiboot_header progbits alloc noexec nowrite align=8
align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd 0x100000000 - (0xE85250D6 + 0 + (header_end - header_start))

    align 8
    dw 5
    dw 1
    dd 20
    dd 1280
    dd 720
    dd 32

    align 8
    dw 0
    dw 0
    dd 8
header_end:

global _start
extern kernel_main
extern __bss_start
extern __bss_end

PML4_COUNT      equ 8
PD_PAGE_COUNT   equ PML4_COUNT * 512
PT_TOTAL_SIZE   equ 4096 * (1 + PML4_COUNT + PD_PAGE_COUNT)
PTE_PRESENT_RW  equ 0x03
PDE_2M_RW       equ 0x83
PDE_UNCACHED    equ 0x10
MMIO_FIRST_PDE  equ 1536
MMIO_LAST_PDE   equ 2048

section .data
align 4
global multiboot_info_ptr
multiboot_info_ptr: dd 0

align 8
usermode_kernel_rsp: dq 0

section .text.start progbits alloc exec nowrite align=16
bits 32

_start:
    mov [multiboot_info_ptr], ebx
    cli
    cld
    mov esp, 0x90000

    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no_long_mode

    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode

    mov edi, pml4
    mov ecx, PT_TOTAL_SIZE / 4
    xor eax, eax
    rep stosd

    xor ecx, ecx
.fill_pml4:
    mov eax, ecx
    shl eax, 12
    add eax, pdpt
    or  eax, PTE_PRESENT_RW
    mov [pml4 + ecx * 8], eax
    mov dword [pml4 + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, PML4_COUNT
    jne .fill_pml4

    xor ecx, ecx
.fill_pdpt:
    mov eax, ecx
    shl eax, 12
    add eax, pd
    or  eax, PTE_PRESENT_RW
    mov [pdpt + ecx * 8], eax
    mov dword [pdpt + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, PD_PAGE_COUNT
    jne .fill_pdpt

    xor ecx, ecx
.fill_pd:
    mov eax, ecx
    shl eax, 21
    mov edx, ecx
    shr edx, 11
    or  eax, PDE_2M_RW
    cmp ecx, MMIO_FIRST_PDE
    jb .cacheable
    cmp ecx, MMIO_LAST_PDE
    jae .cacheable
    or  eax, PDE_UNCACHED
.cacheable:
    mov [pd + ecx * 8], eax
    mov [pd + ecx * 8 + 4], edx
    inc ecx
    cmp ecx, PD_PAGE_COUNT * 512
    jne .fill_pd

    mov eax, cr4
    or  eax, 1 << 5
    mov cr4, eax

    mov eax, pml4
    mov cr3, eax

    mov ecx, 0xC0000080
    rdmsr
    or  eax, 1 << 8
    wrmsr

    lgdt [gdt64_ptr]

    mov eax, cr0
    or  eax, 1 << 31
    mov cr0, eax

    push dword 0x08
    push dword long_mode_entry
    retf

.no_long_mode:
    mov edi, 0xB8000 + 160 * 4
    mov esi, msg_no_long_mode
    mov ah, 0x4F
.print:
    lodsb
    test al, al
    jz .halt
    mov [edi], ax
    add edi, 2
    jmp .print
.halt:
    cli
    hlt
    jmp .halt

msg_no_long_mode:
    db "NwOS: this CPU has no 64-bit mode (QEMU: use qemu-system-x86_64), REALPC: Try to find old version (<1.3.5) in Releases of: https://github.com/User0140155/NwOS/ , make sure there's correct drivers and legacy BIOS for previous versions", 0

align 16
gdt64:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dd gdt64

bits 64

align 16
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor eax, eax
    mov fs, ax
    mov gs, ax

    lea rsp, [rel stack_top]
    cld

    mov rax, cr0
    and rax, ~(1 << 2)
    and rax, ~(1 << 3)
    or  rax, (1 << 1)
    or  rax, (1 << 5)
    mov cr0, rax

    mov rax, cr4
    or  rax, (1 << 9) | (1 << 10)
    mov cr4, rax

    fninit

    lea rdi, [rel __bss_start]
    lea rcx, [rel __bss_end]
    sub rcx, rdi
    xor eax, eax
    rep stosb

    xor ebp, ebp
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

section .pagetables nobits alloc noexec write align=4096
pml4:   resb 4096
pdpt:   resb 4096 * PML4_COUNT
pd:     resb 4096 * PD_PAGE_COUNT

section .stack nobits alloc noexec write align=16
stack_bottom:
    resb 128 * 1024
stack_top:
section .text
bits 64

extern isr_dispatch
extern usermode_exit_pending

%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31
ISR_NOERR 128

isr_common:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, [rsp + 120]
    mov rsi, rsp
    mov rbp, rsp
    and rsp, -16
    call isr_dispatch
    mov rsp, rbp
    cmp qword [rel usermode_exit_pending], 0
    jne usermode_return_to_kernel
    mov [rsp + 112], rax

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 16
    iretq

global usermode_enter
usermode_enter:
    mov r8, rdi
    mov r9, rsi
    mov r10, rdx

    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x1B
    push r9
    push qword 0x2
    push qword 0x23
    push r8
    iretq

global usermode_enter_user
usermode_enter_user:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [rel usermode_kernel_rsp], rsp
    mov r8, rdi
    mov r9, rsi

    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x1B
    push r9
    push qword 0x2
    push qword 0x23
    push r8
    iretq

usermode_return_to_kernel:
    mov qword [rel usermode_exit_pending], 0
    cld
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov rsp, [rel usermode_kernel_rsp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

section .rodata
align 8
global isr_stub_table
isr_stub_table:
    dq isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    dq isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    dq isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    dq isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
