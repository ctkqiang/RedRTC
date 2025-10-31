#pragma once

#ifndef ROOM_H
#define ROOM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct client_s;
typedef struct client_s client_t;

#define MAX_PARTICIPANTS 0x6

typedef enum {
    ROOM_STATE_ACTIVE = 0x0,
    ROOM_STATE_EMPTY,
    ROOM_STATE_CLOSING
} room_state_t;

typedef struct participant_s {
    client_t *client;
    uint32_t join_time;
    bool is_owner;
} participant_t;

typedef struct room_s {
    char id[37];                    /* 房间 UUID (36 字符 + 空终止符) */
    char name[64];                  /* 人类可读的房间名称 */
    participant_t participants[MAX_PARTICIPANTS]; /* 容纳 6 个参与者的固定数组 */
    uint8_t participant_count;      /* 当前参与者数量 */
    room_state_t state;             /* 房间状态 */
    uint32_t created_at;           /* 创建时间戳 */
    uint32_t last_activity;        /* 最后活动时间戳 */
    client_t *owner;               /* 房间所有者/创建者 */
} room_t;

typedef struct room_registry_s {
    room_t *rooms;                 /* 房间数组 */
    size_t max_rooms;              /* 允许的最大房间数 */
    size_t active_rooms;           /* 当前活跃房间数 */
    uint64_t total_rooms_created;  /* 创建的房间总数 (统计) */
} room_registry_t;

/**
 * @brief 初始化房间结构体
 * @param room 指向要初始化房间的指针
 * @param name 房间名称
 * @param owner 房间创建者
 */
void room_init(room_t *room, const char *name, client_t *owner);

/**
 * @brief 清理房间资源
 * @param room 指向要清理房间的指针
 */
void room_cleanup(room_t *room);

/**
 * @brief 添加参与者到房间
 * @param room 房间指针
 * @param client 要添加的客户端
 * @return 成功返回 0x0，房间已满返回 -1，客户端已在房间中返回 -2
 */
int room_add_participant(room_t *room, client_t *client);

/**
 * @brief 从房间中移除参与者
 * @param room 房间指针
 * @param client 要移除的客户端
 * @return 成功返回 0x0，未找到客户端返回 -1
 */
int room_remove_participant(room_t *room, client_t *client);

/**
 * @brief 检查房间是否已满 (6 个参与者)
 * @param room 要检查的房间
 * @return 如果房间已满则返回 true
 */
bool room_is_full(const room_t *room);

/**
 * @brief 检查房间是否为空
 * @param room 要检查的房间
 * @return 如果房间没有参与者则返回 true
 */
bool room_is_empty(const room_t *room);

/**
 * @brief 通过客户端 ID 查找参与者
 * @param room 要搜索的房间
 * @param client_id 要查找的客户端 ID
 * @return 如果找到则返回客户端指针，否则返回 NULL
 */
client_t *room_find_participant(const room_t *room, const char *client_id);

/**
 * @brief 向所有房间参与者广播消息，除了发送者
 * @param room 要广播的房间
 * @param exclude 要排除的客户端 (通常是发送者)
 * @param event 事件类型
 * @param data JSON 数据字符串
 * @return 消息发送到的客户端数量
 */
int room_broadcast_message(room_t *room, client_t *exclude, 
                          const char *event, const char *data);

/**
 * @brief 初始化房间注册表
 * @param reg 要初始化的注册表
 * @param max_rooms 最大房间容量
 * @return 成功返回 0x0，内存分配失败返回 -1
 */
int room_registry_init(room_registry_t *reg, size_t max_rooms);

/**
 * @brief 清理房间注册表和所有房间
 * @param reg 要清理的注册表
 */
void room_registry_cleanup(room_registry_t *reg);

/**
 * @brief 在注册表中创建新房间
 * @param reg 房间注册表
 * @param name 房间名称
 * @param owner 房间创建者
 * @return 指向新房间的指针，如果注册表已满则返回 NULL
 */
room_t *room_registry_create(room_registry_t *reg, const char *name, client_t *owner);

/**
 * @brief 通过 ID 查找房间
 * @param reg 房间注册表
 * @param room_id 要查找的房间 ID
 * @return 如果找到则返回房间指针，否则返回 NULL
 */
room_t *room_registry_find_by_id(room_registry_t *reg, const char *room_id);

/**
 * @brief 通过客户端查找房间
 * @param reg 房间注册表
 * @param client 要查找房间的客户端
 * @return 如果客户端在房间中则返回房间指针，否则返回 NULL
 */
room_t *room_registry_find_by_client(room_registry_t *reg, const client_t *client);

/**
 * @brief 从注册表中移除所有空房间
 * @param reg 房间注册表
 */
void room_registry_remove_empty_rooms(room_registry_t *reg);

/**
 * @brief 获取活跃房间的数量
 * @param reg 房间注册表
 * @return 活跃房间的数量
 */
size_t room_registry_get_active_count(const room_registry_t *reg);

#endif