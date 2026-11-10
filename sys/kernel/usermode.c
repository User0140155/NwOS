#include "nwos_internal.h"
#include "memory/mmu.h"
#include "memory/pmm.h"
#include "usermode.h"
#include <stdint.h>

#include "user_shell_blob.h"

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x1B
#define GDT_USER_CODE   0x23
#define GDT_TSS         0x28

#define IDT_INTERRUPT_GATE 0x8E
#define IDT_USER_GATE      0xEE

#define USER_BLOCK_SIZE 0x200000u

#define SYS_WRITE    0
#define SYS_READLINE 1
#define SYS_EXEC     2
#define SYS_EXIT     3
#define SYS_PROMPT   4

#define SYS_LINUX_READ       0
#define SYS_LINUX_WRITE      1
#define SYS_LINUX_EXIT       60
#define SYS_LINUX_EXIT_GROUP 231

typedef struct
{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct
{
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

typedef struct
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rax;
    uint64_t vector;
    uint64_t error;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} interrupt_frame_t;

extern void *isr_stub_table[32];
extern void isr128(void);
extern void usermode_enter(uint64_t entry, uint64_t stack, uint64_t arg);
extern void usermode_enter_user(uint64_t entry, uint64_t stack);

volatile uint64_t usermode_exit_pending = 0;

static gdt_entry_t gdt[7];
static gdt_ptr_t gdt_ptr;
static tss_entry_t tss;
static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

static uint8_t syscall_stack[128 * 1024] __attribute__((aligned(16)));
static uint8_t native_syscall_stack[128 * 1024] __attribute__((aligned(16)));
static uint64_t user_block_base;
static uint64_t user_block_end;
static uint64_t native_block_base;
static uint64_t native_block_size;
static int native_active;
static int native_abi;
static uint64_t native_exit_code;

static void gdt_set_gate(int index, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t granularity)
{
    gdt[index].base_low = (uint16_t)(base & 0xFFFFu);
    gdt[index].base_middle = (uint8_t)((base >> 16) & 0xFFu);
    gdt[index].base_high = (uint8_t)((base >> 24) & 0xFFu);
    gdt[index].limit_low = (uint16_t)(limit & 0xFFFFu);
    gdt[index].granularity = (uint8_t)(((limit >> 16) & 0x0Fu) |
                                       (granularity & 0xF0u));
    gdt[index].access = access;
}

static void gdt_init(void)
{
    uint64_t tss_base = (uint64_t)(uintptr_t)&tss;
    uint32_t tss_limit = (uint32_t)(sizeof(tss) - 1u);

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0, 0x9A, 0xAF);
    gdt_set_gate(2, 0, 0, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0, 0xF2, 0xCF);
    gdt_set_gate(4, 0, 0, 0xFA, 0xAF);
    gdt_set_gate(5, (uint32_t)tss_base, tss_limit, 0x89, 0x00);
    gdt_set_gate(6, (uint32_t)(tss_base >> 32), 0, 0x00, 0x00);

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1u);
    gdt_ptr.base = (uint64_t)(uintptr_t)&gdt;

    __asm__ volatile ("lgdt %0" : : "m"(gdt_ptr) : "memory");
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "rax", "memory");

    uint16_t tss_selector = GDT_TSS;
    __asm__ volatile ("ltr %0" : : "r"(tss_selector));
}

static void tss_init(void)
{
    uint8_t *bytes = (uint8_t *)&tss;

    for (uint32_t i = 0; i < sizeof(tss); ++i)
        bytes[i] = 0;

    tss.rsp0 = (uint64_t)(uintptr_t)(syscall_stack + sizeof(syscall_stack));
    tss.iomap_base = (uint16_t)sizeof(tss);
}

static void idt_set_gate(int vector, uint64_t handler, uint16_t selector,
                         uint8_t attributes)
{
    idt[vector].offset_low = (uint16_t)(handler & 0xFFFFu);
    idt[vector].selector = selector;
    idt[vector].ist = 0;
    idt[vector].type_attr = attributes;
    idt[vector].offset_mid = (uint16_t)((handler >> 16) & 0xFFFFu);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFFu);
    idt[vector].zero = 0;
}

static void idt_init(void)
{
    for (int i = 0; i < 256; ++i)
    {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attr = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].zero = 0;
    }

    for (int i = 0; i < 32; ++i)
        idt_set_gate(i, (uint64_t)(uintptr_t)isr_stub_table[i],
                     GDT_KERNEL_CODE, IDT_INTERRUPT_GATE);

    idt_set_gate(0x80, (uint64_t)(uintptr_t)isr128,
                 GDT_KERNEL_CODE, IDT_USER_GATE);

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1u);
    idt_ptr.base = (uint64_t)(uintptr_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr) : "memory");
}

static int user_range_ok(uint64_t address, uint64_t length)
{
    if (address < user_block_base || address > user_block_end)
        return 0;
    if (length > user_block_end - address)
        return 0;
    return 1;
}

static uint64_t user_write(uint64_t address, uint64_t length)
{
    if (!user_range_ok(address, length))
        return 0;

    const char *text = (const char *)(uintptr_t)address;
    for (uint64_t i = 0; i < length; ++i)
        putchar_os(text[i]);

    return length;
}

static uint64_t user_readline(uint64_t address, uint64_t capacity)
{
    if (!user_range_ok(address, capacity) || capacity < 2)
        return 0;
    if (capacity > 4096)
        capacity = 4096;

    console_f7_enabled = 1;
    read_line((char *)(uintptr_t)address, (int)capacity);
    console_f7_enabled = 0;

    const char *text = (const char *)(uintptr_t)address;
    uint64_t length = 0;
    while (length < capacity && text[length] != '\0')
        ++length;
    return length;
}

static uint64_t user_prompt(void)
{
    set_color(prompt_color);
    print(cfg_user[0] ? cfg_user : "user");
    putchar_os('@');
    print(cfg_hostname[0] ? cfg_hostname : "nwos");
    print("~$ ");
    set_color(base_color);
    return 1;
}

static uint64_t user_exec(uint64_t address)
{
    if (address < user_block_base || address >= user_block_end)
        return 0;

    const char *text = (const char *)(uintptr_t)address;
    uint64_t limit = user_block_end - address;
    uint64_t length = 0;

    while (length < limit && text[length] != '\0')
        ++length;
    if (length == limit)
        return 0;

    shell_execute(text);
    return 0;
}

static uint64_t native_syscall(interrupt_frame_t *frame)
{
    switch (frame->rax)
    {
        case SYS_LINUX_READ:
            return user_readline(frame->rsi, frame->rdx);
        case SYS_LINUX_WRITE:
            return user_write(frame->rsi, frame->rdx);
        case SYS_LINUX_EXIT:
        case SYS_LINUX_EXIT_GROUP:
            native_exit_code = frame->rdi;
            usermode_exit_pending = 1;
            return 0;
        default:
            return (uint64_t)-1;
    }
}

uint64_t isr_dispatch(uint64_t vector, interrupt_frame_t *frame)
{
    if (vector == 0x80)
    {
        if (native_abi)
            return native_syscall(frame);

        switch (frame->rax)
        {
            case SYS_WRITE:
                return user_write(frame->rsi, frame->rdx);
            case SYS_READLINE:
                return user_readline(frame->rsi, frame->rdx);
            case SYS_EXEC:
                return user_exec(frame->rsi);
            case SYS_EXIT:
                return 0;
            case SYS_PROMPT:
                return user_prompt();
            default:
                return (uint64_t)-1;
        }
    }

    if (native_active)
    {
        print_error("\n[NWO64] CPU exception ");
        print_int((int)vector);
        if (vector == 14)
        {
            uint64_t cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            print(" at ");
            print_uint64(cr2);
        }
        print(" error=");
        print_uint64(frame->error);
        print("\n");

        native_exit_code = (uint64_t)-1;
        usermode_exit_pending = 1;
        return 0;
    }

    print_error("\n[USER] CPU exception ");
    print_int((int)vector);
    if (vector == 14)
    {
        uint64_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        print(" at ");
        print_uint64(cr2);
    }
    print(" error=");
    print_uint64(frame->error);
    print("\n");

    while (1)
        __asm__ volatile ("cli; hlt");
}

int usermode_run_native(const uint8_t *image, uint32_t image_size,
                        uint32_t bss_size, uint32_t entry,
                        const uint32_t *relocs, uint32_t reloc_count,
                        uint64_t *exit_code)
{
    uint64_t saved_block_base;
    uint64_t saved_block_end;
    uint64_t saved_rsp0;
    uint64_t load_size;

    if (!pmm_is_ready() || image == 0 || image_size == 0)
        return 0;

    load_size = (uint64_t)image_size + bss_size;

    if (load_size + 0x10000u > USER_BLOCK_SIZE)
        return 0;

    if (entry >= image_size)
        return 0;

    if (native_block_base == 0)
    {
        uint64_t phys = 0;
        void *block = pmm_alloc_dma32(USER_BLOCK_SIZE, USER_BLOCK_SIZE, &phys);

        if (!block || phys == 0)
            return 0;

        native_block_base = phys;
        native_block_size = USER_BLOCK_SIZE;
        mmu_mark_user(phys, USER_BLOCK_SIZE);
    }

    uint8_t *base = (uint8_t *)(uintptr_t)native_block_base;

    for (uint64_t i = 0; i < native_block_size; ++i)
        base[i] = 0;

    for (uint32_t i = 0; i < image_size; ++i)
        base[i] = image[i];

    for (uint32_t i = 0; i < reloc_count; ++i)
    {
        uint32_t type = relocs[i] >> 30;
        uint32_t offset = relocs[i] & 0x3FFFFFFFu;

        if (type == 1)
        {
            if ((uint64_t)offset + 8u > image_size)
                return 0;

            uint64_t *value = (uint64_t *)(uintptr_t)(base + offset);
            *value += native_block_base;
        }
        else if (type == 2)
        {
            if ((uint64_t)offset + 4u > image_size)
                return 0;

            uint32_t *value = (uint32_t *)(uintptr_t)(base + offset);
            *value += (uint32_t)native_block_base;
        }
        else
        {
            return 0;
        }
    }

    saved_block_base = user_block_base;
    saved_block_end = user_block_end;
    saved_rsp0 = tss.rsp0;

    user_block_base = native_block_base;
    user_block_end = native_block_base + native_block_size;
    tss.rsp0 = (uint64_t)(uintptr_t)(native_syscall_stack +
                                     sizeof(native_syscall_stack));

    native_active = 1;
    native_abi = 1;
    native_exit_code = 0;
    usermode_exit_pending = 0;

    usermode_enter_user(native_block_base + entry,
                        native_block_base + native_block_size);

    native_active = 0;
    native_abi = 0;
    usermode_exit_pending = 0;

    tss.rsp0 = saved_rsp0;
    user_block_base = saved_block_base;
    user_block_end = saved_block_end;

    if (exit_code)
        *exit_code = native_exit_code;

    return 1;
}

int usermode_enter_shell(void)
{
    uint64_t phys = 0;
    void *block;

    if (!pmm_is_ready())
        return 0;

    block = pmm_alloc_dma32(USER_BLOCK_SIZE, USER_BLOCK_SIZE, &phys);
    if (!block || phys == 0)
        return 0;

    uint8_t *bytes = (uint8_t *)(uintptr_t)phys;
    for (uint32_t i = 0; i < USER_BLOCK_SIZE; ++i)
        bytes[i] = 0;

    for (uint32_t i = 0; i < user_shell_blob_len; ++i)
        bytes[i] = user_shell_blob[i];

    user_block_base = phys;
    user_block_end = phys + USER_BLOCK_SIZE;

    mmu_mark_user(phys, USER_BLOCK_SIZE);

    tss_init();
    gdt_init();
    idt_init();

    print_success("[OK] Ring 3 shell prepared.\n");

    uint64_t entry = phys;
    uint64_t stack = phys + USER_BLOCK_SIZE;

    usermode_enter(entry, stack, 0);
    return 0;
}

