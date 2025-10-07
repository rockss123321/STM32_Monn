#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stdint.h>
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

void Auth_Init(void);

/* Per-request tracking */
void Auth_SetCurrentRemoteIp(const ip_addr_t* ip);
void Auth_BeginRequestWithCookieHeader(const char* headers, uint16_t headers_len);

/* Session management (cookie-based) */
bool Auth_CreateSessionForCurrentRequest(uint32_t now_ms, uint32_t ttl_ms);
bool Auth_TakePendingSetCookie(char* out_sid, uint16_t out_len);
void Auth_RevokeCurrentSession(void);

/* Authorization checks */
bool Auth_IsCurrentRequestAuthorized(uint32_t now_ms);

/* Legacy IP-based helpers (kept for compatibility) */
void Auth_GrantForCurrentIp(uint32_t now_ms, uint32_t ttl_ms);
void Auth_RevokeForCurrentIp(void);
bool Auth_IsCurrentIpAuthorized(uint32_t now_ms);
bool Auth_IsIpAuthorized(const ip_addr_t* ip, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* AUTH_H */
