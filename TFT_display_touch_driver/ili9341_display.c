/**
 * @file ili9341_display.c
 * @brief ILI9341 Display Driver Implementation
 */

#include "ili9341_display.h"
#include "hardware/gpio.h"
#include <string.h>

// ILI9341 Commands
#define ILI9341_SWRESET   0x01   // Software reset
#define ILI9341_SLPIN     0x10   // Sleep in
#define ILI9341_SLPOUT    0x11   // Sleep out
#define ILI9341_DISPOFF   0x28   // Display off
#define ILI9341_DISPON    0x29   // Display on
#define ILI9341_CASET     0x2A   // Column address set
#define ILI9341_PASET     0x2B   // Page address set
#define ILI9341_RAMWR     0x2C   // RAM write
#define ILI9341_RAMRD     0x2E   // RAM read
#define ILI9341_MADCTL    0x36   // Memory access control
#define ILI9341_PIXFMT    0x3A   // Pixel format set
#define ILI9341_PWCTR1    0xC0   // Power control 1
#define ILI9341_PWCTR2    0xC1   // Power control 2
#define ILI9341_VMCTR1    0xC5   // VCOM control 1
#define ILI9341_VMCTR2    0xC7   // VCOM control 2
#define ILI9341_FRMCTR1   0xB1   // Frame rate control
#define ILI9341_FRMCTR2   0xB2   // Frame rate control 2
#define ILI9341_FRMCTR3   0xB3   // Frame rate control 3
#define ILI9341_INVCTR    0xB4   // Display inversion control
#define ILI9341_GAMMASET  0x26   // Gamma set
#define ILI9341_PGAMMA    0xE0   // Positive gamma correction
#define ILI9341_NGAMMA    0xE1   // Negative gamma correction
#define ILI9341_IFCTL     0xF6   // Interface control
#define ILI9341_WDBKCTR   0xA4   // Write DBC

// Global display handle for static functions
static const ili9341_t *_display = NULL;

// Simple 5x7 bitmap font for ASCII 32-126
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 Space
    {0x00, 0x00, 0x4F, 0x00, 0x00}, // 33 !
    {0x00, 0x03, 0x00, 0x03, 0x00}, // 34 "
    {0x14, 0x3E, 0x14, 0x3E, 0x14}, // 35 #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 $
    {0x43, 0x33, 0x08, 0x66, 0x61}, // 37 %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 &
    {0x00, 0x00, 0x03, 0x00, 0x00}, // 39 '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 -
    {0x00, 0x30, 0x30, 0x00, 0x00}, // 46 .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 ?
    {0x3E, 0x41, 0x5D, 0x55, 0x1E}, // 64 @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 L
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // 77 M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 R
    {0x26, 0x49, 0x49, 0x49, 0x32}, // 83 S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 87 W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 \
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // 98 b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // 100 d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // 102 f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // 103 g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // 104 h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // 105 i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // 106 j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // 107 k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // 108 l
    {0x78, 0x04, 0x18, 0x04, 0x78}, // 109 m
    {0x78, 0x04, 0x04, 0x04, 0x78}, // 110 n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // 112 p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // 113 q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // 114 r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // 116 t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 z
};

// Forward declarations
static void ili9341_write_command(uint8_t cmd);
static void ili9341_write_data(const uint8_t *data, uint32_t len);
static void ili9341_set_column(uint16_t x0, uint16_t x1);
static void ili9341_set_row(uint16_t y0, uint16_t y1);
static void ili9341_write_pixel(uint16_t color);

// ============ Helper Functions ============

static void ili9341_write_command(uint8_t cmd)
{
    if (!_display) return;
    
    // DC = 0 for command
    gpio_put(_display->dc_pin, 0);
    spi_write_byte(&_display->spi, cmd);
}

static void ili9341_write_data(const uint8_t *data, uint32_t len)
{
    if (!_display) return;
    
    // DC = 1 for data
    gpio_put(_display->dc_pin, 1);
    spi_write(&_display->spi, data, len);
}

static void ili9341_write_pixel(uint16_t color)
{
    uint8_t data[2] = {
        (uint8_t)((color >> 8) & 0xFF),
        (uint8_t)(color & 0xFF)
    };
    ili9341_write_data(data, 2);
}

static void ili9341_set_column(uint16_t x0, uint16_t x1)
{
    ili9341_write_command(ILI9341_CASET);
    uint8_t data[4] = {
        (uint8_t)((x0 >> 8) & 0xFF), (uint8_t)(x0 & 0xFF),
        (uint8_t)((x1 >> 8) & 0xFF), (uint8_t)(x1 & 0xFF)
    };
    ili9341_write_data(data, 4);
}

static void ili9341_set_row(uint16_t y0, uint16_t y1)
{
    ili9341_write_command(ILI9341_PASET);
    uint8_t data[4] = {
        (uint8_t)((y0 >> 8) & 0xFF), (uint8_t)(y0 & 0xFF),
        (uint8_t)((y1 >> 8) & 0xFF), (uint8_t)(y1 & 0xFF)
    };
    ili9341_write_data(data, 4);
}

// ============ Public API ============

bool ili9341_init(const ili9341_t *display)
{
    if (!display) {
        return false;
    }

    _display = display;

    // Initialize SPI
    if (!spi_device_init(&display->spi)) {
        return false;
    }

    // Initialize control pins
    gpio_init(display->reset_pin);
    gpio_set_dir(display->reset_pin, GPIO_OUT);
    gpio_put(display->reset_pin, 0);
    sleep_ms(10);
    gpio_put(display->reset_pin, 1);
    sleep_ms(100);

    gpio_init(display->dc_pin);
    gpio_set_dir(display->dc_pin, GPIO_OUT);

    // Initialize display
    ili9341_write_command(ILI9341_SWRESET);
    sleep_ms(150);

    ili9341_write_command(ILI9341_PIXFMT);
    ili9341_write_data((uint8_t[]){0x55}, 1);  // 16-bit RGB565

    ili9341_write_command(ILI9341_FRMCTR1);
    ili9341_write_data((uint8_t[]){0x00, 0x1F}, 2);

    ili9341_write_command(ILI9341_PWCTR1);
    ili9341_write_data((uint8_t[]){0x23}, 1);

    ili9341_write_command(ILI9341_PWCTR2);
    ili9341_write_data((uint8_t[]){0x10}, 1);

    ili9341_write_command(ILI9341_VMCTR1);
    ili9341_write_data((uint8_t[]){0x3E, 0x28}, 2);

    ili9341_write_command(ILI9341_VMCTR2);
    ili9341_write_data((uint8_t[]){0x86}, 1);

    ili9341_write_command(ILI9341_MADCTL);
    // 0x48 = MX|BGR  → portrait  280×320
    // 0x28 = MV|BGR  → landscape 320×280 (row/column swap via MV bit)
    uint8_t madctl = display->rotation ? 0x28 : 0x48;
    ili9341_write_data(&madctl, 1);

    ili9341_write_command(ILI9341_INVCTR);
    ili9341_write_data((uint8_t[]){0x00}, 1);

    ili9341_write_command(ILI9341_SLPOUT);
    sleep_ms(120);

    ili9341_write_command(ILI9341_DISPON);
    sleep_ms(10);

    return true;
}

void ili9341_reset(const ili9341_t *display)
{
    if (!display) return;
    
    gpio_put(display->reset_pin, 0);
    sleep_ms(10);
    gpio_put(display->reset_pin, 1);
    sleep_ms(100);
}

void ili9341_fill_screen(const ili9341_t *display, uint16_t color)
{
    if (!display) return;
    ili9341_fill_rect(display, 0, 0,
                      ili9341_width(display)  - 1,
                      ili9341_height(display) - 1,
                      color);
}

void ili9341_fill_rect(const ili9341_t *display, 
                       uint16_t x0, uint16_t y0, 
                       uint16_t x1, uint16_t y1, 
                       uint16_t color)
{
    if (!display) return;

    _display = display;

    // Bounds check
    if (x0 > x1) { uint16_t tmp = x0; x0 = x1; x1 = tmp; }
    if (y0 > y1) { uint16_t tmp = y0; y0 = y1; y1 = tmp; }
    uint16_t max_x = ili9341_width(display)  - 1;
    uint16_t max_y = ili9341_height(display) - 1;
    if (x1 > max_x) x1 = max_x;
    if (y1 > max_y) y1 = max_y;

    // Set drawing window
    ili9341_set_column(x0, x1);
    ili9341_set_row(y0, y1);

    // Send RAMWR then stream all pixels in one CS-asserted transaction.
    // Pre-filling a line buffer and repeating per row avoids per-pixel CS toggles,
    // reducing a 280x320 fill from ~90k SPI transactions to 320.
    ili9341_write_command(ILI9341_RAMWR);

    uint16_t w = x1 - x0 + 1;
    uint16_t h = y1 - y0 + 1;

    // ILI9341_HEIGHT is the largest possible row width (landscape mode).
    static uint8_t line_buf[ILI9341_HEIGHT * 2];
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    for (uint16_t i = 0; i < w; i++) {
        line_buf[i * 2]     = hi;
        line_buf[i * 2 + 1] = lo;
    }

    gpio_put(_display->dc_pin, 1);
    spi_cs_assert(&_display->spi);
    for (uint16_t row = 0; row < h; row++) {
        spi_write_raw(&_display->spi, line_buf, (uint32_t)w * 2);
    }
    spi_cs_deassert(&_display->spi);
}

void ili9341_draw_rect(const ili9341_t *display,
                       uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color, uint8_t thickness)
{
    if (!display) return;

    // Draw 4 lines (top, bottom, left, right)
    ili9341_draw_h_line(display, x0, x1, y0, color, thickness);  // Top
    ili9341_draw_h_line(display, x0, x1, y1, color, thickness);  // Bottom
    ili9341_draw_v_line(display, x0, y0, y1, color, thickness);  // Left
    ili9341_draw_v_line(display, x1, y0, y1, color, thickness);  // Right
}

void ili9341_draw_h_line(const ili9341_t *display,
                         uint16_t x0, uint16_t x1, uint16_t y,
                         uint16_t color, uint8_t thickness)
{
    if (!display) return;

    for (uint8_t i = 0; i < thickness; i++) {
        if (y + i < ILI9341_HEIGHT) {
            ili9341_fill_rect(display, x0, y + i, x1, y + i, color);
        }
    }
}

void ili9341_draw_v_line(const ili9341_t *display,
                         uint16_t x, uint16_t y0, uint16_t y1,
                         uint16_t color, uint8_t thickness)
{
    if (!display) return;

    for (uint8_t i = 0; i < thickness; i++) {
        if (x + i < ILI9341_WIDTH) {
            ili9341_fill_rect(display, x + i, y0, x + i, y1, color);
        }
    }
}

void ili9341_draw_pixel(const ili9341_t *display,
                        uint16_t x, uint16_t y,
                        uint16_t color)
{
    if (!display) return;
    ili9341_fill_rect(display, x, y, x, y, color);
}

void ili9341_write_char(const ili9341_t *display,
                        uint16_t x, uint16_t y, char ch,
                        uint16_t fg_color, uint16_t bg_color,
                        uint8_t scale)
{
    if (!display || ch < 32 || ch > 126) {
        return;
    }

    const uint8_t *glyph = font_5x7[ch - 32];

    for (int i = 0; i < 5; i++) {
        uint8_t byte = glyph[i];
        for (int j = 0; j < 7; j++) {
            uint16_t color = (byte & (1 << j)) ? fg_color : bg_color;
            
            // Draw scaled pixel
            for (uint8_t si = 0; si < scale; si++) {
                for (uint8_t sj = 0; sj < scale; sj++) {
                    ili9341_draw_pixel(display, 
                                     x + i * scale + si, 
                                     y + j * scale + sj, 
                                     color);
                }
            }
        }
    }
}

uint16_t ili9341_write_string(const ili9341_t *display,
                              uint16_t x, uint16_t y, const char *str,
                              uint16_t fg_color, uint16_t bg_color,
                              uint8_t scale)
{
    if (!display || !str) {
        return x;
    }

    uint16_t cur_x = x;
    uint16_t char_width = 5 * scale + 1;  // 5px width + 1px spacing

    while (*str) {
        ili9341_write_char(display, cur_x, y, *str, fg_color, bg_color, scale);
        cur_x += char_width;
        str++;
    }

    return cur_x;
}

void ili9341_sleep(const ili9341_t *display, bool sleep)
{
    if (!display) return;

    _display = display;
    ili9341_write_command(sleep ? ILI9341_SLPIN : ILI9341_SLPOUT);
    sleep_ms(10);
}

void ili9341_set_brightness(const ili9341_t *display, uint8_t brightness)
{
    if (!display) return;
    
    _display = display;
    // Note: PWM control pin not defined, using WDBKCTR as alternative
    // In real implementation, use GPIO PWM pin for brightness control
}
