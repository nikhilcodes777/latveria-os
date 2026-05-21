#include "alloc.h"
#include <stdint.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// 50 MB static arena
// ---------------------------------------------------------------------------
#define ARENA_SIZE  (50UL * 1024UL * 1024UL)
#define ALIGN       16   // keep all allocations 16-byte aligned

static uint8_t membuffer[ARENA_SIZE] __attribute__((aligned(ALIGN)));
static size_t  arena_offset = 0;

// ---------------------------------------------------------------------------
// Block header (stored immediately before the user data).
// This lets realloc() know the size of a previous allocation.
// ---------------------------------------------------------------------------
typedef struct {
    size_t size; // usable bytes following this header
} alloc_hdr_t;

#define HDR_SZ (sizeof(alloc_hdr_t))

static size_t align_up(size_t v) {
    return (v + ALIGN - 1) & ~(size_t)(ALIGN - 1);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void alloc_reset(void) {
    arena_offset = 0;
}

size_t alloc_used(void)      { return arena_offset; }
size_t alloc_remaining(void) { return ARENA_SIZE - arena_offset; }

void *malloc(size_t size) {
    if (size == 0) size = 1;
    size_t total = align_up(HDR_SZ + size);
    if (arena_offset + total > ARENA_SIZE) return NULL; // OOM

    alloc_hdr_t *hdr = (alloc_hdr_t *)(membuffer + arena_offset);
    hdr->size = size;
    arena_offset += total;
    return (uint8_t *)hdr + HDR_SZ;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) {
        // zero the block
        uint8_t *b = p;
        for (size_t i = 0; i < total; i++) b[i] = 0;
    }
    return p;
}

// realloc: allocate fresh + copy — old memory is wasted (arena semantics).
void *realloc(void *ptr, size_t size) {
    if (!ptr)   return malloc(size);
    if (!size)  { free(ptr); return NULL; }

    alloc_hdr_t *old_hdr = (alloc_hdr_t *)((uint8_t *)ptr - HDR_SZ);
    size_t old_size = old_hdr->size;

    void *newp = malloc(size);
    if (!newp) return NULL;

    size_t copy = (size < old_size) ? size : old_size;
    uint8_t *src = ptr, *dst = newp;
    for (size_t i = 0; i < copy; i++) dst[i] = src[i];
    return newp;
}

// Arena allocator — free is intentionally a no-op.
// Memory is reclaimed only via alloc_reset().
void free(void *ptr) {
    (void)ptr;
}
