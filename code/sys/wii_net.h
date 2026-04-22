/*
 * wii_net.h — POSIX socket compatibility shim for libogc
 *
 * Maps ioQ3's standard BSD socket calls to libogc's net_* equivalents
 * so net_ip.c compiles and works over Wi-Fi without modification.
 */
#ifndef WII_NET_H
#define WII_NET_H

/* Undef before network.h so libogc's definitions win cleanly,
 * and net_ip.c's later #define doesn't cause redefinition warnings */
#undef INVALID_SOCKET
#undef SOCKET_ERROR
/* Skip heavy network includes when not needed (e.g. huffman.c) */
#ifndef WII_NO_NETWORK_H
#include <network.h>
#endif
#include <stdio.h>   /* FILE, fopen, fprintf, fclose — used throughout */
#include <string.h>  /* memset — used in Wii_Net_Init sendto test */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>  /* usleep — used in Wii_Net_Init */

/* ----------------------------------------------------------------
 * After network.h pulls in gccore/color.h, undef its COLOR_*
 * so q_shared.h can redefine them as Q3 console colour chars.
 * ---------------------------------------------------------------- */
#undef COLOR_BLACK
#undef COLOR_RED
#undef COLOR_GREEN
#undef COLOR_YELLOW
#undef COLOR_BLUE
#undef COLOR_CYAN
#undef COLOR_MAGENTA
#undef COLOR_WHITE

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Network init — call from wii_main.c before Com_Init().
 * -------------------------------------------------------------------------- */
/* local_ip buffer filled by Wii_Net_Init on success; readable after return */
static char wii_net_local_ip[16];

/* Set to 1 if net_init() succeeded — i.e. /dev/net/ip/top is open.
 * Used by wii_net_socket() to distinguish "device not ready" from
 * "Wi-Fi not connected". */
static int s_net_ip_top_ready = 0;

static inline int Wii_Net_Init(void)
{
    char netmask[16] = {0};
    char gateway[16] = {0};
    wii_net_local_ip[0] = '\0';

    /* Diagnostic log — written to SD so we can read it off the card.
     * Tells us exactly which init step failed and with what code. */
    FILE *netlog = fopen("sd:/quake3/net_init.txt", "w");

    /* ---- Step 1: net_init() -----------------------------------------------
     * Opens /dev/net/ip/top (the IOS socket device).
     * if_config() alone uses a DIFFERENT IOS device (/dev/net/ncd/manage)
     * and does NOT open net/ip/top internally.  Without this call every
     * net_socket() returns a non-(-1) IOS error that Q3 silently ignores.
     * Poll on -EAGAIN (-11) or -24 (IOS_ENODEV — device not ready yet)
     * for up to ~5 s.  -24 was observed when IOS hasn't finished bringing up
     * the wireless module; a short retry clears it. */
    {
        s32 r;
        int tries = 0;
        do {
            r = net_init();
            if (r == -EAGAIN || r == -24) usleep(100000); /* 100 ms */
        } while ((r == -EAGAIN || r == -24) && ++tries < 50);

        if (netlog) {
            fprintf(netlog, "net_init: r=%d tries=%d\n", (int)r, tries);
            fflush(netlog);
        }

        if (r < 0) {
            /* Hard failure — /dev/net/ip/top is NOT open; socket() will fail. */
            if (netlog) {
                fprintf(netlog, "RESULT: net_init FAILED (no socket device)\n");
                fclose(netlog);
            }
            return (int)r;
        }

        s_net_ip_top_ready = 1;

        /* Quick smoke-test: can we actually create a socket right now?
         * If this fails the IOS stack is broken even though net_init() returned 0. */
        {
            int test_fd = net_socket(AF_INET, SOCK_DGRAM, 0);
            if (netlog) {
                fprintf(netlog, "test net_socket (pre-ifconfig): fd=%d\n", test_fd);
                fflush(netlog);
            }
            if (test_fd >= 0)
                net_close(test_fd);  /* success — close the test socket */
            /* Leave the result recorded; continue regardless */
        }
    }

    /* ---- Step 2: if_config() ----------------------------------------------
     * Wi-Fi association + DHCP via /dev/net/ncd/manage.
     * max_retries=20 → up to ~20 s of attempts; returns 0 on success. */
    int ifr = if_config(wii_net_local_ip, netmask, gateway, 1, 20);

    if (netlog) {
        fprintf(netlog, "if_config: r=%d ip='%s'\n", ifr, wii_net_local_ip);
        fflush(netlog);
    }

    /* ---- Step 3: comprehensive socket smoke-tests ----------------------------
     * Tests to pinpoint sendto -22 (EINVAL):
     *   A) recvfrom heap check — verifies IOS_IoctlvFormat & __net_heap are OK.
     *   B) sendto UNBOUND — if this also fails, socket creation itself is broken.
     *   C) sendto after explicit bind — log bind result too.
     *   D) sendto broadcast (with SO_BROADCAST) — LAN discovery check.
     * All use fresh sockets so they don't disturb Q3's later FDs. */
    if (netlog) {
        /* IOS version — helps identify firmware-specific socket bugs.
         * IOS_GetVersion() is in <ios.h>; forward-declare to avoid pulling
         * in all of gccore from this header. */
        extern s32 IOS_GetVersion(void);
        s32 ios_ver = IOS_GetVersion();
        fprintf(netlog, "IOS_version=%d\n", (int)ios_ver);
        fflush(netlog);
    }
    if (ifr == 0) {
        char pkt[4] = {0xFF, 0xFF, 0xFF, 0xFF};

        /* --- A: recvfrom heap test ----------------------------------------- */
        {
            int rfd = net_socket(AF_INET, SOCK_DGRAM, 0);
            if (netlog) { fprintf(netlog, "A_recvfrom: sock=%d\n", rfd); fflush(netlog); }
            if (rfd >= 0) {
                u32 nb = 1;
                net_ioctl(rfd, FIONBIO, &nb);
                char rbuf[4];
                struct sockaddr_in rsrc;
                memset(&rsrc, 0, sizeof(rsrc));
                rsrc.sin_len = (uint8_t)sizeof(rsrc);
                socklen_t rlen = (socklen_t)sizeof(rsrc);
                int rr = net_recvfrom(rfd, rbuf, sizeof(rbuf), 0,
                                      (struct sockaddr *)&rsrc, &rlen);
                /* -11=EAGAIN=heap_OK  -22=IPC_ENOMEM=heap_broken */
                if (netlog) { fprintf(netlog, "A_recvfrom: ret=%d\n", rr); fflush(netlog); }
                net_close(rfd);
            }
        }

        /* --- B: sendto UNBOUND (no bind/setsockopt before sendto) -----------
         * If this ALSO returns -22, socket creation is the issue (e.g. the
         * SO_RCVBUF setsockopt that net_socket() calls internally fails and
         * leaves the socket in a broken state in IOS).
         * If this returns 4 (bytes sent) or EAGAIN/-11, bind is not needed
         * and the failure in test C is specific to bind. */
        if (gateway[0]) {
            int ufd = net_socket(AF_INET, SOCK_DGRAM, 0);
            if (netlog) { fprintf(netlog, "B_sendto_unbound: sock=%d gw=%s\n", ufd, gateway); fflush(netlog); }
            if (ufd >= 0) {
                struct sockaddr_in udst;
                memset(&udst, 0, sizeof(udst));
                udst.sin_len    = (uint8_t)sizeof(udst);
                udst.sin_family = AF_INET;
                udst.sin_port   = htons(27960);
                udst.sin_addr.s_addr = inet_addr(gateway);
                int ur = net_sendto(ufd, pkt, sizeof(pkt), 0,
                                    (struct sockaddr *)&udst, sizeof(udst));
                if (netlog) { fprintf(netlog, "B_sendto_unbound: ret=%d\n", ur); fflush(netlog); }
                net_close(ufd);
            }
        }

        /* --- C: sendto after explicit bind — log bind result too ----------- */
        if (gateway[0]) {
            int ufd = net_socket(AF_INET, SOCK_DGRAM, 0);
            if (netlog) { fprintf(netlog, "C_sendto_bound: sock=%d\n", ufd); fflush(netlog); }
            if (ufd >= 0) {
                struct sockaddr_in usrc, udst;
                memset(&usrc, 0, sizeof(usrc));
                usrc.sin_len    = (uint8_t)sizeof(usrc);
                usrc.sin_family = AF_INET;
                usrc.sin_port   = htons(19998);
                usrc.sin_addr.s_addr = htonl(INADDR_ANY);
                int bret = net_bind(ufd, (struct sockaddr *)&usrc, sizeof(usrc));
                if (netlog) { fprintf(netlog, "C_sendto_bound: bind=%d\n", bret); fflush(netlog); }

                memset(&udst, 0, sizeof(udst));
                udst.sin_len    = (uint8_t)sizeof(udst);
                udst.sin_family = AF_INET;
                udst.sin_port   = htons(27960);
                udst.sin_addr.s_addr = inet_addr(gateway);
                int ur = net_sendto(ufd, pkt, sizeof(pkt), 0,
                                    (struct sockaddr *)&udst, sizeof(udst));
                if (netlog) { fprintf(netlog, "C_sendto_bound: ret=%d\n", ur); fflush(netlog); }
                net_close(ufd);
            }
        }

        /* --- D: broadcast sendto (requires SO_BROADCAST) -------------------- */
        {
            int tfd = net_socket(AF_INET, SOCK_DGRAM, 0);
            if (netlog) { fprintf(netlog, "D_sendto_bcast: sock=%d\n", tfd); fflush(netlog); }
            if (tfd >= 0) {
                int one = 1;
                int sbr = net_setsockopt(tfd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
                if (netlog) { fprintf(netlog, "D_sendto_bcast: SO_BROADCAST=%d\n", sbr); fflush(netlog); }

                struct sockaddr_in src, dst;
                memset(&src, 0, sizeof(src));
                src.sin_len    = (uint8_t)sizeof(src);
                src.sin_family = AF_INET;
                src.sin_port   = htons(19999);
                src.sin_addr.s_addr = htonl(INADDR_ANY);
                int br = net_bind(tfd, (struct sockaddr *)&src, sizeof(src));
                if (netlog) { fprintf(netlog, "D_sendto_bcast: bind=%d\n", br); fflush(netlog); }

                memset(&dst, 0, sizeof(dst));
                dst.sin_len    = (uint8_t)sizeof(dst);
                dst.sin_family = AF_INET;
                dst.sin_port   = htons(27960);
                dst.sin_addr.s_addr = htonl(0xFFFFFFFFUL);
                int sr = net_sendto(tfd, pkt, sizeof(pkt), 0,
                                    (struct sockaddr *)&dst, sizeof(dst));
                if (netlog) { fprintf(netlog, "D_sendto_bcast: ret=%d\n", sr); fflush(netlog); }
                net_close(tfd);
            }
        }

        /* --- E: net_connect() + net_send() instead of sendto() ---------------
         * Tests whether the SEND ioctlv works where SENDTO doesn't.
         * Distinguishes hypothesis (a) "SENDTO ioctlv format broken" from
         * hypothesis (b) "IOS lwIP has no route".
         *   connect() returns -22: routing/interface not configured (b)
         *   connect() OK, send() returns -22: SEND ioctlv also broken
         *   send() returns 4: SENDTO ioctlv broken, SEND works → use as fix */
        if (gateway[0]) {
            int efd = net_socket(AF_INET, SOCK_DGRAM, 0);
            if (netlog) { fprintf(netlog, "E_connect_send: sock=%d\n", efd); fflush(netlog); }
            if (efd >= 0) {
                struct sockaddr_in edst;
                memset(&edst, 0, sizeof(edst));
                edst.sin_len    = (uint8_t)sizeof(edst);
                edst.sin_family = AF_INET;
                edst.sin_port   = htons(27960);
                edst.sin_addr.s_addr = inet_addr(gateway);
                int cr = net_connect(efd, (struct sockaddr *)&edst, sizeof(edst));
                if (netlog) { fprintf(netlog, "E_connect_send: connect=%d\n", cr); fflush(netlog); }
                int sr = net_send(efd, pkt, sizeof(pkt), 0);
                if (netlog) { fprintf(netlog, "E_connect_send: send=%d\n", sr); fflush(netlog); }
                net_close(efd);
            }
        }

        /* --- F: sendto with tolen=8 (POSIX-style address, no padding) --------
         * Some IOS revisions pass sockaddr through the ioctlv as an 8-byte
         * struct: uint16_t family + uint16_t port + uint32_t addr (no sin_len,
         * no sin_zero[8]).  If our 16-byte BSD struct is misread by IOS as
         * POSIX, byte-0 (sin_len=16) lands in the high byte of sa_family,
         * giving family=0x1002=4098 → EINVAL.
         * This test tries the 8-byte form explicitly.
         *   ret=4: IOS expects 8-byte POSIX addr → always pass tolen=8
         *   ret=-22: 8-byte also wrong; address format is not the issue */
        if (gateway[0]) {
            int ffd = net_socket(AF_INET, SOCK_DGRAM, 0);
            if (netlog) { fprintf(netlog, "F_sendto_8b: sock=%d\n", ffd); fflush(netlog); }
            if (ffd >= 0) {
                /* Build an 8-byte POSIX-style sockaddr_in manually.
                 * Layout: [sa_family(2)][sin_port(2)][sin_addr(4)] */
                uint8_t ios_addr[8];
                uint16_t fam = (uint16_t)AF_INET;  /* host-endian, as BSD stores it */
                uint16_t por = htons(27960);
                uint32_t adr = inet_addr(gateway);
                memcpy(ios_addr + 0, &fam, 2);
                memcpy(ios_addr + 2, &por, 2);
                memcpy(ios_addr + 4, &adr, 4);
                int sr = net_sendto(ffd, pkt, sizeof(pkt), 0,
                                    (struct sockaddr *)ios_addr, 8);
                if (netlog) { fprintf(netlog, "F_sendto_8b: ret=%d\n", sr); fflush(netlog); }
                net_close(ffd);
            }
        }
    }

    if (netlog) {
        fprintf(netlog, "RESULT: %s\n",
                ifr == 0 ? "OK" : "if_config FAILED (no IP — Wi-Fi not connected?)");
        fclose(netlog);
    }

    return ifr;
}

/* --------------------------------------------------------------------------
 * fcntl shim — libogc uses net_ioctl(FIONBIO) for non-blocking mode.
 * Defined as a real function (not a macro) to avoid self-referencing
 * when fcntl.h's own fcntl declaration is in scope.
 * -------------------------------------------------------------------------- */
static inline int wii_net_fcntl(int s, int cmd, int arg)
{
    if (cmd == F_SETFL && (arg & O_NONBLOCK)) {
        u32 nb = 1;
        return net_ioctl(s, FIONBIO, &nb);
    }
    if (cmd == F_SETFL && !(arg & O_NONBLOCK)) {
        u32 nb = 0;
        return net_ioctl(s, FIONBIO, &nb);
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * Remap POSIX socket calls → libogc net_* equivalents.
 *
 * close() and fcntl() must be undefined first in case system headers
 * already defined them as macros, then redefined pointing to our shims.
 * -------------------------------------------------------------------------- */
#undef  close
#define close(s)                    net_close(s)

#undef  fcntl
#define fcntl(s,cmd,arg)            wii_net_fcntl((s),(cmd),(arg))

/* ioctl — wii_sys.c provides int ioctl(int fd, int request, ...) which
 * forwards to net_ioctl.  No macro needed here; net_ip.c's
 * "#define ioctlsocket ioctl" will resolve to that function at link time. */

/* socket(): IOS's /dev/net/ip/top requires protocol=0 for SOCK_DGRAM/STREAM —
 * it infers the protocol from the socket type. Passing IPPROTO_UDP=17 or
 * IPPROTO_TCP=6 explicitly returns IOS error -123 ("unknown protocol").
 * Also normalize any remaining negative return to INVALID_SOCKET so Q3
 * error checks fire correctly. */
static inline int wii_net_socket(int d, int t, int p)
{
    (void)p; /* IOS ignores explicit protocol; always pass 0 */
    int fd = net_socket(d, t, 0);
    if (fd < 0) {
        /* Log the raw IOS error on the FIRST failure so we know the real
         * reason (e.g. -4 = device not open, -123 = unknown protocol,
         * -22 = EINVAL, etc.) rather than the generic EPROTONOSUPPORT
         * string that Q3 ends up printing. */
        static int s_sock_logged = 0;
        if (!s_sock_logged) {
            s_sock_logged = 1;
            FILE *fp = fopen("sd:/quake3/net_sock.txt", "w");
            if (fp) {
                fprintf(fp,
                    "net_socket(AF=%d SOCK=%d 0) = %d"
                    "  ip_top_ready=%d\n",
                    d, t, fd, s_net_ip_top_ready);
                fclose(fp);
            }
        }
        errno = EPROTONOSUPPORT;
        return -1;
    }
    /* IOS requires SO_BROADCAST to be set before any sendto to a broadcast
     * address.  Set it unconditionally for all DGRAM sockets so Q3's LAN
     * discovery packets (to 255.255.255.255) are not rejected with EINVAL. */
    if (t == SOCK_DGRAM) {
        int one = 1;
        net_setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    }
    return fd;
}
#define socket(d,t,p)               wii_net_socket(d,t,p)

/* bind(): set sin_len (IOS BSD-style) and normalize error codes.
 * Q3 (POSIX) never sets sin_len; IOS validates it on every call. */
static inline int wii_net_bind(int s, struct sockaddr *a, int l)
{
    struct sockaddr_in tmp4;
    if (a && ((struct sockaddr_in *)a)->sin_family == AF_INET) {
        tmp4 = *(struct sockaddr_in *)a;
        tmp4.sin_len = (uint8_t)sizeof(struct sockaddr_in);
        a = (struct sockaddr *)&tmp4;
    }
    int ret = net_bind(s, a, l);
    if (ret < 0 && ret != -1) { errno = EADDRINUSE; return -1; }
    return ret;
}
#define bind(s,a,l)                 wii_net_bind(s,a,l)
#define connect(s,a,l)              net_connect((s),(a),(l))
#define listen(s,b)                 net_listen((s),(b))
#define accept(s,a,l)               net_accept((s),(a),(l))
/* send() conflicts with huffman.c's internal send() function.
 * Only define it when explicitly needed by net_ip.c */
#ifdef WII_NET_NEED_SEND
#define send(s,b,l,f)               net_send((s),(b),(l),(f))
#endif
#define recv(s,b,l,f)               net_recv((s),(b),(l),(f))

/* sendto wrapper: translates BSD 16-byte sockaddr_in → IOS 8-byte POSIX form.
 *
 * Root cause (confirmed 2026-04-15, tests E+F):
 *   IOS /dev/net/ip/top sendto ioctlv reads sa_family as a uint16_t at
 *   bytes 0-1 (POSIX style — no sin_len prefix).  We were passing the BSD
 *   16-byte struct with sin_len at byte 0 and tolen=16.  With sin_len=16,
 *   IOS saw family=(16<<8)|2=4098 → EINVAL.  The previous sin_len=16 fix
 *   made it worse (sin_len=0 → 0x0002=AF_INET would have been fine but
 *   tolen=16 alone caused IOS to reject the call).
 *
 *   Fix: build an 8-byte POSIX-style address and pass tolen=8.
 *   IOS 8-byte layout: [sa_family(uint16_t)][sin_port(uint16_t)][sin_addr(uint32_t)]
 *   On big-endian PPC, AF_INET=2 as uint16_t is [0x00,0x02] — IOS reads 2. */
static inline int wii_net_sendto(int s, const void *b, int l, int f,
                                  const struct sockaddr *a, int al)
{
    uint8_t ios_addr[8];
    const struct sockaddr *send_a  = a;
    int                    send_al = al;
    if (a && a->sa_family == AF_INET) {
        const struct sockaddr_in *sin4 = (const struct sockaddr_in *)a;
        uint16_t fam = (uint16_t)AF_INET;        /* [0x00,0x02] in memory on PPC */
        memcpy(ios_addr + 0, &fam,            2); /* sa_family  */
        memcpy(ios_addr + 2, &sin4->sin_port, 2); /* sin_port (already network order) */
        memcpy(ios_addr + 4, &sin4->sin_addr, 4); /* sin_addr  (already network order) */
        send_a  = (const struct sockaddr *)ios_addr;
        send_al = 8;
    }
    int ret = net_sendto(s, (void *)b, l, f, (struct sockaddr *)send_a, send_al);
    static int s_n = 0;
    if (s_n < 40) {
        FILE *fp = fopen("sd:/quake3/net_send.txt", s_n == 0 ? "w" : "a");
        if (fp) {
            char ip[32] = "?";
            unsigned short port = 0;
            if (a && a->sa_family == AF_INET) {
                const struct sockaddr_in *sin4 = (const struct sockaddr_in *)a;
                const unsigned char *p = (const unsigned char *)&sin4->sin_addr.s_addr;
                snprintf(ip, sizeof(ip), "%u.%u.%u.%u",
                         (unsigned)p[0], (unsigned)p[1],
                         (unsigned)p[2], (unsigned)p[3]);
                port = (unsigned short)(
                    ((unsigned char *)&sin4->sin_port)[0] << 8 |
                    ((unsigned char *)&sin4->sin_port)[1]);
            }
            fprintf(fp, "%d fd=%d dst=%s:%u len=%d al=%d ret=%d err=%d\n",
                    ++s_n, s, ip, (unsigned)port, l, send_al, ret,
                    ret < 0 ? errno : 0);
            fclose(fp);
        }
    }
    return ret;
}
#define sendto(s,b,l,f,a,al) wii_net_sendto((s),(b),(l),(f),(a),(al))
/* recvfrom wrapper: normalise IOS non-(-1) error codes.
 *
 * IOS /dev/net/ip/top returns its own negative codes on failure:
 *   -128 = "would block" (socket has no data, equivalent to EAGAIN)
 *   -11  = EAGAIN (some IOS versions use the standard value)
 *   other negatives = various IOS errors
 * Q3 checks ret == SOCKET_ERROR (-1), so any non-(-1) negative falls
 * through as a "successful 0-byte receive" or negative cursize, corrupting
 * the packet ring.  Previously, real net_select returning 0 (no data)
 * prevented recvfrom from ever being called; with the select bypass above
 * we call recvfrom unconditionally, so normalisation is mandatory. */
static inline int wii_net_recvfrom(int s, void *b, int l, int f,
                                    struct sockaddr *a, socklen_t *al)
{
    int ret = net_recvfrom(s, b, l, f, a, al);
    if (ret >= 0) return ret;           /* bytes received — pass through */
    /* For any failure, always report EAGAIN.
     * libogc may return either the raw IOS negative code (-128, -22, …) or
     * -1 with errno already set (EINVAL, etc.).  Either way, on a non-blocking
     * UDP socket any error means "no data right now" — there are no meaningful
     * recoverable errors.  Setting EAGAIN suppresses Q3's per-frame log spam. */
    errno = EAGAIN;
    return -1;                          /* SOCKET_ERROR so Q3 checks errno */
}
#define recvfrom(s,b,l,f,a,al)      wii_net_recvfrom((s),(b),(l),(f),(a),(al))
#define setsockopt(s,lv,o,v,l)      net_setsockopt((s),(lv),(o),(v),(l))
#define getsockopt(s,lv,o,v,l)      net_getsockopt((s),(lv),(o),(v),(l))
/* select(): libogc's net_select blocks even with {0,0} timeout — it issues an
 * IOS ioctlv that IOS does not honour with a zero timeout.  Q3 uses select()
 * purely as a non-blocking "is data available?" poll before recvfrom.  Since
 * the socket is set non-blocking (FIONBIO via net_ioctl), recvfrom returns
 * EAGAIN immediately when no packet is waiting, and Q3 handles EAGAIN
 * correctly (NET_GetPacket returns qfalse).  So we bypass net_select
 * entirely: always return 1 (success, "1 fd ready"), do not modify the
 * fd_sets (Q3 called FD_SET before us so the bit is already set), and let
 * recvfrom do the real "is there data?" check. */
static inline int wii_net_select(int n, fd_set *r, fd_set *w, fd_set *e,
                                  struct timeval *t)
{
    (void)n; (void)r; (void)w; (void)e; (void)t;
    return 1;
}
#define select(n,r,w,e,t)           wii_net_select((n),(r),(w),(e),(t))
#define gethostbyname(n)            net_gethostbyname(n)

/* --------------------------------------------------------------------------
 * errno values newlib may be missing
 * -------------------------------------------------------------------------- */
#ifndef EWOULDBLOCK
#  define EWOULDBLOCK  EAGAIN
#endif
#ifndef EINPROGRESS
#  define EINPROGRESS  36
#endif
#ifndef ECONNREFUSED
#  define ECONNREFUSED 111
#endif

#ifdef __cplusplus
}
#endif

#endif /* WII_NET_H */
