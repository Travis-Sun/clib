#ifndef TIMES33HASH_H
#define TIMES33HASH_H

#include "win.h"

class Times33Hash {
public:
    static S_UINT32 hash(const S_CHAR *key, S_UINT klen);
    static S_UINT32 hash(const S_WCHAR *key, S_UINT klen);    
};


#endif
