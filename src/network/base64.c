
#include <string.h>
#include <stdlib.h>
#include "base64.h"


static const signed char base64_decode_chars[] = {
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1,  62, - 1, - 1, - 1,  63,
	 52,  53,  54,  55,  56,  57,  58,  59,
	 60,  61, - 1, - 1, - 1,   0, - 1, - 1,
	- 1,   0,   1,   2,   3,   4,   5,   6,
	  7,   8,   9,  10,  11,  12,  13,  14,
	 15,  16,  17,  18,  19,  20,  21,  22,
	 23,  24,  25, - 1, - 1, - 1, - 1, - 1,
	- 1,  26,  27,  28,  29,  30,  31,  32,
	 33,  34,  35,  36,  37,  38,  39,  40,
	 41,  42,  43,  44,  45,  46,  47,  48,
	 49,  50,  51, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1,
	- 1, - 1, - 1, - 1, - 1, - 1, - 1, - 1};


size_t base64_encode(char *buf, size_t nbuf, const unsigned char *p, size_t n)
{
        const char t[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        size_t i, m ;
	m = (n+2)/3*4;
        unsigned x;
        if (nbuf >= m)
	{
            for (i = 0; i < n; ++i) {
                x = p[i] << 0x10;
                x |= (++i < n ? p[i] : 0) << 0x08;
                x |= (++i < n ? p[i] : 0) << 0x00;

                *buf++ = t[x >> 3 * 6 & 0x3f];
                *buf++ = t[x >> 2 * 6 & 0x3f];
                *buf++ = (((n - 0 - i) >> 31) & '=') |
                (~((n - 0 - i) >> 31) & t[x >> 1 * 6 & 0x3f]);
                *buf++ = (((n - 1 - i) >> 31) & '=') |
                (~((n - 1 - i) >> 31) & t[x >> 0 * 6 & 0x3f]);
            }
	}
        return m;
}


int base64_decode(unsigned char **dst, size_t *dstsiz, const unsigned char *src)
{
    union {
        unsigned long x;
        char c[4];
    } base64;
    unsigned char *pdst;
    int i, j = 0;
    size_t srcsiz = strlen((const char *)src);

    if ((srcsiz % 4) != 0) {
        return -1;
    }
    base64.x = 0UL;
    *dst = (unsigned char *)malloc(srcsiz);
    if (!*dst) {
        return -1;
    }
    pdst = *dst;
    *dstsiz = 0;
    for (; srcsiz > 0; src+=4, srcsiz-=4) {
        for (i = 0; i < 4; i++) {
            if (base64_decode_chars[src[i]] == -1) {
                return -1;
            }
            base64.x = base64.x << 6 | base64_decode_chars[src[i]];
            j += (src[i] == '=');
        }
        for (i = 3; i > j; i--, (*dstsiz)++) {
            *pdst++ = base64.c[i - 1];
        }
    }
    *pdst = '\0';
    return 0;
}
