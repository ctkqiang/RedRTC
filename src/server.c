#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>

#include "../include/server.h"

static server_context_t *global_ctx = NULL;

static void signal_handler(int sig) {
    if (global_ctx) {
        printf("\nReceived signal %d, shutting down...\n", sig);
        server_stop(global_ctx);
    }
}

int server_init(server_context_t *ctx, const server_config_t *config) {
    if (!ctx || !config) return -1;
    
    memcpy(&ctx->config, config, sizeof(server_config_t));
    ctx->running = false;
    ctx->total_messages = 0;
    ctx->total_errors = 0;
    ctx->startup_time = get_timestamp_sec();
    
    /* Initialize client registry */
    if (client_registry_init(&ctx->clients, config->max_clients) != 0) {
        fprintf(stderr, "Failed to initialize client registry\n");
        return -2;
    }
    
    /* Initialize room registry */
    if (room_registry_init(&ctx->rooms, config->max_rooms) != 0) {
        fprintf(stderr, "Failed to initialize room registry\n");
        client_registry_cleanup(&ctx->clients);
        return -3;
    }
    
    /* Initialize message queue */
    if (message_queue_init(&ctx->msg_queue, 1024) != 0) {
        fprintf(stderr, "Failed to initialize message queue\n");
        room_registry_cleanup(&ctx->rooms);
        client_registry_cleanup(&ctx->clients);
        return -4;
    }
    
    /* Setup libwebsockets context */
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = config->port;
    info.iface = config->interface;
    info.protocols = (struct lws_protocols[]){
        {
            "webrtc-signaling",
            webrtc_protocol_callback,
            sizeof(void*), /* per_session_data_size */
            4096, /* rx buffer size */
            0, /* id */
            NULL, /* user */
            0 /* tx_packet_size */
        },
        { NULL, NULL, 0, 0, 0, NULL, 0 } /* terminator */
    };
    info.gid = -1;
    info.uid = -1;
    info.user = ctx;
    
    ctx->lws_context = lws_create_context(&info);
    if (!ctx->lws_context) {
        fprintf(stderr, "Failed to create libwebsockets context\n");
        message_queue_cleanup(&ctx->msg_queue);
        room_registry_cleanup(&ctx->rooms);
        client_registry_cleanup(&ctx->clients);
        return -5;
    }
    
    /* Setup signal handlers */
    global_ctx = ctx;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("WebRTC Signaling Server initialized\n");
    printf("  Port: %d\n", config->port);
    printf("  Max clients: %zu\n", config->max_clients);
    printf("  Max rooms: %zu\n", config->max_rooms);
    printf("  Client timeout: %u seconds\n", config->client_timeout_sec);
    
    return 0;
}

int server_run(server_context_t *ctx) {
    if (!ctx || !ctx->lws_context) return -1;
    
    ctx->running = true;
    printf("Server starting...\n");
    
    while (ctx->running) {
        lws_service(ctx->lws_context, 50); /* 50ms timeout */
        
        /* Process any queued messages */
        ws_message_t msg;
        while (message_queue_pop(&ctx->msg_queue, &msg) == 0) {
            process_client_message(ctx, msg.wsi, 
                                 msg.message->event, 
                                 msg.message->data);
            message_unref(msg.message);
        }
        
        /* Cleanup timed out clients (every 10 seconds) */
        static uint32_t last_cleanup = 0;
        uint32_t now = get_timestamp_sec();
        if (now - last_cleanup >= 10) {
            for (size_t i = 0; i < ctx->clients.max_clients; i++) {
                client_t *client = &ctx->clients.clients[i];
                if (client->is_alive && 
                    client_is_timed_out(client, ctx->config.client_timeout_sec)) {
                    printf("Client %s timed out\n", client->id);
                    client_registry_remove(&ctx->clients, client);
                }
            }
            
            /* Remove empty rooms */
            room_registry_remove_empty_rooms(&ctx->rooms);
            last_cleanup = now;
        }
    }
    
    return 0;
}

void server_cleanup(server_context_t *ctx) {
    if (!ctx) return;
    
    printf("Cleaning up server...\n");
    
    if (ctx->lws_context) {
        lws_context_destroy(ctx->lws_context);
        ctx->lws_context = NULL;
    }
    
    message_queue_cleanup(&ctx->msg_queue);
    room_registry_cleanup(&ctx->rooms);
    client_registry_cleanup(&ctx->clients);
    
    printf("Server cleanup completed\n");
}

void server_stop(server_context_t *ctx) {
    if (ctx) {
        ctx->running = false;
    }
}

int webrtc_protocol_callback(struct lws *wsi, enum lws_callback_reasons reason,
                            void *user, void *in, size_t len) {
    server_context_t *ctx = (server_context_t*)lws_context_user(lws_get_context(wsi));
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            client_t *client = client_registry_add(&ctx->clients, wsi);
            if (client) {
                /* Send client their ID */
                json_t *data = json_object();
                json_object_set_new(data, "clientId", json_string(client->id));
                client_send_message(client, EVENT_CLIENT_ID, json_dumps(data, 0));
                json_decref(data);
            }
            break;
        }
        
        case LWS_CALLBACK_RECEIVE: {
            client_t *client = client_registry_find_by_wsi(&ctx->clients, wsi);
            if (client) {
                client_update_activity(client);
                
                message_t *msg = message_deserialize((const char*)in);
                if (msg) {
                    message_queue_push(&ctx->msg_queue, wsi, msg);
                    message_unref(msg); /* Queue took reference */
                } else {
                    ctx->total_errors++;
                }
            }
            break;
        }
        
        case LWS_CALLBACK_CLOSED: {
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

void process_client_message(server_context_t *ctx, struct lws *wsi, 
                           const char *event, json_t *data) {
    client_t *client = client_registry_find_by_wsi(&ctx->clients, wsi);
    if (!client) return;
    
    ctx->total_messages++;
    
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
        fprintf(stderr, "Unknown event: %s\n", event);
        ctx->total_errors++;
    }
}

void handle_join_room(server_context_t *ctx, client_t *client, json_t *data) {
    const char *room_id = NULL;
    const char *room_name = "Unnamed Room";
    
    if (data) {
        json_t *room_id_json = json_object_get(data, "roomId");
        json_t *room_name_json = json_object_get(data, "roomName");
        
        room_id = room_id_json ? json_string_value(room_id_json) : NULL;
        room_name = room_name_json ? json_string_value(room_name_json) : room_name;
    }
    
    /* Leave current room if any */
    handle_leave_room(ctx, client);
    
    /* Find or create room */
    room_t *room = NULL;
    if (room_id) {
        room = room_registry_find_by_id(&ctx->rooms, room_id);
    }
    
    if (!room) {
        room = room_registry_create(&ctx->rooms, room_name, client);
        if (!room) {
            client_send_message(client, EVENT_ERROR, "Cannot create room");
            return;
        }
        
        /* Notify room creator */
        json_t *room_data = json_object();
        json_object_set_new(room_data, "roomId", json_string(room->id));
        json_object_set_new(room_data, "roomName", json_string(room->name));
        client_send_message(client, EVENT_ROOM_CREATED, json_dumps(room_data, 0));
        json_decref(room_data);
    }
    
    /* Join room */
    if (room_add_participant(room, client) != 0) {
        client_send_message(client, EVENT_ERROR, "Room is full (max 6 participants)");
        return;
    }
    
    /* Send participants list to all room members */
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

void handle_leave_room(server_context_t *ctx, client_t *client) {
    if (client->room) {
        room_t *room = client->room;
        room_remove_participant(room, client);
        
        /* Update remaining participants */
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

void handle_offer_message(server_context_t *ctx, client_t *client, json_t *data) {
    if (!client->room) {
        client_send_message(client, EVENT_ERROR, "Not in a room");
        return;
    }
    
    const char *target_client_id = NULL;
    json_t *target_json = json_object_get(data, "targetClientId");
    if (target_json) {
        target_client_id = json_string_value(target_json);
    }
    
    if (!target_client_id) {
        client_send_message(client, EVENT_ERROR, "Missing target client ID");
        return;
    }
    
    client_t *target = room_find_participant(client->room, target_client_id);
    if (!target) {
        client_send_message(client, EVENT_ERROR, "Target client not found in room");
        return;
    }
    
    /* Forward offer to target */
    json_t *offer_data = json_object();
    json_object_set_new(offer_data, "fromClientId", json_string(client->id));
    json_object_set_new(offer_data, "offer", json_incref(json_object_get(data, "offer")));
    
    client_send_message(target, EVENT_OFFER, json_dumps(offer_data, 0));
    json_decref(offer_data);
}

void handle_answer_message(server_context_t *ctx, client_t *client, json_t *data) {
    if (!client->room) {
        client_send_message(client, EVENT_ERROR, "Not in a room");
        return;
    }
    
    const char *target_client_id = NULL;
    json_t *target_json = json_object_get(data, "targetClientId");
    if (target_json) {
        target_client_id = json_string_value(target_json);
    }
    
    if (!target_client_id) {
        client_send_message(client, EVENT_ERROR, "Missing target client ID");
        return;
    }
    
    client_t *target = room_find_participant(client->room, target_client_id);
    if (!target) {
        client_send_message(client, EVENT_ERROR, "Target client not found in room");
        return;
    }
    
    /* Forward answer to target */
    json_t *answer_data = json_object();
    json_object_set_new(answer_data, "fromClientId", json_string(client->id));
    json_object_set_new(answer_data, "answer", json_incref(json_object_get(data, "answer")));
    
    client_send_message(target, EVENT_ANSWER, json_dumps(answer_data, 0));
    json_decref(answer_data);
}

void handle_ice_candidate(server_context_t *ctx, client_t *client, json_t *data) {
    if (!client->room) {
        client_send_message(client, EVENT_ERROR, "Not in a room");
        return;
    }
    
    const char *target_client_id = NULL;
    json_t *target_json = json_object_get(data, "targetClientId");
    if (target_json) {
        target_client_id = json_string_value(target_json);
    }
    
    if (!target_client_id) {
        client_send_message(client, EVENT_ERROR, "Missing target client ID");
        return;
    }
    
    client_t *target = room_find_participant(client->room, target_client_id);
    if (!target) {
        client_send_message(client, EVENT_ERROR, "Target client not found in room");
        return;
    }
    
    /* Forward ICE candidate to target */
    json_t *candidate_data = json_object();
    json_object_set_new(candidate_data, "fromClientId", json_string(client->id));
    json_object_set_new(candidate_data, "candidate", json_incref(json_object_get(data, "candidate")));
    
    client_send_message(target, EVENT_ICE_CANDIDATE, json_dumps(candidate_data, 0));
    json_decref(candidate_data);
}