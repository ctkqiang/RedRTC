/**
 * @file client.c
 * @brief 客户端管理模块的实现文件，包括客户端的初始化、清理、活动状态更新、消息发送以及客户端注册表的管理。
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../include/messages.h"
#include "../include/utilities.h"
#include "../include/client.h"

/**
 * @brief 初始化客户端结构体。
 * @param client 指向 client_t 结构体的指针。
 * @param wsi 指向 libwebsockets 实例的指针。
 */
void client_init(client_t *client, struct lws *wsi) {
    memset(client, 0, sizeof(client_t));
    generate_uuid(client->id, sizeof(client->id));
    client->wsi = wsi;
    client->state = CLIENT_STATE_CONNECTED;
    client->connect_time = get_timestamp_sec();
    client->last_activity = client->connect_time;
    client->is_alive = true;
}

/**
 * @brief 清理客户端资源。
 * @param client 指向 client_t 结构体的指针。
 */
void client_cleanup(client_t *client) {
    client->is_alive = false;
    client->state = CLIENT_STATE_DISCONNECTING;
}

/**
 * @brief 更新客户端的最后活动时间。
 * @param client 指向 client_t 结构体的指针。
 */
void client_update_activity(client_t *client) {
    client->last_activity = get_timestamp_sec();
}

/**
 * @brief 检查客户端是否超时。
 * @param client 指向 client_t 结构体的常量指针。
 * @param timeout_sec 超时时间，单位为秒。
 * @return 如果客户端超时则返回 true，否则返回 false。
 */
bool client_is_timed_out(const client_t *client, uint32_t timeout_sec) {
    return (get_timestamp_sec() - client->last_activity) > timeout_sec;
}

/**
 * @brief 向客户端发送消息。
 * @param client 指向 client_t 结构体的指针。
 * @param event 消息事件名称。
 * @param data 消息数据 (JSON 格式字符串)，可为 NULL。
 * @return 成功发送的字节数，或负数表示错误。
 */
int client_send_message(client_t *client, const char *event, const char *data) {
    if (!client->is_alive || !client->wsi) return -1;
    
    message_t *msg = message_create(event, data ? json_string(data) : NULL);
    if (!msg) return -2;
    
    char *json_str = message_serialize(msg);
    message_unref(msg);
    
    if (!json_str) return -3;
    
    size_t len = strlen(json_str);
    unsigned char *buf = malloc(LWS_PRE + len + 1);
    if (!buf) {
        free(json_str);
        return -4;
    }
    
    memcpy(buf + LWS_PRE, json_str, len);
    buf[LWS_PRE + len] = '\0';
    
    int ret = lws_write(client->wsi, buf + LWS_PRE, len, LWS_WRITE_TEXT);
    
    free(json_str);
    free(buf);
    
    if (ret > 0) {
        client->messages_sent++;
    }
    
    return ret;
}

/**
 * @brief 初始化客户端注册表。
 * @param reg 指向 client_registry_t 结构体的指针。
 * @param max_clients 注册表能容纳的最大客户端数量。
 * @return 0 表示成功，-1 表示内存分配失败。
 */
int client_registry_init(client_registry_t *reg, size_t max_clients) {
    reg->clients = calloc(max_clients, sizeof(client_t));
    if (!reg->clients) return -1;
    
    reg->max_clients = max_clients;
    reg->active_count = 0;
    reg->total_connections = 0;
    
    return 0;
}

/**
 * @brief 清理客户端注册表资源。
 * @param reg 指向 client_registry_t 结构体的指针。
 */
void client_registry_cleanup(client_registry_t *reg) {
    if (reg->clients) {
        free(reg->clients);
        reg->clients = NULL;
    }
    reg->max_clients = 0;
    reg->active_count = 0;
}

/**
 * @brief 向客户端注册表添加一个新客户端。
 * @param reg 指向 client_registry_t 结构体的指针。
 * @param wsi 指向 libwebsockets 实例的指针。
 * @return 指向新添加客户端的指针，如果注册表已满则返回 NULL。
 */
client_t *client_registry_add(client_registry_t *reg, struct lws *wsi) {
    for (size_t i = 0; i < reg->max_clients; i++) {
        if (!reg->clients[i].is_alive) {
            client_init(&reg->clients[i], wsi);
            reg->active_count++;
            reg->total_connections++;
            return &reg->clients[i];
        }
    }
    return NULL;
}

/**
 * @brief 从客户端注册表移除一个客户端。
 * @param reg 指向 client_registry_t 结构体的指针。
 * @param client 指向要移除的 client_t 结构体的指针。
 */
void client_registry_remove(client_registry_t *reg, client_t *client) {
    if (client && client->is_alive) {
        client_cleanup(client);
        reg->active_count--;
    }
}

/**
 * @brief 根据 libwebsockets 实例查找客户端。
 * @param reg 指向 client_registry_t 结构体的指针。
 * @param wsi 指向 libwebsockets 实例的指针。
 * @return 指向找到的客户端的指针，如果未找到则返回 NULL。
 */
client_t *client_registry_find_by_wsi(client_registry_t *reg, struct lws *wsi) {
    for (size_t i = 0; i < reg->max_clients; i++) {
        if (reg->clients[i].is_alive && reg->clients[i].wsi == wsi) {
            return &reg->clients[i];
        }
    }
    return NULL;
}

/**
 * @brief 获取当前活跃客户端的数量。
 * @param reg 指向 client_registry_t 结构体的常量指针。
 * @return 活跃客户端的数量。
 */
size_t client_registry_get_active_count(const client_registry_t *reg) {
    return reg->active_count;
}