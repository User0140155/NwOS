#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"

#define NWO_MAGIC       "NWOB"
#define NWO_VERSION     3
#define NWO_FLAG_NORMAL   0x00
#define NWO_FLAG_LOWLEVEL 0x01
#define NWO_FLAG_GUI      0x02

#define NWO_MAX_LOOPS 16
#define NWO_MAX_BREAKS 64

#define NWO_MAX_SLOTS     768
#define NWO_MAX_FUNCTIONS 128
#define NWO_MAX_PARAMS_V3 16
#define NWO_MAX_FIXUPS    512
#define NWO_NAME_SIZE_V3  32

#define NWC_MOD_NONZERO  1u
#define NWC_MOD_CONST    2u
#define NWC_MOD_NEEDED   4u

typedef struct
{
    FILE* file;

    unsigned int code_size;
    unsigned int data_size;

    int error;

    long loop_start[NWO_MAX_LOOPS];
    long loop_continue[NWO_MAX_LOOPS];
    int loop_break_base[NWO_MAX_LOOPS];
    int loop_continue_base[NWO_MAX_LOOPS];
    int loop_depth;

    long break_patch[NWO_MAX_BREAKS];
    int break_count;

    long continue_patch[NWO_MAX_BREAKS];
    int continue_count;

    int current_owner;

} Codegen;


typedef struct
{
    char name[NWO_NAME_SIZE_V3];
    int owner;
    unsigned int modifiers;
    ValueType value_type;
} CgSymbol;

typedef struct
{
    long pos;
    const char* name;
    int owner;
    int is_store;
    int slot;
} CgFixup;

typedef struct
{
    char name[NWO_NAME_SIZE_V3];
} CgFunc;

static CgSymbol cg_symbols[NWO_MAX_SLOTS];
static int cg_sym_count;

static CgFixup cg_fixups[NWO_MAX_FIXUPS];
static int cg_fixup_count;
static int cg_gui_used;

static CgFunc cg_funcs[NWO_MAX_FUNCTIONS];
static int cg_func_count;
static int cg_main_func;

static void cg_error(Codegen* cg, const char* msg)
{
    fprintf(stderr, "NwC codegen: %s\n", msg);
    cg->error = 1;
}
static void cg_emit_byte(Codegen* cg, unsigned char b)
{
    if (cg->file == NULL)
        return;

    if (fputc(b, cg->file) == EOF)
    {
        cg_error(cg, "write failed");
        return;
    }

    cg->code_size++;
}
static void cg_emit_u32(Codegen* cg, unsigned int v)
{
    cg_emit_byte(cg, (unsigned char)(v & 0xFF));
    cg_emit_byte(cg, (unsigned char)((v >> 8) & 0xFF));
    cg_emit_byte(cg, (unsigned char)((v >> 16) & 0xFF));
    cg_emit_byte(cg, (unsigned char)((v >> 24) & 0xFF));
}
static void cg_emit_u64(Codegen* cg, unsigned long long v)
{
    cg_emit_u32(cg, (unsigned int)(v & 0xFFFFFFFFu));
    cg_emit_u32(cg, (unsigned int)((v >> 32) & 0xFFFFFFFFu));
}
static void cg_emit_int(Codegen* cg, int value)
{
    if (value >= -128 && value <= 127)
    {
        cg_emit_byte(cg, 0x17);
        cg_emit_byte(cg, (unsigned char)(signed char)value);
    }
    else
    {
        cg_emit_byte(cg, 0x18);
        cg_emit_u32(cg, (unsigned int)value);
    }
}

static void cg_emit_double(Codegen* cg, double d)
{
    unsigned long long bits;

    memcpy(&bits, &d, sizeof(bits));

    cg_emit_u64(cg, bits);
}
static void cg_emit_string(Codegen* cg, const char* s)
{
    unsigned int len;

    if (s == NULL)
        s = "";

    len = (unsigned int)strlen(s);

    if (len > 255)
    {
        cg_error(cg, "string too long");
        len = 255;
    }

    cg_emit_byte(cg, (unsigned char)len);

    for (unsigned int i = 0; i < len; i++)
        cg_emit_byte(cg, (unsigned char)s[i]);
}

static long cg_emit_jump(Codegen* cg, unsigned char opcode)
{
    long position;

    cg_emit_byte(cg, opcode);

    position = ftell(cg->file);

    cg_emit_u32(cg, 0);

    return position;
}

static void cg_patch_jump(Codegen* cg, long position, long target)
{
    unsigned char bytes[4];
    long saved;
    unsigned int value = (unsigned int)target;

    if (cg->file == NULL || position < 0)
        return;

    saved = ftell(cg->file);

    bytes[0] = (unsigned char)(value & 0xFF);
    bytes[1] = (unsigned char)((value >> 8) & 0xFF);
    bytes[2] = (unsigned char)((value >> 16) & 0xFF);
    bytes[3] = (unsigned char)((value >> 24) & 0xFF);

    if (fseek(cg->file, position, SEEK_SET) == 0)
    {
        if (fwrite(bytes, 1, 4, cg->file) != 4)
            cg_error(cg, "jump patch failed");
    }

    if (saved >= 0)
        fseek(cg->file, saved, SEEK_SET);
}

/* ---- v3 symbol resolution ---- */

static int cg_sym_find(const char* name, int owner)
{
    int i;

    if (name == NULL)
        return -1;

    for (i = 0; i < cg_sym_count; i++)
    {
        if (cg_symbols[i].owner == owner &&
            strcmp(cg_symbols[i].name, name) == 0)
        {
            return i;
        }
    }

    return -1;
}

static int cg_sym_add(const char* name, int owner, const char* what)
{
    size_t len;

    if (name == NULL)
    {
        fprintf(stderr, "NwC codegen: %s without name\n", what);
        return -1;
    }

    len = strlen(name);

    if (len >= NWO_NAME_SIZE_V3)
    {
        fprintf(stderr, "NwC codegen: %s name too long '%s'\n", what, name);
        return -1;
    }

    if (cg_sym_count >= NWO_MAX_SLOTS)
    {
        fprintf(stderr, "NwC codegen: too many variables (max %d)\n",
                NWO_MAX_SLOTS);
        return -1;
    }

    memcpy(cg_symbols[cg_sym_count].name, name, len + 1);
    cg_symbols[cg_sym_count].owner = owner;

    return cg_sym_count++;
}

static int cg_func_index(const char* name)
{
    int i;

    if (name == NULL)
        return -1;

    for (i = 0; i < cg_func_count; i++)
    {
        if (strcmp(cg_funcs[i].name, name) == 0)
            return i;
    }

    return -1;
}

static int cg_emit_slot(Codegen* cg, const char* name, int is_store)
{
    int slot;

    if (cg->error)
        return 0;

    slot = cg_sym_find(name, cg->current_owner);

    if (slot == -1 && cg->current_owner != cg_main_func)
        slot = cg_sym_find(name, cg_main_func);

    if (slot != -1)
    {
        cg_emit_u32(cg, (unsigned int)slot);
        return 1;
    }

    if (cg_fixup_count >= NWO_MAX_FIXUPS)
    {
        cg_error(cg, "too many unresolved variable references");
        return 0;
    }

    cg_fixups[cg_fixup_count].pos = ftell(cg->file);
    cg_fixups[cg_fixup_count].name = name;
    cg_fixups[cg_fixup_count].owner = cg->current_owner;
    cg_fixups[cg_fixup_count].is_store = is_store;
    cg_fixup_count++;

    cg_emit_u32(cg, 0xFFFFFFFFu);

    return 1;
}

/* Emit the slot operand for a declaration inside the current function.
   Redeclaration in the same function reuses the slot (old VM semantics),
   otherwise a fresh slot is allocated. */
static int cg_emit_decl_slot(Codegen* cg, const char* name,
                             unsigned int modifiers, ValueType value_type)
{
    int slot;

    if (cg->error)
        return 0;

    slot = cg_sym_find(name, cg->current_owner);

    if (slot == -1)
        slot = cg_sym_add(name, cg->current_owner, "variable");

    if (slot == -1)
    {
        cg->error = 1;
        return 0;
    }

    cg_symbols[slot].modifiers = modifiers;
    cg_symbols[slot].value_type = value_type;

    cg_emit_u32(cg, (unsigned int)slot);
    return 1;
}

static unsigned int cg_sym_modifiers(const char* name, int owner)
{
    int slot = cg_sym_find(name, owner);

    if (slot == -1 && owner != cg_main_func)
        slot = cg_sym_find(name, cg_main_func);

    if (slot == -1)
        return 0;

    return cg_symbols[slot].modifiers;
}

static void cg_fixup_patch(Codegen* cg, CgFixup* fx)
{
    cg_patch_jump(cg, fx->pos, fx->slot);
}

static void cg_process_fixups(Codegen* cg)
{
    int pass;
    int i;

    /* Phase 1: main's references first (main runs first at runtime, so
       its auto-declared variables must exist before functions see them).
       Phase 2: everything else in emission order. */
    for (pass = 0; pass < 2; pass++)
    {
        for (i = 0; i < cg_fixup_count; i++)
        {
            CgFixup* fx = &cg_fixups[i];
            int slot;

            if (cg->error)
                return;

            if ((fx->owner == cg_main_func) != (pass == 0))
                continue;

            slot = cg_sym_find(fx->name, fx->owner);

            if (slot == -1 && fx->owner != cg_main_func)
                slot = cg_sym_find(fx->name, cg_main_func);

            if (slot == -1)
            {
                if (!fx->is_store)
                {
                    fprintf(stderr,
                            "NwC codegen: undefined variable '%s'\n",
                            fx->name);
                    cg->error = 1;
                    return;
                }

                slot = cg_sym_add(fx->name, fx->owner, "variable");

                if (slot == -1)
                {
                    cg->error = 1;
                    return;
                }
            }

            fx->slot = slot;
            cg_fixup_patch(cg, fx);
        }
    }
}

typedef enum
{
    OP_NOP          = 0x00,

    /* Стек */
    OP_PUSH_INT     = 0x10,   /* u64 */
    OP_PUSH_FLOAT   = 0x11,   /* double (8 byte) */
    OP_PUSH_STRING  = 0x12,   /* u32 len + bytes */
    OP_PUSH_CHAR    = 0x13,   /* u32 codepoint */
    OP_PUSH_BOOL    = 0x14,   /* u8 0/1 */
    OP_POP          = 0x15,
    OP_LOAD         = 0x20,   /* u32 len + name */
    OP_STORE        = 0x21,   /* u32 len + name */
    OP_DECLARE      = 0x22,   /* u32 len + name, u8 type */
    OP_DECLARE_ARR  = 0x23,   /* u32 len + name, u8 type, u32 size */
    OP_ADD          = 0x30,
    OP_SUB          = 0x31,
    OP_MUL          = 0x32,
    OP_DIV          = 0x33,
    OP_EQ           = 0x40,
    OP_NEQ          = 0x41,
    OP_LT           = 0x42,
    OP_GT           = 0x43,
    OP_LTE          = 0x44,
    OP_GTE          = 0x45,
    OP_OUT          = 0x50,   /* output upper */
    OP_OUT_ENDL     = 0x51,
    OP_IN           = 0x52,
    OP_LINE_IN      = 0x53,   /* u32 count, then names */
    OP_IN_STORE     = 0x54,   /* u32 len + name */
    OP_TIME         = 0x60,
    OP_COLOR        = 0x61,
    OP_GETKEY       = 0x62,
    OP_CLEAR        = 0x63,
    OP_RECT         = 0x64,
    OP_COLOR_PUSH   = 0x65,
    OP_COLOR_POP    = 0x66,
    OP_SLEEP        = 0x67,
    OP_GETKEY_FILTER= 0x68,
    OP_ASM_HLT      = 0x69,
    OP_DRAW_FRAME   = 0x6A,
    OP_DRAW_CUBE    = 0x6B,
    OP_DRAW_WINDOW  = 0x6C,
    OP_DRAW_TEXT    = 0x6D,
    OP_MOUSE_X      = 0x6E,
    OP_MOUSE_Y      = 0x6F,
    OP_FUNC_BEGIN   = 0x70,   /* u32 name, u8 ret_type, params */
    OP_FUNC_END     = 0x71,
    OP_CALL         = 0x72,   /* u32 name */
    OP_RETURN       = 0x73,
    OP_RETURN_VOID  = 0x74,
    OP_POWEROFF     = 0x75,
    OP_REBOOT       = 0x76,
    OP_MOUSE_BUTTONS= 0x77,
    OP_WALLPAPER    = 0x78,
    OP_DRAW_INT     = 0x79,
    OP_FLUSH        = 0x7A,
    OP_MOD          = 0x34,

    OP_BIT_AND_BIN  = 0x35,
    OP_BIT_OR_BIN   = 0x36,
    OP_BIT_XOR_BIN  = 0x37,
    OP_SHL          = 0x38,
    OP_SHR          = 0x39,
    OP_BIT_NOT      = 0x3A,
    OP_VOLATILE     = 0x3B,

    OP_LOW_OUT8         = 0x90,
    OP_LOW_IN8          = 0x91,
    OP_LOW_OUT16        = 0x92,
    OP_LOW_IN16         = 0x93,
    OP_LOW_OUT32        = 0x94,
    OP_LOW_IN32         = 0x95,
    OP_LOW_MEM_WRITE8   = 0x96,
    OP_LOW_MEM_READ8    = 0x97,
    OP_LOW_MEM_WRITE16  = 0x98,
    OP_LOW_MEM_READ16   = 0x99,
    OP_LOW_MEM_WRITE32  = 0x9A,
    OP_LOW_MEM_READ32   = 0x9B,
    OP_LOW_CLI          = 0x9C,
    OP_LOW_STI          = 0x9D,

    OP_AND          = 0x46,
    OP_OR           = 0x47,
    OP_NOT          = 0x48,
    OP_STRCMP       = 0x49,
    OP_STRNCMP      = 0x4A,

    OP_JUMP         = 0x80,
    OP_JUMP_FALSE   = 0x81,

    OP_DUP          = 0x16,

    OP_BREAK        = 0x82,
    OP_CONTINUE     = 0x83,

    OP_LOAD_ARR  = 0x24,
    OP_STORE_ARR = 0x25,

    OP_FS_COUNT  = 0xA0,
    OP_FS_NAME   = 0xA1,
    OP_FS_TYPE   = 0xA2,
    OP_FS_SIZE   = 0xA3,
    OP_FS_ENTER  = 0xA4,
    OP_FS_UP     = 0xA5,
    OP_FS_PATH   = 0xA6,
    OP_RAM_MB    = 0xA7,
    OP_UPTIME_SEC= 0xA8,
    OP_DISK_KB   = 0xA9,
    OP_DISK_FILES= 0xAA,
    OP_TIME_HOUR = 0xAB,
    OP_TIME_MINUTE = 0xAC,
    OP_TIME_SECOND = 0xAD,
    OP_TZ_GET    = 0xAE,
    OP_TZ_SET    = 0xAF,
    OP_CPU_LOAD  = 0xB0,
    OP_GPU_LOAD  = 0xB1,
    OP_CURSOR    = 0xB2,
    OP_FS_CREATE = 0xB3,
    OP_FS_DELETE = 0xB4,
    OP_FS_WRITE  = 0xB5,
    OP_FS_MKDIR  = 0xB6,
    OP_BUF_CLEAR = 0xB7,
    OP_BUF_LEN   = 0xB8,
    OP_BUF_PUT   = 0xB9,
    OP_BUF_BACK  = 0xBA,
    OP_BUF_TEXT  = 0xBB,
    OP_BUF_LOAD  = 0xBC,
    OP_BUF_SAVE  = 0xBD,
    OP_POLLKEY   = 0xBE,
    OP_GRADIENT  = 0xC0,
    OP_RGRAD     = 0xC1,
    OP_GLASS     = 0xC2,
    OP_ORB       = 0xC3,
    OP_ELLIPSE   = 0xC4,
    OP_BUBBLE    = 0xC5,
    OP_SHI       = 0xC6,
    OP_SHS       = 0xC7,
    OP_FS_RENAME = 0xC8,
    OP_FS_NWO_GUI = 0xC9,
    OP_DIALOG_INPUT = 0xCA,
    OP_DIALOG_CONFIRM = 0xCB,
    OP_FS_PROPS = 0xCF,
    OP_RUN_EMBEDDED = 0xCC,
    OP_TERM_RUN = 0xCD,
    OP_TERM_VIEW = 0xCE,

    OP_HALT         = 0xFF

} OpCode;

static void cg_node(Codegen* cg, ASTNode* node);
static void cg_expression(Codegen* cg, ASTNode* node);
static void cg_statement(Codegen* cg, ASTNode* node);
static void cg_for(Codegen* cg, ASTNode* node);
static void cg_do_while(Codegen* cg, ASTNode* node);
static void cg_low_call(Codegen* cg, ASTNode* node);
static int cg_call(Codegen* cg, ASTNode* node);

static unsigned char cg_type_byte(ValueType t)
{
    switch (t)
    {
        case TYPE_INT:       return 1;
        case TYPE_FLOAT:     return 2;
        case TYPE_BOOL:      return 3;
        case TYPE_LONG:      return 4;
        case TYPE_LONG_LONG: return 5;
        case TYPE_CHAR:      return 6;
        case TYPE_STRING:    return 7;
        case TYPE_VOID:      return 0;
        default:             return 0;
    }
}

static unsigned char cg_decl_type_byte(ValueType t, unsigned int modifiers)
{
    unsigned char type = cg_type_byte(t);

    if (modifiers & NWC_MOD_NONZERO)
        type |= 0x80;

    if (modifiers & NWC_MOD_NEEDED)
        type |= 0x40;

    if (modifiers & NWC_MOD_CONST)
        type |= 0x20;

    return type;
}

static int cg_text_equals_ci(const char* a, const char* b)
{
    int i = 0;

    if (a == NULL || b == NULL)
        return 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        char ca = a[i];
        char cb = b[i];

        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');

        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');

        if (ca != cb)
            return 0;

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}


static int cg_key_constant(const char* name, long long* value)
{
    struct KeyName
    {
        const char* name;
        long long value;
    };

    static const struct KeyName keys[] = {
        { "NONE", 0 },
        { "ESC", 255 },
        { "ENTER", 256 },
        { "BACKSPACE", 257 },
        { "TAB", 258 },
        { "UP", 259 },
        { "DOWN", 260 },
        { "LEFT", 261 },
        { "RIGHT", 262 },
        { "SUPER_S", 263 },
        { "F5", 264 },
        { "F7", 267 },
        { "SPACE", 32 },
        { "KEY_NONE", 0 },
        { "KEY_ESC", 255 },
        { "KEY_ENTER", 256 },
        { "KEY_BACKSPACE", 257 },
        { "KEY_TAB", 258 },
        { "KEY_UP", 259 },
        { "KEY_DOWN", 260 },
        { "KEY_LEFT", 261 },
        { "KEY_RIGHT", 262 },
        { "KEY_SUPER_S", 263 },
        { "KEY_F5", 264 },
        { "KEY_F7", 267 },
        { "KEY_SPACE", 32 }
    };

    unsigned int i;

    if (name == NULL)
        return 0;

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        if (strcmp(name, keys[i].name) == 0)
        {
            *value = keys[i].value;
            return 1;
        }
    }

    return 0;
}


static int cg_color_value(Codegen* cg, const char* name)
{
    static const struct
    {
        const char* name;
        int value;
    } colors[] = {
        { "BLACK",           0 },
        { "BLUE",            1 },
        { "GREEN",           2 },
        { "CYAN",            3 },
        { "RED",             4 },
        { "MAGENTA",         5 },
        { "BROWN",           6 },
        { "LIGHT_GRAY",      7 },
        { "LIGHT_GREY",      7 },
        { "GRAY",            7 },
        { "GREY",            7 },
        { "DARK_GRAY",       8 },
        { "DARK_GREY",       8 },
        { "LIGHT_BLUE",      9 },
        { "LIGHT_GREEN",    10 },
        { "LIGHT_CYAN",     11 },
        { "LIGHT_RED",      12 },
        { "LIGHT_MAGENTA",  13 },
        { "YELLOW",         14 },
        { "WHITE",          15 },
        { "DARK_RED",        4 },
        { "DARK_BLUE",       1 },
        { "DARK_GREEN",      2 },
        { "DARK_CYAN",       3 },
        { "DARK_MAGENTA",    5 },
        { "DARK_BROWN",      6 },
        { "DARK_YELLOW",    14 }
    };

    unsigned int i;

    for (i = 0;
         i < sizeof(colors) / sizeof(colors[0]);
         i++)
    {
        if (cg_text_equals_ci(name, colors[i].name))
            return colors[i].value;
    }

    fprintf(
        stderr,
        "NwC codegen: unknown color '%s'\n",
        name
    );

    cg->error = 1;

    return -1;
}


static void cg_inc_dec(Codegen* cg, ASTNode* target, int delta, int post)
{
    unsigned char op = (delta > 0) ? OP_ADD : OP_SUB;

    if (target == NULL || target->type != AST_IDENTIFIER)
    {
        cg_error(cg, "increment requires a variable");
        return;
    }

    if (cg_sym_modifiers(target->text, cg->current_owner) & NWC_MOD_CONST)
    {
        cg_error(cg, "assignment to const variable");
        return;
    }

    cg_emit_byte(cg, OP_LOAD);
    cg_emit_slot(cg, target->text, 0);

    if (post)
    {
        cg_emit_byte(cg, OP_DUP);
        cg_emit_int(cg, 1);
        cg_emit_byte(cg, op);
        cg_emit_byte(cg, OP_STORE);
        cg_emit_slot(cg, target->text, 1);
    }
    else
    {
        cg_emit_int(cg, 1);
        cg_emit_byte(cg, op);
        cg_emit_byte(cg, OP_DUP);
        cg_emit_byte(cg, OP_STORE);
        cg_emit_slot(cg, target->text, 1);
    }
}

static void cg_binary(Codegen* cg, ASTNode* node)
{
    const char* op = node->text;

    if (op == NULL)
    {
        cg_error(cg, "binary op without text");
        return;
    }

    if (node->left == NULL)
    {
        if (strcmp(op, "-") == 0)
        {
            cg_emit_int(cg, 0);
            cg_expression(cg, node->right);
            cg_emit_byte(cg, OP_SUB);
            return;
        }

        if (strcmp(op, "!") == 0)
        {
            cg_expression(cg, node->right);
            cg_emit_byte(cg, OP_NOT);
            return;
        }

        if (strcmp(op, "~") == 0)
        {
            cg_expression(cg, node->right);
            cg_emit_byte(cg, OP_BIT_NOT);
            return;
        }

        if (strcmp(op, "pre++") == 0)
        {
            cg_inc_dec(cg, node->right, 1, 0);
            return;
        }

        if (strcmp(op, "pre--") == 0)
        {
            cg_inc_dec(cg, node->right, -1, 0);
            return;
        }

        cg_error(cg, "unknown unary operator");
        return;
    }

    if (strcmp(op, "post++") == 0)
    {
        cg_inc_dec(cg, node->left, 1, 1);
        return;
    }

    if (strcmp(op, "post--") == 0)
    {
        cg_inc_dec(cg, node->left, -1, 1);
        return;
    }

    cg_expression(cg, node->left);
    cg_expression(cg, node->right);

    if (strcmp(op, "+") == 0)      cg_emit_byte(cg, OP_ADD);
    else if (strcmp(op, "-") == 0) cg_emit_byte(cg, OP_SUB);
    else if (strcmp(op, "*") == 0) cg_emit_byte(cg, OP_MUL);
    else if (strcmp(op, "/") == 0) cg_emit_byte(cg, OP_DIV);
    else if (strcmp(op, "%") == 0) cg_emit_byte(cg, OP_MOD);
    else if (strcmp(op, "&") == 0) cg_emit_byte(cg, OP_BIT_AND_BIN);
    else if (strcmp(op, "|") == 0) cg_emit_byte(cg, OP_BIT_OR_BIN);
    else if (strcmp(op, "^") == 0) cg_emit_byte(cg, OP_BIT_XOR_BIN);
    else if (strcmp(op, "<<") == 0) cg_emit_byte(cg, OP_SHL);
    else if (strcmp(op, ">>") == 0) cg_emit_byte(cg, OP_SHR);
    else if (strcmp(op, "==") == 0) cg_emit_byte(cg, OP_EQ);
    else if (strcmp(op, "!=") == 0) cg_emit_byte(cg, OP_NEQ);
    else if (strcmp(op, "<") == 0)  cg_emit_byte(cg, OP_LT);
    else if (strcmp(op, ">") == 0)  cg_emit_byte(cg, OP_GT);
    else if (strcmp(op, "<=") == 0) cg_emit_byte(cg, OP_LTE);
    else if (strcmp(op, ">=") == 0) cg_emit_byte(cg, OP_GTE);
    else if (strcmp(op, "&&") == 0) cg_emit_byte(cg, OP_AND);
    else if (strcmp(op, "||") == 0) cg_emit_byte(cg, OP_OR);
    else
    {
        cg_error(cg, "unknown binary operator");
    }
}


static void cg_expression(Codegen* cg, ASTNode* node)
{
    if (node == NULL)
    {
        cg_error(cg, "null expression");
        return;
    }

    switch (node->type)
    {
        case AST_NUMBER:
            cg_emit_int(cg, (int)node->integer_value);
            break;

        case AST_FLOAT:
            cg_emit_byte(cg, OP_PUSH_FLOAT);
            cg_emit_double(cg, node->float_value);
            break;

        case AST_STRING:
            cg_emit_byte(cg, OP_PUSH_STRING);
            cg_emit_string(cg, node->text);
            break;

        case AST_CHAR:
            cg_emit_byte(cg, OP_PUSH_CHAR);
            if (node->text != NULL && node->text[0] != '\0')
                cg_emit_u32(cg, (unsigned int)(unsigned char)node->text[0]);
            else
                cg_emit_u32(cg, 0);
            break;

        case AST_BOOLEAN:
            cg_emit_byte(cg, OP_PUSH_BOOL);
            cg_emit_byte(cg, (unsigned char)(node->integer_value ? 1 : 0));
            break;

        case AST_IDENTIFIER:
        {
            long long key_value;

            if (cg_key_constant(node->text, &key_value))
            {
                cg_emit_int(cg, (int)key_value);
            }
            else
            {
                if (cg_sym_modifiers(node->text, cg->current_owner) &
                    NWC_MOD_NEEDED)
                    cg_emit_byte(cg, OP_VOLATILE);

                cg_emit_byte(cg, OP_LOAD);
                cg_emit_slot(cg, node->text, 0);
            }

            break;
        }

        case AST_BINARY_OPERATION:
            cg_binary(cg, node);
            break;

        case AST_LOW_CALL:
            cg_low_call(cg, node);
            break;

        case AST_EXPRESSION:
            if (node->text != NULL &&
                strcmp(node->text, "nw::endl") == 0)
            {
                cg_emit_byte(cg, OP_OUT_ENDL);
                return;
            }

            if (node->text != NULL &&
                strcmp(node->text, "nw::cin") == 0)
            {
                cg_emit_byte(cg, OP_IN);
                return;
            }

            cg_error(cg, "unsupported special expression");
            break;

        case AST_INDEX:
            cg_expression(cg, node->child);
            cg_emit_byte (cg, OP_LOAD_ARR);
            cg_emit_slot(cg, node->text, 0);
            break;

        case AST_CALL:
            if (node->text != NULL &&
                strcmp(node->text, "strcmp") == 0)
            {
                ASTNode* arg = node->child;

                if (arg == NULL || arg->next == NULL ||
                    arg->next->next != NULL)
                {
                    cg_error(cg, "strcmp needs two arguments");
                    break;
                }

                cg_expression(cg, arg);
                cg_expression(cg, arg->next);
                cg_emit_byte(cg, OP_STRCMP);
                break;
            }

            if (node->text != NULL &&
                strcmp(node->text, "strncmp") == 0)
            {
                ASTNode* arg = node->child;

                if (arg == NULL || arg->next == NULL ||
                    arg->next->next == NULL ||
                    arg->next->next->next != NULL)
                {
                    cg_error(cg, "strncmp needs three arguments");
                    break;
                }

                cg_expression(cg, arg);
                cg_expression(cg, arg->next);
                cg_expression(cg, arg->next->next);
                cg_emit_byte(cg, OP_STRNCMP);
                break;
            }

            if (!cg_call(cg, node))
            {
                cg_error(cg, "call does not return a value");
            }
            break;

        default:
            cg_error(cg, "unsupported expression node");
            break;
    }
}

static void cg_variable_decl(Codegen* cg, ASTNode* node)
{
    if (node->type == AST_ARRAY_DECLARATION)
    {
        cg_emit_byte(cg, OP_DECLARE_ARR);
        if (!cg_emit_decl_slot(cg, node->text, node->modifiers,
                               node->value_type))
            return;
        cg_emit_byte(cg, cg_decl_type_byte(node->value_type, node->modifiers));
        cg_emit_u32(cg, node->array_size);
    }
    else
    {
        cg_emit_byte(cg, OP_DECLARE);
        if (!cg_emit_decl_slot(cg, node->text, node->modifiers,
                               node->value_type))
            return;
        cg_emit_byte(cg, cg_decl_type_byte(node->value_type, node->modifiers));
    }
    if (node->child != NULL)
    {
        cg_expression(cg, node->child);

        cg_emit_byte(cg, OP_STORE);
        cg_emit_slot(cg, node->text, 1);
    }
}


static void cg_output(Codegen* cg, ASTNode* node)
{
    ASTNode* item = node->child;
    int colored = node->text != NULL;

    if (colored)
    {
        int color = cg_color_value(cg, node->text);

        if (color < 0)
            return;

        cg_emit_byte(cg, OP_COLOR_PUSH);
        cg_emit_int(cg, color);
        cg_emit_byte(cg, OP_COLOR);
    }

    while (item != NULL)
    {
        if (item->type == AST_EXPRESSION &&
            item->text != NULL &&
            strcmp(item->text, "nw::endl") == 0)
        {
            cg_emit_byte(cg, OP_OUT_ENDL);
        }
        else
        {
            cg_expression(cg, item);
            cg_emit_byte(cg, OP_OUT);
        }

        item = item->next;
    }

    if (colored)
    {
        if (!node->integer_value)
            cg_emit_byte(cg, OP_OUT_ENDL);

        cg_emit_byte(cg, OP_COLOR_POP);
    }
}


static int cg_rgb_color_value(const char* name)
{
    static const struct
    {
        const char* name;
        int value;
    } colors[] = {
        { "BLACK",         0x000000 },
        { "WHITE",         0xFFFFFF },
        { "RED",           0xFF0000 },
        { "DARK_RED",      0x800000 },
        { "LIGHT_RED",     0xFF8080 },
        { "GREEN",         0x00FF00 },
        { "DARK_GREEN",    0x008000 },
        { "LIGHT_GREEN",   0x80FF80 },
        { "BLUE",          0x0000FF },
        { "DARK_BLUE",     0x000080 },
        { "LIGHT_BLUE",    0x8080FF },
        { "CYAN",          0x00FFFF },
        { "DARK_CYAN",     0x008080 },
        { "LIGHT_CYAN",    0x80FFFF },
        { "MAGENTA",       0xFF00FF },
        { "DARK_MAGENTA",  0x800080 },
        { "LIGHT_MAGENTA", 0xFF80FF },
        { "YELLOW",        0xFFFF00 },
        { "BROWN",         0x808000 },
        { "ORANGE",        0xFFA500 },
        { "PURPLE",        0x800080 },
        { "PINK",          0xFFC0CB },
        { "GRAY",          0x808080 },
        { "GREY",          0x808080 },
        { "LIGHT_GRAY",    0xC0C0C0 },
        { "LIGHT_GREY",    0xC0C0C0 },
        { "DARK_GRAY",     0x404040 },
        { "DARK_GREY",     0x404040 },
        { "NAVY",          0x000080 },
        { "TEAL",          0x008080 },
        { "LIME",          0x00FF00 },
        { "MAROON",        0x800000 }
    };

    unsigned int i;

    if (name == NULL)
        return -1;

    for (i = 0; i < sizeof(colors) / sizeof(colors[0]); i++)
    {
        if (cg_text_equals_ci(name, colors[i].name))
            return colors[i].value;
    }

    return -1;
}


static void cg_input(Codegen* cg, ASTNode* node)
{
    if (node->child != NULL &&
        node->child->type == AST_IDENTIFIER)
    {
        cg_emit_byte(cg, OP_IN_STORE);
        cg_emit_slot(cg, node->child->text, 1);
    }
    else
    {
        cg_error(cg, "nw::cin requires identifier target");
    }
}


static void cg_line_input(Codegen* cg, ASTNode* node)
{
    ASTNode* arg = node->child;

    if (arg == NULL)
    {
        cg_error(cg, "nw::line needs arguments");
        return;
    }
    ASTNode* target = NULL;

    while (arg != NULL)
    {
        if (arg->type == AST_IDENTIFIER)
            target = arg;

        arg = arg->next;
    }

    if (target == NULL)
    {
        cg_error(cg, "nw::line needs a variable target");
        return;
    }

    cg_emit_byte(cg, OP_LINE_IN);
    cg_emit_u32(cg, 1);
    cg_emit_slot(cg, target->text, 1);
}


static void cg_return(Codegen* cg, ASTNode* node)
{
    if (node->child != NULL)
    {
        cg_expression(cg, node->child);
        cg_emit_byte(cg, OP_RETURN);
    }
    else
    {
        cg_emit_byte(cg, OP_RETURN_VOID);
    }
}


static int cg_call(Codegen* cg, ASTNode* node)
{
    if (node->text == NULL)
    {
        cg_error(cg, "call without name");
        return 0;
    }

    if (strcmp(node->text, "nw::time") == 0)
    {
        cg_emit_byte(cg, OP_TIME);
        return 0;
    }

    if (strcmp(node->text, "nw::color") == 0)
    {
        if (node->child != NULL)
            cg_expression(cg, node->child);

        cg_emit_byte(cg, OP_COLOR);
        return 0;
    }

    if (strcmp(node->text, "nw::getkey") == 0)
    {
        if (node->child != NULL)
        {
            cg_expression(cg, node->child);
            cg_emit_byte(cg, OP_GETKEY_FILTER);
        }
        else
        {
            cg_emit_byte(cg, OP_GETKEY);
        }

        return 1;
    }

    if (strcmp(node->text, "nw::pollkey") == 0)
    {
        cg_emit_byte(cg, OP_POLLKEY);
        return 1;
    }

    if (strcmp(node->text, "nw::mouse_x") == 0)
    {
        cg_emit_byte(cg, OP_MOUSE_X);
        return 1;
    }

    if (strcmp(node->text, "nw::mouse_y") == 0)
    {
        cg_emit_byte(cg, OP_MOUSE_Y);
        return 1;
    }

    if (strcmp(node->text, "nw::mouse_buttons") == 0)
    {
        cg_emit_byte(cg, OP_MOUSE_BUTTONS);
        return 1;
    }

    if (strcmp(node->text, "nw::poweroff") == 0)
    {
        cg_emit_byte(cg, OP_POWEROFF);
        return 0;
    }

    if (strcmp(node->text, "nw::reboot") == 0)
    {
        cg_emit_byte(cg, OP_REBOOT);
        return 0;
    }

    if (strcmp(node->text, "nw::clear") == 0 ||
        strcmp(node->text, "clear_screen") == 0 ||
        strcmp(node->text, "nw::clear_screen") == 0)
    {
        cg_emit_byte(cg, OP_CLEAR);
        return 0;
    }

    if (strcmp(node->text, "sleep") == 0)
    {
        if (node->child == NULL)
        {
            cg_error(cg, "sleep needs milliseconds");
            return 0;
        }

        cg_expression(cg, node->child);
        cg_emit_byte(cg, OP_SLEEP);
        return 0;
    }

    if (strcmp(node->text, "nw::window") == 0)
    {
        ASTNode* arg = node->child;
        int count = 0;

        while (arg != NULL)
        {
            count++;
            arg = arg->next;
        }

        if (count != 6)
        {
            cg_error(cg, "nw::window needs x, y, width, height, color, title");
            return 0;
        }

        arg = node->child;

        while (arg != NULL)
        {
            cg_expression(cg, arg);
            arg = arg->next;
        }

        cg_gui_used = 1;
        cg_emit_byte(cg, OP_DRAW_WINDOW);
        return 0;
    }

    if (strcmp(node->text, "nw::text") == 0)
    {
        ASTNode* arg = node->child;
        int count = 0;

        while (arg != NULL)
        {
            count++;
            arg = arg->next;
        }

        if (count != 4)
        {
            cg_error(cg, "nw::text needs x, y, color, text");
            return 0;
        }

        arg = node->child;

        while (arg != NULL)
        {
            cg_expression(cg, arg);
            arg = arg->next;
        }

        cg_gui_used = 1;
        cg_emit_byte(cg, OP_DRAW_TEXT);
        return 0;
    }

    if (strcmp(node->text, "nw::wallpaper") == 0)
    {
        cg_gui_used = 1;
        cg_emit_byte(cg, OP_WALLPAPER);
        return 0;
    }

    if (strcmp(node->text, "nw::flush") == 0)
    {
        cg_gui_used = 1;
        cg_emit_byte(cg, OP_FLUSH);
        return 0;
    }

    if (strcmp(node->text, "nw::number") == 0)
    {
        ASTNode* arg = node->child;
        int count = 0;

        while (arg != NULL)
        {
            count++;
            arg = arg->next;
        }

        if (count != 4)
        {
            cg_error(cg, "nw::number needs x, y, color, value");
            return 0;
        }

        arg = node->child;

        while (arg != NULL)
        {
            cg_expression(cg, arg);
            arg = arg->next;
        }

        cg_gui_used = 1;
        cg_emit_byte(cg, OP_DRAW_INT);
        return 0;
    }

    if (strcmp(node->text, "nw::rect") == 0)
    {
        ASTNode* arg = node->child;
        int count = 0;

        while (arg != NULL)
        {
            count++;
            arg = arg->next;
        }

        if (count != 5)
        {
            cg_error(cg, "nw::rect needs x, y, width, height, color");
            return 0;
        }

        arg = node->child;

        while (arg != NULL)
        {
            cg_expression(cg, arg);
            arg = arg->next;
        }

        cg_gui_used = 1;
        cg_emit_byte(cg, OP_RECT);
        return 0;
    }

    if (strcmp(node->text, "nw::gradient") == 0 ||
        strcmp(node->text, "nw::rgrad") == 0 ||
        strcmp(node->text, "nw::glass") == 0 ||
        strcmp(node->text, "nw::orb") == 0 ||
        strcmp(node->text, "nw::ellipse") == 0 ||
        strcmp(node->text, "nw::bubble") == 0)
    {
        int expect = 6;
        int op = OP_GRADIENT;
        ASTNode* arg = node->child;
        int count = 0;

        if (strcmp(node->text, "nw::rgrad") == 0) { expect = 7; op = OP_RGRAD; }
        else if (strcmp(node->text, "nw::glass") == 0) { expect = 7; op = OP_GLASS; }
        else if (strcmp(node->text, "nw::orb") == 0) { expect = 4; op = OP_ORB; }
        else if (strcmp(node->text, "nw::ellipse") == 0) { expect = 7; op = OP_ELLIPSE; }
        else if (strcmp(node->text, "nw::bubble") == 0) { expect = 4; op = OP_BUBBLE; }

        while (arg != NULL) { count++; arg = arg->next; }
        if (count != expect)
        {
            cg_error(cg, "wrong argument count for aero primitive (gradient x,y,w,h,c1,c2 | rgrad x,y,w,h,r,c1,c2 | glass x,y,w,h,r,color,alpha | orb cx,cy,r,color | ellipse x,y,w,h,c1,c2,alpha | bubble cx,cy,r,alpha)");
            return 0;
        }
        arg = node->child;
        while (arg != NULL) { cg_expression(cg, arg); arg = arg->next; }
        cg_gui_used = 1;
        cg_emit_byte(cg, op);
        return 0;
    }

    if (strcmp(node->text, "nw::shi") == 0 || strcmp(node->text, "nw::shs") == 0)
    {
        ASTNode* arg = node->child;
        int count = 0;

        while (arg != NULL) { count++; arg = arg->next; }
        if (count != 2)
        {
            cg_error(cg, "nw::shi / nw::shs take 2 arguments (op, value)");
            return 0;
        }
        arg = node->child;
        while (arg != NULL) { cg_expression(cg, arg); arg = arg->next; }
        cg_emit_byte(cg, strcmp(node->text, "nw::shi") == 0 ? OP_SHI : OP_SHS);
        return 1;
    }

    if (strcmp(node->text, "nw::fs_count") == 0) { cg_emit_byte(cg, OP_FS_COUNT); return 1; }
    if (strcmp(node->text, "nw::fs_path")  == 0) { cg_emit_byte(cg, OP_FS_PATH);  return 1; }
    if (strcmp(node->text, "nw::fs_name")  == 0)
    { cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_NAME); return 1; }
    if (strcmp(node->text, "nw::fs_type")  == 0)
    { cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_TYPE); return 1; }
    if (strcmp(node->text, "nw::fs_size")  == 0)
    { cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_SIZE); return 1; }
    if (strcmp(node->text, "nw::fs_enter") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_ENTER); return 0; }
    if (strcmp(node->text, "nw::fs_up") == 0)
    { cg_emit_byte(cg, OP_FS_UP); return 0; }
    if (strcmp(node->text, "nw::ram_mb") == 0) { cg_emit_byte(cg, OP_RAM_MB); return 1; }
    if (strcmp(node->text, "nw::uptime_sec") == 0) { cg_emit_byte(cg, OP_UPTIME_SEC); return 1; }
    if (strcmp(node->text, "nw::disk_used_kb") == 0) { cg_emit_byte(cg, OP_DISK_KB); return 1; }
    if (strcmp(node->text, "nw::disk_files") == 0) { cg_emit_byte(cg, OP_DISK_FILES); return 1; }
    if (strcmp(node->text, "nw::hour") == 0) { cg_emit_byte(cg, OP_TIME_HOUR); return 1; }
    if (strcmp(node->text, "nw::minute") == 0) { cg_emit_byte(cg, OP_TIME_MINUTE); return 1; }
    if (strcmp(node->text, "nw::second") == 0) { cg_emit_byte(cg, OP_TIME_SECOND); return 1; }
    if (strcmp(node->text, "nw::tz_get") == 0) { cg_emit_byte(cg, OP_TZ_GET); return 1; }
    if (strcmp(node->text, "nw::tz_set") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_TZ_SET); return 0; }
    if (strcmp(node->text, "nw::cpu_load") == 0) { cg_emit_byte(cg, OP_CPU_LOAD); return 1; }
    if (strcmp(node->text, "nw::gpu_load") == 0) { cg_emit_byte(cg, OP_GPU_LOAD); return 1; }
    if (strcmp(node->text, "nw::cursor") == 0)
    {
        ASTNode* arg = node->child;
        int count = 0;
        while (arg != NULL) { count++; arg = arg->next; }
        if (count != 2) { cg_error(cg, "nw::cursor needs x, y"); return 0; }
        arg = node->child;
        while (arg != NULL) { cg_expression(cg, arg); arg = arg->next; }
        cg_gui_used = 1;
        cg_emit_byte(cg, OP_CURSOR);
        return 0;
    }
    if (strcmp(node->text, "nw::fs_create") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_CREATE); return 1; }
    if (strcmp(node->text, "nw::fs_delete") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_DELETE); return 1; }
    if (strcmp(node->text, "nw::fs_mkdir") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_MKDIR); return 1; }
    if (strcmp(node->text, "nw::fs_write") == 0)
    {
        if (node->child && node->child->next)
        {
            cg_expression(cg, node->child);
            cg_expression(cg, node->child->next);
        }
        cg_emit_byte(cg, OP_FS_WRITE);
        return 1;
    }
    if (strcmp(node->text, "nw::fs_rename") == 0)
    {
        if (node->child && node->child->next)
        {
            cg_expression(cg, node->child);
            cg_expression(cg, node->child->next);
        }
        cg_emit_byte(cg, OP_FS_RENAME);
        return 1;
    }
    if (strcmp(node->text, "nw::fs_props") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_PROPS); return 0; }
    if (strcmp(node->text, "nw::fs_nwo_gui") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_FS_NWO_GUI); return 1; }
    if (strcmp(node->text, "nw::run_embedded") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_RUN_EMBEDDED); return 1; }
    if (strcmp(node->text, "nw::term_run") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_TERM_RUN); return 1; }
    if (strcmp(node->text, "nw::dialog") == 0)
    {
        if (node->child && node->child->next)
        {
            cg_expression(cg, node->child);
            cg_expression(cg, node->child->next);
        }
        cg_emit_byte(cg, OP_DIALOG_INPUT);
        return 1;
    }
    if (strcmp(node->text, "nw::confirm") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_DIALOG_CONFIRM); return 1; }
    if (strcmp(node->text, "nw::term_view") == 0)
    {
        ASTNode* arg = node->child;
        int count = 0;
        while (arg != NULL) { count++; arg = arg->next; }
        if (count != 5)
        {
            cg_error(cg, "nw::term_view needs x, y, width, height, scroll");
            return 0;
        }
        arg = node->child;
        while (arg != NULL) { cg_expression(cg, arg); arg = arg->next; }
        cg_emit_byte(cg, OP_TERM_VIEW);
        return 0;
    }
    if (strcmp(node->text, "nw::buf_clear") == 0) { cg_emit_byte(cg, OP_BUF_CLEAR); return 0; }
    if (strcmp(node->text, "nw::buf_len") == 0) { cg_emit_byte(cg, OP_BUF_LEN); return 1; }
    if (strcmp(node->text, "nw::buf_put") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_BUF_PUT); return 0; }
    if (strcmp(node->text, "nw::buf_back") == 0) { cg_emit_byte(cg, OP_BUF_BACK); return 0; }
    if (strcmp(node->text, "nw::buf_text") == 0) { cg_emit_byte(cg, OP_BUF_TEXT); return 1; }
    if (strcmp(node->text, "nw::buf_load") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_BUF_LOAD); return 1; }
    if (strcmp(node->text, "nw::buf_save") == 0)
    { if (node->child) cg_expression(cg, node->child); cg_emit_byte(cg, OP_BUF_SAVE); return 1; }

    if (node->text != NULL && strncmp(node->text, "nw::draw:", 9) == 0)
    {
        const char* shape = node->text + 9;
        ASTNode* arg = node->child;
        long long x = 0, y = 0, x2 = -1, y2 = -1;
        long long w = 100, h = 100;
        int has_x2 = 0, has_y2 = 0;
        int color = -1;

        if (node->text_color_name != NULL)
            color = cg_rgb_color_value(node->text_color_name);

        while (arg != NULL)
        {
            if (arg->text != NULL && strncmp(arg->text, "K:", 2) == 0)
            {
                const char* p = arg->text + 2;
                const char* v = strchr(p, '=');

                if (v != NULL)
                {
                    long long n = strtoll(v + 1, NULL, 0);

                    if (strncmp(p, "x2=", 3) == 0) { x2 = n; has_x2 = 1; }
                    else if (strncmp(p, "y2=", 3) == 0) { y2 = n; has_y2 = 1; }
                    else if (strncmp(p, "x=", 2) == 0) x = n;
                    else if (strncmp(p, "y=", 2) == 0) y = n;
                    else if (strncmp(p, "w=", 2) == 0) w = n;
                    else if (strncmp(p, "h=", 2) == 0) h = n;
                    else if (strncmp(p, "width=", 6) == 0) w = n;
                    else if (strncmp(p, "height=", 7) == 0) h = n;
                    else if (strncmp(p, "size=", 5) == 0) w = h = n;
                }
            }

            arg = arg->next;
        }

        if (has_x2 && x2 >= x) w = x2 - x;
        if (has_y2 && y2 >= y) h = y2 - y;

        int no_window = (node->draw_flags & 1) != 0;

        if (color < 0)
        {
            if (node->text_color_name != NULL)
            {
                cg_error(cg, "unknown draw color");
                return 0;
            }

            if (cg_text_equals_ci(shape, "CUBE"))
                color = 0x0000FF;
            else if (cg_text_equals_ci(shape, "FRAME") && !no_window)
                color = 0x000080;
            else
                color = 0xFFFFFF;
        }

        if (cg_text_equals_ci(shape, "FRAME") && !no_window)
        {
            cg_emit_int(cg, x);
            cg_emit_int(cg, y);
            cg_emit_int(cg, w);
            cg_emit_int(cg, h);
            cg_emit_int(cg, color);
            cg_emit_byte(cg, OP_PUSH_STRING);
            cg_emit_string(cg, node->text_label != NULL ? node->text_label : "");
            cg_emit_byte(cg, OP_DRAW_WINDOW);
            return 0;
        }

        if (cg_text_equals_ci(shape, "FRAME") ||
            cg_text_equals_ci(shape, "RECT"))
        {
            cg_emit_int(cg, x);
            cg_emit_int(cg, y);
            cg_emit_int(cg, w);
            cg_emit_int(cg, h);
            cg_emit_int(cg, color);
            cg_emit_byte(cg, OP_DRAW_FRAME);
            return 0;
        }

        if (cg_text_equals_ci(shape, "TEXT"))
        {
            cg_emit_int(cg, x);
            cg_emit_int(cg, y);
            cg_emit_int(cg, color);
            cg_emit_byte(cg, OP_PUSH_STRING);
            cg_emit_string(cg, node->text_label != NULL ? node->text_label : "");
            cg_emit_byte(cg, OP_DRAW_TEXT);
            return 0;
        }

        if (cg_text_equals_ci(shape, "CUBE"))
        {
            cg_emit_int(cg, h);
            cg_emit_int(cg, color);
            cg_emit_byte(cg, OP_DRAW_CUBE);
            return 0;
        }

        cg_error(cg, "unknown draw shape");
        return 0;
    }

    {
        ASTNode* arg = node->child;
        int count = 0;
        int func_index = cg_func_index(node->text);

        if (func_index == -1)
        {
            fprintf(stderr,
                    "NwC codegen: unknown function '%s'\n",
                    node->text);
            cg->error = 1;
            return 0;
        }

        while (arg != NULL)
        {
            cg_expression(cg, arg);
            count++;
            arg = arg->next;
        }

        cg_emit_byte(cg, OP_CALL);
        cg_emit_u32(cg, (unsigned int)func_index);
        cg_emit_u32(cg, (unsigned int)count);
    }

    return 1;
}

static void cg_if(Codegen* cg, ASTNode* node)
{
    long false_jump;

    cg_expression(cg, node->child);

    false_jump = cg_emit_jump(cg, OP_JUMP_FALSE);

    cg_statement(cg, node->left);

    if (node->right != NULL)
    {
        long end_jump = cg_emit_jump(cg, OP_JUMP);

        cg_patch_jump(cg, false_jump, ftell(cg->file));

        cg_statement(cg, node->right);

        cg_patch_jump(cg, end_jump, ftell(cg->file));
    }
    else
    {
        cg_patch_jump(cg, false_jump, ftell(cg->file));
    }
}

static void cg_while(Codegen* cg, ASTNode* node)
{
    int depth = cg->loop_depth;
    int break_base;
    int continue_base;
    long loop_start;
    long exit_jump;
    long loop_end;
    int i;

    if (depth >= NWO_MAX_LOOPS)
    {
        cg_error(cg, "too many nested loops");
        return;
    }

    loop_start = ftell(cg->file);
    break_base = cg->break_count;
    continue_base = cg->continue_count;

    cg->loop_start[depth] = loop_start;
    cg->loop_continue[depth] = loop_start;
    cg->loop_break_base[depth] = break_base;
    cg->loop_continue_base[depth] = continue_base;
    cg->loop_depth = depth + 1;

    cg_expression(cg, node->child);

    exit_jump = cg_emit_jump(cg, OP_JUMP_FALSE);

    cg_statement(cg, node->left);

    cg_emit_byte(cg, OP_JUMP);
    cg_emit_u32(cg, (unsigned int)loop_start);

    loop_end = ftell(cg->file);

    cg_patch_jump(cg, exit_jump, loop_end);

    for (i = break_base; i < cg->break_count; i++)
        cg_patch_jump(cg, cg->break_patch[i], loop_end);

    for (i = continue_base; i < cg->continue_count; i++)
        cg_patch_jump(cg, cg->continue_patch[i], loop_start);

    cg->break_count = break_base;
    cg->continue_count = continue_base;
    cg->loop_depth = depth;
}

static void cg_for(Codegen* cg, ASTNode* node)
{
    int depth = cg->loop_depth;
    int break_base;
    int continue_base;
    long loop_start;
    long exit_jump = -1;
    long step_start;
    long loop_end;
    int i;

    if (depth >= NWO_MAX_LOOPS)
    {
        cg_error(cg, "too many nested loops");
        return;
    }

    if (node->body != NULL)
        cg_statement(cg, node->body);

    loop_start = ftell(cg->file);
    break_base = cg->break_count;
    continue_base = cg->continue_count;

    cg->loop_start[depth] = loop_start;
    cg->loop_break_base[depth] = break_base;
    cg->loop_continue_base[depth] = continue_base;
    cg->loop_depth = depth + 1;

    if (node->child != NULL)
    {
        cg_expression(cg, node->child);
        exit_jump = cg_emit_jump(cg, OP_JUMP_FALSE);
    }

    cg_statement(cg, node->left);

    step_start = ftell(cg->file);

    cg->loop_continue[depth] = step_start;

    for (i = continue_base; i < cg->continue_count; i++)
        cg_patch_jump(cg, cg->continue_patch[i], step_start);

    if (node->right != NULL)
        cg_statement(cg, node->right);

    cg_emit_byte(cg, OP_JUMP);
    cg_emit_u32(cg, (unsigned int)loop_start);

    loop_end = ftell(cg->file);

    if (exit_jump != -1)
        cg_patch_jump(cg, exit_jump, loop_end);

    for (i = break_base; i < cg->break_count; i++)
        cg_patch_jump(cg, cg->break_patch[i], loop_end);

    cg->break_count = break_base;
    cg->continue_count = continue_base;
    cg->loop_depth = depth;
}

static void cg_do_while(Codegen* cg, ASTNode* node)
{
    int depth = cg->loop_depth;
    int break_base;
    int continue_base;
    long loop_start;
    long continue_target;
    long loop_end;
    int i;

    if (depth >= NWO_MAX_LOOPS)
    {
        cg_error(cg, "too many nested loops");
        return;
    }

    loop_start = ftell(cg->file);
    break_base = cg->break_count;
    continue_base = cg->continue_count;

    cg->loop_start[depth] = loop_start;
    cg->loop_break_base[depth] = break_base;
    cg->loop_continue_base[depth] = continue_base;
    cg->loop_depth = depth + 1;

    cg_statement(cg, node->left);

    continue_target = ftell(cg->file);

    for (i = continue_base; i < cg->continue_count; i++)
        cg_patch_jump(cg, cg->continue_patch[i], continue_target);

    cg->loop_continue[depth] = continue_target;

    cg_expression(cg, node->child);

    {
        long back = cg_emit_jump(cg, OP_JUMP_FALSE);

        cg_emit_byte(cg, OP_JUMP);
        cg_emit_u32(cg, (unsigned int)loop_start);

        loop_end = ftell(cg->file);

        cg_patch_jump(cg, back, loop_end);
    }

    for (i = break_base; i < cg->break_count; i++)
        cg_patch_jump(cg, cg->break_patch[i], loop_end);

    cg->break_count = break_base;
    cg->continue_count = continue_base;
    cg->loop_depth = depth;
}

static unsigned char cg_assign_opcode(long long token)
{
    switch ((TokenType)token)
    {
        case TOKEN_PLUS_ASSIGN:    return OP_ADD;
        case TOKEN_MINUS_ASSIGN:   return OP_SUB;
        case TOKEN_STAR_ASSIGN:    return OP_MUL;
        case TOKEN_SLASH_ASSIGN:   return OP_DIV;
        case TOKEN_PERCENT_ASSIGN: return OP_MOD;
        case TOKEN_BIT_AND_ASSIGN: return OP_BIT_AND_BIN;
        case TOKEN_BIT_OR_ASSIGN:  return OP_BIT_OR_BIN;
        case TOKEN_BIT_XOR_ASSIGN: return OP_BIT_XOR_BIN;
        case TOKEN_SHL_ASSIGN:     return OP_SHL;
        case TOKEN_SHR_ASSIGN:     return OP_SHR;
        default:                   return 0;
    }
}

static void cg_low_call(Codegen* cg, ASTNode* node)
{
    unsigned char opcode = 0;
    int arg_count = 0;
    int expected = 0;
    ASTNode* arg;

    if (node->text == NULL)
    {
        cg_error(cg, "low-level call without name");
        return;
    }

    if (strcmp(node->text, "low::out8") == 0)
    { opcode = OP_LOW_OUT8; expected = 2; }
    else if (strcmp(node->text, "low::in8") == 0)
    { opcode = OP_LOW_IN8; expected = 1; }
    else if (strcmp(node->text, "low::out16") == 0)
    { opcode = OP_LOW_OUT16; expected = 2; }
    else if (strcmp(node->text, "low::in16") == 0)
    { opcode = OP_LOW_IN16; expected = 1; }
    else if (strcmp(node->text, "low::out32") == 0)
    { opcode = OP_LOW_OUT32; expected = 2; }
    else if (strcmp(node->text, "low::in32") == 0)
    { opcode = OP_LOW_IN32; expected = 1; }
    else if (strcmp(node->text, "low::memory_write8") == 0)
    { opcode = OP_LOW_MEM_WRITE8; expected = 2; }
    else if (strcmp(node->text, "low::memory_read8") == 0)
    { opcode = OP_LOW_MEM_READ8; expected = 1; }
    else if (strcmp(node->text, "low::memory_write16") == 0)
    { opcode = OP_LOW_MEM_WRITE16; expected = 2; }
    else if (strcmp(node->text, "low::memory_read16") == 0)
    { opcode = OP_LOW_MEM_READ16; expected = 1; }
    else if (strcmp(node->text, "low::memory_write32") == 0)
    { opcode = OP_LOW_MEM_WRITE32; expected = 2; }
    else if (strcmp(node->text, "low::memory_read32") == 0)
    { opcode = OP_LOW_MEM_READ32; expected = 1; }
    else if (strcmp(node->text, "low::cli") == 0)
    { opcode = OP_LOW_CLI; expected = 0; }
    else if (strcmp(node->text, "low::sti") == 0)
    { opcode = OP_LOW_STI; expected = 0; }
    else
    {
        cg_error(cg, "unknown low-level command");
        return;
    }

    for (arg = node->child; arg != NULL; arg = arg->next)
        arg_count++;

    if (arg_count != expected)
    {
        cg_error(cg, "wrong number of low-level arguments");
        return;
    }

    for (arg = node->child; arg != NULL; arg = arg->next)
        cg_expression(cg, arg);

    cg_emit_byte(cg, opcode);
}

static void cg_assignment(Codegen* cg, ASTNode* node)
{
    unsigned int modifiers = cg_sym_modifiers(node->text, cg->current_owner);
    unsigned char opcode;
    int volatile_access = (modifiers & NWC_MOD_NEEDED) != 0;

    if (modifiers & NWC_MOD_CONST)
    {
        cg_error(cg, "assignment to const variable");
        return;
    }

    if (node->integer_value == TOKEN_PLUS_PLUS ||
        node->integer_value == TOKEN_MINUS_MINUS)
    {
        cg_emit_byte(cg, OP_LOAD);
        cg_emit_slot(cg, node->text, 0);
        cg_emit_int(cg, 1);
        cg_emit_byte(cg, node->integer_value == TOKEN_PLUS_PLUS ?
                         OP_ADD : OP_SUB);
        if (volatile_access)
            cg_emit_byte(cg, OP_VOLATILE);
        cg_emit_byte(cg, OP_STORE);
        cg_emit_slot(cg, node->text, 1);
        return;
    }

    if (node->integer_value == TOKEN_ASSIGN)
    {
        if (node->left != NULL)
        {
            cg_expression(cg, node->left);
            cg_expression(cg, node->child);
            cg_emit_byte(cg, OP_STORE_ARR);
            cg_emit_slot(cg, node->text, 1);
        }
        else
        {
            cg_expression(cg, node->child);
            if (volatile_access)
                cg_emit_byte(cg, OP_VOLATILE);
            cg_emit_byte(cg, OP_STORE);
            cg_emit_slot(cg, node->text, 1);
        }
        return;
    }

    opcode = cg_assign_opcode(node->integer_value);

    if (opcode == 0)
    {
        cg_error(cg, "unsupported assignment operator");
        return;
    }

    if (node->left != NULL)
    {
        cg_expression(cg, node->left);
        cg_emit_byte(cg, OP_DUP);
        cg_emit_byte(cg, OP_LOAD_ARR);
        cg_emit_slot(cg, node->text, 0);
        cg_expression(cg, node->child);
        cg_emit_byte(cg, opcode);
        cg_emit_byte(cg, OP_STORE_ARR);
        cg_emit_slot(cg, node->text, 1);
    }
    else
    {
        cg_emit_byte(cg, OP_LOAD);
        cg_emit_slot(cg, node->text, 0);
        cg_expression(cg, node->child);
        cg_emit_byte(cg, opcode);
        if (volatile_access)
            cg_emit_byte(cg, OP_VOLATILE);
        cg_emit_byte(cg, OP_STORE);
        cg_emit_slot(cg, node->text, 1);
    }
}

static void cg_statement(Codegen* cg, ASTNode* node)
{
    if (node == NULL)
        return;

    switch (node->type)
    {
        case AST_VARIABLE_DECLARATION:
        case AST_ARRAY_DECLARATION:
            cg_variable_decl(cg, node);
            break;

        case AST_OUTPUT:
            cg_output(cg, node);
            break;

        case AST_INPUT:
            cg_input(cg, node);
            break;

        case AST_LINE_INPUT:
            cg_line_input(cg, node);
            break;

        case AST_RETURN:
            cg_return(cg, node);
            break;

        case AST_ASSIGNMENT:
            cg_assignment(cg, node);
            break;

        case AST_ASM:
            if (node->text != NULL &&
                strcmp(node->text, "hlt") == 0)
            {
                cg_emit_byte(cg, OP_ASM_HLT);
            }
            else
            {
                cg_error(cg, "unsupported asm instruction");
            }
            break;

        case AST_IF:
            cg_if(cg, node);
            break;

        case AST_WHILE:
            cg_while(cg, node);
            break;

        case AST_BREAK:
            if (cg->loop_depth == 0)
            {
                cg_error(cg, "break outside loop");
                break;
            }

            if (cg->break_count >= NWO_MAX_BREAKS)
            {
                cg_error(cg, "too many break statements");
                break;
            }

            cg->break_patch[cg->break_count++] =
                cg_emit_jump(cg, OP_JUMP);
            break;

        case AST_CONTINUE:
            if (cg->loop_depth == 0)
            {
                cg_error(cg, "continue outside loop");
                break;
            }

            if (cg->continue_count >= NWO_MAX_BREAKS)
            {
                cg_error(cg, "too many continue statements");
                break;
            }

            cg->continue_patch[cg->continue_count++] =
                cg_emit_jump(cg, OP_JUMP);
            break;

        case AST_LOW_CALL:
            cg_low_call(cg, node);
            break;

        case AST_FOR:
            cg_for(cg, node);
            break;

        case AST_DO_WHILE:
            cg_do_while(cg, node);
            break;

        case AST_CALL:
            if (cg_call(cg, node))
                cg_emit_byte(cg, OP_POP);
            break;

        case AST_BLOCK:
            cg_node(cg, node->child);
            break;

        default:
            cg_expression(cg, node);
            cg_emit_byte(cg, OP_POP);
            break;
    }
}


static void cg_function(Codegen* cg, ASTNode* node)
{
    ASTNode* param;
    int param_count = 0;
    int func_index;
    int saved_owner;

    if (node->text == NULL)
    {
        cg_error(cg, "function without name");
        return;
    }

    func_index = cg_func_index(node->text);

    if (func_index == -1)
    {
        cg_error(cg, "function missing from pre-scan table");
        return;
    }

    param = node->left;

    while (param != NULL)
    {
        param_count++;
        param = param->next;
    }

    if (param_count > NWO_MAX_PARAMS_V3)
    {
        cg_error(cg, "too many function parameters (max 16)");
        return;
    }

    saved_owner = cg->current_owner;
    cg->current_owner = func_index;

    cg_emit_byte(cg, OP_FUNC_BEGIN);
    cg_emit_u32(cg, (unsigned int)func_index);
    cg_emit_string(cg, node->text);   /* name kept for diagnostics/main lookup */
    cg_emit_byte(cg, cg_type_byte(node->value_type));
    cg_emit_byte(cg, (unsigned char)param_count);

    param = node->left;

    while (param != NULL)
    {
        if (!cg_emit_decl_slot(cg, param->text, param->modifiers,
                               param->value_type))
        {
            cg->current_owner = saved_owner;
            return;
        }
        cg_emit_byte(cg, cg_decl_type_byte(param->value_type,
                                           param->modifiers));
        cg_emit_byte(
            cg,
            (param->type == AST_ARRAY_DECLARATION ||
             param->array_size > 0) ? 1 : 0
        );
        param = param->next;
    }

    /* body */
    if (node->child != NULL &&
        node->child->type == AST_BLOCK)
    {
        cg_node(cg, node->child->child);
    }
    else
    {
        cg_node(cg, node->child);
    }

    cg_emit_byte(cg, OP_FUNC_END);
    cg->current_owner = saved_owner;
}

static void cg_node(Codegen* cg, ASTNode* node)
{
    while (node != NULL && !cg->error)
    {
        switch (node->type)
        {
            case AST_INCLUDE:
                break;

            case AST_FUNCTION:
                cg_function(cg, node);
                break;

            case AST_BLOCK:
                cg_node(cg, node->child);
                break;

            default:
                cg_statement(cg, node);
                break;
        }

        node = node->next;
    }
}

/* Pre-scan A: register every function in emission order so call sites
   can reference functions defined later in the file. */
static int cg_prepass_functions(ASTNode* ast)
{
    ASTNode* node = ast->child;

    cg_func_count = 0;
    cg_main_func = -1;

    while (node != NULL)
    {
        if (node->type == AST_FUNCTION &&
            node->text != NULL)
        {
            size_t len = strlen(node->text);

            if (len >= NWO_NAME_SIZE_V3)
            {
                fprintf(stderr,
                        "NwC codegen: function name too long '%s'\n",
                        node->text);
                return 0;
            }

            if (cg_func_index(node->text) != -1)
            {
                fprintf(stderr,
                        "NwC codegen: duplicate function '%s'\n",
                        node->text);
                return 0;
            }

            if (cg_func_count >= NWO_MAX_FUNCTIONS)
            {
                fprintf(stderr,
                        "NwC codegen: too many functions (max %d)\n",
                        NWO_MAX_FUNCTIONS);
                return 0;
            }

            memcpy(cg_funcs[cg_func_count].name, node->text, len + 1);

            if (strcmp(node->text, "main") == 0)
                cg_main_func = cg_func_count;

            cg_func_count++;
        }

        node = node->next;
    }

    if (cg_main_func == -1)
    {
        fprintf(stderr, "NwC codegen: no main() function\n");
        return 0;
    }

    return 1;
}

/* Pre-scan B: assign slots to every declaration inside main() before any
   function body is emitted, so functions defined earlier in the file can
   resolve main's variables ("globals"). */
static void cg_prepass_main_vars(ASTNode* stmt)
{
    while (stmt != NULL)
    {
        switch (stmt->type)
        {
            case AST_VARIABLE_DECLARATION:
            case AST_ARRAY_DECLARATION:
                if (stmt->text != NULL &&
                    cg_sym_find(stmt->text, cg_main_func) == -1)
                {
                    cg_sym_add(stmt->text, cg_main_func, "variable");
                }
                break;

            case AST_BLOCK:
                cg_prepass_main_vars(stmt->child);
                break;

            case AST_IF:
                cg_prepass_main_vars(stmt->left);
                cg_prepass_main_vars(stmt->right);
                break;

            case AST_WHILE:
                cg_prepass_main_vars(stmt->left);
                break;

            default:
                break;
        }

        stmt = stmt->next;
    }
}

static ASTNode* cg_find_function_node(ASTNode* ast, const char* name)
{
    ASTNode* node = ast->child;

    while (node != NULL)
    {
        if (node->type == AST_FUNCTION &&
            node->text != NULL &&
            strcmp(node->text, name) == 0)
        {
            return node;
        }

        node = node->next;
    }

    return NULL;
}

static int cg_write_header(
    FILE* f,
    unsigned int code_size,
    unsigned int data_size,
    int flags)
{
    unsigned char version = NWO_VERSION;
    unsigned char reserved[3] = { 0, 0, 0 };

    if (flags & NWO_FLAG_GUI)
        reserved[0] = NWO_FLAG_GUI;

    if (fwrite(NWO_MAGIC, 1, 4, f) != 4) return 0;
    if (fwrite(&version, 1, 1, f) != 1)  return 0;
    if (fwrite(reserved, 1, 3, f) != 3)  return 0;

    unsigned char b[4];

    b[0] = (unsigned char)(code_size & 0xFF);
    b[1] = (unsigned char)((code_size >> 8) & 0xFF);
    b[2] = (unsigned char)((code_size >> 16) & 0xFF);
    b[3] = (unsigned char)((code_size >> 24) & 0xFF);
    if (fwrite(b, 1, 4, f) != 4) return 0;

    b[0] = (unsigned char)(data_size & 0xFF);
    b[1] = (unsigned char)((data_size >> 8) & 0xFF);
    b[2] = (unsigned char)((data_size >> 16) & 0xFF);
    b[3] = (unsigned char)((data_size >> 24) & 0xFF);
    if (fwrite(b, 1, 4, f) != 4) return 0;

    b[0] = b[1] = b[2] = b[3] = 0;
    if (fwrite(b, 1, 4, f) != 4) return 0;

    return 1;
}

int codegen_write_nwo(ASTNode* ast, const char* path, int flags)
{
    Codegen cg;
    FILE* f;
    FILE* code_tmp;
    char shell_exports[1024];
    int shell_export_count = 0;

    shell_exports[0] = '\0';
    cg_gui_used = 0;

    if (ast == NULL || path == NULL)
    {
        fprintf(stderr, "NwC codegen: invalid arguments\n");
        return 0;
    }

    if (ast->type != AST_PROGRAM)
    {
        fprintf(stderr, "NwC codegen: expected program node\n");
        return 0;
    }

    code_tmp = tmpfile();

    if (code_tmp == NULL)
    {
        fprintf(stderr, "NwC codegen: cannot create temp file\n");
        return 0;
    }

    cg.file = code_tmp;
    cg.code_size = 0;
    cg.data_size = 0;
    cg.error = 0;
    cg.loop_depth = 0;
    cg.break_count = 0;
    cg.current_owner = -1;

    cg_sym_count = 0;
    cg_fixup_count = 0;

    if (!cg_prepass_functions(ast))
    {
        fclose(code_tmp);
        return 0;
    }

    {
        ASTNode* main_node = cg_find_function_node(ast, "main");

        if (main_node != NULL)
        {
            if (main_node->child != NULL &&
                main_node->child->type == AST_BLOCK)
            {
                cg_prepass_main_vars(main_node->child->child);
            }
            else
            {
                cg_prepass_main_vars(main_node->child);
            }
        }
    }

    cg_node(&cg, ast->child);

    if (cg.error)
    {
        fclose(code_tmp);
        return 0;
    }

    cg_process_fixups(&cg);

    if (cg.error)
    {
        fclose(code_tmp);
        return 0;
    }

    cg_emit_byte(&cg, OP_HALT);

    if (cg.error)
    {
        fclose(code_tmp);
        return 0;
    }
    f = fopen(path, "wb");

    if (f == NULL)
    {
        fprintf(stderr,
                "NwC codegen: cannot open '%s' for writing\n",
                path);

        fclose(code_tmp);
        return 0;
    }
    if (!cg_write_header(f, cg.code_size, cg.data_size,
                         flags | (cg_gui_used ? NWO_FLAG_GUI : 0)))
    {
        fprintf(stderr,
                "NwC codegen: failed to write header\n");

        fclose(f);
        fclose(code_tmp);
        return 0;
    }
    if (fseek(code_tmp, 0, SEEK_SET) != 0)
    {
        fclose(f);
        fclose(code_tmp);
        return 0;
    }

    {
        unsigned char buf[4096];
        size_t n;

        while ((n = fread(buf, 1, sizeof(buf), code_tmp)) > 0)
        {
            if (fwrite(buf, 1, n, f) != n)
            {
                fprintf(stderr,
                        "NwC codegen: write failed\n");

                fclose(f);
                fclose(code_tmp);
                return 0;
            }
        }
    }

    fclose(f);
    fclose(code_tmp);

    return 1;
}