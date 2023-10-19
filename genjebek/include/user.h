#ifndef USER
#define USER

struct user {
    char hostname[32];
    char ip[18];
    unsigned short port;

    struct user* prev;
    struct user* next;
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

void user_list_debug(struct user_list* list);
void user_list_free(struct user_list* list); 

#endif
