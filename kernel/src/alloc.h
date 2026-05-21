#pragma once
#include <stddef.h>
#include <stdint.h>

// Arena allocator over a 50 MB static buffer.
// malloc/calloc/realloc/free are provided.
// free() is a no-op (linear arena — memory is only reclaimed via alloc_reset()).
// realloc() copies to a fresh allocation.

void  alloc_reset(void);           // reset arena (frees everything)
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);             // no-op, kept for API compat

size_t alloc_used(void);           // bytes currently allocated
size_t alloc_remaining(void);      // bytes left in arena
