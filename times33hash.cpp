#include "times33hash.h"

/*
  for single char
 */
S_UINT32 Times33Hash::hash(const S_CHAR *key, S_UINT klen)
{
    const S_CHAR* k = key;
    S_UINT32 hashval = 5381;
    for (; klen>=8; klen-=8) {
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
    }
    switch(klen) {
    case 7: hashval = (hashval<<5) + hashval + *(k++);
    case 6: hashval = (hashval<<5) + hashval + *(k++);
    case 5: hashval = (hashval<<5) + hashval + *(k++);
    case 4: hashval = (hashval<<5) + hashval + *(k++);
    case 3: hashval = (hashval<<5) + hashval + *(k++);
    case 2: hashval = (hashval<<5) + hashval + *(k++);
    case 1: hashval = (hashval<<5) + hashval + *(k++); break;
    case 0: break;
    }        
    return hashval;
}

/*
  for wide char such as chinese, japan.
 */
S_UINT32 Times33Hash::hash(const S_WCHAR *key, S_UINT klen)
{
    const S_WCHAR* k = key;
    S_UINT32 hashval = 5381;
    for (; klen>=8; klen-=8) {
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
        hashval = (hashval<<5) + hashval + *(k++);
    }
    switch(klen) {
    case 7: hashval = (hashval<<5) + hashval + *(k++);
    case 6: hashval = (hashval<<5) + hashval + *(k++);
    case 5: hashval = (hashval<<5) + hashval + *(k++);
    case 4: hashval = (hashval<<5) + hashval + *(k++);
    case 3: hashval = (hashval<<5) + hashval + *(k++);
    case 2: hashval = (hashval<<5) + hashval + *(k++);
    case 1: hashval = (hashval<<5) + hashval + *(k++); break;
    case 0: break;
    }        
    return hashval;
}
