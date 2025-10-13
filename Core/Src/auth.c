#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "lwip/arch.h"
#include "lwip/ip_addr.h"
#include "auth.h"

/* --- Simple session table keyed by random SID (hex string) --- */
typedef struct {
    char      sid[33];      /* 32 hex chars + NUL */
    ip_addr_t ip;           /* last seen ip (optional) */
    uint32_t  deadline_ms;  /* expiration time */
    bool      in_use;
} SessionEntry;

#define SESSION_TABLE_SIZE 8
static SessionEntry g_sessions[SESSION_TABLE_SIZE];

static ip_addr_t g_req_ip;
static bool g_req_ip_set = false;
static char g_req_cookie_sid[33];
static bool g_req_cookie_sid_present = false;
static char g_resp_setcookie_sid[33];
static bool g_resp_setcookie_pending = false;

static uint32_t rng32(void) {
    /* Weak RNG placeholder; replace with HW RNG if available */
    static uint32_t x = 0x12345678u;
    x ^= (x << 13);
    x ^= (x >> 17);
    x ^= (x << 5);
    return x ^ sys_now();
}

static void gen_sid(char out[33]) {
    for (int i = 0; i < 16; i++) {
        uint8_t b = (uint8_t)(rng32() & 0xFF);
        static const char* hex = "0123456789abcdef";
        out[i*2]   = hex[(b >> 4) & 0xF];
        out[i*2+1] = hex[b & 0xF];
    }
    out[32] = '\0';
}

void Auth_Init(void) {
    memset(g_sessions, 0, sizeof(g_sessions));
    g_req_ip_set = false;
    g_req_cookie_sid_present = false;
    g_resp_setcookie_pending = false;
}

void Auth_SetCurrentRemoteIp(const ip_addr_t* ip) {
    if (ip) {
        g_req_ip = *ip;
        g_req_ip_set = true;
    } else {
        g_req_ip_set = false;
    }
}

void Auth_BeginRequestWithCookieHeader(const char* headers, uint16_t headers_len) {
    /* Parse a very simple "Cookie: SID=<sid>" header if present */
    g_req_cookie_sid_present = false;
    g_req_cookie_sid[0] = '\0';
    if (!headers || headers_len == 0) return;
    const char* p = headers;
    const char* end = headers + headers_len;
    while (p < end) {
        const char* line_end = (const char*)memchr(p, '\n', (size_t)(end - p));
        size_t line_len = line_end ? (size_t)(line_end - p) : (size_t)(end - p);
        if (line_len >= 7 && !strncasecmp(p, "Cookie:", 7)) {
            const char* v = p + 7;
            while (v < p + line_len && (*v == ' ' || *v == '\t')) v++;
            const char* sid_pos = strcasestr(v, "SID=");
            if (sid_pos) {
                sid_pos += 4;
                size_t i = 0;
                while (i < 32 && sid_pos + i < p + line_len) {
                    char c = sid_pos[i];
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) break;
                    g_req_cookie_sid[i] = (char)tolower((unsigned char)c);
                    i++;
                }
                g_req_cookie_sid[i] = '\0';
                if (i == 32) g_req_cookie_sid_present = true;
            }
            break;
        }
        if (!line_end) break;
        p = line_end + 1;
    }
}

static int session_find_by_sid(const char* sid) {
    if (!sid) return -1;
    for (int i = 0; i < SESSION_TABLE_SIZE; i++) {
        if (g_sessions[i].in_use && strcmp(g_sessions[i].sid, sid) == 0) return i;
    }
    return -1;
}

bool Auth_CreateSessionForCurrentRequest(uint32_t now_ms, uint32_t ttl_ms) {
    char sid[33];
    gen_sid(sid);
    int idx = -1;
    for (int i = 0; i < SESSION_TABLE_SIZE; i++) {
        if (!g_sessions[i].in_use) { idx = i; break; }
    }
    if (idx < 0) return false;
    g_sessions[idx].in_use = true;
    strncpy(g_sessions[idx].sid, sid, sizeof(g_sessions[idx].sid));
    g_sessions[idx].sid[32] = '\0';
    if (g_req_ip_set) g_sessions[idx].ip = g_req_ip; else memset(&g_sessions[idx].ip, 0, sizeof(g_sessions[idx].ip));
    g_sessions[idx].deadline_ms = now_ms + ttl_ms;
    strncpy(g_resp_setcookie_sid, sid, sizeof(g_resp_setcookie_sid));
    g_resp_setcookie_sid[32] = '\0';
    g_resp_setcookie_pending = true;
    return true;
}

bool Auth_TakePendingSetCookie(char* out_sid, uint16_t out_len) {
    if (!g_resp_setcookie_pending || out_len < 33) return false;
    strncpy(out_sid, g_resp_setcookie_sid, out_len);
    out_sid[32] = '\0';
    g_resp_setcookie_pending = false;
    return true;
}

void Auth_RevokeCurrentSession(void) {
    if (!g_req_cookie_sid_present) return;
    int idx = session_find_by_sid(g_req_cookie_sid);
    if (idx >= 0) g_sessions[idx].in_use = false;
}

bool Auth_IsCurrentRequestAuthorized(uint32_t now_ms) {
    if (!g_req_cookie_sid_present) return false;
    int idx = session_find_by_sid(g_req_cookie_sid);
    if (idx < 0) return false;
    SessionEntry* e = &g_sessions[idx];
    if (!e->in_use) return false;
    if ((int32_t)(e->deadline_ms - now_ms) > 0) return true;
    e->in_use = false; /* expired */
    return false;
}

/* Legacy IP-based API below (optional, kept for compatibility) */
typedef struct {
    ip_addr_t ip;
    uint32_t   deadline_ms;
    bool       in_use;
} AuthEntry;

#define AUTH_TABLE_SIZE_LEGACY 8
static AuthEntry g_auth_table[AUTH_TABLE_SIZE_LEGACY];

static int auth_find_index_for_ip(const ip_addr_t* ip) {
    if (!ip) return -1;
    for (int i = 0; i < AUTH_TABLE_SIZE_LEGACY; i++) {
        if (g_auth_table[i].in_use && ip_addr_cmp(&g_auth_table[i].ip, ip)) return i;
    }
    return -1;
}

void Auth_GrantForCurrentIp(uint32_t now_ms, uint32_t ttl_ms) {
    if (!g_req_ip_set) return;
    int idx = auth_find_index_for_ip(&g_req_ip);
    if (idx < 0) {
        for (int i = 0; i < AUTH_TABLE_SIZE_LEGACY; i++) {
            if (!g_auth_table[i].in_use) { idx = i; break; }
        }
    }
    if (idx >= 0) {
        g_auth_table[idx].ip = g_req_ip;
        g_auth_table[idx].deadline_ms = now_ms + ttl_ms;
        g_auth_table[idx].in_use = true;
    }
}

void Auth_RevokeForCurrentIp(void) {
    int idx = auth_find_index_for_ip(&g_req_ip);
    if (idx >= 0) g_auth_table[idx].in_use = false;
}

bool Auth_IsCurrentIpAuthorized(uint32_t now_ms) {
    int idx = auth_find_index_for_ip(&g_req_ip);
    if (idx < 0) return false;
    AuthEntry* e = &g_auth_table[idx];
    if (!e->in_use) return false;
    if ((int32_t)(e->deadline_ms - now_ms) > 0) return true;
    e->in_use = false; return false;
}

bool Auth_IsIpAuthorized(const ip_addr_t* ip, uint32_t now_ms) {
    int idx = auth_find_index_for_ip(ip);
    if (idx < 0) return false;
    AuthEntry* e = &g_auth_table[idx];
    if (!e->in_use) return false;
    if ((int32_t)(e->deadline_ms - now_ms) > 0) return true;
    e->in_use = false; return false;
}
