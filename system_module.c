#include "si_module.h"
#include "ils.h"
#include <netdb.h>
#include <stdio.h>

#define WRAPFN(f) __wrap_##f
#define REALFN(f) __real_##f

extern struct hostent *REALFN(gethostbyname)(const char *name);
extern struct hostent *REALFN(gethostbyname2)(const char *name, int af);
extern int REALFN(getaddrinfo)(const char *, const char *, const struct addrinfo *, struct addrinfo **);
extern struct servent *REALFN(getservbyname)(const char *, const char *);
extern struct hostent *REALFN(gethostbyaddr)(const void *addr, socklen_t len, int type);

static int system_is_valid(si_mod_t *si, si_item_t *item)
{
    return 0;
}

static void system_close(si_mod_t *si)
{

}

static si_item_t *
system_hostbyname(si_mod_t *si, const char *name, int af, const char *interface, uint32_t *err)
{
    si_item_t *out = NULL;
    struct hostent *host = REALFN(gethostbyname2)(name, af);
    uint32_t bb = 0;

    switch (af) {
        case AF_INET:
            out = (si_item_t *)LI_ils_create("L4444s*44a", (unsigned long)si, CATEGORY_HOST_IPV4, 1, bb, 0, host->h_name, host->h_aliases, host->h_addrtype, host->h_length, host->h_addr_list);
            break;
        case AF_INET6:
            out = (si_item_t *)LI_ils_create("L4444s*44c", (unsigned long)si, CATEGORY_HOST_IPV6, 1, bb, 0, host->h_name, host->h_aliases, host->h_addrtype, host->h_length, host->h_addr_list);
            break;
    }

    if (out == NULL && err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;

    return out;
}

static si_item_t *
system_hostbyaddr(si_mod_t *si, const void *addr, int af, const char *interface, uint32_t *err)
{
    si_item_t *out = NULL;
    struct hostent *host = NULL;
    uint32_t bb = 0;

    switch (af) {
        case AF_INET:
            host = REALFN(gethostbyaddr)(addr, sizeof(struct in_addr), af);
            out = (si_item_t *)LI_ils_create("L4444s*44a", (unsigned long)si, CATEGORY_HOST_IPV4, 1, bb, 0, host->h_name, host->h_aliases, host->h_addrtype, host->h_length, host->h_addr_list);
            break;
        case AF_INET6:
            host = REALFN(gethostbyaddr)(addr, sizeof(struct in6_addr), af);
            out = (si_item_t *)LI_ils_create("L4444s*44a", (unsigned long)si, CATEGORY_HOST_IPV6, 1, bb, 0, host->h_name, host->h_aliases, host->h_addrtype, host->h_length, host->h_addr_list);
            break;
    }

    if (out == NULL && err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;

    return out;
}

static si_list_t *
system_addrinfo(si_mod_t *si, const void *node, const void *serv, uint32_t family, uint32_t socktype, uint32_t proto, uint32_t flags, const char *interface, uint32_t *err)
{
    const uint32_t unused = 0;
    si_list_t *out = NULL;
    struct addrinfo hints, *res, *res0;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_protocol = proto;
    hints.ai_flags = flags;
    char* service = NULL;
    char porttext[12];
    if (serv && (flags & AI_NUMERICSERV))
    {
        snprintf(porttext, sizeof(porttext), "%d", *(uint16_t*)serv);
        service = porttext;
    }
    
    int error = REALFN(getaddrinfo)(node, service, &hints, &res0);
    if (error) {
        if (err != NULL){
            *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
        }
        return NULL;
    }

    for (res = res0; res; res = res->ai_next) {
        socket_data_t sdata = {0};
        memcpy(&sdata, res->ai_addr, res->ai_addrlen);
        si_item_t *item = (si_item_t *)LI_ils_create("L444444444Ss", 
            (unsigned long)si, // L
            CATEGORY_ADDRINFO, // 4
            1,                 // 4
            unused,            // 4
            unused,            // 4
            res->ai_flags,     // 4
            res->ai_family,    // 4
            res->ai_socktype,  // 4
            res->ai_protocol,  // 4
            res->ai_addrlen,   // 4
            sdata,             // S
            res->ai_canonname);// s
        out = si_list_add(out, item);
        si_item_release(item);
    }

    return out;
}

// static si_list_t *
// system_srv_byname(si_mod_t* si, const char *qname, const char *interface, uint32_t *err)
// {
//     REALFN(getservbyname)(qname, interface);

//     if (res == 0) {
//         srv = reply.srv;
//         while (srv) {
//             si_item_t *item;
//             item = (si_item_t *)LI_ils_create("L4444222s", (unsigned long)si, CATEGORY_SRV, 1, unused, unused, srv->srv.priority, srv->srv.weight, srv->srv.port, srv->srv.target);
//             out = si_list_add(out, item);
//             si_item_release(item);
//             srv = srv->next;
//         }
//     }
// }

si_mod_t *
si_module_static_system(void)
{
    static const struct si_mod_vtable_s mdns_vtable =
    {
        .sim_close = &system_close,
        .sim_is_valid = &system_is_valid,
        .sim_host_byname = &system_hostbyname,
        .sim_host_byaddr = &system_hostbyaddr,
        .sim_addrinfo = &system_addrinfo,
        // .sim_srv_byname = &system_srv_byname,
    };

    static si_mod_t si =
    {
        .vers = 1,
        .refcount = 1,
        .flags = SI_MOD_FLAG_STATIC,

        .private = NULL,
        .vtable = &mdns_vtable,
    };

    static dispatch_once_t once;
    
    dispatch_once(&once, ^{
        si.name = strdup("system");
    });

    return (si_mod_t*)&si;
}