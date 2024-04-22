#include "header/stdlib/keyboard.h"
#include "header/text/framebuffer.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"
#include "interrupt/idt.h"
#include "interrupt/interrupt.h"

const char keyboard_scancode_1_to_ascii_map[256] = {
      0, 0x1B, '1', '2', '3', '4', '5', '6',  '7', '8', '9',  '0',  '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', '\n',   0,  'a',  's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\',  'z', 'x',  'c',  'v',
    'b',  'n', 'm', ',', '.', '/',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '-',    0,    0,   0,  '+',    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,

      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
};

char shift_map[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // a
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // b
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // c
    0, 0, 0, 0, 0, 0, 0, 0, 0, '"',
    0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
    '@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
    0, '+', 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, '{', '|', '}', 0, 0, '~'

};

static struct KeyboardDriverState keyboard_state;
static struct KeyboardEvents events;

/* -- Driver Interfaces -- */

// Activate keyboard ISR / start listen keyboard & save to buffer
void keyboard_state_activate(void) {
    memset(keyboard_state.keyboard_buffer, '\0', sizeof(keyboard_state.keyboard_buffer));
    keyboard_state.buffer_index = 0;
    keyboard_state.text_mode = false;
    keyboard_state.keyboard_input_on = true;
    keyboard_state.shift_left = false;
    keyboard_state.shift_right = false;
    keyboard_state.capslock = false;
    keyboard_state.ctrl = false;
    keyboard_state.aborted = false;
    framebuffer_state.start_col = framebuffer_state.col;
    framebuffer_state.start_row = framebuffer_state.row;
    framebuffer_set_cursor(framebuffer_state.row, framebuffer_state.col);
}

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void) {
    keyboard_state.keyboard_input_on = false;
    disable_cursor();
}

void keyboard_text_mode(void) {
    keyboard_state.text_mode = true;
}

// Get keyboard buffer values - @param buf Pointer to char buffer, recommended size at least KEYBOARD_BUFFER_SIZE
void get_keyboard_buffer(char *buf) {
    char newC = keyboard_state.keyboard_buffer[0]; 
    *buf = newC;
    keyboard_state.keyboard_buffer[0] = '\0';
}

void get_keyboard_events(struct KeyboardEvents *buf) {
    memcpy(buf, &events, sizeof(struct KeyboardEvents));
}

void reset_events(void) {
    memset(events.scancodes, false, sizeof(struct KeyboardEvents));
}

// Check whether keyboard ISR is active or not - @return Equal with keyboard_input_on value
bool is_keyboard_blocking(void) {
    return keyboard_state.keyboard_input_on;
}

bool is_keyboard_aborted(void) {
    return keyboard_state.aborted;
}

/* -- Keyboard Interrupt Service Routine -- */

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Will start listen and process keyboard scancode if keyboard_input_on.
 *
 * Will only print printable character into framebuffer.
 * Stop processing when enter key (line feed) is pressed.
 *
 * Note that, with keyboard interrupt & ISR, keyboard reading is non-blocking.
 * This can be made into blocking input with `while (is_keyboard_blocking());`
 * after calling `keyboard_state_activate();`
 */

bool is_shift() {
    return keyboard_state.shift_left || keyboard_state.shift_right;
}

uint8_t get_current_buffer_index() {
    return framebuffer_state.col - framebuffer_state.start_col + (framebuffer_state.row - framebuffer_state.start_row) * COLUMN;
}

void keyboard_isr(void) {
    uint8_t scancode = in(KEYBOARD_DATA_PORT);
    events.scancodes[scancode] = true;
    keyboard_state.keyboard_buffer[0] = (char) keyboard_scancode_1_to_ascii_map[scancode];
    char ascii;
    get_keyboard_buffer(&ascii);

    if (!keyboard_state.keyboard_input_on)
        keyboard_state.buffer_index = 0;
    else {
        // handle capslock and shift
        switch (scancode) {
        case 0x3a:
            keyboard_state.capslock ^= true;
            break;
        case 0x2a:
            keyboard_state.shift_left = true;
            break;
        case 0xaa:
            keyboard_state.shift_left = false;
            break;
        case 0x36:
            keyboard_state.shift_right = true;
            break;
        case 0xb6:
            keyboard_state.shift_right = false;
            break;
        case 0x1d:
            keyboard_state.ctrl = true;
            break;
        case 0x9d:
            keyboard_state.ctrl = false;
            break;
        case 0x4b:
            // left arrow
            if (get_current_buffer_index() == 0)
                break;
            framebuffer_state.col--;
            if (framebuffer_state.col < 0) {
                framebuffer_state.col = COLUMN - 1;
                framebuffer_state.row--;
            }
            framebuffer_set_cursor(framebuffer_state.row, framebuffer_state.col);
            break;
        case 0x4d:
            // right arrow
            // if (get_current_buffer_index() == keyboard_state.buffer_index)
                // break;
            framebuffer_state.col++;
            if (framebuffer_state.col == COLUMN) {
                framebuffer_state.col = 0;
                framebuffer_state.row++;
            }
            framebuffer_set_cursor(framebuffer_state.row, framebuffer_state.col);
            break;
        default:
            break;
        }
        char mapped_char = keyboard_scancode_1_to_ascii_map[scancode];

        // check char valid
        if (mapped_char == 0) {
            pic_ack(IRQ_KEYBOARD);
            return;
        }
        uint8_t current_buffer_index = get_current_buffer_index();
        if (keyboard_state.text_mode && keyboard_state.ctrl && (mapped_char == 'c' || mapped_char == 'C')) {
            // abort text mode
            keyboard_state.aborted = true;
            keyboard_state_deactivate();
        } else if (mapped_char == '\b') {
            // backspace
            if (keyboard_state.buffer_index > 0) {
                // Pindahkan karakter satu posisi ke kiri dalam buffer
                for (int i = current_buffer_index; i < keyboard_state.buffer_index; i++) {
                    keyboard_state.keyboard_buffer[i - 1] = keyboard_state.keyboard_buffer[i];
                }
                // Nolkan karakter terakhir dalam buffer
                keyboard_state.keyboard_buffer[keyboard_state.buffer_index - 1] = '\0';
                keyboard_state.buffer_index--;

                // Periksa apakah perlu menggeser posisi kursor framebuffer
                if (framebuffer_state.col > 0) {
                    framebuffer_state.col--; // Geser kursor satu kolom ke kiri
                } else if (framebuffer_state.row > 0) {
                    // Geser kursor ke kolom terakhir baris sebelumnya
                    framebuffer_state.row--;
                    framebuffer_state.col = COLUMN - 1;
                }

                // Hapus karakter terakhir dari framebuffer
                framebuffer_write(framebuffer_state.row, framebuffer_state.col, ' ', 0xFF, 0);

                // Perbarui posisi kursor framebuffer
                framebuffer_set_cursor(framebuffer_state.row, framebuffer_state.col);
            }
        }
        else if (mapped_char == '\n') {
            if (keyboard_state.text_mode) {
                // add newline if text mode
                keyboard_state.keyboard_buffer[keyboard_state.buffer_index] = '\n';
            }
            puts(&mapped_char, 1, 0xF);
            // Do not deactivate keyboard input on Enter press
            // keyboard_state_deactivate();
        } else {
            // check if letter needs to be uppercased
            if (mapped_char >= 'a' && mapped_char <= 'z') {
                if (keyboard_state.capslock ^ is_shift())
                    mapped_char = 'A' + mapped_char - 'a';
            }
            // map character to its shift representation if there any
            else if (is_shift() && mapped_char < 97 && shift_map[(uint8_t)mapped_char] != 0) {
                mapped_char = shift_map[(uint8_t)mapped_char];
            } for (int i = keyboard_state.buffer_index - 1; i >= current_buffer_index; i--) {
                keyboard_state.keyboard_buffer[i + 1] = keyboard_state.keyboard_buffer[i];
            }
            keyboard_state.keyboard_buffer[current_buffer_index] = mapped_char;
            keyboard_state.buffer_index++;
            int prevrow = framebuffer_state.row;
            int prevcol = framebuffer_state.col;
            prevcol++;
            if (prevcol == COLUMN) {
                prevcol = 0;
                prevrow++;
            }
            puts(keyboard_state.keyboard_buffer + current_buffer_index, keyboard_state.buffer_index - current_buffer_index, 0xF);
            framebuffer_state.row = prevrow;
            framebuffer_state.col = prevcol;
            framebuffer_set_cursor(framebuffer_state.row, framebuffer_state.col);
        }
        framebuffer_write(framebuffer_state.row, framebuffer_state.col, ' ', 0xFF, 0);
    }
    pic_ack(IRQ_KEYBOARD);
}