/* sys/mman.h — Wii stub */
#ifndef WII_MMAN_H
#define WII_MMAN_H
#include <stddef.h>
#include <sys/types.h>
#ifndef PROT_READ
#define PROT_READ     1
#define PROT_WRITE    2
#define PROT_EXEC     4
#endif
#ifndef MAP_SHARED
#define MAP_SHARED    0x01
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE   0x02
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#ifndef MAP_ANON
#define MAP_ANON      MAP_ANONYMOUS
#endif
#ifndef MAP_FAILED
#define MAP_FAILED    ((void *)-1)
#endif
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int   munmap(void *addr, size_t len);
#endif
