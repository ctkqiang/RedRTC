#pragma once

#ifndef CLIENT_H
#define CLIENT_H

#include <libwebsockets.h>
#include <stdint.h>
#include <stdbool.h>

struct room_s;
typedef struct room_s room_t;

typedef enum {
    CLIENT_STATE_CONNECTED = 0,
    CLIENT_STATE_JOINING_ROOM,
    CLIENT_STATE_IN_ROOM,
    CLIENT_STATE_DISCONNECTING
} client_state_t;

typedef struct client_s {
    char id[37];                   /* UUID string (36 chars + null) */
    struct lws *wsi;               /* WebSocket connection */
    room_t *room;                  /* Joined room */
    client_state_t state;          /* Client State */
    uint32_t last_activity;        /* Last message timestamp */
    uint32_t connect_time;         /* Connection timestamp */
    uint64_t messages_sent;        /* Messages sent */
    uint64_t messages_received;    /* Messages received */
    bool is_alive;                 /* Connection health flag */
} client_t;

void client_init(client_t *client, struct lws *wsi);
void client_cleanup(client_t *client);
void client_update_activity(client_t *client);
bool client_is_timed_out(const client_t *client, uint32_t timeout_sec);

int client_send_message(client_t *client, const char *event, const char *data);

typedef struct client_registry_s {
    client_t *clients;
    size_t max_clients;
    size_t active_count;
    uint64_t total_connections;
} client_registry_t;

int client_registry_init(client_registry_t *reg, size_t max_clients);

void client_registry_cleanup(client_registry_t *reg);

client_t *client_registry_add(client_registry_t *reg, struct lws *wsi);

void client_registry_remove(client_registry_t *reg, client_t *client);

client_t *client_registry_find_by_wsi(client_registry_t *reg, struct lws *wsi);

size_t client_registry_get_active_count(const client_registry_t *reg);

#endif