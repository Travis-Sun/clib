#ifndef HASH_H
#define    HASH_H

#include "win.h"

#ifdef    __cplusplus
extern "C" {
#endif

S_UINT32 hash(const void *key, S_UINT length, const S_UINT32 initval);

#ifdef    __cplusplus
}
#endif

#endif    /* HASH_H */

