#pragma once

#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <time.h>
#include <sys/time.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define container_of(ptr, type, member) ({ \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); })

void generate_uuid(char *buffer, size_t size);

uint64_t get_timestamp_ms(void);
uint32_t get_timestamp_sec(void);

int safe_strncpy(char *dest, const char *src, size_t dest_size);
int safe_strncat(char *dest, const char *src, size_t dest_size);

typedef struct memory_pool {
    void **free_list;
    size_t free_count;
    size_t total_allocated;
    size_t max_size;
} memory_pool_t;

void memory_pool_init(memory_pool_t *pool, size_t max_size);
void *memory_pool_alloc(memory_pool_t *pool, size_t size);
void memory_pool_free(memory_pool_t *pool, void *ptr);
void memory_pool_cleanup(memory_pool_t *pool);

#endif