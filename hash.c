/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Hash table
 *
 * The hash function used here is by Bob Jenkins, 1996:
 *    <http://burtleburtle.net/bob/hash/doobs.html>
 *       "By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.
 *       You may use this code any way you wish, private, educational,
 *       or commercial.  It's free."
 *
 */
#include "hash.h"

/*
 * Since the hash function does bit manipulation, it needs to know
 * whether it's big or little-endian. ENDIAN_LITTLE and ENDIAN_BIG
 * are set in the configure script.
 */


#if ENDIAN_BIG == 1
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# if ENDIAN_LITTLE == 1
#  define HASH_LITTLE_ENDIAN 1
#  define HASH_BIG_ENDIAN 0
# else
#  define HASH_LITTLE_ENDIAN 0
#  define HASH_BIG_ENDIAN 0
# endif
#endif

#define rot(x,k) (((x)<<(k)) ^ ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

#if HASH_LITTLE_ENDIAN == 1
S_UINT32 hash(
  const void *key,       /* the key to hash */
  S_UINT      length,    /* length of the key */
  const S_UINT32    initval)   /* initval */
{
  S_UINT32 a,b,c;                                          /* internal state */
  union { const void *ptr; S_UINT i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((S_UINT32)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const S_UINT32 *k = key;                           /* read 32-bit chunks */
#ifdef VALGRIND
    const S_UINT8  *k8;
#endif /* ifdef VALGRIND */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : return c;  /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    k8 = (const S_UINT8 *)k;
    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((S_UINT32)k8[10])<<16;  /* fall through */
    case 10: c+=((S_UINT32)k8[9])<<8;    /* fall through */
    case 9 : c+=k8[8];                   /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((S_UINT32)k8[6])<<16;   /* fall through */
    case 6 : b+=((S_UINT32)k8[5])<<8;    /* fall through */
    case 5 : b+=k8[4];                   /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((S_UINT32)k8[2])<<16;   /* fall through */
    case 2 : a+=((S_UINT32)k8[1])<<8;    /* fall through */
    case 1 : a+=k8[0]; break;
    case 0 : return c;  /* zero length strings require no mixing */
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const S_UINT16 *k = key;                           /* read 16-bit chunks */
    const S_UINT8 *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((S_UINT32)k[1])<<16);
      b += k[2] + (((S_UINT32)k[3])<<16);
      c += k[4] + (((S_UINT32)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const S_UINT8 *)k;
    switch(length)
    {
    case 12: c+=k[4]+(((S_UINT32)k[5])<<16);
             b+=k[2]+(((S_UINT32)k[3])<<16);
             a+=k[0]+(((S_UINT32)k[1])<<16);
             break;
    case 11: c+=((S_UINT32)k8[10])<<16;     /* @fallthrough */
    case 10: c+=k[4];                       /* @fallthrough@ */
             b+=k[2]+(((S_UINT32)k[3])<<16);
             a+=k[0]+(((S_UINT32)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* @fallthrough */
    case 8 : b+=k[2]+(((S_UINT32)k[3])<<16);
             a+=k[0]+(((S_UINT32)k[1])<<16);
             break;
    case 7 : b+=((S_UINT32)k8[6])<<16;      /* @fallthrough */
    case 6 : b+=k[2];
             a+=k[0]+(((S_UINT32)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* @fallthrough */
    case 4 : a+=k[0]+(((S_UINT32)k[1])<<16);
             break;
    case 3 : a+=((S_UINT32)k8[2])<<16;      /* @fallthrough */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return c;  /* zero length strings require no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const S_UINT8 *k = key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((S_UINT32)k[1])<<8;
      a += ((S_UINT32)k[2])<<16;
      a += ((S_UINT32)k[3])<<24;
      b += k[4];
      b += ((S_UINT32)k[5])<<8;
      b += ((S_UINT32)k[6])<<16;
      b += ((S_UINT32)k[7])<<24;
      c += k[8];
      c += ((S_UINT32)k[9])<<8;
      c += ((S_UINT32)k[10])<<16;
      c += ((S_UINT32)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=((S_UINT32)k[11])<<24;
    case 11: c+=((S_UINT32)k[10])<<16;
    case 10: c+=((S_UINT32)k[9])<<8;
    case 9 : c+=k[8];
    case 8 : b+=((S_UINT32)k[7])<<24;
    case 7 : b+=((S_UINT32)k[6])<<16;
    case 6 : b+=((S_UINT32)k[5])<<8;
    case 5 : b+=k[4];
    case 4 : a+=((S_UINT32)k[3])<<24;
    case 3 : a+=((S_UINT32)k[2])<<16;
    case 2 : a+=((S_UINT32)k[1])<<8;
    case 1 : a+=k[0];
             break;
    case 0 : return c;  /* zero length strings require no mixing */
    }
  }

  final(a,b,c);
  return c;             /* zero length strings require no mixing */
}

#elif HASH_BIG_ENDIAN == 1
/*
 * hashbig():
 * This is the same as hashword() on big-endian machines.  It is different
 * from hashlittle() on all machines.  hashbig() takes advantage of
 * big-endian byte ordering.
 */
S_UINT32 hash( const void *key, S_UINT length, const S_UINT32 initval)
{
  S_UINT32 a,b,c;
  union { const void *ptr; S_UINT i; } u; /* to cast key to (S_UINT) happily */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((S_UINT32)length) + initval;

  u.ptr = key;
  if (HASH_BIG_ENDIAN && ((u.i & 0x3) == 0)) {
    const S_UINT32 *k = key;                           /* read 32-bit chunks */
#ifdef VALGRIND
    const S_UINT8  *k8;
#endif /* ifdef VALGRIND */

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]<<8" actually reads beyond the end of the string, but
     * then shifts out the part it's not allowed to read.  Because the
     * string is aligned, the illegal read is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff00; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff0000; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff000000; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff00; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff0000; a+=k[0]; break;
    case 5 : b+=k[1]&0xff000000; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff00; break;
    case 2 : a+=k[0]&0xffff0000; break;
    case 1 : a+=k[0]&0xff000000; break;
    case 0 : return c;              /* zero length strings require no mixing */
    }

#else  /* make valgrind happy */

    k8 = (const S_UINT8 *)k;
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((S_UINT32)k8[10])<<8;  /* fall through */
    case 10: c+=((S_UINT32)k8[9])<<16;  /* fall through */
    case 9 : c+=((S_UINT32)k8[8])<<24;  /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((S_UINT32)k8[6])<<8;   /* fall through */
    case 6 : b+=((S_UINT32)k8[5])<<16;  /* fall through */
    case 5 : b+=((S_UINT32)k8[4])<<24;  /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((S_UINT32)k8[2])<<8;   /* fall through */
    case 2 : a+=((S_UINT32)k8[1])<<16;  /* fall through */
    case 1 : a+=((S_UINT32)k8[0])<<24; break;
    case 0 : return c;
    }

#endif /* !VALGRIND */

  } else {                        /* need to read the key one byte at a time */
    const S_UINT8 *k = key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += ((S_UINT32)k[0])<<24;
      a += ((S_UINT32)k[1])<<16;
      a += ((S_UINT32)k[2])<<8;
      a += ((S_UINT32)k[3]);
      b += ((S_UINT32)k[4])<<24;
      b += ((S_UINT32)k[5])<<16;
      b += ((S_UINT32)k[6])<<8;
      b += ((S_UINT32)k[7]);
      c += ((S_UINT32)k[8])<<24;
      c += ((S_UINT32)k[9])<<16;
      c += ((S_UINT32)k[10])<<8;
      c += ((S_UINT32)k[11]);
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=k[11];
    case 11: c+=((S_UINT32)k[10])<<8;
    case 10: c+=((S_UINT32)k[9])<<16;
    case 9 : c+=((S_UINT32)k[8])<<24;
    case 8 : b+=k[7];
    case 7 : b+=((S_UINT32)k[6])<<8;
    case 6 : b+=((S_UINT32)k[5])<<16;
    case 5 : b+=((S_UINT32)k[4])<<24;
    case 4 : a+=k[3];
    case 3 : a+=((S_UINT32)k[2])<<8;
    case 2 : a+=((S_UINT32)k[1])<<16;
    case 1 : a+=((S_UINT32)k[0])<<24;
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}
#else /* HASH_XXX_ENDIAN == 1 */
#error Must define HASH_BIG_ENDIAN or HASH_LITTLE_ENDIAN
#endif /* HASH_XXX_ENDIAN == 1 */
