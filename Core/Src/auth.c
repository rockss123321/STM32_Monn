#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "lwip/ip_addr.h"
#include "auth.h"

typedef struct {
    ip_addr_t ip;
    uint32_t   deadline_ms;
    bool       in_use;
} AuthEntry;

#define AUTH_TABLE_SIZE 8
static AuthEntry g_auth_table[AUTH_TABLE_SIZE];
static ip_addr_t g_current_ip;
static bool g_current_ip_set = false;

void Auth_Init(void) {
    memset(g_auth_table, 0, sizeof(g_auth_table));
    g_current_ip_set = false;
}

void Auth_SetCurrentRemoteIp(const ip_addr_t* ip) {
    if (ip) {
        g_current_ip = *ip;
        g_current_ip_set = true;
    } else {
        g_current_ip_set = false;
    }
}

const ip_addr_t* Auth_GetCurrentRemoteIp(void) {
    return g_current_ip_set ? &g_current_ip : NULL;
}

static int auth_find_index_for_ip(const ip_addr_t* ip) {
    if (!ip) return -1;
    for (int i = 0; i < AUTH_TABLE_SIZE; i++) {
        if (g_auth_table[i].in_use && ip_addr_cmp(&g_auth_table[i].ip, ip)) {
            return i;
        }
    }
    return -1;
}

void Auth_GrantForCurrentIp(uint32_t now_ms, uint32_t ttl_ms) {
    if (!g_current_ip_set) return;
    int idx = auth_find_index_for_ip(&g_current_ip);
    if (idx < 0) {
        for (int i = 0; i < AUTH_TABLE_SIZE; i++) {
            if (!g_auth_table[i].in_use) { idx = i; break; }
        }
    }
    if (idx >= 0) {
        g_auth_table[idx].ip = g_current_ip;
        g_auth_table[idx].deadline_ms = now_ms + ttl_ms;
        g_auth_table[idx].in_use = true;
    }
}

void Auth_RevokeForCurrentIp(void) {
    if (!g_current_ip_set) return;
    int idx = auth_find_index_for_ip(&g_current_ip);
    if (idx >= 0) {
        g_auth_table[idx].in_use = false;
    }
}

bool Auth_IsCurrentIpAuthorized(uint32_t now_ms) {
    if (!g_current_ip_set) return false;
    return Auth_IsIpAuthorized(&g_current_ip, now_ms);
}

bool Auth_IsIpAuthorized(const ip_addr_t* ip, uint32_t now_ms) {
    int idx = auth_find_index_for_ip(ip);
    if (idx < 0) return false;
    AuthEntry* e = &g_auth_table[idx];
    if (!e->in_use) return false;
    /* signed compare to handle wrap-around gracefully */
    if ((int32_t)(e->deadline_ms - now_ms) > 0) {
        return true;
    }
    e->in_use = false; /* expired */
    return false;
}
