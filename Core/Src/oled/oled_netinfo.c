#include "ssd1306_driver/ssd1306.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include <stdio.h>
#include <string.h>
#include "ssd1306_driver/ssd1306_fonts.h"


#define SSD1306_ROTATED_WIDTH  (SSD1306_HEIGHT)
#define SSD1306_ROTATED_HEIGHT (SSD1306_WIDTH)

/**
 * @brief Нарисовать IPv4-адрес с переносом строки.
 *        Пример: 192.168.0.1 -> "192.168.0.\n1"
 *
 * @param x   позиция курсора X
 * @param y   позиция курсора Y
 * @param addr указатель на ip4_addr_t
 */
void OLED_DrawIP_Split(uint8_t x, uint8_t y, const ip4_addr_t *addr) {
    char line[20];

    if (!addr) return;

    uint8_t o1 = ip4_addr1_16(addr);
    uint8_t o2 = ip4_addr2_16(addr);
    uint8_t o3 = ip4_addr3_16(addr);
    uint8_t o4 = ip4_addr4_16(addr);

    // Вторая строка: первые два октета
    snprintf(line, sizeof(line), "%u.%u.", o1, o2);
    ssd1306_SetCursor(x, y);
    ssd1306_WriteString(line, Font_7x10, White);

    // Третья строка: последние два октета
    snprintf(line, sizeof(line), "%u.%u", o3, o4);
    ssd1306_SetCursor(x, y + Font_7x10.height + 2);
    ssd1306_WriteString(line, Font_7x10, White);
}



/**
 * @brief Вывод IP / MASK / GW на экран (с переносами).
 *
 * @param netif  структура сетевого интерфейса LwIP
 * @param startX начальная позиция X
 * @param startY начальная позиция Y
 */
void OLED_DrawNetInfo(struct netif *netif, uint8_t startX, uint8_t startY) {
	char buffer[64];
	uint8_t lineY = startY;

    ssd1306_Fill(Black);

    ssd1306_SetCursor(startX, startY);
    ssd1306_WriteString("NETINF", Font_7x10, White);

    // Полоса под заголовком
    ssd1306_DrawRectangle(0, startY + Font_7x10.height + 1, SSD1306_ROTATED_WIDTH-1, startY + Font_7x10.height + 2, White);

    // IP
    lineY = startY + Font_7x10.height + 6;
    ssd1306_SetCursor(startX, lineY);
    ssd1306_WriteString("IP:", Font_7x10, White);
    OLED_DrawIP_Split(startX , lineY + Font_7x10.height + 2, netif_ip4_addr(netif));

    // линия-разделитель
    lineY += 3*(Font_7x10.height + 1);
    ssd1306_DrawRectangle(0, lineY, SSD1306_ROTATED_WIDTH-1, lineY, White);

    // MASK
    lineY += 4;
    ssd1306_SetCursor(startX, lineY);
    ssd1306_WriteString("MASK:", Font_7x10, White);
    OLED_DrawIP_Split(startX, lineY + Font_7x10.height + 2, netif_ip4_netmask(netif));

    // линия-разделитель
    lineY += 3*(Font_7x10.height + 1);
    ssd1306_DrawRectangle(0, lineY, SSD1306_ROTATED_WIDTH-1, lineY, White);

    // GW
    lineY += 4;
    ssd1306_SetCursor(startX, lineY);
    ssd1306_WriteString("GW:", Font_7x10, White);
    OLED_DrawIP_Split(startX, lineY + Font_7x10.height + 2, netif_ip4_gw(netif));
    // отправить буфер на дисплей
    ssd1306_UpdateScreen();
}
