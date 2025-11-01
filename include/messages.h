#pragma once

#ifndef MESSAGES_H
#define MESSAGES_H

#include <jansson.h>
#include <libwebsockets.h>
#include <stdbool.h>

#define EVENT_CLIENT_ID         "client-id"
#define EVENT_JOIN_ROOM         "join-room"
#define EVENT_LEAVE_ROOM        "leave-room"
#define EVENT_OFFER             "offer"
#define EVENT_ANSWER            "answer"
#define EVENT_ICE_CANDIDATE     "ice-candidate"
#define EVENT_PARTICIPANTS_LIST "participants"
#define EVENT_ROOM_CREATED      "room-created"
#define EVENT_ERROR             "error"
#define EVENT_PONG              "pong"

typedef struct message_s {
    char *event;
    json_t *data;
    size_t ref_count; 
} message_t;


message_t *message_create(const char *event, json_t *data);

void message_ref(message_t *msg);
void message_unref(message_t *msg);

char *message_serialize(const message_t *msg);

message_t *message_deserialize(const char *json_str);
void message_destroy(message_t *msg);

typedef struct ws_message_s {
    struct lws *wsi;
    message_t *message;
    uint64_t timestamp;
} ws_message_t;

typedef struct message_queue_s {
    ws_message_t *messages;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    pthread_mutex_t lock;
} message_queue_t;

int message_queue_init(message_queue_t *queue, size_t capacity);
void message_queue_cleanup(message_queue_t *queue);

int message_queue_push(message_queue_t *queue, struct lws *wsi, message_t *msg);
int message_queue_pop(message_queue_t *queue, ws_message_t *result);

bool message_queue_is_empty(const message_queue_t *queue);
bool message_queue_is_full(const message_queue_t *queue);

#endif