#ifndef HASHTABLE
#define HASHTABLE

#include <stddef.h>
#include <stdint.h>
#include "main.h"
#include "win.h"

typedef struct node item, *pitem;

void hashtable_init(const int hashpower_init);
item *hashtable_find(const S_CHAR *key, const S_UINT nkey, const S_UINT32 hv);
int hashtable_insert(item *item, const S_UINT32 hv);
void hashtable_delete(const S_CHAR *key, const S_UINT nkey, const S_UINT32 hv);
void do_hashtable_move_next_bucket(void);
int start_hashtable_maintenance_thread(void);
void stop_hashtable_maintenance_thread(void);
//extern unsigned int hashpower;


/*
  TODO
  this only can deal with single char but wide char,
  i think I will extent the function to wide char later with template
 */
struct node {
    S_CHAR * key;
    S_UINT nkey;
    S_CHAR* value;
    S_UINT32 nvalue;
    struct node* h_next;
};




#endif
