#include "nwo_vm.h"

void print(const char* text);
void print_int(int number);
void putchar_os(char c);

void read_line(char* buffer, int max);

void clear(void);
void set_color(unsigned char color);

int keyboard_getkey(void);
int keyboard_pollkey(void);
int mouse_x(void);
int mouse_y(void);
int mouse_buttons(void);
void poweroff(void);
void reboot(void);
void get_time(int* hour, int* minute, int* second);
int nwf_tz_get(void);
void nwf_tz_set(int hours);
int nwf_cpu_load(void);
int nwf_gpu_load(void);
int nwf_fs_create(const char* name);
int nwf_fs_mkdir(const char* name);
int nwf_fs_delete(const char* name);
int nwf_fs_write(const char* name, const char* text);
int nwf_fs_read(const char* name, char* buffer, unsigned int max_size);
void fb_draw_cursor(int x, int y);
void fb_draw_rect(int x, int y, int w, int h, unsigned int color);
int gui_term_shi(int op, int a);
const char* gui_term_shs(int op, int a);
void fb_draw_gradient(int x, int y, int w, int h, unsigned int top, unsigned int bottom);
void fb_draw_rgrad(int x, int y, int w, int h, int r, unsigned int top, unsigned int bottom);
void fb_draw_glass(int x, int y, int w, int h, int r, unsigned int color, int alpha);
void fb_draw_orb(int cx, int cy, int r, unsigned int color);
void fb_draw_ellipse(int x, int y, int w, int h, unsigned int c1, unsigned int c2, int alpha);
void fb_draw_bubble(int cx, int cy, int r, int alpha);
void fb_draw_frame(int x, int y, int w, int h, unsigned int color);
void fb_draw_cube_default(unsigned int color);
void fb_draw_window(int x, int y, int w, int h, unsigned int color, const char* title);
void fb_draw_text(int x, int y, const char* text, unsigned int color);
void fb_draw_int(int x, int y, int value, unsigned int color);
void fb_draw_wallpaper(void);
void fb_present(void);
void fb_set_backbuffer_enabled(int enabled);
void graphics_begin(void);
void sleep(unsigned int ms);
unsigned int nwf_ram_mb(void);
unsigned int nwf_disk_used_kb(void);
unsigned int nwf_disk_files(void);
const char* nwf_random_word(void);
unsigned long long timer_ms(void);


#define OP_LOAD_ARR    0x24
#define OP_STORE_ARR   0x25

#define OP_FS_COUNT    0xA0
#define OP_FS_NAME     0xA1
#define OP_FS_TYPE     0xA2
#define OP_FS_SIZE     0xA3
#define OP_FS_ENTER    0xA4
#define OP_FS_UP       0xA5
#define OP_FS_PATH     0xA6
#define OP_RAM_MB      0xA7
#define OP_UPTIME_SEC  0xA8
#define OP_DISK_KB     0xA9
#define OP_DISK_FILES  0xAA
#define OP_TIME_HOUR   0xAB
#define OP_TIME_MINUTE 0xAC
#define OP_TIME_SECOND 0xAD
#define OP_TZ_GET      0xAE
#define OP_TZ_SET      0xAF
#define OP_CPU_LOAD    0xB0
#define OP_GPU_LOAD    0xB1
#define OP_CURSOR      0xB2
#define OP_GRADIENT    0xC0
#define OP_RGRAD       0xC1
#define OP_GLASS       0xC2
#define OP_ORB         0xC3
#define OP_ELLIPSE     0xC4
#define OP_BUBBLE      0xC5
#define OP_SHI         0xC6
#define OP_SHS         0xC7
#define OP_FS_CREATE   0xB3
#define OP_FS_DELETE   0xB4
#define OP_FS_WRITE    0xB5
#define OP_FS_MKDIR    0xB6
#define OP_BUF_CLEAR   0xB7
#define OP_BUF_LEN     0xB8
#define OP_BUF_PUT     0xB9
#define OP_BUF_BACK    0xBA
#define OP_BUF_TEXT    0xBB
#define OP_BUF_LOAD    0xBC
#define OP_BUF_SAVE    0xBD
#define OP_POLLKEY     0xBE
#define OP_RANDOM_WORD 0xBF

#define OP_JUMP         0x80
#define OP_JUMP_FALSE   0x81

#define NWO_STACK_SIZE      128
#define NWO_VARIABLES       768
#define NWO_ARRAY_POOL_SIZE 4096
#define NWO_MAX_FUNCTIONS   128
#define NWO_SHADOW_POOL     2048

extern int  fs_dir_count(void);
extern int  fs_dir_entry(int, const char**, int*, unsigned int*);
extern const char* fs_current_path(void);
extern void fs_cd(const char* name);
int  fs_dir_count(void);
int  fs_dir_entry(int index, const char** name, int* type, unsigned int* size);
const char* fs_current_path(void);
void fs_cd(const char* name);
#define NWO_NAME_SIZE       32
#define NWO_INPUT_SIZE      256

#define NWO_MAX_CALL_DEPTH  16
#define NWO_MAX_PARAMS      16

static char vm_string_storage[NWO_VARIABLES][NWO_INPUT_SIZE];

#define VM_LITERAL_SLOTS 16
static char vm_literal_storage[VM_LITERAL_SLOTS][NWO_INPUT_SIZE];
static int  vm_literal_next = 0;

#define VM_FILE_BUF_SIZE 4096
static char vm_file_buf[VM_FILE_BUF_SIZE];
static int  vm_file_len = 0;

#define OP_NOP          0x00

#define OP_PUSH_INT     0x10
#define OP_PUSH_FLOAT   0x11
#define OP_PUSH_STRING  0x12
#define OP_PUSH_CHAR    0x13
#define OP_PUSH_BOOL    0x14
#define OP_POP          0x15
#define OP_PUSH_INT8    0x17
#define OP_PUSH_INT32   0x18

#define OP_LOAD         0x20
#define OP_STORE        0x21
#define OP_DECLARE      0x22
#define OP_DECLARE_ARR  0x23

#define OP_ADD          0x30
#define OP_SUB          0x31
#define OP_MUL          0x32
#define OP_DIV          0x33

#define OP_EQ           0x40
#define OP_NEQ          0x41
#define OP_LT           0x42
#define OP_GT           0x43
#define OP_LTE          0x44
#define OP_GTE          0x45
#define OP_AND          0x46
#define OP_OR           0x47
#define OP_NOT          0x48

#define OP_STRCMP       0x49
#define OP_STRNCMP      0x4A

#define OP_OUT          0x50
#define OP_OUT_ENDL     0x51
#define OP_IN           0x52
#define OP_LINE_IN      0x53
#define OP_IN_STORE     0x54

#define OP_TIME         0x60
#define OP_COLOR        0x61
#define OP_GETKEY       0x62
#define OP_CLEAR        0x63
#define OP_RECT         0x64
#define OP_COLOR_PUSH   0x65
#define OP_COLOR_POP    0x66
#define OP_SLEEP        0x67
#define OP_GETKEY_FILTER 0x68
#define OP_ASM_HLT      0x69
#define OP_DRAW_FRAME   0x6A
#define OP_DRAW_CUBE    0x6B
#define OP_DRAW_WINDOW  0x6C
#define OP_DRAW_TEXT    0x6D
#define OP_MOUSE_X      0x6E
#define OP_MOUSE_Y      0x6F

#define OP_FUNC_BEGIN   0x70
#define OP_FUNC_END     0x71
#define OP_CALL         0x72
#define OP_RETURN       0x73
#define OP_RETURN_VOID  0x74
#define OP_POWEROFF     0x75
#define OP_REBOOT       0x76
#define OP_MOUSE_BUTTONS 0x77
#define OP_WALLPAPER    0x78
#define OP_DRAW_INT     0x79
#define OP_FLUSH        0x7A

#define OP_JUMP         0x80
#define OP_JUMP_FALSE   0x81

#define OP_HALT         0xFF

typedef enum
{
    VM_NONE = 0,
    VM_INT,
    VM_FLOAT,
    VM_BOOL,
    VM_CHAR,
    VM_STRING

} VmValueType;


typedef struct
{
    VmValueType type;

    unsigned int string_length;

    union
    {
        int integer;

        double floating;

        unsigned int boolean;
        unsigned int character;

        const char* string;

    } as;

} VmValue;

typedef struct
{
    int active;
    unsigned char declared_type;
    int is_array;
    int array_start;
    unsigned int array_size;
    int binding_frame;
    VmValue value;
} VmVariable;

static VmValue vm_array_pool[NWO_ARRAY_POOL_SIZE];
static int     vm_array_pool_used = 0;

/* Shadow records keep the previous binding of a slot so that recursion
   restores outer values when a frame returns (stack-disciplined pool). */
typedef struct
{
    int slot;
    int prev_active;
    unsigned char prev_declared_type;
    int prev_is_array;
    int prev_array_start;
    unsigned int prev_array_size;
    int prev_binding_frame;
    VmValue prev_value;
} VmShadow;

static VmShadow vm_shadow_pool[NWO_SHADOW_POOL];
static int vm_shadow_used = 0;

#define NWO_TYPE_VOID   0
#define NWO_TYPE_STRING 7
#define NWO_ARRAY_STRING_SLOTS 64
static char vm_array_string_storage[NWO_ARRAY_STRING_SLOTS][NWO_INPUT_SIZE];
static int  vm_array_string_used = 0;

typedef struct
{
    unsigned int return_ip;
    int array_pool_mark;
    int array_string_mark;
    int shadow_mark;

} VmCallFrame;

typedef struct
{
    unsigned int body_ip;
    unsigned char param_count;

    unsigned int param_slots[NWO_MAX_PARAMS];
    unsigned char param_types[NWO_MAX_PARAMS];
    unsigned char param_arrays[NWO_MAX_PARAMS];

    char name[NWO_NAME_SIZE];

} VmFunctionEntry;

static VmFunctionEntry vm_functions[NWO_MAX_FUNCTIONS];
static int vm_function_count = 0;
static int vm_main_index = -1;

typedef struct
{
    NwoProgram* program;

    unsigned int ip;

    VmValue stack[NWO_STACK_SIZE];
    int sp;

    VmVariable variables[NWO_VARIABLES];

    VmCallFrame call_stack[NWO_MAX_CALL_DEPTH];
    int call_sp;

    int running;

    int result;

    unsigned char text_color;

} NwoVM;


static int vm_push(
    NwoVM* vm,
    VmValue value)
{
    if (vm->sp >= NWO_STACK_SIZE)
    {
        print("[NWO VM] Stack overflow.\n");
        return 0;
    }

    vm->stack[vm->sp] = value;
    vm->sp++;

    return 1;
}


static int vm_pop(
    NwoVM* vm,
    VmValue* value)
{
    if (vm->sp <= 0)
    {
        print("[NWO VM] Stack underflow.\n");
        return 0;
    }

    vm->sp--;

    if (value != 0)
        *value = vm->stack[vm->sp];

    return 1;
}


static int vm_pop_string(
    NwoVM* vm,
    char* out,
    unsigned int max)
{
    VmValue value;
    unsigned int i;
    unsigned int n;

    if (!vm_pop(vm, &value))
        return 0;

    n = (value.type == VM_STRING) ? value.string_length : 0;
    if (max == 0)
        return 0;
    if (n > max - 1)
        n = max - 1;

    if (value.type == VM_STRING && value.as.string != 0)
    {
        for (i = 0; i < n; i++)
            out[i] = value.as.string[i];
    }
    else
    {
        n = 0;
    }

    out[n] = '\0';
    return 1;
}


static int vm_read_u8(
    NwoVM* vm,
    unsigned char* value)
{
    if (vm->ip >= vm->program->code_size)
    {
        print("[NWO VM] Unexpected end of code.\n");
        return 0;
    }

    if (value != 0)
        *value = vm->program->code[vm->ip];

    vm->ip++;

    return 1;
}


static int vm_read_u32(
    NwoVM* vm,
    unsigned int* value)
{
    unsigned char b0;
    unsigned char b1;
    unsigned char b2;
    unsigned char b3;

    if (!vm_read_u8(vm, &b0))
        return 0;

    if (!vm_read_u8(vm, &b1))
        return 0;

    if (!vm_read_u8(vm, &b2))
        return 0;

    if (!vm_read_u8(vm, &b3))
        return 0;

    if (value != 0)
    {
        *value =
            ((unsigned int)b0) |
            ((unsigned int)b1 << 8) |
            ((unsigned int)b2 << 16) |
            ((unsigned int)b3 << 24);
    }

    return 1;
}


static int vm_read_len(
    NwoVM* vm,
    unsigned int* value)
{
    if (vm->program->version >= 2)
    {
        unsigned char b;

        if (!vm_read_u8(vm, &b))
            return 0;

        if (value != 0)
            *value = (unsigned int)b;

        return 1;
    }

    return vm_read_u32(vm, value);
}

static int vm_read_u64(
    NwoVM* vm,
    unsigned long long* value)
{
    unsigned int low;
    unsigned int high;

    if (!vm_read_u32(vm, &low))
        return 0;

    if (!vm_read_u32(vm, &high))
        return 0;

    if (value != 0)
    {
        *value =
            ((unsigned long long)low) |
            ((unsigned long long)high << 32);
    }

    return 1;
}
static int vm_read_string(
    NwoVM* vm,
    const char** value,
    unsigned int* length)
{
    unsigned int len;
    unsigned int start;

    if (!vm_read_len(vm, &len))
        return 0;

    start = vm->ip;

    if (start > vm->program->code_size)
        return 0;

    if (len >
        vm->program->code_size - start)
    {
        print("[NWO VM] Invalid string length.\n");
        return 0;
    }

    if (value != 0)
    {
        *value =
            (const char*)
            (vm->program->code + start);
    }

    if (length != 0)
        *length = len;

    vm->ip += len;

    return 1;
}
static int vm_string_equals(
    const char* a,
    unsigned int a_len,
    const char* b,
    unsigned int b_len)
{
    unsigned int i;

    if (a == 0 || b == 0)
        return 0;

    if (a_len != b_len)
        return 0;

    for (i = 0; i < a_len; i++)
    {
        if (a[i] != b[i])
            return 0;
    }

    return 1;
}

static void vm_clear_variables(
    NwoVM* vm)
{
    int i;

    for (i = 0; i < NWO_VARIABLES; i++)
    {
        vm->variables[i].active = 0;
        vm->variables[i].declared_type = 0;
        vm->variables[i].is_array = 0;
        vm->variables[i].array_start = -1;
        vm->variables[i].array_size = 0;
        vm->variables[i].binding_frame = -1;

        vm->variables[i].value.type =
            VM_NONE;

        vm->variables[i].value.string_length = 0;
        vm->variables[i].value.as.integer = 0;
    }

    vm_shadow_used = 0;
}

/* Restore every binding created by the frame that is being popped. */
static void vm_restore_shadows(
    NwoVM* vm,
    int mark)
{
    int i;

    for (i = vm_shadow_used - 1; i >= mark; i--)
    {
        VmShadow* rec = &vm_shadow_pool[i];
        VmVariable* var = &vm->variables[rec->slot];

        var->active = rec->prev_active;
        var->declared_type = rec->prev_declared_type;
        var->is_array = rec->prev_is_array;
        var->array_start = rec->prev_array_start;
        var->array_size = rec->prev_array_size;
        var->binding_frame = rec->prev_binding_frame;
        var->value = rec->prev_value;
    }

    vm_shadow_used = mark;
}

static int vm_push_shadow(
    NwoVM* vm,
    unsigned int slot)
{
    VmVariable* var;
    VmShadow* rec;

    if (vm_shadow_used >= NWO_SHADOW_POOL)
    {
        print("[NWO VM] Shadow pool exhausted.\n");
        return 0;
    }

    var = &vm->variables[slot];
    rec = &vm_shadow_pool[vm_shadow_used++];

    rec->slot = (int)slot;
    rec->prev_active = var->active;
    rec->prev_declared_type = var->declared_type;
    rec->prev_is_array = var->is_array;
    rec->prev_array_start = var->array_start;
    rec->prev_array_size = var->array_size;
    rec->prev_binding_frame = var->binding_frame;
    rec->prev_value = var->value;

    return 1;
}

/* Bind a slot for the current call frame.
   - inactive slot: activate it (recorded so the frame return releases it)
   - same frame: reuse the existing binding (value kept)
   - other frame (recursion): save the old binding and rebind
   Returns 1 on success, 0 on pool overflow. */
static int vm_bind_slot(
    NwoVM* vm,
    unsigned int slot)
{
    VmVariable* var;

    if (slot >= NWO_VARIABLES)
    {
        print("[NWO VM] Invalid variable slot.\n");
        return 0;
    }

    var = &vm->variables[slot];

    if (!var->active)
    {
        if (vm->call_sp > 0)
        {
            if (!vm_push_shadow(vm, slot))
                return 0;
        }

        var->active = 1;
        var->is_array = 0;
        var->array_start = -1;
        var->array_size = 0;
        var->binding_frame = vm->call_sp;
        var->value.type = VM_NONE;
        var->value.string_length = 0;
        var->value.as.integer = 0;

        return 1;
    }

    if (var->binding_frame == vm->call_sp)
        return 1;

    if (!vm_push_shadow(vm, slot))
        return 0;

    var->binding_frame = vm->call_sp;
    var->value.type = VM_NONE;
    var->value.string_length = 0;
    var->value.as.integer = 0;

    return 1;
}

static int vm_to_int(
    VmValue* value)
{
    if (value == 0)
        return 0;

    switch (value->type)
    {
        case VM_INT:
            return value->as.integer;

        case VM_BOOL:
            return value->as.boolean ? 1 : 0;

        case VM_CHAR:
            return (int)value->as.character;

        case VM_FLOAT:
            return (int)value->as.floating;

        default:
            return 0;
    }
}

static int vm_string_compare(
    VmValue* a,
    VmValue* b)
{
    const unsigned char* left;
    const unsigned char* right;
    unsigned int left_len;
    unsigned int right_len;
    unsigned int count;
    unsigned int i;

    if (a == 0 || b == 0)
        return 0;

    if (a->type != VM_STRING || b->type != VM_STRING)
        return vm_to_int(a) - vm_to_int(b);

    left = (const unsigned char*)a->as.string;
    right = (const unsigned char*)b->as.string;

    if (left == 0)
        left = (const unsigned char*)"";
    if (right == 0)
        right = (const unsigned char*)"";

    left_len = a->string_length;
    right_len = b->string_length;

    count = left_len < right_len ? left_len : right_len;

    for (i = 0; i < count; i++)
    {
        if (left[i] != right[i])
            return (int)left[i] - (int)right[i];
    }

    if (left_len == right_len)
        return 0;

    return left_len < right_len ? -1 : 1;
}

static int vm_string_compare_n(
    VmValue* a,
    VmValue* b,
    int n)
{
    const unsigned char* left;
    const unsigned char* right;
    unsigned int left_len;
    unsigned int right_len;
    unsigned int count;
    unsigned int i;

    if (a == 0 || b == 0 || n <= 0)
        return 0;

    if (a->type != VM_STRING || b->type != VM_STRING)
        return vm_to_int(a) - vm_to_int(b);

    left = (const unsigned char*)a->as.string;
    right = (const unsigned char*)b->as.string;

    if (left == 0)
        left = (const unsigned char*)"";
    if (right == 0)
        right = (const unsigned char*)"";

    left_len = a->string_length;
    right_len = b->string_length;

    count = left_len < right_len ? left_len : right_len;

    if ((unsigned int)n < count)
        count = (unsigned int)n;

    for (i = 0; i < count; i++)
    {
        if (left[i] != right[i])
            return (int)left[i] - (int)right[i];
    }

    if ((unsigned int)n <= count)
        return 0;

    if (left_len == right_len)
        return 0;

    return left_len < right_len ? -1 : 1;
}

static int vm_values_equal(
    VmValue* a,
    VmValue* b)
{
    if (a == 0 || b == 0)
        return 0;

    if (a->type == VM_STRING ||
        b->type == VM_STRING)
    {
        return vm_string_compare(a, b) == 0;
    }

    return vm_to_int(a) == vm_to_int(b);
}

static int vm_binary_arithmetic(
    NwoVM* vm,
    unsigned char opcode)
{
    VmValue left;
    VmValue right;
    VmValue result;

    int a;
    int b;

    if (!vm_pop(vm, &right))
        return 0;

    if (!vm_pop(vm, &left))
        return 0;

    a = vm_to_int(&left);
    b = vm_to_int(&right);

    result.type = VM_INT;
    result.as.integer = 0;

    switch (opcode)
    {
        case OP_ADD:
            result.as.integer = a + b;
            break;

        case OP_SUB:
            result.as.integer = a - b;
            break;

        case OP_MUL:
            result.as.integer = a * b;
            break;

        case OP_DIV:

            if (b == 0)
            {
                print("[NWO VM] Division by zero.\n");
                return 0;
            }

            result.as.integer = a / b;
            break;

        default:
            print("[NWO VM] Invalid arithmetic opcode.\n");
            return 0;
    }

    return vm_push(vm, result);
}

static int vm_compare(
    NwoVM* vm,
    unsigned char opcode)
{
    VmValue left;
    VmValue right;
    VmValue result;

    int a;
    int b;
    int r = 0;

    if (!vm_pop(vm, &right))
        return 0;

    if (!vm_pop(vm, &left))
        return 0;
    a = vm_to_int(&left);
    b = vm_to_int(&right);

    switch (opcode)
    {
        case OP_EQ:
            r = vm_values_equal(&left, &right);
            break;

        case OP_NEQ:
            r = !vm_values_equal(&left, &right);
            break;

        case OP_LT:
            r = a < b;
            break;

        case OP_GT:
            r = a > b;
            break;

        case OP_LTE:
            r = a <= b;
            break;

        case OP_GTE:
            r = a >= b;
            break;

        default:
            print("[NWO VM] Invalid comparison opcode.\n");
            return 0;
    }

    result.type = VM_BOOL;
    result.as.boolean =
        r ? 1 : 0;

    return vm_push(vm, result);
}

static void vm_print_string(
    const char* string,
    unsigned int length)
{
    unsigned int i;

    if (string == 0)
        return;

    for (i = 0; i < length; i++)
        putchar_os(string[i]);
}

static void vm_output(
    VmValue* value)
{
    if (value == 0)
        return;

    switch (value->type)
    {
        case VM_INT:
            print_int(value->as.integer);
            break;

        case VM_BOOL:
            if (value->as.boolean)
                print("true");
            else
                print("false");
            break;

        case VM_CHAR:
            putchar_os(
                (char)value->as.character);
            break;

        case VM_STRING:
            vm_print_string(
                value->as.string,
                value->string_length);

            break;

        case VM_FLOAT:
            print_int(
                (int)value->as.floating);
            break;

        default:
            break;
    }
}

static int vm_read_len_at(
    NwoProgram* program,
    unsigned int* ip,
    unsigned int* value)
{
    if (program->version >= 2)
    {
        if (*ip + 1 > program->code_size)
            return 0;

        *value = (unsigned int)program->code[*ip];
        *ip += 1;

        return 1;
    }

    if (*ip + 4 > program->code_size)
        return 0;

    *value =
        ((unsigned int)program->code[*ip]) |
        ((unsigned int)program->code[*ip + 1] << 8) |
        ((unsigned int)program->code[*ip + 2] << 16) |
        ((unsigned int)program->code[*ip + 3] << 24);

    *ip += 4;

    return 1;
}

static int vm_skip_name(
    NwoProgram* program,
    unsigned int* ip)
{
    unsigned int len;

    if (!vm_read_len_at(program, ip, &len))
        return 0;

    if (len > program->code_size - *ip)
        return 0;

    *ip += len;

    return 1;
}

/* freestanding name check (no libc in kernel build) */
static int vm_name_is(const char* a, const char* b)
{
    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return 0;
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

/* v3: single startup pass over the bytecode that collects the function
   table.  The compiler assigns indices in emission order, so the scan
   order matches.  Also resolves main(). */
static int vm_scan_functions(
    NwoProgram* program)
{
    unsigned int ip = 0;

    vm_function_count = 0;
    vm_main_index = -1;

    while (ip < program->code_size)
    {
        unsigned char opcode = program->code[ip++];

        switch (opcode)
        {
            case OP_FUNC_BEGIN:
            {
                VmFunctionEntry* entry;
                unsigned char name_len;
                unsigned char return_type;
                unsigned char param_count;
                unsigned int i;

                if (ip + 4 > program->code_size)
                    return 0;

                /* u32 func_index must match the scan order */
                {
                    unsigned int func_index =
                        ((unsigned int)program->code[ip]) |
                        ((unsigned int)program->code[ip + 1] << 8) |
                        ((unsigned int)program->code[ip + 2] << 16) |
                        ((unsigned int)program->code[ip + 3] << 24);
                    ip += 4;

                    if (func_index != (unsigned int)vm_function_count ||
                        func_index >= NWO_MAX_FUNCTIONS)
                    {
                        print("[NWO VM] Corrupt function index.\n");
                        return 0;
                    }
                }

                entry = &vm_functions[vm_function_count];

                if (ip >= program->code_size)
                    return 0;

                name_len = program->code[ip++];

                if (name_len >= NWO_NAME_SIZE ||
                    name_len > program->code_size - ip)
                    return 0;

                for (i = 0; i < name_len; i++)
                    entry->name[i] = (char)program->code[ip + i];
                entry->name[name_len] = '\0';
                ip += name_len;

                if (ip + 2 > program->code_size)
                    return 0;

                return_type = program->code[ip++];
                (void)return_type;

                param_count = program->code[ip++];

                if (param_count > NWO_MAX_PARAMS)
                    return 0;

                entry->param_count = param_count;
                entry->body_ip = 0;

                for (i = 0; i < param_count; i++)
                {
                    if (ip + 6 > program->code_size)
                        return 0;

                    entry->param_slots[i] =
                        ((unsigned int)program->code[ip]) |
                        ((unsigned int)program->code[ip + 1] << 8) |
                        ((unsigned int)program->code[ip + 2] << 16) |
                        ((unsigned int)program->code[ip + 3] << 24);
                    ip += 4;

                    if (entry->param_slots[i] >= NWO_VARIABLES)
                        return 0;

                    entry->param_types[i] = program->code[ip++];
                    entry->param_arrays[i] = program->code[ip++];
                }

                entry->body_ip = ip;

                if (vm_name_is(entry->name, "main"))
                    vm_main_index = vm_function_count;

                vm_function_count++;
                break;
            }

            case OP_PUSH_INT:
            case OP_PUSH_FLOAT:
                if (ip + 8 > program->code_size)
                    return 0;
                ip += 8;
                break;

            case OP_PUSH_INT8:
                if (ip + 1 > program->code_size)
                    return 0;
                ip += 1;
                break;

            case OP_PUSH_INT32:
            case OP_PUSH_CHAR:
                if (ip + 4 > program->code_size)
                    return 0;
                ip += 4;
                break;

            case OP_PUSH_BOOL:
                if (ip + 1 > program->code_size)
                    return 0;
                ip += 1;
                break;

            case OP_PUSH_STRING:
                if (!vm_skip_name(program, &ip))
                    return 0;
                break;

            case OP_LOAD:
            case OP_STORE:
            case OP_IN_STORE:
            case OP_LOAD_ARR:
            case OP_STORE_ARR:
                if (ip + 4 > program->code_size)
                    return 0;
                ip += 4;
                break;

            case OP_CALL:
                if (ip + 8 > program->code_size)
                    return 0;
                ip += 8;
                break;

            case OP_DECLARE:
                if (ip + 5 > program->code_size)
                    return 0;
                ip += 5;
                break;

            case OP_DECLARE_ARR:
                if (ip + 9 > program->code_size)
                    return 0;
                ip += 9;
                break;

            case OP_LINE_IN:
            {
                unsigned int count;
                unsigned int i;

                if (ip + 4 > program->code_size)
                    return 0;

                count =
                    ((unsigned int)program->code[ip]) |
                    ((unsigned int)program->code[ip + 1] << 8) |
                    ((unsigned int)program->code[ip + 2] << 16) |
                    ((unsigned int)program->code[ip + 3] << 24);

                ip += 4;

                if (count > NWO_VARIABLES)
                    return 0;

                for (i = 0; i < count; i++)
                {
                    if (ip + 4 > program->code_size)
                        return 0;
                    ip += 4;
                }

                break;
            }

            case OP_JUMP:
            case OP_JUMP_FALSE:
                if (ip + 4 > program->code_size)
                    return 0;
                ip += 4;
                break;

            default:
                break;
        }
    }

    if (vm_main_index == -1)
    {
        print("[NWO VM] main() not found.\n");
        return 0;
    }

    return 1;
}

int nwo_execute(
    NwoProgram* program)
{
    fb_set_backbuffer_enabled(program->version >= 2);
    NwoVM vm;

    if (program == 0)
        return -1;

    if (program->version != 3)
    {
        print("[NWO VM] Unsupported NWO version (need 3, recompile the program).\n");
        return -1;
    }

    if (program->code == 0 ||
        program->code_size == 0)
    {
        print("[NWO VM] Empty program.\n");
        return -1;
    }
    vm.program = program;

    vm.ip = 0;

    vm.sp = 0;
    vm.call_sp = 0;

    vm.running = 1;
    vm.result = 0;

    vm_clear_variables(&vm);

    vm_array_pool_used = 0;
    vm_array_string_used = 0;
    vm_literal_next = 0;

    vm.text_color = 7;
    set_color(vm.text_color);

    if (!vm_scan_functions(program))
    {
        return -1;
    }
    vm.ip = vm_functions[vm_main_index].body_ip;


    while (vm.running)
    {
        unsigned char opcode;

        if (!vm_read_u8(
                &vm,
                &opcode))
        {
            return -1;
        }


        switch (opcode)
        {
            case OP_NOP:
                break;

            case OP_PUSH_INT8:
            {
                unsigned char raw;
                VmValue value;

                if (!vm_read_u8(&vm, &raw))
                    return -1;

                value.type = VM_INT;
                value.as.integer = (int)(signed char)raw;

                if (!vm_push(&vm, value))
                    return -1;

                break;
            }

            case OP_PUSH_INT32:
            {
                unsigned int raw;
                VmValue value;

                if (!vm_read_u32(&vm, &raw))
                    return -1;

                value.type = VM_INT;
                value.as.integer = (int)raw;

                if (!vm_push(&vm, value))
                    return -1;

                break;
            }

            case OP_PUSH_INT:
            {
                unsigned long long raw;
                VmValue value;

                if (!vm_read_u64(
                        &vm,
                        &raw))
                {
                    return -1;
                }

                value.type =
                    VM_INT;

                value.as.integer =
                    (int)raw;

                if (!vm_push(
                        &vm,
                        value))
                {
                    return -1;
                }

                break;
            }

            case OP_PUSH_FLOAT:
            {
                unsigned long long bits;
                double value;
                VmValue vm_value;

                if (!vm_read_u64(
                        &vm,
                        &bits))
                {
                    return -1;
                }

                {
                    unsigned char* dst =
                        (unsigned char*)&value;

                    unsigned char* src =
                        (unsigned char*)&bits;

                    int i;

                    for (i = 0; i < 8; i++)
                        dst[i] = src[i];
                }

                vm_value.type =
                    VM_FLOAT;

                vm_value.as.floating =
                    value;

                if (!vm_push(
                        &vm,
                        vm_value))
                {
                    return -1;
                }

                break;
            }
            case OP_PUSH_STRING:
            {
                const char* string;
                unsigned int length;
                unsigned int copy_length;
                unsigned int i;
                char* literal;

                VmValue value;

                if (!vm_read_string(
                        &vm,
                        &string,
                        &length))
                {
                    return -1;
                }

                copy_length = length;
                if (copy_length >= NWO_INPUT_SIZE)
                    copy_length = NWO_INPUT_SIZE - 1;

                literal = vm_literal_storage[vm_literal_next];
                vm_literal_next = (vm_literal_next + 1) % VM_LITERAL_SLOTS;

                for (i = 0; i < copy_length; i++)
                    literal[i] = string[i];
                literal[copy_length] = '\0';

                value.type =
                    VM_STRING;

                value.string_length =
                    copy_length;

                value.as.string =
                    literal;

                if (!vm_push(
                        &vm,
                        value))
                {
                    return -1;
                }

                break;
            }

            case OP_PUSH_CHAR:
            {
                unsigned int character;

                VmValue value;

                if (!vm_read_u32(
                        &vm,
                        &character))
                {
                    return -1;
                }

                value.type =
                    VM_CHAR;

                value.as.character =
                    character;

                if (!vm_push(
                        &vm,
                        value))
                {
                    return -1;
                }

                break;
            }

            case OP_PUSH_BOOL:
            {
                unsigned char boolean;

                VmValue value;

                if (!vm_read_u8(
                        &vm,
                        &boolean))
                {
                    return -1;
                }

                value.type =
                    VM_BOOL;

                value.as.boolean =
                    boolean ? 1 : 0;

                if (!vm_push(
                        &vm,
                        value))
                {
                    return -1;
                }

                break;
            }

            case OP_POP:
            {
                VmValue unused;

                if (!vm_pop(
                        &vm,
                        &unused))
                {
                    return -1;
                }

                break;
            }

            case OP_DECLARE:
            {
                unsigned int slot;
                unsigned char type;

                if (!vm_read_u32(&vm, &slot))
                    return -1;

                if (!vm_read_u8(&vm, &type))
                    return -1;

                if (slot >= NWO_VARIABLES)
                {
                    print("[NWO VM] Invalid variable slot.\n");
                    return -1;
                }

                if (!vm_bind_slot(&vm, slot))
                    return -1;

                if (!vm.variables[slot].active ||
                    vm.variables[slot].declared_type == 0)
                    vm.variables[slot].declared_type = type;

                break;
            }

            case OP_DECLARE_ARR:
            {
                unsigned int slot;
                unsigned char type;
                unsigned int size;
                unsigned int k;

                if (!vm_read_u32(&vm, &slot))
                    return -1;

                if (!vm_read_u8(&vm, &type))
                    return -1;

                if (!vm_read_u32(&vm, &size))
                    return -1;

                if (slot >= NWO_VARIABLES)
                {
                    print("[NWO VM] Invalid variable slot.\n");
                    return -1;
                }

                if (!vm_bind_slot(&vm, slot))
                    return -1;

                vm.variables[slot].declared_type = type;
                vm.variables[slot].is_array = 1;
                vm.variables[slot].array_size = size;
                vm.variables[slot].array_start = -1;

                if (type != NWO_TYPE_VOID && size > 0)
                {
                    if (vm_array_pool_used + (int)size > NWO_ARRAY_POOL_SIZE)
                    {
                        print("[NWO VM] Array pool exhausted.\n");
                        return -1;
                    }

                    if (type == NWO_TYPE_STRING &&
                        vm_array_string_used + (int)size > NWO_ARRAY_STRING_SLOTS)
                    {
                        print("[NWO VM] String array pool exhausted.\n");
                        return -1;
                    }

                    vm.variables[slot].array_start = vm_array_pool_used;
                    vm_array_pool_used += (int)size;

                    for (k = 0; k < size; k++)
                    {
                        VmValue* elem =
                            &vm_array_pool[vm.variables[slot].array_start + k];

                        if (type == NWO_TYPE_STRING)
                        {
                            elem->type = VM_STRING;
                            elem->string_length = 0;
                            elem->as.string =
                                vm_array_string_storage[vm_array_string_used];
                            vm_array_string_storage[vm_array_string_used][0] = '\0';
                            vm_array_string_used++;
                        }
                        else
                        {
                            elem->type = VM_INT;
                            elem->as.integer = 0;
                            elem->string_length = 0;
                        }
                    }
                }

                break;
            }

            case OP_LOAD:
            {
                unsigned int slot;
                VmValue value;

                if (!vm_read_u32(&vm, &slot))
                    return -1;

                if (slot >= NWO_VARIABLES ||
                    !vm.variables[slot].active)
                {
                    print("[NWO VM] Variable used before assignment (slot ");
                    print_int((int)slot);
                    print(").\n");
                    return -1;
                }

                value = vm.variables[slot].value;

                if (value.type == VM_NONE)
                {
                    value.type = VM_INT;
                    value.as.integer = 0;
                }

                if (!vm_push(&vm, value))
                    return -1;

                break;
            }

            case OP_STORE:
            {
                unsigned int slot;
                VmValue value;

                if (!vm_pop(&vm, &value))
                    return -1;

                if (!vm_read_u32(&vm, &slot))
                    return -1;

                if (slot >= NWO_VARIABLES)
                {
                    print("[NWO VM] Invalid variable slot.\n");
                    return -1;
                }

                /* auto-activate on first store (auto-declare semantics) */
                if (!vm.variables[slot].active)
                {
                    if (!vm_bind_slot(&vm, slot))
                        return -1;

                    vm.variables[slot].declared_type = 1;
                }

                vm.variables[slot].value = value;

                break;
            }

            case OP_LOAD_ARR:
            {
                unsigned int slot;
                VmValue idx_val;
                int idx;
                VmVariable* v;

                if (!vm_read_u32(&vm, &slot)) return -1;

                if (!vm_pop(&vm, &idx_val)) return -1;
                idx = vm_to_int(&idx_val);

                if (slot >= NWO_VARIABLES || !vm.variables[slot].active)
                {
                    print("[NWO VM] Unknown array.\n");
                    return -1;
                }

                v = &vm.variables[slot];
                if (!v->is_array || v->array_start < 0 ||
                    idx < 0 || idx >= (int)v->array_size)
                {
                    print("[NWO VM] Array index out of bounds.\n");
                    return -1;
                }

                if (!vm_push(&vm, vm_array_pool[v->array_start + idx])) return -1;
                break;
            }

            case OP_STORE_ARR:
            {
                unsigned int slot;
                VmValue val, idx_val;
                int idx;
                VmVariable* v;

                if (!vm_read_u32(&vm, &slot)) return -1;

                if (!vm_pop(&vm, &val))     return -1;
                if (!vm_pop(&vm, &idx_val)) return -1;
                idx = vm_to_int(&idx_val);

                if (slot >= NWO_VARIABLES || !vm.variables[slot].active)
                {
                    print("[NWO VM] Unknown array.\n");
                    return -1;
                }

                v = &vm.variables[slot];
                if (!v->is_array || v->array_start < 0 ||
                    idx < 0 || idx >= (int)v->array_size)
                {
                    print("[NWO VM] Array index out of bounds.\n");
                    return -1;
                }

                if (v->declared_type == NWO_TYPE_STRING)
                {
                    unsigned int n = 0;
                    char* dest;

                    if (val.type != VM_STRING)
                    {
                        print("[NWO VM] Type mismatch for string array.\n");
                        return -1;
                    }

                    dest = (char*)vm_array_pool[v->array_start + idx].as.string;

                    while (n < NWO_INPUT_SIZE - 1 &&
                           val.as.string != 0 &&
                           val.as.string[n] != '\0')
                    {
                        dest[n] = val.as.string[n];
                        n++;
                    }

                    dest[n] = '\0';
                    vm_array_pool[v->array_start + idx].type = VM_STRING;
                    vm_array_pool[v->array_start + idx].string_length = n;
                    vm_array_pool[v->array_start + idx].as.string = dest;
                }
                else
                {
                    vm_array_pool[v->array_start + idx] = val;
                }
                break;
            }

            case OP_FS_COUNT:
            {
                VmValue v; v.type = VM_INT; v.as.integer = fs_dir_count();
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_FS_PATH:
            {
                const char* p = fs_current_path();
                VmValue v; v.type = VM_STRING;
                unsigned int n = 0; while (p[n]) n++;
                v.string_length = n; v.as.string = p;
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_FS_NAME:
            {
                VmValue idx_val;
                if (!vm_pop(&vm, &idx_val)) return -1;
                const char* nm = 0; int t = 0; unsigned int s = 0;
                fs_dir_entry(vm_to_int(&idx_val), &nm, &t, &s);
                VmValue v; v.type = VM_STRING;
                if (nm == 0) { v.string_length = 0; v.as.string = ""; }
                else { unsigned int n = 0; while (nm[n]) n++;
                       v.string_length = n; v.as.string = nm; }
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_FS_TYPE:
            {
                VmValue idx_val;
                if (!vm_pop(&vm, &idx_val)) return -1;
                const char* nm = 0; int t = 0; unsigned int s = 0;
                fs_dir_entry(vm_to_int(&idx_val), &nm, &t, &s);
                VmValue v; v.type = VM_INT; v.as.integer = t;
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_FS_SIZE:
            {
                VmValue idx_val;
                if (!vm_pop(&vm, &idx_val)) return -1;
                const char* nm = 0; int t = 0; unsigned int s = 0;
                fs_dir_entry(vm_to_int(&idx_val), &nm, &t, &s);
                VmValue v; v.type = VM_INT; v.as.integer = (int)s;
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_FS_ENTER:
            {
                VmValue val;
                if (!vm_pop(&vm, &val)) return -1;
                char buf[256];
                unsigned int n = val.type == VM_STRING ? val.string_length : 0;
                if (n > 255) n = 255;
                for (unsigned int i = 0; i < n; i++) buf[i] = val.as.string[i];
                buf[n] = '\0';
                fs_cd(buf);
                break;
            }
            case OP_FS_UP:
                fs_cd("..");
                break;
            case OP_RAM_MB:
            {
                VmValue v; v.type = VM_INT; v.as.integer = (int)nwf_ram_mb();
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_UPTIME_SEC:
            {
                VmValue v; v.type = VM_INT; v.as.integer = (int)(timer_ms() / 1000ULL);
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_DISK_KB:
            {
                VmValue v; v.type = VM_INT; v.as.integer = (int)nwf_disk_used_kb();
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_DISK_FILES:
            {
                VmValue v; v.type = VM_INT; v.as.integer = (int)nwf_disk_files();
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_TIME_HOUR:
            case OP_TIME_MINUTE:
            case OP_TIME_SECOND:
            {
                int hour, minute, second;
                VmValue v; v.type = VM_INT;
                get_time(&hour, &minute, &second);
                if (opcode == OP_TIME_HOUR) v.as.integer = hour;
                else if (opcode == OP_TIME_MINUTE) v.as.integer = minute;
                else v.as.integer = second;
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_TZ_GET:
            {
                VmValue v; v.type = VM_INT; v.as.integer = nwf_tz_get();
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_TZ_SET:
            {
                VmValue v;
                if (!vm_pop(&vm, &v)) return -1;
                nwf_tz_set(vm_to_int(&v));
                break;
            }
            case OP_CPU_LOAD:
            {
                VmValue v; v.type = VM_INT; v.as.integer = nwf_cpu_load();
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_GPU_LOAD:
            {
                VmValue v; v.type = VM_INT; v.as.integer = nwf_gpu_load();
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_CURSOR:
            {
                VmValue y;
                VmValue x;
                if (!vm_pop(&vm, &y)) return -1;
                if (!vm_pop(&vm, &x)) return -1;
                graphics_begin();
                fb_draw_cursor(vm_to_int(&x), vm_to_int(&y));
                break;
            }
            case OP_FS_CREATE:
            {
                char name[256];
                VmValue v;
                if (!vm_pop_string(&vm, name, sizeof(name))) return -1;
                v.type = VM_INT; v.as.integer = nwf_fs_create(name);
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_FS_MKDIR:
            {
                char name[256];
                VmValue v;
                if (!vm_pop_string(&vm, name, sizeof(name))) return -1;
                v.type = VM_INT; v.as.integer = nwf_fs_mkdir(name);
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_FS_DELETE:
            {
                char name[256];
                VmValue v;
                if (!vm_pop_string(&vm, name, sizeof(name))) return -1;
                v.type = VM_INT; v.as.integer = nwf_fs_delete(name);
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_FS_WRITE:
            {
                char name[256];
                char text[VM_FILE_BUF_SIZE];
                VmValue v;
                if (!vm_pop_string(&vm, text, sizeof(text))) return -1;
                if (!vm_pop_string(&vm, name, sizeof(name))) return -1;
                v.type = VM_INT; v.as.integer = nwf_fs_write(name, text);
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_BUF_CLEAR:
                vm_file_len = 0;
                vm_file_buf[0] = '\0';
                break;
            case OP_BUF_LEN:
            {
                VmValue v; v.type = VM_INT; v.as.integer = vm_file_len;
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_BUF_PUT:
            {
                VmValue v;
                int ch;
                if (!vm_pop(&vm, &v)) return -1;
                ch = vm_to_int(&v);
                if (ch < 0) ch = 0;
                if (ch > 255) ch = 255;
                if (vm_file_len < VM_FILE_BUF_SIZE - 1)
                {
                    vm_file_buf[vm_file_len++] = (char)ch;
                    vm_file_buf[vm_file_len] = '\0';
                }
                break;
            }
            case OP_BUF_BACK:
                if (vm_file_len > 0)
                {
                    vm_file_len--;
                    vm_file_buf[vm_file_len] = '\0';
                }
                break;
            case OP_BUF_TEXT:
            {
                VmValue v;
                v.type = VM_STRING;
                v.string_length = (unsigned int)vm_file_len;
                v.as.string = vm_file_buf;
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_BUF_LOAD:
            {
                char name[256];
                VmValue v;
                if (!vm_pop_string(&vm, name, sizeof(name))) return -1;
                vm_file_len = 0;
                vm_file_buf[0] = '\0';
                nwf_fs_read(name, vm_file_buf, VM_FILE_BUF_SIZE);
                while (vm_file_len < VM_FILE_BUF_SIZE - 1 &&
                       vm_file_buf[vm_file_len] != '\0')
                    vm_file_len++;
                v.type = VM_INT; v.as.integer = vm_file_len;
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_BUF_SAVE:
            {
                char name[256];
                VmValue v;
                if (!vm_pop_string(&vm, name, sizeof(name))) return -1;
                v.type = VM_INT; v.as.integer = nwf_fs_write(name, vm_file_buf);
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_POLLKEY:
            {
                VmValue v;
                v.type = VM_INT;
                v.as.integer = keyboard_pollkey();
                if (!vm_push(&vm, v)) return -1;
                break;
            }
            case OP_RANDOM_WORD:
            {
                const char* word = nwf_random_word();
                VmValue v;
                unsigned int n = 0;
                while (word[n]) n++;
                v.type = VM_STRING;
                v.string_length = n;
                v.as.string = word;
                if (!vm_push(&vm, v)) return -1;
                break;
            }

            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:

                if (!vm_binary_arithmetic(
                        &vm,
                        opcode))
                {
                    return -1;
                }
                break;

            case OP_EQ:
            case OP_NEQ:
            case OP_LT:
            case OP_GT:
            case OP_LTE:
            case OP_GTE:

                if (!vm_compare(
                        &vm,
                        opcode))
                {
                    return -1;
                }

                break;

            case OP_AND:
            case OP_OR:
            {
                VmValue right;
                VmValue left;
                VmValue result;

                if (!vm_pop(&vm, &right))
                    return -1;
                if (!vm_pop(&vm, &left))
                    return -1;

                result.type = VM_INT;
                result.string_length = 0;

                if (opcode == OP_AND)
                    result.as.integer = vm_to_int(&left) && vm_to_int(&right);
                else
                    result.as.integer = vm_to_int(&left) || vm_to_int(&right);

                if (!vm_push(&vm, result))
                    return -1;

                break;
            }

            case OP_NOT:
            {
                VmValue value;
                VmValue result;

                if (!vm_pop(&vm, &value))
                    return -1;

                result.type = VM_INT;
                result.string_length = 0;
                result.as.integer = !vm_to_int(&value);

                if (!vm_push(&vm, result))
                    return -1;

                break;
            }

            case OP_STRCMP:
            {
                VmValue right;
                VmValue left;
                VmValue result;

                if (!vm_pop(&vm, &right))
                    return -1;

                if (!vm_pop(&vm, &left))
                    return -1;

                result.type = VM_INT;
                result.string_length = 0;
                result.as.integer =
                    vm_string_compare(&left, &right);

                if (!vm_push(&vm, result))
                    return -1;

                break;
            }

            case OP_STRNCMP:
            {
                VmValue count;
                VmValue right;
                VmValue left;
                VmValue result;

                if (!vm_pop(&vm, &count))
                    return -1;

                if (!vm_pop(&vm, &right))
                    return -1;

                if (!vm_pop(&vm, &left))
                    return -1;

                result.type = VM_INT;
                result.string_length = 0;
                result.as.integer =
                    vm_string_compare_n(
                        &left,
                        &right,
                        vm_to_int(&count));

                if (!vm_push(&vm, result))
                    return -1;

                break;
            }

            case OP_OUT:
            {
                VmValue value;

                if (!vm_pop(
                        &vm,
                        &value))
                {
                    return -1;
                }

                vm_output(&value);

                break;
            }
            case OP_OUT_ENDL:
                putchar_os('\n');
                break;

            case OP_IN:
            {
                char buffer[NWO_INPUT_SIZE];

                int result = 0;
                int sign = 1;
                int i = 0;

                VmValue value;

                print("> ");

                read_line(
                    buffer,
                    NWO_INPUT_SIZE);

                if (buffer[0] == '-')
                {
                    sign = -1;
                    i = 1;
                }

                while (buffer[i] >= '0' &&
                       buffer[i] <= '9')
                {
                    result =
                        result * 10 +
                        (buffer[i] - '0');

                    i++;
                }

                value.type =
                    VM_INT;

                value.as.integer =
                    result * sign;

                if (!vm_push(
                        &vm,
                        value))
                {
                    return -1;
                }

                break;
            }

            case OP_IN_STORE:
            {
                unsigned int slot;
                char buffer[NWO_INPUT_SIZE];
                VmVariable* variable;

                if (!vm_read_u32(&vm, &slot))
                    return -1;

                if (slot >= NWO_VARIABLES)
                {
                    print("[NWO VM] Invalid variable slot.\n");
                    return -1;
                }

                read_line(buffer, NWO_INPUT_SIZE);

                if (!vm.variables[slot].active)
                {
                    if (!vm_bind_slot(&vm, slot))
                        return -1;

                    vm.variables[slot].declared_type = 1;
                }

                variable = &vm.variables[slot];

                if (variable->declared_type == 7 ||
                    variable->is_array)
                {
                    unsigned int length = 0;

                    while (buffer[length] != '\0' &&
                           length < NWO_INPUT_SIZE - 1)
                    {
                        vm_string_storage[slot][length] =
                            buffer[length];
                        length++;
                    }

                    vm_string_storage[slot][length] = '\0';

                    variable->value.type = VM_STRING;
                    variable->value.string_length = length;
                    variable->value.as.string =
                        vm_string_storage[slot];
                }
                else
                {
                    int result = 0;
                    int sign = 1;
                    int pos = 0;

                    if (buffer[0] == '-')
                    {
                        sign = -1;
                        pos = 1;
                    }

                    while (buffer[pos] >= '0' &&
                           buffer[pos] <= '9')
                    {
                        result = result * 10 +
                            (buffer[pos] - '0');
                        pos++;
                    }

                    variable->value.type = VM_INT;
                    variable->value.string_length = 0;
                    variable->value.as.integer =
                        result * sign;
                }

                break;
            }

            case OP_LINE_IN:
            {
                unsigned int count;
                unsigned int item;

                static char line_storage[NWO_INPUT_SIZE];

                if (!vm_read_u32(&vm, &count))
                {
                    return -1;
                }

                for (item = 0;
                     item < count;
                     item++)
                {
                    unsigned int slot;
                    VmVariable* variable;

                    if (!vm_read_u32(&vm, &slot))
                        return -1;

                    if (slot >= NWO_VARIABLES)
                    {
                        print("[NWO VM] Invalid variable slot.\n");
                        return -1;
                    }

                    read_line(
                        line_storage,
                        NWO_INPUT_SIZE);

                    if (!vm.variables[slot].active)
                    {
                        if (!vm_bind_slot(&vm, slot))
                            return -1;

                        vm.variables[slot].declared_type = 7;
                    }

                    variable = &vm.variables[slot];

                    variable->value.type =
                        VM_STRING;

                    {
                        unsigned int length = 0;

                        while (line_storage[length] != '\0' &&
                               length < NWO_INPUT_SIZE - 1)
                        {
                            length++;
                        }

                        variable->value.string_length =
                            length;
                    }

                    variable->value.as.string =
                        line_storage;
                }

                break;
            }
            case OP_RECT:
            {
                VmValue color;
                VmValue height;
                VmValue width;
                VmValue y;
                VmValue x;

                if (!vm_pop(&vm, &color))
                    return -1;

                if (!vm_pop(&vm, &height))
                    return -1;

                if (!vm_pop(&vm, &width))
                    return -1;

                if (!vm_pop(&vm, &y))
                    return -1;

                if (!vm_pop(&vm, &x))
                    return -1;

                graphics_begin();

                fb_draw_rect(
                    vm_to_int(&x),
                    vm_to_int(&y),
                    vm_to_int(&width),
                    vm_to_int(&height),
                    (unsigned int)vm_to_int(&color));

                break;
            }

            case OP_SHI:
            case OP_SHS:
            {
                VmValue sop, sarg;
                VmValue res;

                if (!vm_pop(&vm, &sarg)) return -1;
                if (!vm_pop(&vm, &sop)) return -1;

                if (opcode == OP_SHI)
                {
                    res.type = VM_INT;
                    res.string_length = 0;
                    res.as.integer = gui_term_shi(vm_to_int(&sop),
                                                  vm_to_int(&sarg));
                }
                else
                {
                    const char* str = gui_term_shs(vm_to_int(&sop),
                                                   vm_to_int(&sarg));
                    unsigned int n = 0;

                    while (str[n]) n++;
                    res.type = VM_STRING;
                    res.string_length = n;
                    res.as.string = str;
                }

                if (!vm_push(&vm, res)) return -1;
                break;
            }

            case OP_GRADIENT:
            case OP_RGRAD:
            case OP_GLASS:
            case OP_ORB:
            case OP_ELLIPSE:
            case OP_BUBBLE:
            {
                VmValue a[8];
                int n = (opcode == OP_GRADIENT) ? 6 :
                        (opcode == OP_RGRAD)    ? 7 :
                        (opcode == OP_GLASS)    ? 7 :
                        (opcode == OP_ELLIPSE)  ? 7 : 4;
                int k;

                for (k = n - 1; k >= 0; k--)
                    if (!vm_pop(&vm, &a[k]))
                        return -1;

                graphics_begin();

                if (opcode == OP_GRADIENT)
                    fb_draw_gradient(vm_to_int(&a[0]), vm_to_int(&a[1]),
                                     vm_to_int(&a[2]), vm_to_int(&a[3]),
                                     (unsigned int)vm_to_int(&a[4]),
                                     (unsigned int)vm_to_int(&a[5]));
                else if (opcode == OP_RGRAD)
                    fb_draw_rgrad(vm_to_int(&a[0]), vm_to_int(&a[1]),
                                  vm_to_int(&a[2]), vm_to_int(&a[3]),
                                  vm_to_int(&a[4]),
                                  (unsigned int)vm_to_int(&a[5]),
                                  (unsigned int)vm_to_int(&a[6]));
                else if (opcode == OP_GLASS)
                    fb_draw_glass(vm_to_int(&a[0]), vm_to_int(&a[1]),
                                  vm_to_int(&a[2]), vm_to_int(&a[3]),
                                  vm_to_int(&a[4]),
                                  (unsigned int)vm_to_int(&a[5]),
                                  vm_to_int(&a[6]));
                else if (opcode == OP_ELLIPSE)
                    fb_draw_ellipse(vm_to_int(&a[0]), vm_to_int(&a[1]),
                                    vm_to_int(&a[2]), vm_to_int(&a[3]),
                                    (unsigned int)vm_to_int(&a[4]),
                                    (unsigned int)vm_to_int(&a[5]),
                                    vm_to_int(&a[6]));
                else if (opcode == OP_BUBBLE)
                    fb_draw_bubble(vm_to_int(&a[0]), vm_to_int(&a[1]),
                                   vm_to_int(&a[2]), vm_to_int(&a[3]));
                else
                    fb_draw_orb(vm_to_int(&a[0]), vm_to_int(&a[1]),
                                vm_to_int(&a[2]),
                                (unsigned int)vm_to_int(&a[3]));

                break;
            }

            case OP_DRAW_FRAME:
            {
                VmValue color;
                VmValue height;
                VmValue width;
                VmValue y;
                VmValue x;

                if (!vm_pop(&vm, &color))
                    return -1;
                if (!vm_pop(&vm, &height))
                    return -1;
                if (!vm_pop(&vm, &width))
                    return -1;
                if (!vm_pop(&vm, &y))
                    return -1;
                if (!vm_pop(&vm, &x))
                    return -1;

                graphics_begin();

                fb_draw_frame(
                    vm_to_int(&x),
                    vm_to_int(&y),
                    vm_to_int(&width),
                    vm_to_int(&height),
                    (unsigned int)vm_to_int(&color));

                break;
            }

            case OP_DRAW_CUBE:
            {
                VmValue color;
                VmValue size;

                if (!vm_pop(&vm, &color))
                    return -1;
                if (!vm_pop(&vm, &size))
                    return -1;

                (void)size;

                graphics_begin();
                fb_draw_cube_default((unsigned int)vm_to_int(&color));

                break;
            }

            case OP_DRAW_WINDOW:
            {
                VmValue title;
                VmValue color;
                VmValue height;
                VmValue width;
                VmValue y;
                VmValue x;

                if (!vm_pop(&vm, &title))
                    return -1;
                if (!vm_pop(&vm, &color))
                    return -1;
                if (!vm_pop(&vm, &height))
                    return -1;
                if (!vm_pop(&vm, &width))
                    return -1;
                if (!vm_pop(&vm, &y))
                    return -1;
                if (!vm_pop(&vm, &x))
                    return -1;

                graphics_begin();

                fb_draw_window(
                    vm_to_int(&x),
                    vm_to_int(&y),
                    vm_to_int(&width),
                    vm_to_int(&height),
                    (unsigned int)vm_to_int(&color),
                    title.type == VM_STRING ? title.as.string : "");

                break;
            }

            case OP_DRAW_TEXT:
            {
                VmValue text;
                VmValue color;
                VmValue y;
                VmValue x;

                if (!vm_pop(&vm, &text))
                    return -1;
                if (!vm_pop(&vm, &color))
                    return -1;
                if (!vm_pop(&vm, &y))
                    return -1;
                if (!vm_pop(&vm, &x))
                    return -1;

                graphics_begin();

                fb_draw_text(
                    vm_to_int(&x),
                    vm_to_int(&y),
                    text.type == VM_STRING ? text.as.string : "",
                    (unsigned int)vm_to_int(&color));

                break;
            }

            case OP_COLOR_PUSH:
            {
                VmValue value;

                value.type = VM_INT;
                value.string_length = 0;
                value.as.integer = (int)vm.text_color;

                if (!vm_push(&vm, value))
                    return -1;

                break;
            }

            case OP_COLOR_POP:
            {
                VmValue value;

                if (!vm_pop(&vm, &value))
                    return -1;

                vm.text_color =
                    (unsigned char)vm_to_int(&value);

                set_color(vm.text_color);

                break;
            }

            case OP_COLOR:
            {
                VmValue value;

                if (!vm_pop(
                        &vm,
                        &value))
                {
                    return -1;
                }

                vm.text_color =
                    (unsigned char)vm_to_int(&value);

                set_color(vm.text_color);

                break;
            }

            case OP_CLEAR:
                clear();
                break;

            case OP_SLEEP:
            {
                VmValue value;

                if (!vm_pop(&vm, &value))
                    return -1;

                sleep((unsigned int)vm_to_int(&value));

                break;
            }

            case OP_GETKEY:
            {
                VmValue value;

                value.type =
                    VM_INT;

                value.as.integer =
                    keyboard_getkey();

                if (!vm_push(
                        &vm,
                        value))
                {
                    return -1;
                }

                break;
            }

            case OP_DRAW_INT:
            {
                VmValue value;
                VmValue color;
                VmValue y;
                VmValue x;

                if (!vm_pop(&vm, &value))
                    return -1;
                if (!vm_pop(&vm, &color))
                    return -1;
                if (!vm_pop(&vm, &y))
                    return -1;
                if (!vm_pop(&vm, &x))
                    return -1;

                graphics_begin();

                fb_draw_int(
                    vm_to_int(&x),
                    vm_to_int(&y),
                    vm_to_int(&value),
                    (unsigned int)vm_to_int(&color));

                break;
            }

            case OP_FLUSH:
                fb_present();
                break;

            case OP_WALLPAPER:
                graphics_begin();
                fb_draw_wallpaper();
                break;

            case OP_MOUSE_X:
            case OP_MOUSE_Y:
            case OP_MOUSE_BUTTONS:
            {
                VmValue value;
                int result = 0;

                if (opcode == OP_MOUSE_X)
                    result = mouse_x();
                else if (opcode == OP_MOUSE_Y)
                    result = mouse_y();
                else
                    result = mouse_buttons();

                value.type = VM_INT;
                value.as.integer = result;

                if (!vm_push(&vm, value))
                    return -1;

                break;
            }

            case OP_POWEROFF:
                poweroff();
                break;

            case OP_REBOOT:
                reboot();
                break;

            case OP_GETKEY_FILTER:
            {
                VmValue filter;
                VmValue value;
                int key;

                if (!vm_pop(&vm, &filter))
                    return -1;

                do
                {
                    key = keyboard_getkey();
                }
                while (key != vm_to_int(&filter));

                value.type = VM_INT;
                value.as.integer = key;

                if (!vm_push(&vm, value))
                    return -1;

                break;
            }

            case OP_ASM_HLT:
#ifdef NWC_HOST_TEST
                print("[hlt]");
#else
                __asm__ volatile("hlt");
#endif
                break;

            case OP_TIME:
            {
                int hour;
                int minute;
                int second;

                get_time(
                    &hour,
                    &minute,
                    &second);

                if (hour < 10)
                    putchar_os('0');
                print_int(hour);
                putchar_os(':');

                if (minute < 10)
                    putchar_os('0');
                print_int(minute);
                putchar_os(':');

                if (second < 10)
                    putchar_os('0');
                print_int(second);
                putchar_os('\n');

                break;
            }

            case OP_CALL:
            {
                unsigned int func_index;
                unsigned int argc;
                unsigned int i;
                VmFunctionEntry* func;

                if (!vm_read_u32(&vm, &func_index))
                    return -1;

                if (!vm_read_u32(&vm, &argc))
                    return -1;

                if (func_index >= (unsigned int)vm_function_count)
                {
                    print("[NWO VM] Unknown function index.\n");
                    return -1;
                }

                func = &vm_functions[func_index];

                if (argc != func->param_count)
                {
                    print("[NWO VM] Wrong argument count for ");
                    print(func->name);
                    putchar_os('\n');
                    return -1;
                }

                if (vm.call_sp >= NWO_MAX_CALL_DEPTH)
                {
                    print("[NWO VM] Call stack overflow.\n");
                    return -1;
                }

                vm.call_stack[vm.call_sp].return_ip = vm.ip;
                vm.call_stack[vm.call_sp].array_pool_mark = vm_array_pool_used;
                vm.call_stack[vm.call_sp].array_string_mark = vm_array_string_used;
                vm.call_stack[vm.call_sp].shadow_mark = vm_shadow_used;
                vm.call_sp++;

                for (i = func->param_count; i > 0; i--)
                {
                    VmValue value;
                    unsigned int slot = func->param_slots[i - 1];

                    if (!vm_pop(&vm, &value))
                        return -1;

                    if (!vm_bind_slot(&vm, slot))
                        return -1;

                    vm.variables[slot].declared_type =
                        func->param_types[i - 1];
                    vm.variables[slot].is_array =
                        func->param_arrays[i - 1];
                    vm.variables[slot].value = value;
                }

                vm.ip = func->body_ip;

                break;
            }


            case OP_JUMP:
            {
                unsigned int target;

                if (!vm_read_u32(&vm, &target))
                    return -1;

                if (target > program->code_size)
                    return -1;

                vm.ip = target;

                break;
            }

            case OP_JUMP_FALSE:
            {
                unsigned int target;
                VmValue value;

                if (!vm_read_u32(&vm, &target))
                    return -1;

                if (!vm_pop(&vm, &value))
                    return -1;

                if (target > program->code_size)
                    return -1;

                if (!vm_to_int(&value))
                    vm.ip = target;

                break;
            }

            case OP_FUNC_END:
            {
                VmValue value;

                value.type = VM_INT;
                value.as.integer = 0;

                if (vm.call_sp > 0)
                {
                    vm.call_sp--;
                    vm_restore_shadows(&vm, vm.call_stack[vm.call_sp].shadow_mark);
                    vm_array_pool_used = vm.call_stack[vm.call_sp].array_pool_mark;
                    vm_array_string_used = vm.call_stack[vm.call_sp].array_string_mark;
                    vm.ip = vm.call_stack[vm.call_sp].return_ip;

                    if (!vm_push(&vm, value))
                        return -1;
                }
                else
                {
                    vm.running = 0;
                    vm.result = 0;
                }

                break;
            }


            case OP_RETURN:
            {
                VmValue value;
                VmValue result;

                if (!vm_pop(
                        &vm,
                        &value))
                {
                    return -1;
                }

                result.type = VM_INT;
                result.as.integer = vm_to_int(&value);

                if (vm.call_sp > 0)
                {
                    vm.call_sp--;
                    vm_restore_shadows(&vm, vm.call_stack[vm.call_sp].shadow_mark);
                    vm_array_pool_used = vm.call_stack[vm.call_sp].array_pool_mark;
                    vm_array_string_used = vm.call_stack[vm.call_sp].array_string_mark;
                    vm.ip = vm.call_stack[vm.call_sp].return_ip;

                    if (!vm_push(&vm, result))
                        return -1;
                }
                else
                {
                    vm.result = result.as.integer;
                    vm.running = 0;
                }

                break;
            }

            case OP_RETURN_VOID:
            {
                VmValue value;

                value.type = VM_INT;
                value.as.integer = 0;

                if (vm.call_sp > 0)
                {
                    vm.call_sp--;
                    vm_restore_shadows(&vm, vm.call_stack[vm.call_sp].shadow_mark);
                    vm_array_pool_used = vm.call_stack[vm.call_sp].array_pool_mark;
                    vm_array_string_used = vm.call_stack[vm.call_sp].array_string_mark;
                    vm.ip = vm.call_stack[vm.call_sp].return_ip;

                    if (!vm_push(&vm, value))
                        return -1;
                }
                else
                {
                    vm.result = 0;
                    vm.running = 0;
                }

                break;
            }

            case OP_HALT:

                vm.result = 0;
                vm.running = 0;

                break;

            default:

                print("[NWO VM] Unknown opcode: ");
                print_int((int)opcode);
                putchar_os('\n');

                return -1;
        }
    }

    return vm.result;
}