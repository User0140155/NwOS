ENTRY(_start)

SECTIONS
{
    . = 16M;
    __kernel_start = .;

    .multiboot_header : ALIGN(8)
    {
        KEEP(*(.multiboot_header))
    }

    .text : ALIGN(16)
    {
        *(.text.start)
        *(.text*)
    }

    .rodata : ALIGN(16)
    {
        *(.rodata*)
    }

    .data : ALIGN(16)
    {
        *(.data*)
    }

    .bss (NOLOAD) : ALIGN(16)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }

    .pagetables (NOLOAD) : ALIGN(4096)
    {
        *(.pagetables)
    }

    .stack (NOLOAD) : ALIGN(16)
    {
        *(.stack)
    }

    __kernel_end = .;

    ASSERT(__kernel_end > __kernel_start, "empty kernel image")
    ASSERT(__kernel_end <= 0x100000000, "kernel image exceeds current 4 GiB identity map")
}