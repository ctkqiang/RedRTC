// #include "../include/server.h"
// #include "../include/client.h"
// #include "../include/room.h"
// #include <assert.h>
// #include <stdio.h>

// static void test_client_management(void) {
//     printf("Testing client management...\n");
    
//     client_registry_t reg;
//     assert(client_registry_init(&reg, 10) == 0);
    
//     /* Test adding clients */
//     client_t *client1 = client_registry_add(&reg, NULL);
//     assert(client1 != NULL);
//     assert(client1->is_alive == true);
//     assert(strlen(client1->id) == 36); /* UUID length */
    
//     client_t *client2 = client_registry_add(&reg, NULL);
//     assert(client2 != NULL);
//     assert(client2 != client1);
    
//     assert(client_registry_get_active_count(&reg) == 2);
    
//     /* Test finding client */
//     client_t *found = client_registry_find_by_wsi(&reg, NULL);
//     assert(found == NULL); /* No real wsi */
    
//     /* Test removal */
//     client_registry_remove(&reg, client1);
//     assert(client1->is_alive == false);
//     assert(client_registry_get_active_count(&reg) == 1);
    
//     client_registry_cleanup(&reg);
//     printf("Client management tests passed\n");
// }

// static void test_room_management(void) {
//     printf("Testing room management...\n");
    
//     room_registry_t reg;
//     assert(room_registry_init(&reg, 5) == 0);
    
//     client_t owner;
//     memset(&owner, 0, sizeof(owner));
//     strcpy(owner.id, "test-owner");
    
//     /* Test room creation */
//     room_t *room = room_registry_create(&reg, "Test Room", &owner);
//     assert(room != NULL);
//     assert(strlen(room->id) == 36);
//     assert(strcmp(room->name, "Test Room") == 0);
//     assert(room->owner == &owner);
//     assert(room->participant_count == 1);
//     assert(room_registry_get_active_count(&reg) == 1);
    
//     /* Test finding room */
//     room_t *found = room_registry_find_by_id(&reg, room->id);
//     assert(found == room);
    
//     /* Test adding participants */
//     client_t client2;
//     memset(&client2, 0, sizeof(client2));
//     strcpy(client2.id, "client2");
    
//     assert(room_add_participant(room, &client2) == 0);
//     assert(room->participant_count == 2);
//     assert(client2.room == room);
    
//     /* Test room full */
//     client_t clients[10];
//     int added = 0;
//     for (int i = 0; i < 10; i++) {
//         memset(&clients[i], 0, sizeof(client_t));
//         sprintf(clients[i].id, "client%d", i + 3);
//         if (room_add_participant(room, &clients[i]) == 0) {
//             added++;
//         }
//     }
//     assert(room->participant_count == 6); /* MAX_PARTICIPANTS */
//     assert(added == 4); /* Only 4 more could be added (2 + 4 = 6) */
    
//     /* Test participant removal */
//     assert(room_remove_participant(room, &client2) == 0);
//     assert(room->participant_count == 5);
//     assert(client2.room == NULL);
    
//     room_registry_cleanup(&reg);
//     printf("Room management tests passed\n");
// }

// static void test_message_serialization(void) {
//     printf("Testing message serialization...\n");
    
//     json_t *data = json_object();
//     json_object_set_new(data, "test", json_string("value"));
    
//     message_t *msg = message_create("test-event", data);
//     assert(msg != NULL);
//     assert(strcmp(msg->event, "test-event") == 0);
    
//     char *json_str = message_serialize(msg);
//     assert(json_str != NULL);
//     assert(strstr(json_str, "test-event") != NULL);
//     assert(strstr(json_str, "value") != NULL);
    
//     message_t *parsed = message_deserialize(json_str);
//     assert(parsed != NULL);
//     assert(strcmp(parsed->event, "test-event") == 0);
    
//     free(json_str);
//     message_unref(msg);
//     message_unref(parsed);
//     printf("Message serialization tests passed\n");
// }

// int main(void) {
//     printf("Running WebRTC Signaling Server tests...\n\n");
    
//     test_client_management();
//     test_room_management();
//     test_message_serialization();
    
//     printf("\nAll tests passed!\n");
//     return 0;
// }