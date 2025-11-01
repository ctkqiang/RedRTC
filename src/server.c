#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>

#include "../include/server.h"
#include "../include/utilities.h"

/* 全局服务器上下文指针，用于信号处理 */
static server_context_t *global_ctx = NULL;

/* 信号处理函数，处理服务器关闭信号 */
static void signal_handler(int sig) {
    if (global_ctx) {
        printf("\n收到信号 %d，正在关闭服务器...\n", sig);
        server_stop(global_ctx);
    }
}

/* 服务器初始化函数 */
int server_init(server_context_t *ctx, const server_config_t *config) {
    if (!ctx || !config) return -1;
    
    /* 复制配置信息并初始化服务器状态 */
    memcpy(&ctx->config, config, sizeof(server_config_t));
    ctx->running = false;
    ctx->total_messages = 0;
    ctx->total_errors = 0;
    ctx->startup_time = get_timestamp_sec();
    
    /* 初始化客户端注册表 */
    if (client_registry_init(&ctx->clients, config->max_clients) != 0) {
        fprintf(stderr, "客户端注册表初始化失败\n");
        return -2;
    }
    
    /* 初始化房间注册表 */
    if (room_registry_init(&ctx->rooms, config->max_rooms) != 0) {
        fprintf(stderr, "房间注册表初始化失败\n");
        client_registry_cleanup(&ctx->clients);
        return -3;
    }
    
    /* 初始化消息队列 */
    if (message_queue_init(&ctx->msg_queue, 1024) != 0) {
        fprintf(stderr, "消息队列初始化失败\n");
        room_registry_cleanup(&ctx->rooms);
        client_registry_cleanup(&ctx->clients);
        return -4;
    }
    
    /* 设置 libwebsockets 上下文 */
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = config->port;
    info.iface = config->interface;
    info.protocols = (struct lws_protocols[]){
        {
            "webrtc-signaling",
            webrtc_protocol_callback,
            sizeof(void*), /* 每个会话数据大小 */
            4096, /* 接收缓冲区大小 */
            0, /* ID */
            NULL, /* 用户数据 */
            0 /* 发送包大小 */
        },
        { NULL, NULL, 0, 0, 0, NULL, 0 } /* 终止符 */
    };
    info.gid = -1;
    info.uid = -1;
    info.user = ctx;
    
    /* 创建 libwebsockets 上下文 */
    ctx->lws_context = lws_create_context(&info);
    if (!ctx->lws_context) {
        fprintf(stderr, "创建 libwebsockets 上下文失败\n");
        message_queue_cleanup(&ctx->msg_queue);
        room_registry_cleanup(&ctx->rooms);
        client_registry_cleanup(&ctx->clients);
        return -5;
    }
    
    /* 设置信号处理器 */
    global_ctx = ctx;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("WebRTC 信令服务器初始化完成\n");
    printf("  端口: %d\n", config->port);
    printf("  最大客户端数: %zu\n", config->max_clients);
    printf("  最大房间数: %zu\n", config->max_rooms);
    printf("  客户端超时时间: %u 秒\n", config->client_timeout_sec);
    
    return 0;
}

/* 服务器运行函数 */
int server_run(server_context_t *ctx) {
    if (!ctx || !ctx->lws_context) return -1;
    
    ctx->running = true;
    printf("服务器正在启动...\n");
    
    while (ctx->running) {
        lws_service(ctx->lws_context, 50); /* 50毫秒超时 */
        
        /* 处理队列中的消息 */
        ws_message_t msg;
        while (message_queue_pop(&ctx->msg_queue, &msg) == 0) {
            process_client_message(ctx, msg.wsi, 
                                 msg.message->event, 
                                 msg.message->data);
            message_unref(msg.message);
        }
        
        /* 每10秒清理超时客户端 */
        static uint32_t last_cleanup = 0;
        uint32_t now = get_timestamp_sec();
        if (now - last_cleanup >= 10) {
            for (size_t i = 0; i < ctx->clients.max_clients; i++) {
                client_t *client = &ctx->clients.clients[i];
                if (client->is_alive && 
                    client_is_timed_out(client, ctx->config.client_timeout_sec)) {
                    printf("客户端 %s 超时\n", client->id);
                    client_registry_remove(&ctx->clients, client);
                }
            }
            
            /* 移除空房间 */
            room_registry_remove_empty_rooms(&ctx->rooms);
            last_cleanup = now;
        }
    }
    
    return 0;
}

/* 服务器清理函数 */
void server_cleanup(server_context_t *ctx) {
    if (!ctx) return;
    
    printf("正在清理服务器...\n");
    
    if (ctx->lws_context) {
        lws_context_destroy(ctx->lws_context);
        ctx->lws_context = NULL;
    }
    
    message_queue_cleanup(&ctx->msg_queue);
    room_registry_cleanup(&ctx->rooms);
    client_registry_cleanup(&ctx->clients);
    
    printf("服务器清理完成\n");
}

/* 服务器停止函数 */
void server_stop(server_context_t *ctx) {
    if (ctx) {
        ctx->running = false;
    }
}

/* WebRTC 协议回调函数 */
int webrtc_protocol_callback(struct lws *wsi, enum lws_callback_reasons reason,
                            void *user, void *in, size_t len) {
    server_context_t *ctx = (server_context_t*)lws_context_user(lws_get_context(wsi));
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            /* 客户端连接建立 */
            client_t *client = client_registry_add(&ctx->clients, wsi);
            if (client) {
                /* 发送客户端ID */
                json_t *data = json_object();
                json_object_set_new(data, "clientId", json_string(client->id));
                client_send_message(client, EVENT_CLIENT_ID, json_dumps(data, 0));
                json_decref(data);
            }
            break;
        }
        
        case LWS_CALLBACK_RECEIVE: {
            /* 接收客户端消息 */
            client_t *client = client_registry_find_by_wsi(&ctx->clients, wsi);
            if (client) {
                client_update_activity(client);
                
                message_t *msg = message_deserialize((const char*)in);
                if (msg) {
                    message_queue_push(&ctx->msg_queue, wsi, msg);
                    message_unref(msg); /* 队列已获取引用 */
                } else {
                    ctx->total_errors++;
                }
            }
            break;
        }
        
        case LWS_CALLBACK_CLOSED: {
            /* 客户端连接关闭 */
            client_t *client = client_registry_find_by_wsi(&ctx->clients, wsi);
            if (client) {
                handle_leave_room(ctx, client);
                client_registry_remove(&ctx->clients, client);
            }
            break;
        }
        
        default:
            break;
    }
    
    return 0;
}

/* 处理客户端消息函数 */
void process_client_message(server_context_t *ctx, struct lws *wsi, 
                           const char *event, json_t *data) {
    client_t *client = client_registry_find_by_wsi(&ctx->clients, wsi);
    if (!client) return;
    
    ctx->total_messages++;
    
    /* 根据事件类型处理消息 */
    if (strcmp(event, EVENT_JOIN_ROOM) == 0) {
        handle_join_room(ctx, client, data);
    } else if (strcmp(event, EVENT_LEAVE_ROOM) == 0) {
        handle_leave_room(ctx, client);
    } else if (strcmp(event, EVENT_OFFER) == 0) {
        handle_offer_message(ctx, client, data);
    } else if (strcmp(event, EVENT_ANSWER) == 0) {
        handle_answer_message(ctx, client, data);
    } else if (strcmp(event, EVENT_ICE_CANDIDATE) == 0) {
        handle_ice_candidate(ctx, client, data);
    } else {
        fprintf(stderr, "未知事件: %s\n", event);
        ctx->total_errors++;
    }
}

/* 处理加入房间请求 */
void handle_join_room(server_context_t *ctx, client_t *client, json_t *data) {
    const char *room_id = NULL;
    const char *room_name = "未命名房间";
    
    if (data) {
        json_t *room_id_json = json_object_get(data, "roomId");
        json_t *room_name_json = json_object_get(data, "roomName");
        
        room_id = room_id_json ? json_string_value(room_id_json) : NULL;
        room_name = room_name_json ? json_string_value(room_name_json) : room_name;
    }
    
    /* 离开当前房间（如果有） */
    handle_leave_room(ctx, client);
    
    /* 查找或创建房间 */
    room_t *room = NULL;
    if (room_id) {
        room = room_registry_find_by_id(&ctx->rooms, room_id);
    }
    
    if (!room) {
        room = room_registry_create(&ctx->rooms, room_name, client);
        if (!room) {
            client_send_message(client, EVENT_ERROR, "无法创建房间");
            return;
        }
        
        /* 通知房间创建者 */
        json_t *room_data = json_object();
        json_object_set_new(room_data, "roomId", json_string(room->id));
        json_object_set_new(room_data, "roomName", json_string(room->name));
        client_send_message(client, EVENT_ROOM_CREATED, json_dumps(room_data, 0));
        json_decref(room_data);
    }
    
    /* 加入房间 */
    if (room_add_participant(room, client) != 0) {
        client_send_message(client, EVENT_ERROR, "房间已满（最多6名参与者）");
        return;
    }
    
    /* 向所有房间成员发送参与者列表 */
    json_t *participants = json_array();
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        if (room->participants[i].client) {
            json_array_append_new(participants, 
                                json_string(room->participants[i].client->id));
        }
    }
    
    json_t *participants_data = json_object();
    json_object_set_new(participants_data, "roomId", json_string(room->id));
    json_object_set_new(participants_data, "participants", participants);
    
    room_broadcast_message(room, NULL, EVENT_PARTICIPANTS_LIST, 
                          json_dumps(participants_data, 0));
    json_decref(participants_data);
}

/* 处理离开房间请求 */
void handle_leave_room(server_context_t *ctx, client_t *client) {
    if (client->room) {
        room_t *room = client->room;
        room_remove_participant(room, client);
        
        /* 更新剩余参与者 */
        if (!room_is_empty(room)) {
            json_t *participants = json_array();
            for (int i = 0; i < MAX_PARTICIPANTS; i++) {
                if (room->participants[i].client) {
                    json_array_append_new(participants, 
                                        json_string(room->participants[i].client->id));
                }
            }
            
            json_t *participants_data = json_object();
            json_object_set_new(participants_data, "roomId", json_string(room->id));
            json_object_set_new(participants_data, "participants", participants);
            
            room_broadcast_message(room, NULL, EVENT_PARTICIPANTS_LIST, 
                                  json_dumps(participants_data, 0));
            json_decref(participants_data);
        }
    }
}

/* 处理 WebRTC Offer 消息 */
void handle_offer_message(server_context_t *ctx, client_t *client, json_t *data) {
    if (!client->room) {
        client_send_message(client, EVENT_ERROR, "未在房间中");
        return;
    }
    
    const char *target_client_id = NULL;
    json_t *target_json = json_object_get(data, "targetClientId");
    if (target_json) {
        target_client_id = json_string_value(target_json);
    }
    
    if (!target_client_id) {
        client_send_message(client, EVENT_ERROR, "缺少目标客户端ID");
        return;
    }
    
    client_t *target = room_find_participant(client->room, target_client_id);
    if (!target) {
        client_send_message(client, EVENT_ERROR, "在房间中未找到目标客户端");
        return;
    }
    
    /* 转发 offer 到目标客户端 */
    json_t *offer_data = json_object();
    json_object_set_new(offer_data, "fromClientId", json_string(client->id));
    json_object_set_new(offer_data, "offer", json_incref(json_object_get(data, "offer")));
    
    client_send_message(target, EVENT_OFFER, json_dumps(offer_data, 0));
    json_decref(offer_data);
}

/* 处理 WebRTC Answer 消息 */
void handle_answer_message(server_context_t *ctx, client_t *client, json_t *data) {
    if (!client->room) {
        client_send_message(client, EVENT_ERROR, "未在房间中");
        return;
    }
    
    const char *target_client_id = NULL;
    json_t *target_json = json_object_get(data, "targetClientId");
    if (target_json) {
        target_client_id = json_string_value(target_json);
    }
    
    if (!target_client_id) {
        client_send_message(client, EVENT_ERROR, "缺少目标客户端ID");
        return;
    }
    
    client_t *target = room_find_participant(client->room, target_client_id);
    if (!target) {
        client_send_message(client, EVENT_ERROR, "在房间中未找到目标客户端");
        return;
    }
    
    /* 转发 answer 到目标客户端 */
    json_t *answer_data = json_object();
    json_object_set_new(answer_data, "fromClientId", json_string(client->id));
    json_object_set_new(answer_data, "answer", json_incref(json_object_get(data, "answer")));
    
    client_send_message(target, EVENT_ANSWER, json_dumps(answer_data, 0));
    json_decref(answer_data);
}

/* 处理 ICE Candidate 消息 */
void handle_ice_candidate(server_context_t *ctx, client_t *client, json_t *data) {
    if (!client->room) {
        client_send_message(client, EVENT_ERROR, "未在房间中");
        return;
    }
    
    const char *target_client_id = NULL;
    json_t *target_json = json_object_get(data, "targetClientId");
    if (target_json) {
        target_client_id = json_string_value(target_json);
    }
    
    if (!target_client_id) {
        client_send_message(client, EVENT_ERROR, "缺少目标客户端ID");
        return;
    }
    
    client_t *target = room_find_participant(client->room, target_client_id);
    if (!target) {
        client_send_message(client, EVENT_ERROR, "在房间中未找到目标客户端");
        return;
    }
    
    /* 转发 ICE candidate 到目标客户端 */
    json_t *candidate_data = json_object();
    json_object_set_new(candidate_data, "fromClientId", json_string(client->id));
    json_object_set_new(candidate_data, "candidate", json_incref(json_object_get(data, "candidate")));
    
    client_send_message(target, EVENT_ICE_CANDIDATE, json_dumps(candidate_data, 0));
    json_decref(candidate_data);
}