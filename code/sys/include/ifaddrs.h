/*
 * ifaddrs.h — devkitPPC/Wii stub
 * Also stubs out IPv6 multicast types that net_ip.c references.
 */
#ifndef _IFADDRS_H
#define _IFADDRS_H

#include <sys/socket.h>

/* ifaddrs — not available on Wii */
struct ifaddrs {
    struct ifaddrs  *ifa_next;
    char            *ifa_name;
    unsigned int     ifa_flags;
    struct sockaddr *ifa_addr;
    struct sockaddr *ifa_netmask;
    struct sockaddr *ifa_broadaddr;
    void            *ifa_data;
};
static inline int  getifaddrs(struct ifaddrs **p) { (void)p; return -1; }
static inline void freeifaddrs(struct ifaddrs *p)  { (void)p; }

/* IPv6 multicast — not available on Wii, stub so net_ip.c compiles */
struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr;
    unsigned int    ipv6mr_interface;
};

static inline unsigned int if_nametoindex(const char *name)
{
    (void)name;
    return 0;
}

/* IPv6 socket option constants net_ip.c references */
#ifndef IPV6_JOIN_GROUP
#  define IPV6_JOIN_GROUP  20
#endif
#ifndef IPV6_LEAVE_GROUP
#  define IPV6_LEAVE_GROUP 21
#endif
#ifndef IPPROTO_IPV6
#  define IPPROTO_IPV6     41
#endif

#endif /* _IFADDRS_H */
