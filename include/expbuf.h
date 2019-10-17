// An expandable buffer

#ifndef _EXPBUF_H
#define _EXPBUF_H

#define EXPBUF_MAXSIZE 16384

struct _Expbuf {
    char *buf;
    size_t len;
};
typedef struct _Expbuf Expbuf;

void Expbuf_memcpy(Expbufk *buf, const void *src, size_t n);
// Expbuf_memcmp();
// Expbuf_memsep();  // use memchr

#endif
