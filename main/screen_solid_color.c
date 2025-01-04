//
// Created by sergy on 1/3/2025.
//
/*
    This function fills a screen with a solid color.
    Remember to define the SPI handle, IO, Screen, and have the screen component
*/

static void fillScreen(esp_lcd_panel_handle_t panel_handle, uint16_t color) {
    // Fill the buffer with red
    for (int i = 0; i < 320 * 40; i++) {
        buffer[i] = __builtin_bswap16(color);
    }

    // Draw screen in chunks
    for (int y = 0; y < 320; y += 40) { // Draw in chunks
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y + 40, buffer));
    }
}