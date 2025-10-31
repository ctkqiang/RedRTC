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
    /** @brief 检查缓冲区大小是否足够，UUID 字符串至少需要 37 个字符 (36 个字符 + 1 个空终止符) */
    if (size < 37) return;
    
    /** @brief 获取当前时间，用于生成随机数种子 */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    /** @brief 使用时间、微秒和进程ID作为种子，提高随机性 */
    unsigned int seed = (unsigned int)(tv.tv_sec ^ tv.tv_usec ^ getpid());
    
    /**
     * @brief 格式化 UUID 字符串，遵循 UUID v4 规范
     * %08x-%04x-4%03x-%04x-%08x%04x
     * 其中 '4' 表示 UUID 版本号为 4 (随机生成)
     * 第三个部分的第一个字符固定为 '4'
     * 第四个部分的第一个字符固定为 '8', '9', 'a', 或 'b' (这里通过 | 0x8000 实现)
     */
    snprintf(buffer, size,
            "%08x-%04x-4%03x-%04x-%08x%04x",
            rand_r(&seed) & 0xffffffff, /**< 8位十六进制数 */
            rand_r(&seed) & 0xffff,     /**< 4位十六进制数 */
            rand_r(&seed) & 0x0fff,     /**< 3位十六进制数 */
            (rand_r(&seed) & 0x3fff) | 0x8000, /**< 4位十六进制数，设置特定位 */
            rand_r(&seed) & 0xffffffff, /**< 8位十六进制数 */
            rand_r(&seed) & 0xffff);    /**< 4位十六进制数 */

    
}


/**
 * @brief 获取当前时间戳，单位为毫秒
 * @return 当前时间戳，单位为毫秒
 */
uint64_t get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    /** @brief 将秒转换为毫秒，并加上微秒部分 */
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
    /** @brief 检查参数有效性 */
    if (!dest || !src || dest_size == 0) return -1;
    
    size_t i;
    /** @brief 复制字符，直到达到目标缓冲区大小减一或源字符串结束 */
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    /** @brief 确保目标字符串以空终止符结束 */
    dest[i] = '\0';
    
    /** @brief 如果源字符串在复制前就结束了，则表示完全复制成功 */
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
    /** @brief 检查参数有效性 */
    if (!dest || !src || dest_size == 0) return -1;
    
    /** @brief 获取目标字符串当前长度 */
    size_t dest_len = strlen(dest);
    /** @brief 如果目标缓冲区已满，无法连接 */
    if (dest_len >= dest_size) return -2;
    
    /** @brief 调用 safe_strncpy 将源字符串连接到目标字符串的末尾 */
    return safe_strncpy(dest + dest_len, src, dest_size - dest_len);
}

/**
 * @brief 初始化内存池
 * @param pool 内存池结构体指针
 * @param max_size 内存池中可存储的最大指针数量
 */
void memory_pool_init(memory_pool_t *pool, size_t max_size) {
    /** @brief 分配一个指针数组作为空闲列表 */
    pool->free_list = calloc(max_size, sizeof(void *));
    pool->free_count = 0; /**< 当前空闲的内存块数量 */
    pool->total_allocated = 0; /**< 总共分配的内存块数量 */
    pool->max_size = max_size; /**< 内存池最大容量 */
}

/**
 * @brief 从内存池中分配内存
 * @param pool 内存池结构体指针
 * @param size 需要分配的内存大小
 * @return 分配的内存指针，或 NULL (如果分配失败)
 */
void *memory_pool_alloc(memory_pool_t *pool, size_t size) {
    /** @brief 如果空闲列表中有可用的内存块，则直接从空闲列表获取 */
    if (pool->free_count > 0) {
        void *ptr = pool->free_list[--pool->free_count];
        /** @brief 清零获取到的内存块 */
        memset(ptr, 0, size);
        return ptr;
    }
    
    /** @brief 如果空闲列表为空，且总分配数量未达到最大容量，则分配新的内存块 */
    if (pool->total_allocated < pool->max_size) {
        void *ptr = calloc(1, size);
        if (ptr) pool->total_allocated++; 
        return ptr;
    }
    
    /** @brief 内存池已满，无法分配 */
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
        pool->total_allocated--; /**< 减少总分配计数 */
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

