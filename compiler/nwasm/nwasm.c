#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nwasm_format.h"

#define CODE 0
#define DATA 1
#define BSS  2

#define MAX_OPERANDS 16
#define MAX_NAME     128

typedef struct
{
    uint8_t *data;
    size_t   cap;
    uint64_t size;
} Section;

typedef struct
{
    char    *name;
    int      section;
    uint64_t value;
    int      binding;
    int      defined;
} Symbol;

typedef struct
{
    int      section;
    int      type;
    uint32_t offset;
    int      sym;
    int64_t  addend;
} Reloc;

typedef struct
{
    const char *path;
    int         line;
    int         target;
    int         current;
    Section     sec[3];
    Symbol     *syms;
    size_t      sym_count;
    size_t      sym_cap;
    Reloc      *relocs;
    size_t      reloc_count;
    size_t      reloc_cap;
    int         entry_sym;
} Assembler;

typedef struct
{
    int      ok;
    int      size;
    int      num;
} Reg;

typedef struct
{
    int     has_symbol;
    int     sym;
    int64_t addend;
} AddrExpr;

typedef struct
{
    int      ok;
    int      is_rip;
    int      base;
    int      has_disp;
    int64_t  disp;
    int      has_symbol;
    int      sym;
    int64_t  addend;
} Mem;

typedef struct
{
    Assembler  *as;
    const char *p;
    int         line;
    int         section;
    uint64_t    offset;
} ExprParser;

typedef struct
{
    Assembler  *as;
    const char *p;
    int         line;
} AddrParser;

static void fail(Assembler *as, int line, const char *fmt, ...)
{
    va_list ap;

    fprintf(stderr, "nwasm: %s:%d: ", as->path, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);

    if (!p)
    {
        fprintf(stderr, "nwasm: out of memory\n");
        exit(1);
    }

    return p;
}

static void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size ? size : 1);

    if (!p)
    {
        fprintf(stderr, "nwasm: out of memory\n");
        exit(1);
    }

    return p;
}

static char *xstrndup(const char *s, size_t n)
{
    char *p = xmalloc(n + 1);

    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    if (alignment <= 1)
        return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

static char *trim(char *s)
{
    char *end;

    while (*s && isspace((unsigned char)*s))
        ++s;

    end = s + strlen(s);

    while (end > s && isspace((unsigned char)end[-1]))
        --end;

    *end = '\0';
    return s;
}

static void lower(char *s)
{
    while (*s)
    {
        *s = (char)tolower((unsigned char)*s);
        ++s;
    }
}

static int starts_word(const char *s, const char *word)
{
    size_t n = strlen(word);

    if (strncmp(s, word, n) != 0)
        return 0;

    return s[n] == '\0' || isspace((unsigned char)s[n]) || s[n] == ',';
}

static void strip_comment(char *s)
{
    char quote = 0;
    int escaped = 0;

    for (char *p = s; *p; ++p)
    {
        if (quote)
        {
            if (escaped)
                escaped = 0;
            else if (*p == '\\')
                escaped = 1;
            else if (*p == quote)
                quote = 0;
        }
        else if (*p == '\'' || *p == '"')
        {
            quote = *p;
        }
        else if (*p == ';')
        {
            *p = '\0';
            return;
        }
    }
}

static char *find_colon(char *s)
{
    char quote = 0;
    int escaped = 0;

    for (char *p = s; *p; ++p)
    {
        if (quote)
        {
            if (escaped)
                escaped = 0;
            else if (*p == '\\')
                escaped = 1;
            else if (*p == quote)
                quote = 0;
        }
        else if (*p == '\'' || *p == '"')
        {
            quote = *p;
        }
        else if (*p == ':')
        {
            return p;
        }
    }

    return NULL;
}

static int split_operands(char *s, char **out, int max)
{
    int count = 0;
    int depth = 0;
    char quote = 0;
    int escaped = 0;
    char *start = trim(s);

    if (!*start)
        return 0;

    for (char *p = start;; ++p)
    {
        char c = *p;

        if (quote)
        {
            if (escaped)
                escaped = 0;
            else if (c == '\\')
                escaped = 1;
            else if (c == quote)
                quote = 0;
        }
        else if (c == '\'' || c == '"')
        {
            quote = c;
        }
        else if (c == '[' || c == '(')
        {
            ++depth;
        }
        else if (c == ']' || c == ')')
        {
            if (depth > 0)
                --depth;
        }
        else if ((c == ',' && depth == 0) || c == '\0')
        {
            char saved = *p;

            *p = '\0';
            if (count >= max)
                return -1;
            out[count++] = trim(start);
            if (saved == '\0')
                break;
            start = p + 1;
        }
    }

    return count;
}

static void sec_append(Assembler *as, int section, const void *data, size_t n)
{
    Section *s;

    if (section == BSS)
        fail(as, as->line, "initialized data is not allowed in section bss");

    s = &as->sec[section];

    if (s->size + n > s->cap)
    {
        size_t cap = s->cap ? s->cap : 256;

        while (cap < s->size + n)
            cap *= 2;

        s->data = xrealloc(s->data, cap);
        s->cap = cap;
    }

    if (n)
        memcpy(s->data + s->size, data, n);

    s->size += n;
}

static void sec_reserve(Assembler *as, int section, uint64_t n)
{
    Section *s = &as->sec[section];

    if (section == BSS)
    {
        s->size += n;
        return;
    }

    if (s->size + n > s->cap)
    {
        size_t cap = s->cap ? s->cap : 256;

        while (cap < s->size + n)
            cap *= 2;

        s->data = xrealloc(s->data, cap);
        s->cap = cap;
    }

    memset(s->data + s->size, 0, (size_t)n);
    s->size += n;
}

static void emit8(Assembler *as, uint8_t value)
{
    sec_append(as, as->current, &value, 1);
}

static void emit16(Assembler *as, uint16_t value)
{
    uint8_t bytes[2];

    bytes[0] = (uint8_t)(value & 0xFF);
    bytes[1] = (uint8_t)(value >> 8);
    sec_append(as, as->current, bytes, 2);
}

static void emit32(Assembler *as, uint32_t value)
{
    uint8_t bytes[4];

    bytes[0] = (uint8_t)(value & 0xFF);
    bytes[1] = (uint8_t)((value >> 8) & 0xFF);
    bytes[2] = (uint8_t)((value >> 16) & 0xFF);
    bytes[3] = (uint8_t)((value >> 24) & 0xFF);
    sec_append(as, as->current, bytes, 4);
}

static void emit64(Assembler *as, uint64_t value)
{
    uint8_t bytes[8];

    for (int i = 0; i < 8; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8));

    sec_append(as, as->current, bytes, 8);
}

static void add_reloc(Assembler *as, int type, uint64_t offset, int sym,
                      int64_t addend)
{
    if (as->current == BSS)
        fail(as, as->line, "relocation in section bss");

    if (as->reloc_count == as->reloc_cap)
    {
        as->reloc_cap = as->reloc_cap ? as->reloc_cap * 2 : 16;
        as->relocs = xrealloc(as->relocs,
                              as->reloc_cap * sizeof(Reloc));
    }

    as->relocs[as->reloc_count].section = as->current;
    as->relocs[as->reloc_count].type = type;
    as->relocs[as->reloc_count].offset = (uint32_t)offset;
    as->relocs[as->reloc_count].sym = sym;
    as->relocs[as->reloc_count].addend = addend;
    ++as->reloc_count;
}

static int sym_find(Assembler *as, const char *name)
{
    for (size_t i = 0; i < as->sym_count; ++i)
    {
        if (strcmp(as->syms[i].name, name) == 0)
            return (int)i;
    }

    return -1;
}

static int sym_get(Assembler *as, const char *name)
{
    int index = sym_find(as, name);

    if (index >= 0)
        return index;

    if (as->sym_count == as->sym_cap)
    {
        as->sym_cap = as->sym_cap ? as->sym_cap * 2 : 32;
        as->syms = xrealloc(as->syms,
                            as->sym_cap * sizeof(Symbol));
    }

    index = (int)as->sym_count++;
    as->syms[index].name = xstrndup(name, strlen(name));
    as->syms[index].section = NWOBJ_SEC_UNDEF;
    as->syms[index].value = 0;
    as->syms[index].binding = NWOBJ_BIND_LOCAL;
    as->syms[index].defined = 0;
    return index;
}

static int sym_define(Assembler *as, const char *name, int section,
                      uint64_t value, int line)
{
    int index = sym_get(as, name);

    if (as->syms[index].defined)
        fail(as, line, "duplicate symbol '%s'", name);

    as->syms[index].section = section;
    as->syms[index].value = value;
    as->syms[index].defined = 1;
    return index;
}

static int sym_global(Assembler *as, const char *name)
{
    int index = sym_get(as, name);

    as->syms[index].binding = NWOBJ_BIND_GLOBAL;
    return index;
}

static Reg parse_reg(const char *name)
{
    static const char *r64[16] =
    {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
    };
    static const char *r32[16] =
    {
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
    };
    Reg r = {0, 0, 0};
    char temp[32];
    size_t n = strlen(name);

    if (n >= sizeof(temp))
        return r;

    memcpy(temp, name, n + 1);
    lower(temp);

    for (int i = 0; i < 16; ++i)
    {
        if (strcmp(temp, r64[i]) == 0)
        {
            r.ok = 1;
            r.size = 64;
            r.num = i;
            return r;
        }

        if (strcmp(temp, r32[i]) == 0)
        {
            r.ok = 1;
            r.size = 32;
            r.num = i;
            return r;
        }
    }

    return r;
}

static void expr_skip(ExprParser *e)
{
    while (*e->p && isspace((unsigned char)*e->p))
        ++e->p;
}

static uint64_t parse_number(ExprParser *e)
{
    const char *p = e->p;
    uint64_t value = 0;
    int base = 10;

    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        base = 16;
        p += 2;
    }
    else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B'))
    {
        base = 2;
        p += 2;
    }

    if (base == 16)
    {
        while (isxdigit((unsigned char)*p))
        {
            int d = isdigit((unsigned char)*p)
                    ? *p - '0'
                    : tolower((unsigned char)*p) - 'a' + 10;
            value = value * 16 + (uint64_t)d;
            ++p;
        }
    }
    else if (base == 2)
    {
        while (*p == '0' || *p == '1')
        {
            value = value * 2 + (uint64_t)(*p - '0');
            ++p;
        }
    }
    else
    {
        while (isdigit((unsigned char)*p))
        {
            value = value * 10 + (uint64_t)(*p - '0');
            ++p;
        }
    }

    if (p == e->p || (base != 10 && p == e->p + 2))
        fail(e->as, e->line, "expected a number");

    e->p = p;
    return value;
}

static int64_t expr_add(ExprParser *e);

static int64_t expr_primary(ExprParser *e)
{
    expr_skip(e);

    if (*e->p == '(')
    {
        int64_t value;

        ++e->p;
        value = expr_add(e);
        expr_skip(e);

        if (*e->p != ')')
            fail(e->as, e->line, "expected ')'");

        ++e->p;
        return value;
    }

    if (*e->p == '$')
    {
        ++e->p;
        return (int64_t)e->offset;
    }

    if (*e->p == '\'')
    {
        char value;

        ++e->p;

        if (*e->p == '\\')
        {
            ++e->p;

            switch (*e->p)
            {
                case 'n': value = '\n'; break;
                case 'r': value = '\r'; break;
                case 't': value = '\t'; break;
                case '0': value = '\0'; break;
                case '\\': value = '\\'; break;
                case '\'': value = '\''; break;
                case '"': value = '"'; break;
                default: value = *e->p; break;
            }
        }
        else
        {
            value = *e->p;
        }

        ++e->p;

        if (*e->p != '\'')
            fail(e->as, e->line, "invalid character literal");

        ++e->p;
        return (int64_t)(unsigned char)value;
    }

    if (isdigit((unsigned char)*e->p))
        return (int64_t)parse_number(e);

    if (isalpha((unsigned char)*e->p) || *e->p == '_' || *e->p == '.')
    {
        const char *start = e->p;
        char name[MAX_NAME];
        size_t n;
        int index;

        while (isalnum((unsigned char)*e->p) || *e->p == '_' ||
               *e->p == '.' || *e->p == '$')
            ++e->p;

        n = (size_t)(e->p - start);

        if (n >= sizeof(name))
            fail(e->as, e->line, "symbol name is too long");

        memcpy(name, start, n);
        name[n] = '\0';

        index = sym_find(e->as, name);

        if (index < 0 || !e->as->syms[index].defined)
            fail(e->as, e->line, "undefined symbol '%s'", name);

        return (int64_t)e->as->syms[index].value;
    }

    fail(e->as, e->line, "invalid expression");
    return 0;
}

static int64_t expr_unary(ExprParser *e)
{
    expr_skip(e);

    if (*e->p == '+')
    {
        ++e->p;
        return expr_unary(e);
    }

    if (*e->p == '-')
    {
        ++e->p;
        return -expr_unary(e);
    }

    return expr_primary(e);
}

static int64_t expr_mul(ExprParser *e)
{
    int64_t value = expr_unary(e);

    for (;;)
    {
        expr_skip(e);

        if (*e->p == '*')
        {
            ++e->p;
            value *= expr_unary(e);
        }
        else if (*e->p == '/')
        {
            int64_t rhs;

            ++e->p;
            rhs = expr_unary(e);

            if (rhs == 0)
                fail(e->as, e->line, "division by zero");

            value /= rhs;
        }
        else if (*e->p == '%')
        {
            int64_t rhs;

            ++e->p;
            rhs = expr_unary(e);

            if (rhs == 0)
                fail(e->as, e->line, "division by zero");

            value %= rhs;
        }
        else
        {
            return value;
        }
    }
}

static int64_t expr_add(ExprParser *e)
{
    int64_t value = expr_mul(e);

    for (;;)
    {
        expr_skip(e);

        if (*e->p == '+')
        {
            ++e->p;
            value += expr_mul(e);
        }
        else if (*e->p == '-')
        {
            ++e->p;
            value -= expr_mul(e);
        }
        else
        {
            return value;
        }
    }
}

static int64_t eval_expr(Assembler *as, int line, const char *text,
                         int section, uint64_t offset)
{
    ExprParser e;

    e.as = as;
    e.p = text;
    e.line = line;
    e.section = section;
    e.offset = offset;

    int64_t value = expr_add(&e);

    expr_skip(&e);

    if (*e.p != '\0')
        fail(as, line, "unexpected text in expression: '%s'", e.p);

    return value;
}

static void addr_skip(AddrParser *p)
{
    while (*p->p && isspace((unsigned char)*p->p))
        ++p->p;
}

static void addr_term(AddrParser *p, AddrExpr *out, int sign)
{
    const char *start;
    char name[MAX_NAME];
    size_t n;
    int index;
    uint64_t value;

    addr_skip(p);

    if (*p->p == '+' || *p->p == '-')
    {
        if (*p->p == '-')
            sign = -sign;
        ++p->p;
        addr_skip(p);
    }

    if (*p->p == '$')
    {
        ++p->p;
        out->addend += sign * (int64_t)p->as->sec[p->as->current].size;
        return;
    }

    if (isdigit((unsigned char)*p->p))
    {
        ExprParser e;

        e.as = p->as;
        e.p = p->p;
        e.line = p->line;
        e.section = p->as->current;
        e.offset = p->as->sec[p->as->current].size;

        value = parse_number(&e);
        p->p = e.p;
        out->addend += sign * (int64_t)value;
        return;
    }

    if (!isalpha((unsigned char)*p->p) && *p->p != '_' && *p->p != '.')
        fail(p->as, p->line, "invalid address expression");

    start = p->p;

    while (isalnum((unsigned char)*p->p) || *p->p == '_' ||
           *p->p == '.' || *p->p == '$')
        ++p->p;

    n = (size_t)(p->p - start);

    if (n >= sizeof(name))
        fail(p->as, p->line, "symbol name is too long");

    memcpy(name, start, n);
    name[n] = '\0';

    index = sym_find(p->as, name);

    if (index >= 0 && p->as->syms[index].defined &&
        p->as->syms[index].section == NWOBJ_SEC_ABS)
    {
        out->addend += sign * (int64_t)p->as->syms[index].value;
        return;
    }

    if (out->has_symbol)
        fail(p->as, p->line,
             "address expression must contain only one symbol");

    if (sign < 0)
        fail(p->as, p->line, "negative symbol address is not supported");

    out->has_symbol = 1;
    out->sym = index >= 0 ? index : sym_get(p->as, name);
}

static AddrExpr parse_addr_expr(Assembler *as, int line, const char *text)
{
    AddrParser p;
    AddrExpr out;

    memset(&out, 0, sizeof(out));

    p.as = as;
    p.p = text;
    p.line = line;

    addr_skip(&p);

    if (*p.p == '\0')
        fail(as, line, "expected an address expression");

    addr_term(&p, &out, 1);

    for (;;)
    {
        addr_skip(&p);

        if (*p.p == '+' || *p.p == '-')
        {
            int sign = *p.p == '-' ? -1 : 1;

            ++p.p;
            addr_term(&p, &out, sign);
        }
        else
        {
            break;
        }
    }

    addr_skip(&p);

    if (*p.p != '\0')
        fail(as, line, "unexpected text in address expression: '%s'", p.p);

    return out;
}

static void emit_rex(Assembler *as, int w, int r, int x, int b)
{
    uint8_t rex = 0x40;

    if (w)
        rex |= 0x08;
    if (r)
        rex |= 0x04;
    if (x)
        rex |= 0x02;
    if (b)
        rex |= 0x01;

    if (rex != 0x40)
        emit8(as, rex);
}

static uint8_t modrm(int mod, int reg, int rm)
{
    return (uint8_t)((mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

static Mem parse_mem(Assembler *as, int line, const char *text)
{
    Mem m;
    char buffer[512];
    char *inside;
    char *p;
    size_t n = strlen(text);

    memset(&m, 0, sizeof(m));

    if (n < 2 || text[0] != '[' || text[n - 1] != ']')
        return m;

    if (n - 2 >= sizeof(buffer))
        fail(as, line, "memory operand is too long");

    memcpy(buffer, text + 1, n - 2);
    buffer[n - 2] = '\0';

    inside = trim(buffer);
    p = inside;

    if (starts_word(p, "rel"))
    {
        p += 3;
        p = trim(p);
        m.ok = 1;
        m.is_rip = 1;

        if (*p)
        {
            AddrExpr a = parse_addr_expr(as, line, p);
            m.has_symbol = a.has_symbol;
            m.sym = a.sym;
            m.addend = a.addend;
        }

        return m;
    }

    while (*p && !isspace((unsigned char)*p) && *p != '+' && *p != '-')
        ++p;

    char saved = *p;
    *p = '\0';

    Reg base = parse_reg(trim(inside));
    *p = saved;

    if (base.ok)
    {
        if (base.size != 64)
            fail(as, line, "memory base register must be 64-bit");

        m.ok = 1;
        m.base = base.num;

        p = trim(p);

        if (*p)
        {
            int64_t disp = eval_expr(as, line, p, as->current,
                                     as->sec[as->current].size);
            m.has_disp = 1;
            m.disp = disp;
        }

        return m;
    }

    m.ok = 1;
    m.is_rip = 1;

    if (*inside)
    {
        AddrExpr a = parse_addr_expr(as, line, inside);
        m.has_symbol = a.has_symbol;
        m.sym = a.sym;
        m.addend = a.addend;
    }

    return m;
}

static void emit_rip_disp(Assembler *as, Mem *m)
{
    if (m->has_symbol)
    {
        uint64_t off = as->sec[as->current].size;
        emit32(as, 0);
        add_reloc(as, NWOBJ_RELOC_PC32, off, m->sym, m->addend);
    }
    else
    {
        emit32(as, (uint32_t)m->addend);
    }
}

static void emit_modrm_mem(Assembler *as, Mem *m, int reg_field)
{
    if (m->is_rip)
    {
        emit8(as, modrm(0, reg_field, 5));
        emit_rip_disp(as, m);
        return;
    }

    int base = m->base;
    int mod;
    int64_t disp = m->disp;

    if (!m->has_disp)
        disp = 0;

    if (disp == 0 && (base & 7) != 5)
        mod = 0;
    else if (disp >= -128 && disp <= 127)
        mod = 1;
    else
        mod = 2;

    emit8(as, modrm(mod, reg_field, base));

    if ((base & 7) == 4)
        emit8(as, 0x24);

    if (mod == 1)
        emit8(as, (uint8_t)disp);
    else if (mod == 2)
        emit32(as, (uint32_t)disp);
}

static void emit_mov_reg_reg(Assembler *as, Reg dst, Reg src, int line)
{
    if (dst.size != src.size)
        fail(as, line, "mov operands have different sizes");

    emit_rex(as, dst.size == 64, src.num >= 8, 0, dst.num >= 8);
    emit8(as, 0x89);
    emit8(as, modrm(3, src.num, dst.num));
}

static void emit_mov_reg_imm(Assembler *as, Reg dst, const char *text,
                             int line)
{
    AddrExpr a = parse_addr_expr(as, line, text);

    if (dst.size == 64)
    {
        emit_rex(as, 1, 0, 0, dst.num >= 8);
        emit8(as, (uint8_t)(0xB8 + (dst.num & 7)));

        if (a.has_symbol)
        {
            uint64_t off = as->sec[as->current].size;
            emit64(as, 0);
            add_reloc(as, NWOBJ_RELOC_ABS64, off, a.sym, a.addend);
        }
        else
        {
            emit64(as, (uint64_t)a.addend);
        }
    }
    else if (dst.size == 32)
    {
        emit_rex(as, 0, 0, 0, dst.num >= 8);
        emit8(as, (uint8_t)(0xB8 + (dst.num & 7)));

        if (a.has_symbol)
        {
            uint64_t off = as->sec[as->current].size;
            emit32(as, 0);
            add_reloc(as, NWOBJ_RELOC_ABS32, off, a.sym, a.addend);
        }
        else
        {
            emit32(as, (uint32_t)a.addend);
        }
    }
    else
    {
        fail(as, line, "immediate mov supports only 32-bit and 64-bit registers");
    }
}

static void instr_mov(Assembler *as, char **ops, int n, int line)
{
    Reg dst;
    Reg src;
    Mem m;

    if (n != 2)
        fail(as, line, "mov expects two operands");

    dst = parse_reg(ops[0]);
    src = parse_reg(ops[1]);

    if (dst.ok && src.ok)
    {
        emit_mov_reg_reg(as, dst, src, line);
        return;
    }

    if (dst.ok && ops[1][0] == '[')
    {
        m = parse_mem(as, line, ops[1]);

        if (!m.ok)
            fail(as, line, "invalid memory operand");

        if (dst.size != 64)
            fail(as, line, "memory mov supports only 64-bit registers");

        emit_rex(as, 1, dst.num >= 8, 0, m.is_rip ? 0 : (m.base >= 8));
        emit8(as, 0x8B);
        emit_modrm_mem(as, &m, dst.num);
        return;
    }

    if (ops[0][0] == '[' && src.ok)
    {
        m = parse_mem(as, line, ops[0]);

        if (!m.ok)
            fail(as, line, "invalid memory operand");

        if (src.size != 64)
            fail(as, line, "memory mov supports only 64-bit registers");

        emit_rex(as, 1, src.num >= 8, 0, m.is_rip ? 0 : (m.base >= 8));
        emit8(as, 0x89);
        emit_modrm_mem(as, &m, src.num);
        return;
    }

    if (!dst.ok)
        fail(as, line, "invalid destination register");

    if (ops[1][0] == '[')
        fail(as, line, "invalid operands for mov");

    emit_mov_reg_imm(as, dst, ops[1], line);
}

static void instr_lea(Assembler *as, char **ops, int n, int line)
{
    Reg dst;
    Mem m;

    if (n != 2)
        fail(as, line, "lea expects two operands");

    dst = parse_reg(ops[0]);

    if (!dst.ok || dst.size != 64)
        fail(as, line, "lea destination must be a 64-bit register");

    m = parse_mem(as, line, ops[1]);

    if (!m.ok)
        fail(as, line, "lea source must be a memory operand");

    emit_rex(as, 1, dst.num >= 8, 0, m.is_rip ? 0 : (m.base >= 8));
    emit8(as, 0x8D);
    emit_modrm_mem(as, &m, dst.num);
}

static void instr_call(Assembler *as, char **ops, int n, int line)
{
    Reg r;

    if (n != 1)
        fail(as, line, "call expects one operand");

    r = parse_reg(ops[0]);

    if (r.ok)
    {
        if (r.size != 64)
            fail(as, line, "call register must be 64-bit");

        emit_rex(as, 0, 0, 0, r.num >= 8);
        emit8(as, 0xFF);
        emit8(as, modrm(3, 2, r.num));
        return;
    }

    AddrExpr a = parse_addr_expr(as, line, ops[0]);
    uint64_t off;

    emit8(as, 0xE8);
    off = as->sec[as->current].size;

    if (a.has_symbol)
    {
        emit32(as, 0);
        add_reloc(as, NWOBJ_RELOC_PC32, off, a.sym, a.addend);
    }
    else
    {
        emit32(as, (uint32_t)(a.addend - (int64_t)(off + 4)));
    }
}

static void instr_jmp(Assembler *as, char **ops, int n, int line)
{
    Reg r;

    if (n != 1)
        fail(as, line, "jmp expects one operand");

    r = parse_reg(ops[0]);

    if (r.ok)
    {
        if (r.size != 64)
            fail(as, line, "jmp register must be 64-bit");

        emit_rex(as, 0, 0, 0, r.num >= 8);
        emit8(as, 0xFF);
        emit8(as, modrm(3, 4, r.num));
        return;
    }

    AddrExpr a = parse_addr_expr(as, line, ops[0]);
    uint64_t off;

    emit8(as, 0xE9);
    off = as->sec[as->current].size;

    if (a.has_symbol)
    {
        emit32(as, 0);
        add_reloc(as, NWOBJ_RELOC_PC32, off, a.sym, a.addend);
    }
    else
    {
        emit32(as, (uint32_t)(a.addend - (int64_t)(off + 4)));
    }
}

static int condition_code(const char *name)
{
    if (!strcmp(name, "jo")) return 0x80;
    if (!strcmp(name, "jno")) return 0x81;
    if (!strcmp(name, "jb") || !strcmp(name, "jc") || !strcmp(name, "jnae")) return 0x82;
    if (!strcmp(name, "jae") || !strcmp(name, "jnb") || !strcmp(name, "jnc")) return 0x83;
    if (!strcmp(name, "je") || !strcmp(name, "jz")) return 0x84;
    if (!strcmp(name, "jne") || !strcmp(name, "jnz")) return 0x85;
    if (!strcmp(name, "jbe") || !strcmp(name, "jna")) return 0x86;
    if (!strcmp(name, "ja") || !strcmp(name, "jnbe")) return 0x87;
    if (!strcmp(name, "js")) return 0x88;
    if (!strcmp(name, "jns")) return 0x89;
    if (!strcmp(name, "jp") || !strcmp(name, "jpe")) return 0x8A;
    if (!strcmp(name, "jnp") || !strcmp(name, "jpo")) return 0x8B;
    if (!strcmp(name, "jl") || !strcmp(name, "jnge")) return 0x8C;
    if (!strcmp(name, "jge") || !strcmp(name, "jnl")) return 0x8D;
    if (!strcmp(name, "jle") || !strcmp(name, "jng")) return 0x8E;
    if (!strcmp(name, "jg") || !strcmp(name, "jnle")) return 0x8F;
    return -1;
}

static void instr_jcc(Assembler *as, const char *mnemonic, char **ops, int n,
                      int line)
{
    int cc = condition_code(mnemonic);
    AddrExpr a;
    uint64_t off;

    if (cc < 0)
        fail(as, line, "unknown condition");

    if (n != 1)
        fail(as, line, "%s expects one operand", mnemonic);

    a = parse_addr_expr(as, line, ops[0]);

    emit8(as, 0x0F);
    emit8(as, (uint8_t)cc);
    off = as->sec[as->current].size;

    if (a.has_symbol)
    {
        emit32(as, 0);
        add_reloc(as, NWOBJ_RELOC_PC32, off, a.sym, a.addend);
    }
    else
    {
        emit32(as, (uint32_t)(a.addend - (int64_t)(off + 4)));
    }
}

static void instr_test(Assembler *as, char **ops, int n, int line)
{
    Reg dst;
    Reg src;
    AddrExpr a;

    if (n != 2)
        fail(as, line, "test expects two operands");

    dst = parse_reg(ops[0]);
    src = parse_reg(ops[1]);

    if (!dst.ok)
        fail(as, line, "test destination must be a register");

    if (src.ok)
    {
        if (dst.size != src.size)
            fail(as, line, "test operands have different sizes");

        emit_rex(as, dst.size == 64, src.num >= 8, 0, dst.num >= 8);
        emit8(as, 0x85);
        emit8(as, modrm(3, src.num, dst.num));
        return;
    }

    a = parse_addr_expr(as, line, ops[1]);

    if (a.has_symbol)
        fail(as, line, "test immediate cannot contain a symbol");

    if (a.addend < INT32_MIN || a.addend > INT32_MAX)
        fail(as, line, "test immediate does not fit in 32 bits");

    emit_rex(as, dst.size == 64, 0, 0, dst.num >= 8);
    emit8(as, 0xF7);
    emit8(as, modrm(3, 0, dst.num));
    emit32(as, (uint32_t)a.addend);
}

static void instr_arith(Assembler *as, int opcode_reg_reg, int group,
                        const char *mnemonic, char **ops, int n, int line)
{
    Reg dst;
    Reg src;
    AddrExpr a;

    if (n != 2)
        fail(as, line, "%s expects two operands", mnemonic);

    dst = parse_reg(ops[0]);

    if (!dst.ok)
        fail(as, line, "%s destination must be a register", mnemonic);

    src = parse_reg(ops[1]);

    if (src.ok)
    {
        if (dst.size != src.size)
            fail(as, line, "%s operands have different sizes", mnemonic);

        emit_rex(as, dst.size == 64, src.num >= 8, 0, dst.num >= 8);
        emit8(as, (uint8_t)opcode_reg_reg);
        emit8(as, modrm(3, src.num, dst.num));
        return;
    }

    a = parse_addr_expr(as, line, ops[1]);

    if (a.has_symbol)
        fail(as, line, "%s immediate cannot contain a symbol", mnemonic);

    if (a.addend < INT32_MIN || a.addend > INT32_MAX)
        fail(as, line, "%s immediate does not fit in 32 bits", mnemonic);

    emit_rex(as, dst.size == 64, 0, 0, dst.num >= 8);
    emit8(as, 0x81);
    emit8(as, modrm(3, group, dst.num));
    emit32(as, (uint32_t)a.addend);
}

static void instr_push_pop(Assembler *as, int is_push, char **ops, int n,
                           int line)
{
    Reg r;

    if (n != 1)
        fail(as, line, "%s expects one register", is_push ? "push" : "pop");

    r = parse_reg(ops[0]);

    if (!r.ok || r.size != 64)
        fail(as, line, "%s expects a 64-bit register",
             is_push ? "push" : "pop");

    emit_rex(as, 0, 0, 0, r.num >= 8);
    emit8(as, (uint8_t)((is_push ? 0x50 : 0x58) + (r.num & 7)));
}

static void process_instruction(Assembler *as, char *mnemonic_line, int line)
{
    char mnemonic[32];
    char *ops[MAX_OPERANDS];
    char *rest;
    int n;
    size_t len;
    Reg r;

    rest = mnemonic_line;

    while (*rest && !isspace((unsigned char)*rest))
        ++rest;

    len = (size_t)(rest - mnemonic_line);

    if (len >= sizeof(mnemonic))
        fail(as, line, "instruction name is too long");

    memcpy(mnemonic, mnemonic_line, len);
    mnemonic[len] = '\0';
    lower(mnemonic);

    while (*rest && isspace((unsigned char)*rest))
        ++rest;

    n = split_operands(rest, ops, MAX_OPERANDS);

    if (n < 0)
        fail(as, line, "too many operands");

    if (!strcmp(mnemonic, "mov"))
    {
        instr_mov(as, ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "lea"))
    {
        instr_lea(as, ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "call"))
    {
        instr_call(as, ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "jmp"))
    {
        instr_jmp(as, ops, n, line);
        return;
    }

    if (condition_code(mnemonic) >= 0)
    {
        instr_jcc(as, mnemonic, ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "ret") || !strcmp(mnemonic, "syscall") ||
        !strcmp(mnemonic, "nop") || !strcmp(mnemonic, "hlt") ||
        !strcmp(mnemonic, "leave"))
    {
        if (n != 0)
            fail(as, line, "%s expects no operands", mnemonic);

        if (!strcmp(mnemonic, "ret"))
            emit8(as, 0xC3);
        else if (!strcmp(mnemonic, "syscall"))
        {
            if (as->target == NWOBJ_TARGET_NWO64)
            {
                emit8(as, 0xCD);
                emit8(as, 0x80);
            }
            else
            {
                emit8(as, 0x0F);
                emit8(as, 0x05);
            }
        }
        else if (!strcmp(mnemonic, "nop"))
            emit8(as, 0x90);
        else if (!strcmp(mnemonic, "hlt"))
            emit8(as, 0xF4);
        else
            emit8(as, 0xC9);

        return;
    }

    if (!strcmp(mnemonic, "int"))
    {
        AddrExpr a;

        if (n != 1)
            fail(as, line, "int expects one operand");

        a = parse_addr_expr(as, line, ops[0]);

        if (a.has_symbol || a.addend < 0 || a.addend > 255)
            fail(as, line, "int operand must be a byte");

        emit8(as, 0xCD);
        emit8(as, (uint8_t)a.addend);
        return;
    }

    if (!strcmp(mnemonic, "push") || !strcmp(mnemonic, "pop"))
    {
        instr_push_pop(as, !strcmp(mnemonic, "push"), ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "test"))
    {
        instr_test(as, ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "add"))
    {
        instr_arith(as, 0x01, 0, "add", ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "sub"))
    {
        instr_arith(as, 0x29, 5, "sub", ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "xor"))
    {
        instr_arith(as, 0x31, 6, "xor", ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "cmp"))
    {
        instr_arith(as, 0x39, 7, "cmp", ops, n, line);
        return;
    }

    if (!strcmp(mnemonic, "inc") || !strcmp(mnemonic, "dec"))
    {
        if (n != 1)
            fail(as, line, "%s expects one register", mnemonic);

        r = parse_reg(ops[0]);

        if (!r.ok)
            fail(as, line, "%s expects a register", mnemonic);

        emit_rex(as, r.size == 64, 0, 0, r.num >= 8);
        emit8(as, 0xFF);
        emit8(as, modrm(3, !strcmp(mnemonic, "inc") ? 0 : 1, r.num));
        return;
    }

    fail(as, line, "unknown instruction '%s'", mnemonic);
}

static int hex_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static void emit_string(Assembler *as, char *text, int line)
{
    char quote = text[0];
    char *p = text + 1;

    if (quote != '\'' && quote != '"')
        fail(as, line, "invalid string literal");

    while (*p && *p != quote)
    {
        unsigned char c = (unsigned char)*p++;

        if (c == '\\')
        {
            c = (unsigned char)*p++;

            switch (c)
            {
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case '0': c = '\0'; break;
                case '\\': c = '\\'; break;
                case '\'': c = '\''; break;
                case '"': c = '"'; break;
                case 'x':
                {
                    int hi = hex_value((unsigned char)*p++);
                    int lo = hex_value((unsigned char)*p++);

                    if (hi < 0 || lo < 0)
                        fail(as, line, "invalid hex escape");

                    c = (unsigned char)((hi << 4) | lo);
                    break;
                }
                default:
                    break;
            }
        }

        emit8(as, c);
    }

    if (*p != quote)
        fail(as, line, "unterminated string literal");

    ++p;
    p = trim(p);

    if (*p)
        fail(as, line, "unexpected text after string literal");
}

static void emit_data(Assembler *as, char *args, int width, int line)
{
    char *parts[MAX_OPERANDS];
    int n = split_operands(args, parts, MAX_OPERANDS);

    if (n < 0)
        fail(as, line, "too many data values");

    if (n == 0)
        fail(as, line, "data directive needs at least one value");

    for (int i = 0; i < n; ++i)
    {
        char *part = parts[i];

        if (!*part)
            fail(as, line, "empty data value");

        if (width == 1 && (part[0] == '\'' || part[0] == '"'))
        {
            emit_string(as, part, line);
            continue;
        }

        int64_t value = eval_expr(as, line, part, as->current,
                                  as->sec[as->current].size);

        if (width == 1)
        {
            if (value < -128 || value > 255)
                fail(as, line, "db value does not fit in 8 bits");
            emit8(as, (uint8_t)value);
        }
        else if (width == 2)
        {
            if (value < -32768 || value > 65535)
                fail(as, line, "dw value does not fit in 16 bits");
            emit16(as, (uint16_t)value);
        }
        else if (width == 4)
        {
            emit32(as, (uint32_t)value);
        }
        else
        {
            emit64(as, (uint64_t)value);
        }
    }
}

static void process_line(Assembler *as, char *line, int number)
{
    char *colon;
    char *rest;
    char *word;
    char *args;
    size_t len;

    as->line = number;
    strip_comment(line);
    line = trim(line);

    if (!*line)
        return;

    colon = find_colon(line);

    if (colon)
    {
        char *label;
        int index;

        *colon = '\0';
        label = trim(line);
        rest = trim(colon + 1);

        if (!*label)
            fail(as, number, "empty label");

        for (char *p = label; *p; ++p)
        {
            if (!isalnum((unsigned char)*p) && *p != '_' && *p != '.')
                fail(as, number, "invalid label '%s'", label);
        }

        if (starts_word(rest, "equ"))
        {
            char *expr = rest + 3;

            while (*expr && isspace((unsigned char)*expr))
                ++expr;

            int64_t value = eval_expr(as, number, expr, as->current,
                                      as->sec[as->current].size);
            sym_define(as, label, NWOBJ_SEC_ABS, (uint64_t)value, number);
            return;
        }

        index = sym_define(as, label, as->current,
                           as->sec[as->current].size, number);
        (void)index;
        line = rest;

        if (!*line)
            return;
    }

    rest = line;

    while (*rest && !isspace((unsigned char)*rest))
        ++rest;

    len = (size_t)(rest - line);

    word = xstrndup(line, len);
    lower(word);

    while (*rest && isspace((unsigned char)*rest))
        ++rest;

    args = rest;

    if (!strcmp(word, "section"))
    {
        char section[32];
        size_t n = strlen(args);

        if (n >= sizeof(section))
            fail(as, number, "section name is too long");

        memcpy(section, args, n + 1);
        lower(section);
        args = trim(section);

        if (!strcmp(args, "code") || !strcmp(args, ".code") ||
            !strcmp(args, "text") || !strcmp(args, ".text"))
            as->current = CODE;
        else if (!strcmp(args, "data") || !strcmp(args, ".data") ||
                 !strcmp(args, "rodata") || !strcmp(args, ".rodata"))
            as->current = DATA;
        else if (!strcmp(args, "bss") || !strcmp(args, ".bss"))
            as->current = BSS;
        else
            fail(as, number, "unknown section '%s'", args);

        free(word);
        return;
    }

    if (!strcmp(word, "entry") || !strcmp(word, "global"))
    {
        char *names[MAX_OPERANDS];
        int n;

        for (char *p = args; *p; ++p)
        {
            if (*p == ',')
                *p = ' ';
        }

        n = 0;
        char *save = args;

        while (*save && n < MAX_OPERANDS)
        {
            while (*save && isspace((unsigned char)*save))
                ++save;

            if (!*save)
                break;

            names[n++] = save;

            while (*save && !isspace((unsigned char)*save))
                ++save;

            if (*save)
                *save++ = '\0';
        }

        if (n == 0)
            fail(as, number, "%s expects a symbol", word);

        for (int i = 0; i < n; ++i)
        {
            int index = sym_global(as, trim(names[i]));

            if (!strcmp(word, "entry"))
            {
                if (as->entry_sym >= 0)
                    fail(as, number, "entry is already defined");

                as->entry_sym = index;
            }
        }

        free(word);
        return;
    }

    if (!strcmp(word, "db") || !strcmp(word, "dw") ||
        !strcmp(word, "dd") || !strcmp(word, "dq"))
    {
        int width = word[1] == 'b' ? 1 : word[1] == 'w' ? 2 :
                    word[1] == 'd' ? 4 : 8;
        emit_data(as, args, width, number);
        free(word);
        return;
    }

    if (!strcmp(word, "bytes") || !strcmp(word, "resb") ||
        !strcmp(word, "resw") || !strcmp(word, "resd") ||
        !strcmp(word, "resq"))
    {
        int width = !strcmp(word, "resw") ? 2 :
                    !strcmp(word, "resd") ? 4 :
                    !strcmp(word, "resq") ? 8 : 1;
        int64_t count = eval_expr(as, number, args, as->current,
                                  as->sec[as->current].size);

        if (count < 0)
            fail(as, number, "negative reserve size");

        sec_reserve(as, as->current, (uint64_t)count * (uint64_t)width);
        free(word);
        return;
    }

    if (!strcmp(word, "align"))
    {
        int64_t alignment = eval_expr(as, number, args, as->current,
                                      as->sec[as->current].size);
        uint64_t size;
        uint64_t aligned;

        if (alignment <= 0 || (alignment & (alignment - 1)) != 0)
            fail(as, number, "align expects a power of two");

        size = as->sec[as->current].size;
        aligned = align_up(size, (uint64_t)alignment);

        if (aligned > size)
            sec_reserve(as, as->current, aligned - size);

        free(word);
        return;
    }

    free(word);
    process_instruction(as, line, number);
}

static int parse_target(const char *name)
{
    if (!strcmp(name, "elf64"))
        return NWOBJ_TARGET_ELF64;
    if (!strcmp(name, "nwo64"))
        return NWOBJ_TARGET_NWO64;
    if (!strcmp(name, "elf32") || !strcmp(name, "nwo32"))
    {
        fprintf(stderr, "nwasm: %s is not implemented yet\n", name);
        exit(1);
    }

    fprintf(stderr, "nwasm: unknown format '%s' (expected elf64 or nwo64)\n",
            name);
    exit(1);
}

static char *default_output(const char *path)
{
    const char *dot = strrchr(path, '.');
    size_t n = dot ? (size_t)(dot - path) : strlen(path);
    char *out = xmalloc(n + 3);

    memcpy(out, path, n);
    memcpy(out + n, ".o", 3);
    return out;
}

static void write_object(Assembler *as, const char *path)
{
    FILE *f;
    NwObjHeader header;
    uint64_t symbol_bytes = 0;
    uint64_t reloc_bytes = 0;
    uint64_t cursor;

    if (as->entry_sym < 0)
    {
        int index = sym_find(as, "_start");

        if (index >= 0 && as->syms[index].defined)
            as->entry_sym = index;
    }

    if (as->entry_sym < 0 || !as->syms[as->entry_sym].defined)
        fail(as, as->line, "no entry symbol defined");

    for (size_t i = 0; i < as->sym_count; ++i)
    {
        if (!as->syms[i].defined)
            as->syms[i].binding = NWOBJ_BIND_GLOBAL;
        symbol_bytes += sizeof(NwObjSymbol) + strlen(as->syms[i].name);
    }

    reloc_bytes = as->reloc_count * sizeof(NwObjReloc);

    memset(&header, 0, sizeof(header));
    memcpy(header.magic, NWOBJ_MAGIC, NWOBJ_MAGIC_SIZE);
    header.version = NWOBJ_VERSION;
    header.target = (uint32_t)as->target;
    header.code_size = (uint32_t)as->sec[CODE].size;
    header.data_size = (uint32_t)as->sec[DATA].size;
    header.bss_size = (uint32_t)as->sec[BSS].size;
    header.entry_sym = (uint32_t)as->entry_sym;
    header.sym_count = (uint32_t)as->sym_count;
    header.reloc_count = (uint32_t)as->reloc_count;
    header.symbol_off = (uint32_t)(sizeof(NwObjHeader) +
                                   as->sec[CODE].size +
                                   as->sec[DATA].size);
    header.reloc_off = (uint32_t)align_up(header.symbol_off + symbol_bytes, 4);

    f = fopen(path, "wb");

    if (!f)
    {
        fprintf(stderr, "nwasm: cannot create '%s': %s\n",
                path, strerror(errno));
        exit(1);
    }

    if (fwrite(&header, sizeof(header), 1, f) != 1 ||
        (as->sec[CODE].size &&
         fwrite(as->sec[CODE].data, 1, (size_t)as->sec[CODE].size, f) !=
             (size_t)as->sec[CODE].size) ||
        (as->sec[DATA].size &&
         fwrite(as->sec[DATA].data, 1, (size_t)as->sec[DATA].size, f) !=
             (size_t)as->sec[DATA].size))
    {
        fprintf(stderr, "nwasm: failed to write '%s'\n", path);
        fclose(f);
        exit(1);
    }

    cursor = sizeof(header) + as->sec[CODE].size + as->sec[DATA].size;

    for (size_t i = 0; i < as->sym_count; ++i)
    {
        NwObjSymbol sym;
        size_t n = strlen(as->syms[i].name);

        memset(&sym, 0, sizeof(sym));
        sym.section = (uint8_t)as->syms[i].section;
        sym.binding = (uint8_t)as->syms[i].binding;
        sym.offset = (uint32_t)as->syms[i].value;
        sym.name_len = (uint32_t)n;

        if (fwrite(&sym, sizeof(sym), 1, f) != 1 ||
            fwrite(as->syms[i].name, 1, n, f) != n)
        {
            fprintf(stderr, "nwasm: failed to write '%s'\n", path);
            fclose(f);
            exit(1);
        }

        cursor += sizeof(sym) + n;
    }

    while (cursor < header.reloc_off)
    {
        fputc(0, f);
        ++cursor;
    }

    for (size_t i = 0; i < as->reloc_count; ++i)
    {
        NwObjReloc reloc;

        memset(&reloc, 0, sizeof(reloc));
        reloc.section = (uint8_t)as->relocs[i].section;
        reloc.type = (uint8_t)as->relocs[i].type;
        reloc.offset = as->relocs[i].offset;
        reloc.symbol = (uint32_t)as->relocs[i].sym;
        reloc.addend = as->relocs[i].addend;

        if (fwrite(&reloc, sizeof(reloc), 1, f) != 1)
        {
            fprintf(stderr, "nwasm: failed to write '%s'\n", path);
            fclose(f);
            exit(1);
        }
    }

    if (fclose(f) != 0)
    {
        fprintf(stderr, "nwasm: failed to close '%s'\n", path);
        exit(1);
    }

    (void)reloc_bytes;
}

static void usage(void)
{
    printf("NwAsm 0.1 - NwOS assembler\n\n");
    printf("Usage:\n");
    printf("  nwasm [-f elf64|nwo64] <input.asm> [-o <output.o>]\n\n");
    printf("Examples:\n");
    printf("  nwasm -f nwo64 hello.asm -o hello.o\n");
    printf("  link hello.o -o hello.nwo\n");
    printf("  nwasm -f elf64 hello.asm -o hello.o\n");
    printf("  link hello.o -o hello\n");
}

int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *output = NULL;
    int target = NWOBJ_TARGET_ELF64;
    Assembler as;
    FILE *f;
    char *line = NULL;
    size_t cap = 0;
    ssize_t length;
    int number = 0;

    memset(&as, 0, sizeof(as));
    as.entry_sym = -1;
    as.current = CODE;

    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
        {
            usage();
            return 0;
        }
        else if (!strcmp(argv[i], "-f"))
        {
            if (++i >= argc)
            {
                fprintf(stderr, "nwasm: -f needs a format\n");
                return 1;
            }

            target = parse_target(argv[i]);
        }
        else if (!strcmp(argv[i], "-o"))
        {
            if (++i >= argc)
            {
                fprintf(stderr, "nwasm: -o needs a file name\n");
                return 1;
            }

            output = argv[i];
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "nwasm: unknown option '%s'\n", argv[i]);
            return 1;
        }
        else if (!input)
        {
            input = argv[i];
        }
        else
        {
            fprintf(stderr, "nwasm: multiple input files are not supported\n");
            return 1;
        }
    }

    if (!input)
    {
        usage();
        return 1;
    }

    char *generated = NULL;

    if (!output)
    {
        generated = default_output(input);
        output = generated;
    }

    f = fopen(input, "rb");

    if (!f)
    {
        fprintf(stderr, "nwasm: cannot open '%s': %s\n",
                input, strerror(errno));
        return 1;
    }

    as.path = input;
    as.target = target;

    while ((length = getline(&line, &cap, f)) >= 0)
    {
        ++number;

        while (length > 0 &&
               (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';

        process_line(&as, line, number);
    }

    if (ferror(f))
    {
        fprintf(stderr, "nwasm: failed to read '%s'\n", input);
        fclose(f);
        free(line);
        return 1;
    }

    fclose(f);
    free(line);

    write_object(&as, output);

    printf("NwAsm: %s -> %s\n", input, output);
    printf("  target: %s\n",
           target == NWOBJ_TARGET_NWO64 ? "nwo64" : "elf64");
    printf("  code:   %llu bytes\n", (unsigned long long)as.sec[CODE].size);
    printf("  data:   %llu bytes\n", (unsigned long long)as.sec[DATA].size);
    printf("  bss:    %llu bytes\n", (unsigned long long)as.sec[BSS].size);
    printf("  relocs: %llu\n", (unsigned long long)as.reloc_count);

    free(generated);
    return 0;
}
