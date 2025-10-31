#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "../include/messages.h"
#include "../include/utilities.h"

message_t *message_create(const char *event, json_t *data) {
    message_t *msg = malloc(sizeof(message_t));
    if (!msg) return NULL;
    
    msg->event = strdup(event);
    msg->data = data ? json_incref(data) : NULL;
    msg->ref_count = 1;
    
    return msg;
}

void message_ref(message_t *msg) {
    if (msg) {
        msg->ref_count++;
    }
}

void message_unref(message_t *msg) {
    if (msg && --msg->ref_count == 0) {
        message_destroy(msg);
    }
}

char *message_serialize(const message_t *msg) {
    if (!msg) return NULL;
    
    json_t *json_msg = json_object();
    if (!json_msg) return NULL;
    
    json_object_set_new(json_msg, "event", json_string(msg->event));
    if (msg->data) {
        json_object_set_new(json_msg, "data", json_incref(msg->data));
    }
    
    char *result = json_dumps(json_msg, JSON_COMPACT);
    json_decref(json_msg);
    
    return result;
}

message_t *message_deserialize(const char *json_str) {
    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n", error.text);
        return NULL;
    }
    
    json_t *event = json_object_get(root, "event");
    json_t *data = json_object_get(root, "data");
    
    if (!event || !json_is_string(event)) {
        json_decref(root);
        return NULL;
    }
    
    message_t *msg = message_create(json_string_value(event), 
                                   data ? json_incref(data) : NULL);
    json_decref(root);
    
    return msg;
}

void message_destroy(message_t *msg) {
    if (msg) {
        free(msg->event);
        if (msg->data) {
            json_decref(msg->data);
        }
        free(msg);
    }
}

int message_queue_init(message_queue_t *queue, size_t capacity) {
    queue->messages = calloc(capacity, sizeof(ws_message_t));
    if (!queue->messages) return -1;
    
    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    
    pthread_mutex_init(&queue->lock, NULL);
    return 0;
}

void message_queue_cleanup(message_queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    
    /* Free any remaining messages */
    while (queue->count > 0) {
        ws_message_t *msg = &queue->messages[queue->head];
        if (msg->message) {
            message_unref(msg->message);
        }
        queue->head = (queue->head + 1) % queue->capacity;
        queue->count--;
    }
    
    pthread_mutex_unlock(&queue->lock);
    pthread_mutex_destroy(&queue->lock);
    
    free(queue->messages);
    queue->messages = NULL;
    queue->capacity = 0;
}

int message_queue_push(message_queue_t *queue, struct lws *wsi, message_t *msg) {
    pthread_mutex_lock(&queue->lock);
    
    if (queue->count >= queue->capacity) {
        pthread_mutex_unlock(&queue->lock);
        return -1; /* Queue full */
    }
    
    ws_message_t *slot = &queue->messages[queue->tail];
    slot->wsi = wsi;
    slot->message = msg;
    slot->timestamp = get_timestamp_ms();
    
    message_ref(msg); /* Take reference */
    
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    pthread_mutex_unlock(&queue->lock);
    return 0;
}

int message_queue_pop(message_queue_t *queue, ws_message_t *result) {
    pthread_mutex_lock(&queue->lock);
    
    if (queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return -1; /* Queue empty */
    }
    
    ws_message_t *msg = &queue->messages[queue->head];
    if (result) {
        *result = *msg;
    }
    
    /* Don't unref here - caller takes ownership */
    memset(msg, 0, sizeof(ws_message_t));
    
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    pthread_mutex_unlock(&queue->lock);
    return 0;
}

bool message_queue_is_empty(const message_queue_t *queue) {
    return queue->count == 0;
}

bool message_queue_is_full(const message_queue_t *queue) {
    return queue->count >= queue->capacity;
}