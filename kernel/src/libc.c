// Minimal libc implementation for kernel / doomgeneric port.
// Provides every symbol in libc_funcs.txt.

#include "libc.h"
#include "serial.h"
#include "alloc.h"
#include "memory.h"
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

// ============================================================
// errno
// ============================================================
static int _errno_val = 0;
int *__errno_location(void) { return &_errno_val; }

// ============================================================
// ctype  (__ctype_b_loc)
// GNU ctype uses a 384-entry table; we implement toupper directly
// and provide a minimal __ctype_b_loc returning a pointer to a
// zeroed table (Doom only uses it through macros like isalpha).
// ============================================================
#define _ISupper  (1 << 8)
#define _ISlower  (1 << 9)
#define _ISalpha  (1 << 10)
#define _ISdigit  (1 << 11)
#define _ISspace  (1 << 12)
#define _ISprint  (1 << 14)

static unsigned short ctype_table[384]; // zeroed by BSS
static unsigned short *ctype_ptr = ctype_table + 128;

static void ctype_init(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    for (int c = 0; c < 256; c++) {
        unsigned short v = 0;
        if (c >= 'A' && c <= 'Z') v |= _ISupper | _ISalpha | _ISprint;
        if (c >= 'a' && c <= 'z') v |= _ISlower | _ISalpha | _ISprint;
        if (c >= '0' && c <= '9') v |= _ISdigit | _ISprint;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') v |= _ISspace;
        if (c >= 32 && c < 127) v |= _ISprint;
        ctype_table[c + 128] = v;
    }
}

unsigned short **__ctype_b_loc(void) {
    ctype_init();
    return &ctype_ptr;
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

// ============================================================
// string
// ============================================================
size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}

char *strcat(char *dst, const char *src) {
    strcpy(dst + strlen(dst), src);
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (!a[i] || a[i] != b[i])
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

static int lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int strcasecmp(const char *a, const char *b) {
    while (*a && lc(*a) == lc(*b)) { a++; b++; }
    return lc((unsigned char)*a) - lc((unsigned char)*b);
}

int strncasecmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (!a[i] || lc((unsigned char)a[i]) != lc((unsigned char)b[i]))
            return lc((unsigned char)a[i]) - lc((unsigned char)b[i]);
    }
    return 0;
}

char *strchr(const char *s, int c) {
    for (; *s; s++) if (*s == (char)c) return (char *)s;
    return (c == 0) ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    for (; *s; s++) if (*s == (char)c) last = s;
    if (c == 0) return (char *)s;
    return (char *)last;
}

char *strstr(const char *h, const char *n) {
    if (!*n) return (char *)h;
    size_t nl = strlen(n);
    for (; *h; h++)
        if (*h == *n && strncmp(h, n, nl) == 0) return (char *)h;
    return NULL;
}

char *strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

// ============================================================
// conversion
// ============================================================
int atoi(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    int sign = 1, v = 0;
    if (*s == '-') { sign = -1; s++; } else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return sign * v;
}

int atof(const char *s) {
    while (*s == ' ') s++;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; } else if (*s == '+') s++;
    int v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    if (*s == '.') { s++; while (*s >= '0' && *s <= '9') s++; }
    return sign * v;
}

// ============================================================
// FILE / stdio  (serial-backed + tiny ramdisk for Doom saves)
// ============================================================
#define MAX_FILES    16
#define FILE_BUF_SZ  (512 * 1024)  // 512 KB per file

typedef struct {
    char   path[128];
    uint8_t *buf;
    size_t  size;    // bytes written / file length
    size_t  pos;     // seek position
    int     writable;
    int     in_use;
} file_impl_t;

static file_impl_t file_table[MAX_FILES];

struct _FILE {
    int idx;    // index into file_table, or -1 for serial
    int is_serial;
};

static FILE _stdout = { .idx = -1, .is_serial = 1 };
static FILE _stderr = { .idx = -1, .is_serial = 1 };
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

void set_wad_module(void *data, size_t size) {
    file_table[0].buf = data;
    file_table[0].size = size;
    file_table[0].pos = 0;
    file_table[0].writable = 0;
    file_table[0].in_use = 1;
    strncpy(file_table[0].path, "doom1.wad", 127);
    serial_printf("[libc] Mounted doom1.wad at %p (size %u)\n", data, (unsigned int)size);
}

static int find_file(const char *path) {
    for (int i = 0; i < MAX_FILES; i++)
        if (file_table[i].in_use && strcmp(file_table[i].path, path) == 0)
            return i;
    return -1;
}

FILE *fopen(const char *path, const char *mode) {
    int writable = (mode[0] == 'w' || mode[0] == 'a');
    int idx = find_file(path);

    if (idx < 0) {
        if (!writable) { errno = 2; return NULL; } // ENOENT
        // allocate new slot
        for (idx = 0; idx < MAX_FILES; idx++)
            if (!file_table[idx].in_use) break;
        if (idx == MAX_FILES) return NULL;
        file_table[idx].buf = malloc(FILE_BUF_SZ);
        if (!file_table[idx].buf) return NULL;
        file_table[idx].size = 0;
        file_table[idx].in_use = 1;
        strncpy(file_table[idx].path, path, 127);
    }

    FILE *f = malloc(sizeof(FILE));
    if (!f) return NULL;
    f->idx = idx;
    f->is_serial = 0;
    file_table[idx].writable = writable;
    file_table[idx].pos = (mode[0] == 'a') ? file_table[idx].size : 0;
    if (mode[0] == 'w') file_table[idx].size = 0;
    return f;
}

int fclose(FILE *f) {
    if (!f || f->is_serial) return 0;
    free(f);
    return 0;
}

size_t fread(void *ptr, size_t sz, size_t nmemb, FILE *f) {
    if (!f || f->is_serial) return 0;
    file_impl_t *fi = &file_table[f->idx];
    size_t want = sz * nmemb;
    size_t avail = fi->size - fi->pos;
    if (want > avail) want = avail;
    memcpy(ptr, fi->buf + fi->pos, want);
    fi->pos += want;
    return want / sz;
}

size_t fwrite(const void *ptr, size_t sz, size_t nmemb, FILE *f) {
    if (!f) return 0;
    if (f->is_serial) {
        const char *s = ptr;
        for (size_t i = 0; i < sz * nmemb; i++) serial_putc(s[i]);
        return nmemb;
    }
    file_impl_t *fi = &file_table[f->idx];
    size_t want = sz * nmemb;
    if (fi->pos + want > FILE_BUF_SZ) want = FILE_BUF_SZ - fi->pos;
    memcpy(fi->buf + fi->pos, ptr, want);
    fi->pos += want;
    if (fi->pos > fi->size) fi->size = fi->pos;
    return want / sz;
}


int fseek(FILE *f, long offset, int whence) {
    if (!f || f->is_serial) return -1;
    file_impl_t *fi = &file_table[f->idx];
    long newpos;
    if      (whence == SEEK_SET) newpos = offset;
    else if (whence == SEEK_CUR) newpos = (long)fi->pos + offset;
    else                          newpos = (long)fi->size + offset;
    if (newpos < 0) newpos = 0;
    fi->pos = (size_t)newpos;
    return 0;
}

long ftell(FILE *f) {
    if (!f || f->is_serial) return -1;
    return (long)file_table[f->idx].pos;
}

int fflush(FILE *f) { (void)f; return 0; }

int remove(const char *path) {
    int idx = find_file(path);
    if (idx < 0) { errno = 2; return -1; }
    file_table[idx].in_use = 0;
    return 0;
}

int rename(const char *old, const char *newp) {
    int idx = find_file(old);
    if (idx < 0) { errno = 2; return -1; }
    strncpy(file_table[idx].path, newp, 127);
    return 0;
}

int mkdir(const char *path, uint32_t mode) {
    (void)path; (void)mode;
    return 0; // stub — we have no FS
}

// ============================================================
// vsnprintf  (the core formatter everything else calls)
// ============================================================
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    size_t pos = 0;
#define OUT(c) do { if (pos + 1 < n) buf[pos++] = (c); } while(0)

    for (; *fmt; fmt++) {
        if (*fmt != '%') { OUT(*fmt); continue; }
        fmt++;
        // flags
        int zero_pad = 0, left_align = 0, plus_sign = 0, space_sign = 0, hash_flag = 0;
        int width = 0;
        int has_prec = 0, prec = 0;
        for (;;) {
            if      (*fmt == '0') { zero_pad = 1; fmt++; }
            else if (*fmt == '-') { left_align = 1; fmt++; }
            else if (*fmt == '+') { plus_sign = 1; fmt++; }
            else if (*fmt == ' ') { space_sign = 1; fmt++; }
            else if (*fmt == '#') { hash_flag = 1; fmt++; }
            else break;
        }
        (void)plus_sign; (void)space_sign; (void)hash_flag;
        // width
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) { left_align = 1; width = -width; }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        }
        // precision
        if (*fmt == '.') {
            fmt++;
            has_prec = 1;
            if (*fmt == '*') {
                prec = va_arg(ap, int);
                if (prec < 0) { has_prec = 0; prec = 0; }
                fmt++;
            } else {
                prec = 0;
                while (*fmt >= '0' && *fmt <= '9') prec = prec * 10 + (*fmt++ - '0');
            }
        }
        // length modifier (skip)
        if (*fmt == 'l') { fmt++; if (*fmt == 'l') fmt++; }
        if (*fmt == 'z') fmt++;
        if (*fmt == 'h') { fmt++; if (*fmt == 'h') fmt++; }

        char tmp[64]; int tl = 0;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            size_t sl = strlen(s);
            if (has_prec && (size_t)prec < sl) sl = (size_t)prec;
            if (!left_align) { int pad = width - (int)sl; while (pad-- > 0) OUT(' '); }
            for (size_t i = 0; i < sl; i++) OUT(s[i]);
            if (left_align)  { int pad = width - (int)sl; while (pad-- > 0) OUT(' '); }
            break;
        }
        case 'c': OUT((char)va_arg(ap, int)); break;
        case 'd': case 'i': {
            long v = va_arg(ap, int);
            int negative = 0;
            if (v < 0) { negative = 1; v = -v; }
            int start = 0;
            do { tmp[tl++] = '0' + (v % 10); v /= 10; } while(v);
            // reverse digits
            for (int a = start, b = tl-1; a < b; a++, b--) { char t=tmp[a]; tmp[a]=tmp[b]; tmp[b]=t; }
            // precision: minimum digits (pad with leading zeros)
            int min_digits = has_prec ? prec : 1;
            int num_digits = tl;
            int leading_zeros = (min_digits > num_digits) ? min_digits - num_digits : 0;
            int total_len = (negative ? 1 : 0) + leading_zeros + num_digits;
            // When precision is specified, zero_pad flag is ignored
            int pad_char = (zero_pad && !has_prec && !left_align) ? '0' : ' ';
            if (!left_align && pad_char == ' ') { int pad = width - total_len; while (pad-- > 0) OUT(' '); }
            if (negative) OUT('-');
            if (!left_align && pad_char == '0') { int pad = width - total_len; while (pad-- > 0) OUT('0'); }
            for (int i = 0; i < leading_zeros; i++) OUT('0');
            for (int i = 0; i < tl; i++) OUT(tmp[i]);
            if (left_align) { int pad = width - total_len; while (pad-- > 0) OUT(' '); }
            break;
        }
        case 'u': {
            unsigned long v = va_arg(ap, unsigned int);
            do { tmp[tl++] = '0' + (v % 10); v /= 10; } while(v);
            for (int a=0,b=tl-1;a<b;a++,b--){char t=tmp[a];tmp[a]=tmp[b];tmp[b]=t;}
            int min_digits = has_prec ? prec : 1;
            int num_digits = tl;
            int leading_zeros = (min_digits > num_digits) ? min_digits - num_digits : 0;
            int total_len = leading_zeros + num_digits;
            int pad_char = (zero_pad && !has_prec && !left_align) ? '0' : ' ';
            if (!left_align && pad_char == ' ') { int pad = width - total_len; while (pad-- > 0) OUT(' '); }
            if (!left_align && pad_char == '0') { int pad = width - total_len; while (pad-- > 0) OUT('0'); }
            for (int i = 0; i < leading_zeros; i++) OUT('0');
            for (int i=0;i<tl;i++) OUT(tmp[i]);
            if (left_align) { int pad = width - total_len; while (pad-- > 0) OUT(' '); }
            break;
        }
        case 'x': case 'X': {
            unsigned long v = va_arg(ap, unsigned int);
            const char *h = (*fmt=='x')?"0123456789abcdef":"0123456789ABCDEF";
            do { tmp[tl++] = h[v & 0xF]; v >>= 4; } while(v);
            for (int a=0,b=tl-1;a<b;a++,b--){char t=tmp[a];tmp[a]=tmp[b];tmp[b]=t;}
            int min_digits = has_prec ? prec : 1;
            int num_digits = tl;
            int leading_zeros = (min_digits > num_digits) ? min_digits - num_digits : 0;
            int total_len = leading_zeros + num_digits;
            int pad_char = (zero_pad && !has_prec && !left_align) ? '0' : ' ';
            if (!left_align && pad_char == ' ') { int pad = width - total_len; while (pad-- > 0) OUT(' '); }
            if (!left_align && pad_char == '0') { int pad = width - total_len; while (pad-- > 0) OUT('0'); }
            for (int i = 0; i < leading_zeros; i++) OUT('0');
            for (int i=0;i<tl;i++) OUT(tmp[i]);
            if (left_align) { int pad = width - total_len; while (pad-- > 0) OUT(' '); }
            break;
        }
        case 'p': {
            unsigned long v = (unsigned long)va_arg(ap, void *);
            OUT('0'); OUT('x');
            do { tmp[tl++] = "0123456789abcdef"[v & 0xF]; v >>= 4; } while(v);
            for (int a=0,b=tl-1;a<b;a++,b--){char t=tmp[a];tmp[a]=tmp[b];tmp[b]=t;}
            for (int i=0;i<tl;i++) OUT(tmp[i]);
            break;
        }
        case '%': OUT('%'); break;
        default:  OUT('%'); OUT(*fmt); break;
        }
    }
    if (n > 0) buf[pos] = '\0';
    return (int)pos;
#undef OUT
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap) {
    char buf[512];
    int r = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (stream && stream->is_serial) {
        serial_puts(buf);
    } else if (stream) {
        fwrite(buf, 1, (size_t)r, stream);
    } else {
        serial_puts(buf);
    }
    return r;
}

int fprintf(FILE *stream, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}

int printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return r;
}

int putchar(int c) {
    serial_putc((char)c);
    return c;
}

int puts(const char *s) {
    serial_puts(s);
    serial_putc('\n');
    return 0;
}

// ============================================================
// sscanf  (limited: %d %s %c %u)
// ============================================================
int __isoc99_sscanf(const char *str, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int matched = 0;
    const char *s = str;
    for (; *fmt && *s; fmt++) {
        if (*fmt == ' ') { while (*s == ' ' || *s == '\t') s++; continue; }
        if (*fmt != '%') { if (*s == *fmt) s++; continue; }
        fmt++;
        switch (*fmt) {
        case 'd': case 'i': {
            while (*s == ' ') s++;
            int sign = 1; if (*s == '-') { sign = -1; s++; }
            int v = 0;
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            *va_arg(ap, int *) = sign * v;
            matched++;
            break;
        }
        case 'u': {
            while (*s == ' ') s++;
            unsigned v = 0;
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            *va_arg(ap, unsigned *) = v;
            matched++;
            break;
        }
        case 's': {
            while (*s == ' ') s++;
            char *out = va_arg(ap, char *);
            while (*s && *s != ' ' && *s != '\t') *out++ = *s++;
            *out = 0; matched++;
            break;
        }
        case 'c': {
            *va_arg(ap, char *) = *s++;
            matched++;
            break;
        }
        }
    }
    va_end(ap);
    return matched;
}

// ============================================================
// exit / system
// ============================================================
__attribute__((noreturn)) void exit(int status) {
    (void)status;
    serial_printf("[libc] exit(%d) called — halting\n", status);
    for (;;) __asm__ volatile("cli; hlt");
}

int system(const char *cmd) { (void)cmd; return -1; }

// ============================================================
// feof
// ============================================================
int feof(FILE *f) {
    if (!f || f->is_serial) return 0;
    file_impl_t *fi = &file_table[f->idx];
    return fi->pos >= fi->size;
}

// ============================================================
// sscanf — wrapper around __isoc99_sscanf
// ============================================================
int sscanf(const char *str, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int matched = 0;
    const char *s = str;
    for (; *fmt && *s; fmt++) {
        if (*fmt == ' ') { while (*s == ' ' || *s == '\t') s++; continue; }
        if (*fmt != '%') { if (*s == *fmt) s++; continue; }
        fmt++;
        // skip width specifier
        int maxw = 0;
        while (*fmt >= '0' && *fmt <= '9') { maxw = maxw * 10 + (*fmt++ - '0'); }
        // handle scanset %[...]
        if (*fmt == '[') {
            fmt++; // skip '['
            int negate = 0;
            if (*fmt == '^') { negate = 1; fmt++; }
            // collect set chars
            char set[256] = {0};
            while (*fmt && *fmt != ']') { set[(unsigned char)*fmt] = 1; fmt++; }
            // fmt now points to ']'
            char *out = va_arg(ap, char *);
            int count = 0;
            while (*s) {
                int in_set = set[(unsigned char)*s];
                if (negate) in_set = !in_set;
                if (!in_set) break;
                if (maxw > 0 && count >= maxw) break;
                *out++ = *s++; count++;
            }
            *out = 0;
            if (count > 0) matched++;
            break; // simplified: stop after scanset
        }
        switch (*fmt) {
        case 'd': case 'i': {
            while (*s == ' ') s++;
            int sign = 1; if (*s == '-') { sign = -1; s++; }
            int v = 0;
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            *va_arg(ap, int *) = sign * v;
            matched++;
            break;
        }
        case 'x': {
            while (*s == ' ') s++;
            unsigned v = 0;
            while (1) {
                if (*s >= '0' && *s <= '9') v = v * 16 + (*s++ - '0');
                else if (*s >= 'a' && *s <= 'f') v = v * 16 + (*s++ - 'a' + 10);
                else if (*s >= 'A' && *s <= 'F') v = v * 16 + (*s++ - 'A' + 10);
                else break;
            }
            *va_arg(ap, unsigned *) = v;
            matched++;
            break;
        }
        case 'o': {
            while (*s == ' ') s++;
            unsigned v = 0;
            while (*s >= '0' && *s <= '7') v = v * 8 + (*s++ - '0');
            *va_arg(ap, unsigned *) = v;
            matched++;
            break;
        }
        case 'u': {
            while (*s == ' ') s++;
            unsigned v = 0;
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            *va_arg(ap, unsigned *) = v;
            matched++;
            break;
        }
        case 's': {
            while (*s == ' ') s++;
            char *out = va_arg(ap, char *);
            int count = 0;
            while (*s && *s != ' ' && *s != '\t' && *s != '\n') {
                if (maxw > 0 && count >= maxw) break;
                *out++ = *s++; count++;
            }
            *out = 0; matched++;
            break;
        }
        case 'c': {
            *va_arg(ap, char *) = *s++;
            matched++;
            break;
        }
        }
    }
    va_end(ap);
    return matched;
}

// ============================================================
// fscanf — stub (config loading is #if ORIGCODE, never called)
// ============================================================
int fscanf(FILE *f, const char *fmt, ...) {
    (void)f; (void)fmt;
    return 0;
}
