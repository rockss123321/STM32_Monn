#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stdint.h>
#include "lwip/ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

void Auth_Init(void);

/* Track the remote IP of the currently handled HTTP request */
void Auth_SetCurrentRemoteIp(const ip_addr_t* ip);
const ip_addr_t* Auth_GetCurrentRemoteIp(void);

/* Grant/revoke authorization for the current remote IP */
void Auth_GrantForCurrentIp(uint32_t now_ms, uint32_t ttl_ms);
void Auth_RevokeForCurrentIp(void);

/* Query helpers */
bool Auth_IsCurrentIpAuthorized(uint32_t now_ms);
bool Auth_IsIpAuthorized(const ip_addr_t* ip, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* AUTH_H */
