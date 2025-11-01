// 防止头文件被重复包含
#pragma once

// 如果 SERVER_H 未定义，则定义它，防止重复编译
#ifndef SERVER_H
#define SERVER_H

// 引入 libwebsockets 库，用于 WebSocket 通信
#include <libwebsockets.h>
// 引入标准整数类型定义，如 uint32_t, uint64_t
#include <stdint.h>
// 引入布尔类型定义，如 bool
#include <stdbool.h>

// 引入项目内部的头文件
#include "client.h"   // 客户端相关定义
#include "room.h"     // 房间相关定义
#include "messages.h" // 消息相关定义

// 服务器配置结构体
typedef struct server_config_s {
    int port;                   // 服务器监听端口
    size_t max_clients;         // 最大客户端连接数
    size_t max_rooms;           // 最大房间数
    uint32_t client_timeout_sec; // 客户端超时时间 (秒)
    bool enable_stats;          // 是否启用统计功能
    const char *interface;      // 监听的网络接口 (例如 "eth0" 或 NULL)
} server_config_t;

// 服务器上下文结构体，包含服务器运行所需的所有状态和数据
typedef struct server_context_s {
    struct lws_context *lws_context; // libwebsockets 上下文对象
    client_registry_t clients;       // 客户端注册表
    room_registry_t rooms;           // 房间注册表
    message_queue_t msg_queue;       // 消息队列
    server_config_t config;          // 服务器配置
    
    // 统计信息
    uint64_t total_messages;    // 总消息数
    uint64_t total_errors;      // 总错误数
    uint64_t startup_time;      // 服务器启动时间
    
    // 线程相关
    pthread_t processing_thread; // 消息处理线程
    bool running;                // 服务器运行状态标志
} server_context_t;

// 服务器 API 函数声明

// 初始化服务器上下文
// ctx: 服务器上下文指针
// config: 服务器配置指针
// 返回值: 0 成功，非 0 失败
int server_init(server_context_t *ctx, const server_config_t *config);

// 运行服务器主循环
// ctx: 服务器上下文指针
// 返回值: 0 正常退出，非 0 异常退出
int server_run(server_context_t *ctx);

// 清理服务器资源
// ctx: 服务器上下文指针
void server_cleanup(server_context_t *ctx);

// 停止服务器运行
// ctx: 服务器上下文指针
void server_stop(server_context_t *ctx);

// WebSocket 协议回调函数
// wsi: WebSocket 连接会话信息
// reason: 回调原因 (事件类型)
// user: 用户自定义数据
// in: 输入数据
// len: 输入数据长度
// 返回值: 0 成功，非 0 失败
int webrtc_protocol_callback(struct lws *wsi, enum lws_callback_reasons reason,
                            void *user, void *in, size_t len);

// 消息处理函数

// 处理客户端发送的消息
// ctx: 服务器上下文指针
// wsi: WebSocket 连接会话信息
// event: 事件类型
// data: JSON 格式的消息数据
void process_client_message(server_context_t *ctx, struct lws *wsi,
                           const char *event, json_t *data);

// 房间消息处理器

// 处理客户端加入房间请求
// ctx: 服务器上下文指针
// client: 客户端信息指针
// data: 包含房间信息的 JSON 数据
void handle_join_room(server_context_t *ctx, client_t *client, json_t *data);

// 处理客户端离开房间请求
// ctx: 服务器上下文指针
// client: 客户端信息指针
void handle_leave_room(server_context_t *ctx, client_t *client);

// 处理客户端发送的 Offer 消息 (WebRTC SDP Offer)
// ctx: 服务器上下文指针
// client: 客户端信息指针
// data: 包含 Offer 信息的 JSON 数据
void handle_offer_message(server_context_t *ctx, client_t *client, json_t *data);

// 处理客户端发送的 Answer 消息 (WebRTC SDP Answer)
// ctx: 服务器上下文指针
// client: 客户端信息指针
// data: 包含 Answer 信息的 JSON 数据
void handle_answer_message(server_context_t *ctx, client_t *client, json_t *data);

// 处理客户端发送的 ICE Candidate 消息 (WebRTC ICE 候选者)
// ctx: 服务器上下文指针
// client: 客户端信息指针
// data: 包含 ICE Candidate 信息的 JSON 数据
void handle_ice_candidate(server_context_t *ctx, client_t *client, json_t *data);

#endif // SERVER_H