#ifndef HASHTABLE
#define HASHTABLE

#define HASHPOWER_DEFAULT 16

#include <stddef.h>
#include <stdint.h>
#include "main.h"

typedef struct node item, *pitem;

void hashtable_init(const int hashpower_init);
item *hashtable_find(const char *key, const size_t nkey, const uint32_t hv);
int hashtable_insert(item *item, const uint32_t hv);
void hashtable_delete(const char *key, const size_t nkey, const uint32_t hv);
void do_hashtable_move_next_bucket(void);
int start_hashtable_maintenance_thread(void);
void stop_hashtable_maintenance_thread(void);
//extern unsigned int hashpower;



struct node {
    char* key;
    uint32_t nkey;
    char* value;
    uint32_t nvalue;
    struct node* h_next;
};




#endif
