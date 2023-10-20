#ifndef USER
#define USER

#include <stdbool.h>

/* structs for blocked users */
struct user_blocked {
    struct user* user;
    struct user_blocked* prev;
    struct user_blocked* next;
};
struct user_blocked_list {
    struct user_blocked* head;
    struct user_blocked* tail;
    int size;
};

void user_blocked_list_init(struct user_blocked_list*);
void user_blocked_list_insert(struct user_blocked_list*, struct user_blocked*); 
void user_blocked_list_remove(struct user_blocked_list*, struct user_blocked*); 

struct user_blocked* user_blocked_list_find_by_ip(struct user_blocked_list* list, char* ip);

void user_blocked_list_debug(struct user_blocked_list* list);
void user_blocked_list_free(struct user_blocked_list* list); 
/* end */

/* structs for msg */
struct msg {
    void* data;
    struct msg* prev;
    struct msg* next;
    int size;
};
struct msg_buffer {
    struct msg* head;
    struct msg* tail;
    int size;
};
void msg_buffer_init(struct msg_buffer*);
void msg_buffer_insert(struct msg_buffer*, struct msg*); 
void msg_buffer_remove(struct msg_buffer*, struct msg*); 
void msg_buffer_debug(struct msg_buffer* list);
void msg_buffer_free(struct msg_buffer* list); 
/* end */

/* structs for users */
struct user {
    char hostname[32];
    char ip[18];
    unsigned short port;

    struct user* prev;
    struct user* next;

    /* data hidden from clients */
    struct msg_buffer msg_buffer;
    struct user_blocked_list blocked_list;
    int sd;
    int num_msg_sent;
    int num_msg_rcv;
    bool is_logged_in;
};

struct user_stripped {
    char hostname[32];
    char ip[18];
    unsigned short port;
};

struct user_list {
    struct user* head;
    struct user* tail;
    int size;
};

void user_list_init(struct user_list*);
void user_list_insert(struct user_list*, struct user*); 
void user_list_remove(struct user_list*, struct user*); 
void user_list_remove_by_search(struct user_list*, struct user_stripped*);

struct user* user_list_find_current_user(struct user_list* list);
struct user* user_list_find_by_ip(struct user_list* list, char* ip);
struct user* user_list_find_by_ip_and_port(struct user_list* list, char* ip, int port);
struct user* user_list_find_by_sd(struct user_list* list, int sd);

void user_list_debug(struct user_list* list);
void user_list_free(struct user_list* list); 

/* structs for users */
#endif
