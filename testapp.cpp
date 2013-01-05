#include "main.h"
#include "hashtable.h"
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int main(void)
{
    
    item i1;
    i1.key = "key";
    i1.nkey = strlen(i1.key)+1;
    i1.value = "value";
    i1.nvalue = strlen(i1.value)+1;
    i1.h_next = 0;
    hashtable_init();
    hashtable_insert(i1,0);
    return 0;
}

