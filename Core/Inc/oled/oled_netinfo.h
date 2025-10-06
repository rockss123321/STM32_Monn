#ifndef __OLED_NETINFO_H
#define __OLED_NETINFO_H

#include "ssd1306_driver/ssd1306.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

/**
 * @brief Нарисовать IPv4-адрес с переносом строки.
 *        Пример: 192.168.0.1 -> "192.168.0.\n1"
 *
 * @param x    позиция курсора X
 * @param y    позиция курсора Y
 * @param addr указатель на ip4_addr_t
 */
void OLED_DrawIP_Split(uint8_t x, uint8_t y, const ip4_addr_t *addr);

/**
 * @brief Вывод IP / MASK / GW на экран (с переносами).
 *
 * @param netif  структура сетевого интерфейса LwIP
 * @param startX начальная позиция X
 * @param startY начальная позиция Y
 */
void OLED_DrawNetInfo(struct netif *netif, uint8_t startX, uint8_t startY);

#endif /* __OLED_NETINFO_H */
