#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <jansson.h>

#include "../include/room.h"
#include "../include/client.h"
#include "../include/messages.h"
#include "../include/utilities.h"

void room_init(room_t *room, const char *name, client_t *owner) {
    if (!room) return;
    
    /* Zero the entire structure */
    memset(room, 0, sizeof(room_t));
    
    /* Generate unique room ID */
    generate_uuid(room->id, sizeof(room->id));
    
    /* Copy room name safely */
    if (name) {
        safe_strncpy(room->name, name, sizeof(room->name));
    } else {
        safe_strncpy(room->name, "Unnamed Room", sizeof(room->name));
    }
    
    /* Set initial state */
    room->state = ROOM_STATE_ACTIVE;
    room->created_at = get_timestamp_sec();
    room->last_activity = room->created_at;
    room->owner = owner;
    
    /* Add owner as first participant if provided */
    if (owner) {
        room_add_participant(room, owner);
    }
    
    printf("Room initialized: %s (%s)\n", room->id, room->name);
}

void room_cleanup(room_t *room) {
    if (!room) return;
    
    printf("Cleaning up room: %s\n", room->id);
    
    /* Remove all participants from the room */
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        if (room->participants[i].client) {
            room->participants[i].client->room = NULL;
            room->participants[i].client->state = CLIENT_STATE_CONNECTED;
        }
    }
    
    room->participant_count = 0;
    room->state = ROOM_STATE_CLOSING;
    room->owner = NULL;
}

int room_add_participant(room_t *room, client_t *client) {
    if (!room || !client) {
        return -1;
    }
    
    /* Check if room is full */
    if (room_is_full(room)) {
        printf("Room %s is full, cannot add client %s\n", room->id, client->id);
        return -1;
    }
    
    /* Check if client is already in room */
    if (room_find_participant(room, client->id) != NULL) {
        printf("Client %s already in room %s\n", client->id, room->id);
        return -2;
    }
    
    /* Check if client is already in another room */
    if (client->room != NULL && client->room != room) {
        printf("Client %s is already in room %s\n", client->id, client->room->id);
        return -3;
    }
    
    /* Find empty slot in participants array */
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        if (room->participants[i].client == NULL) {
            /* Add client to room */
            room->participants[i].client = client;
            room->participants[i].join_time = get_timestamp_sec();
            room->participants[i].is_owner = (room->owner == client);
            room->participant_count++;
            room->last_activity = get_timestamp_sec();
            
            /* Update client state */
            client->room = room;
            client->state = CLIENT_STATE_IN_ROOM;
            
            printf("Client %s added to room %s (participants: %d/%d)\n", 
                   client->id, room->id, room->participant_count, MAX_PARTICIPANTS);
            return 0;
        }
    }
    
    /* This should never happen if room_is_full check passed */
    printf("CRITICAL: No empty slot found in room %s\n", room->id);
    return -4;
}

int room_remove_participant(room_t *room, client_t *client) {
    if (!room || !client) {
        return -1;
    }
    
    /* Find and remove client from room */
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        if (room->participants[i].client == client) {
            /* Clear participant slot */
            room->participants[i].client = NULL;
            room->participants[i].is_owner = false;
            room->participant_count--;
            room->last_activity = get_timestamp_sec();
            
            /* Update client state */
            client->room = NULL;
            client->state = CLIENT_STATE_CONNECTED;
            
            printf("Client %s removed from room %s (participants: %d/%d)\n", 
                   client->id, room->id, room->participant_count, MAX_PARTICIPANTS);
            
            /* Handle owner transfer if owner left */
            if (room->owner == client && room->participant_count > 0) {
                /* Find first available participant to become new owner */
                for (int j = 0; j < MAX_PARTICIPANTS; j++) {
                    if (room->participants[j].client != NULL) {
                        room->owner = room->participants[j].client;
                        room->participants[j].is_owner = true;
                        printf("Transferred room ownership to %s\n", room->owner->id);
                        break;
                    }
                }
            }
            
            return 0;
        }
    }
    
    printf("Client %s not found in room %s\n", client->id, room->id);
    return -1;
}

bool room_is_full(const room_t *room) {
    return room && room->participant_count >= MAX_PARTICIPANTS;
}

bool room_is_empty(const room_t *room) {
    return room && room->participant_count == 0;
}

client_t *room_find_participant(const room_t *room, const char *client_id) {
    if (!room || !client_id) {
        return NULL;
    }
    
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        if (room->participants[i].client != NULL && 
            strcmp(room->participants[i].client->id, client_id) == 0) {
            return room->participants[i].client;
        }
    }
    
    return NULL;
}

int room_broadcast_message(room_t *room, client_t *exclude, 
                          const char *event, const char *data) {
    if (!room || !event) {
        return 0;
    }
    
    int sent_count = 0;
    
    /* Send message to all participants except excluded client */
    for (int i = 0; i < MAX_PARTICIPANTS; i++) {
        client_t *client = room->participants[i].client;
        
        /* Skip if: no client, client is excluded, or client is not alive */
        if (client == NULL || client == exclude || !client->is_alive) {
            continue;
        }
        
        /* Send message and count successful sends */
        if (client_send_message(client, event, data) > 0) {
            sent_count++;
        }
    }
    
    /* Update room activity timestamp */
    room->last_activity = get_timestamp_sec();
    
    return sent_count;
}

int room_registry_init(room_registry_t *reg, size_t max_rooms) {
    if (!reg || max_rooms == 0) {
        return -1;
    }
    
    /* Allocate room array */
    reg->rooms = calloc(max_rooms, sizeof(room_t));
    if (!reg->rooms) {
        fprintf(stderr, "Failed to allocate room registry: %zu rooms\n", max_rooms);
        return -1;
    }
    
    /* Initialize registry state */
    reg->max_rooms = max_rooms;
    reg->active_rooms = 0;
    reg->total_rooms_created = 0;
    
    printf("Room registry initialized: %zu max rooms\n", max_rooms);
    return 0;
}

void room_registry_cleanup(room_registry_t *reg) {
    if (!reg) return;
    
    printf("Cleaning up room registry (%zu active rooms)\n", reg->active_rooms);
    
    /* Clean up all active rooms */
    if (reg->rooms) {
        for (size_t i = 0; i < reg->max_rooms; i++) {
            if (reg->rooms[i].state == ROOM_STATE_ACTIVE) {
                room_cleanup(&reg->rooms[i]);
            }
        }
        
        /* Free the room array */
        free(reg->rooms);
        reg->rooms = NULL;
    }
    
    reg->max_rooms = 0;
    reg->active_rooms = 0;
    reg->total_rooms_created = 0;
}

room_t *room_registry_create(room_registry_t *reg, const char *name, client_t *owner) {
    if (!reg || !reg->rooms || !name) {
        return NULL;
    }
    
    /* Find first available room slot */
    for (size_t i = 0; i < reg->max_rooms; i++) {
        if (reg->rooms[i].state != ROOM_STATE_ACTIVE) {
            /* Initialize the room */
            room_init(&reg->rooms[i], name, owner);
            reg->active_rooms++;
            reg->total_rooms_created++;
            
            printf("Room created in registry: %s (active: %zu/%zu)\n", 
                   reg->rooms[i].id, reg->active_rooms, reg->max_rooms);
            return &reg->rooms[i];
        }
    }
    
    printf("Room registry full: %zu/%zu rooms\n", reg->active_rooms, reg->max_rooms);
    return NULL;
}

room_t *room_registry_find_by_id(room_registry_t *reg, const char *room_id) {
    if (!reg || !reg->rooms || !room_id) {
        return NULL;
    }
    
    /* Linear search for room by ID */
    for (size_t i = 0; i < reg->max_rooms; i++) {
        if (reg->rooms[i].state == ROOM_STATE_ACTIVE && 
            strcmp(reg->rooms[i].id, room_id) == 0) {
            return &reg->rooms[i];
        }
    }
    
    return NULL;
}

room_t *room_registry_find_by_client(room_registry_t *reg, const client_t *client) {
    if (!reg || !reg->rooms || !client) {
        return NULL;
    }
    
    /* Search all rooms for this client */
    for (size_t i = 0; i < reg->max_rooms; i++) {
        if (reg->rooms[i].state == ROOM_STATE_ACTIVE) {
            for (int j = 0; j < MAX_PARTICIPANTS; j++) {
                if (reg->rooms[i].participants[j].client == client) {
                    return &reg->rooms[i];
                }
            }
        }
    }
    
    return NULL;
}

void room_registry_remove_empty_rooms(room_registry_t *reg) {
    if (!reg || !reg->rooms) return;
    
    size_t removed_count = 0;
    
    /* Find and remove empty rooms */
    for (size_t i = 0; i < reg->max_rooms; i++) {
        if (reg->rooms[i].state == ROOM_STATE_ACTIVE && 
            room_is_empty(&reg->rooms[i])) {
            
            room_cleanup(&reg->rooms[i]);
            reg->active_rooms--;
            removed_count++;
        }
    }
    
    if (removed_count > 0) {
        printf("Removed %zu empty rooms (active: %zu/%zu)\n", 
               removed_count, reg->active_rooms, reg->max_rooms);
    }
}

size_t room_registry_get_active_count(const room_registry_t *reg) {
    return reg ? reg->active_rooms : 0;
}