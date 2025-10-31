#pragma once

#ifndef ROOM_H
#define ROOM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct client_s;
typedef struct client_s client_t;

#define MAX_PARTICIPANTS 6

typedef enum {
    ROOM_STATE_ACTIVE = 0,
    ROOM_STATE_EMPTY,
    ROOM_STATE_CLOSING
} room_state_t;

typedef struct participant_s {
    client_t *client;
    uint32_t join_time;
    bool is_owner;
} participant_t;

typedef struct room_s {
    char id[37];
    char name[64];
    participant_t participants[MAX_PARTICIPANTS];
    uint8_t participant_count;
    room_state_t state;
    uint32_t created_at;
    uint32_t last_activity;
    client_t *owner;
} room_t;

void room_init(room_t *room, const char *name, client_t *owner);
void room_cleanup(room_t *room);
int room_add_participant(room_t *room, client_t *client);
int room_remove_participant(room_t *room, client_t *client);
bool room_is_full(const room_t *room);
bool room_is_empty(const room_t *room);
client_t *room_find_participant(const room_t *room, const char *client_id);
int room_broadcast_message(room_t *room, client_t *exclude, 
                          const char *event, const char *data);

typedef struct room_registry_s {
    room_t *rooms;
    size_t max_rooms;
    size_t active_rooms;
    uint64_t total_rooms_created;
} room_registry_t;

int room_registry_init(room_registry_t *reg, size_t max_rooms);

void room_registry_cleanup(room_registry_t *reg);

room_t *room_registry_create(room_registry_t *reg, const char *name, client_t *owner);
room_t *room_registry_find_by_id(room_registry_t *reg, const char *room_id);

void room_registry_remove_empty_rooms(room_registry_t *reg);

size_t room_registry_get_active_count(const room_registry_t *reg);

#endif