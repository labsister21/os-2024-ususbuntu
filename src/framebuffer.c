#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/driver/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

struct FramebufferState framebuffer_state = {
    .cur_col = 0,
    .cur_row = 0
};

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    // TODO : Implement

    uint16_t pos = r * 80 + c;
    out(CURSOR_PORT_CMD, 0x0F);
    out(CURSOR_PORT_DATA, (uint8_t)(pos & 0xFF));
    out(CURSOR_PORT_CMD, 0x0E);
    out(CURSOR_PORT_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    // TODO : Implement

    uint16_t position = row * 80 + col;
    uint8_t color = (bg << 4) | (fg & 0x0F);
    FRAMEBUFFER_MEMORY_OFFSET[position * 2] = c;
    FRAMEBUFFER_MEMORY_OFFSET[position * 2 + 1] = color;
}

void framebuffer_clear(void) {
    // TODO : Implement
    for (int i = 0; i < 25; i++) {
        for (int j = 0; j < 80; j++) {
            framebuffer_write(i, j, 0x00, 0x07, 0x00);
        }
    }
}