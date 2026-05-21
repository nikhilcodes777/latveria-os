#pragma once
// Minimal libc subset for doomgeneric kernel port.
// Covers every symbol listed in libc_funcs.txt.

#include "alloc.h"
#include "memory.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ---- string ----------------------------------------------------------------
size_t strlen(const char *s);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
char *strcat(char *dst, const char *src);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int strcasecmp(const char *a, const char *b);
int strncasecmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strdup(const char *s); // uses malloc

// ---- conversion ------------------------------------------------------------
int atoi(const char *s);
int atof(const char *s);
int toupper(int c);
static inline int abs(int n) { return n < 0 ? -n : n; }

// ---- stdio (minimal / serial-backed) ---------------------------------------
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define EISDIR 21

// FILE is opaque; we expose stdout/stderr as serial, and a very small
// file-backed system for Doom's save files.
typedef struct _FILE FILE;
extern FILE *stdout;
extern FILE *stderr;

int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int putchar(int c);
int puts(const char *s);

// sscanf — limited: %d %s %c
int __isoc99_sscanf(const char *str, const char *fmt, ...);

// ---- file I/O (ramdisk-backed, simple) -------------------------------------
void set_wad_module(void *data, size_t size);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *f);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f);
int fseek(FILE *f, long offset, int whence);
long ftell(FILE *f);
int fflush(FILE *f);
int fprintf(FILE *stream, const char *fmt, ...);

// ---- filesystem helpers ----------------------------------------------------
int mkdir(const char *path, uint32_t mode); // no-op / stub
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);

// ---- process ---------------------------------------------------------------
__attribute__((noreturn)) void exit(int status);

// ---- ctype -----------------------------------------------------------------
// __ctype_b_loc — GCC uses this for ctype macros
unsigned short **__ctype_b_loc(void);

static inline int isspace(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
         c == '\f';
}
static inline int isprint(int c) { return c >= 32 && c < 127; }
static inline int isdigit(int c) { return c >= '0' && c <= '9'; }
static inline int isalpha(int c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static inline int isalnum(int c) { return isalpha(c) || isdigit(c); }
static inline int islower(int c) { return c >= 'a' && c <= 'z'; }
static inline int isupper(int c) { return c >= 'A' && c <= 'Z'; }
static inline int tolower(int c) { return isupper(c) ? c + 32 : c; }

// errno
int *__errno_location(void);
#define errno (*__errno_location())

// ---- environment -----------------------------------------------------------
static inline char *getenv(const char *name) {
  (void)name;
  return (char *)0;
}

// ---- additional file I/O ---------------------------------------------------
int feof(FILE *f);
int sscanf(const char *str, const char *fmt, ...);
int fscanf(FILE *f, const char *fmt, ...);

// ---- system ----------------------------------------------------------------
int system(const char *cmd); // stub, returns -1

// ---- Doom generic callbacks (declared here for one-stop inclusion) ---------
// The kernel must implement these.
void DG_Init(void);
void DG_DrawFrame(void);
int DG_GetKey(int *pressed, unsigned char *doomKey);
void DG_SetWindowTitle(const char *title);
uint32_t DG_GetTicksMs(void);
void DG_SleepMs(uint32_t ms);
