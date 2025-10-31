#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "../include/utilities.h"

/**
 * @brief 生成一个唯一的 UUID (通用唯一标识符)
 * @param buffer 存储 UUID 的缓冲区
 * @param size 缓冲区的大小
 */
void generate_uuid(char *buffer, size_t size) {
    if (size < 37) return;
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    unsigned int seed = (unsigned int)(tv.tv_sec ^ tv.tv_usec ^ getpid());
    
    snprintf(buffer, size,
            "%08x-%04x-4%03x-%04x-%08x%04x",
            rand_r(&seed) & 0xffffffff,
            rand_r(&seed) & 0xffff,
            rand_r(&seed) & 0x0fff,
            (rand_r(&seed) & 0x3fff) | 0x8000,
            rand_r(&seed) & 0xffffffff,
            rand_r(&seed) & 0xffff);

    
}


/**
 * @brief 获取当前时间戳，单位为毫秒
 * @return 当前时间戳，单位为毫秒
 */
uint64_t get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

/**
 * @brief 获取当前时间戳，单位为秒
 * @return 当前时间戳，单位为秒
 */
uint32_t get_timestamp_sec(void) {
    return (uint32_t)time(NULL);
}

/**
 * @brief 安全地复制字符串，防止缓冲区溢出
 * @param dest 目标缓冲区
 * @param src 源字符串
 * @param dest_size 目标缓冲区的大小
 * @return 0 成功，-1 参数无效，-2 源字符串被截断
 */
int safe_strncpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return -1;
    
    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    
    return (src[i] == '\0') ? 0 : -2;
}

/**
 * @brief 安全地连接字符串，防止缓冲区溢出
 * @param dest 目标缓冲区 (已包含字符串)
 * @param src 源字符串
 * @param dest_size 目标缓冲区的大小
 * @return 0 成功，-1 参数无效，-2 目标缓冲区已满或源字符串被截断
 */
int safe_strncat(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return -1;
    
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size) return -2;
    
    return safe_strncpy(dest + dest_len, src, dest_size - dest_len);
}

/**
 * @brief 初始化内存池
 * @param pool 内存池结构体指针
 * @param max_size 内存池中可存储的最大指针数量
 */
void memory_pool_init(memory_pool_t *pool, size_t max_size) {
    pool->free_list = calloc(max_size, sizeof(void *));
    pool->free_count = 0;
    pool->total_allocated = 0;
    pool->max_size = max_size;
}

/**
 * @brief 从内存池中分配内存
 * @param pool 内存池结构体指针
 * @param size 需要分配的内存大小
 * @return 分配的内存指针，或 NULL (如果分配失败)
 */
void *memory_pool_alloc(memory_pool_t *pool, size_t size) {
    if (pool->free_count > 0) {
        void *ptr = pool->free_list[--pool->free_count];
        memset(ptr, 0, size);
        return ptr;
    }
    
    if (pool->total_allocated < pool->max_size) {
        void *ptr = calloc(1, size);
        if (ptr) pool->total_allocated++; 
        return ptr;
    }
    
    return NULL;
}

/**
 * @brief 释放内存到内存池
 * @param pool 内存池结构体指针
 * @param ptr 需要释放的内存指针
 */
void memory_pool_free(memory_pool_t *pool, void *ptr) {
    if (ptr && pool->free_count < pool->max_size) {
        pool->free_list[pool->free_count++] = ptr;
    } else if (ptr) {
        free(ptr);
        pool->total_allocated--;
    }
}

/**
 * @brief 清理内存池，释放所有已分配的内存
 * @param pool 内存池结构体指针
 */
void memory_pool_cleanup(memory_pool_t *pool) {
    for (size_t i = 0; i < pool->free_count; i++) {
        free(pool->free_list[i]);
    }

    free(pool->free_list);

    pool->free_count = 0;
    pool->total_allocated = 0;
}

