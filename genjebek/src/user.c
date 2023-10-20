#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../include/user.h"
#include "../include/cmds.h"

void user_list_init(struct user_list* list) {
    struct user* head = (struct user*) calloc(1, sizeof(struct user));
    struct user* tail = (struct user*) calloc(1, sizeof(struct user));

    head->next = tail;
    tail->prev = head;

    list->head = head;
    list->tail = tail;
    list->size = 0;
}

void user_list_insert(struct user_list* list, struct user* user) {
    struct user* cur = list->head->next;
    while (cur != list->tail) {
        if (cur->port > user->port)
            break;
        cur = cur->next;
    }
    user->next = cur;
    user->prev = cur->prev;
    cur->prev->next = user;
    cur->prev = user;

    ++list->size;
}

void user_list_remove(struct user_list* list, struct user* user) {
    user->prev->next = user->next;
    user->next->prev = user->prev;
    free(user);
    --list->size;
}

void user_list_remove_by_search(struct user_list* list, struct user_stripped* user) {
    struct user* cur = list->head->next;
    while (cur != list->tail) {
        if (strcmp(cur->hostname, user->hostname) == 0 && strcmp(cur->ip, user->ip) == 0 && cur->port == user->port)
            break;
        cur = cur->next;
    }

    struct user* prev = cur->prev;
    struct user* next = cur->next;
    prev->next = next;
    next->prev = prev;
    free(cur);
    --list->size;
}

struct user* user_list_find_current_user(struct user_list* list) {
    struct user* cur = list->head->next;
    while (cur != list->tail) {
        if (strcmp(get_my_ip(), cur->ip) == 0 && atoi(get_my_port()) == cur->port) {
            break;
        }
        cur = cur->next;
    }

    return cur;
}

struct user* user_list_find_by_ip(struct user_list* list, char* ip) {
    struct user* cur = list->head->next;
    while (cur != list->tail) {
        if (strcmp(ip, cur->ip) == 0) { 
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

struct user* user_list_find_by_ip_and_port(struct user_list* list, char* ip, int port) {
    struct user* cur = list->head->next;
    while (cur != list->tail) {
        if (strcmp(ip, cur->ip) == 0 && cur->port == port) { 
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

struct user* user_list_find_by_sd(struct user_list* list, int sd) {
    struct user* cur = list->head->next;
    while (cur != list->tail) {
        if (cur->sd == sd)
            return cur;

        cur = cur->next;
    }
    return NULL;
}

void user_list_debug(struct user_list* list) {
    struct user* cur = list->head->next;
    int i = 0;

    printf("\nuser_list_debug()\nsize:%d\n", list->size);

    while (cur != list->tail) {
        printf("%d) hostname: %s ip: %s port: %d\n", i++, cur->hostname, cur->ip, cur->port);
        cur = cur->next;
    }
}

void user_list_free(struct user_list* list) {
    struct user* cur = list->head->next;
    while (cur != list->tail) {
        struct user* next = cur->next;
        free(cur);
        cur = next;
    }
    list->head->next = list->tail;
    list->tail->prev = list->head;
    list->size = 0;
}



/* functions for blocked users */
void user_blocked_list_init(struct user_blocked_list* list) {
    struct user_blocked* head = (struct user_blocked*) calloc(1, sizeof(struct user));
    struct user_blocked* tail = (struct user_blocked*) calloc(1, sizeof(struct user));

    head->next = tail;
    tail->prev = head;

    list->head = head;
    list->tail = tail;
    list->size = 0;
}
void user_blocked_list_insert(struct user_blocked_list* list, struct user_blocked* user_blocked) {
    struct user_blocked* cur = list->head->next;
    while (cur != list->tail) {
        if (cur->user->port > user_blocked->user->port)
            break;
        cur = cur->next;
    }
    user_blocked->next = cur;
    user_blocked->prev = cur->prev;
    cur->prev->next = user_blocked;
    cur->prev = user_blocked;

    ++list->size;
}
void user_blocked_list_remove(struct user_blocked_list* list, struct user_blocked* user_blocked) {
    user_blocked->prev->next = user_blocked->next;
    user_blocked->next->prev = user_blocked->prev;
    free(user_blocked);
    --list->size;
}

struct user_blocked* user_blocked_list_find_by_ip(struct user_blocked_list* list, char* ip) {
    struct user_blocked* cur = list->head->next;
    while (cur != list->tail) {
        if (strcmp(ip, cur->user->ip) == 0) { 
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void user_blocked_list_debug(struct user_blocked_list* list) {
    struct user_blocked* cur = list->head->next;
    int i = 0;

    printf("\nuser_list_debug()\nsize:%d\n", list->size);

    while (cur != list->tail) {
        printf("%d) hostname: %s ip: %s port: %d\n", i++, cur->user->hostname, cur->user->ip, cur->user->port);
        cur = cur->next;
    }
}
void user_blocked_list_free(struct user_blocked_list* list) {
    struct user_blocked* cur = list->head->next;
    while (cur != list->tail) {
        struct user_blocked* next = cur->next;
        free(cur);
        cur = next;
    }
    list->head->next = list->tail;
    list->tail->prev = list->head;
    list->size = 0;
}

/* msg_buffer functions */
void msg_buffer_init(struct msg_buffer* list) {
    struct msg* head = (struct msg*) calloc(1, sizeof(struct msg));
    struct msg* tail = (struct msg*) calloc(1, sizeof(struct msg));

    head->next = tail;
    tail->prev = head;

    list->head = head;
    list->tail = tail;
    list->size = 0;
}
void msg_buffer_insert(struct msg_buffer* list, struct msg* msg) {
    msg->prev = list->tail->prev;
    msg->next = list->tail;

    list->tail->prev->next = msg;
    list->tail->prev = msg;

    ++list->size;
}
void msg_buffer_remove(struct msg_buffer* list, struct msg* msg) {
    msg->prev->next = msg->next;
    msg->next->prev = msg->prev;
    free(msg);
    --list->size;
}
void msg_buffer_debug(struct msg_buffer* list) {
    struct msg* cur = list->head->next;
    int i = 0;

    printf("\nmsg_buffer_debug()\nsize:%d\n", list->size);

    while (cur != list->tail) {
        printf("%d) data size: %d\n", i, cur->size);
        cur = cur->next;
    }
}
void msg_buffer_free(struct msg_buffer* list) {
    struct msg* cur = list->head->next;
    while (cur != list->tail) {
        struct msg* next = cur->next;
        free(cur);
        cur = next;
    }
    list->head->next = list->tail;
    list->tail->prev = list->head;
    list->size = 0;
}
