#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#include "lwip/ip_addr.h"
#include "stm32f2xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BKP_MAGIC_REG        RTC_BKP_DR19
#define BKP_MAGIC_VALUE      0xBEEFCAFE

void Settings_Init(void);
void Settings_Save_To_Backup(ip4_addr_t ip, ip4_addr_t mask, ip4_addr_t gw, uint8_t dhcp,
                             const char *snmp_read, const char *snmp_write, const char *snmp_trap);
void Settings_Load_From_Backup(ip4_addr_t *ip, ip4_addr_t *mask, ip4_addr_t *gw, uint8_t *dhcp,
                              char *snmp_read, int snmp_read_size,
                              char *snmp_write, int snmp_write_size,
                              char *snmp_trap, int snmp_trap_size);


#ifdef __cplusplus
}
#endif

#endif
