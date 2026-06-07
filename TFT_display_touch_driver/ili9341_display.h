/**
 * @file ili9341_display.h
 * @brief ILI9341 TFT Display Driver (280x320)
 * 
 * Fasizi 2.8" TFT Display with ILI9341 controller
 * Resolution: 280x320 pixels (or 320x240, configurable)
 * Interface: SPI (8-bit or 16-bit data bus via SPI)
 */

#ifndef ILI9341_DISPLAY_H
#define ILI9341_DISPLAY_H

#include "spi_interface.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * Display dimensions
 */
#define ILI9341_WIDTH   280
#define ILI9341_HEIGHT  320

/**
 * Color definitions (RGB565 format: RRRRRGGGGGGBBBBB)
 */
#define ILI9341_BLACK   0x0000
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_BLUE    0x001F
#define ILI9341_WHITE   0xFFFF
#define ILI9341_CYAN    0x07FF
#define ILI9341_MAGENTA 0xF81F
#define ILI9341_YELLOW  0xFFE0
#define ILI9341_GRAY    0x7BEF

/**
 * ILI9341 display configuration
 */
typedef struct {
    spi_device_t spi;       // SPI device config
    uint32_t reset_pin;     // Reset pin (GPIO)
    uint32_t dc_pin;        // Data/Command pin (GPIO)
    bool rotation;          // false = portrait 280x320, true = landscape 320x280
} ili9341_t;

/**
 * Return effective display width, accounting for rotation.
 */
static inline uint16_t ili9341_width(const ili9341_t *display)
{
    return display->rotation ? ILI9341_HEIGHT : ILI9341_WIDTH;
}

/**
 * Return effective display height, accounting for rotation.
 */
static inline uint16_t ili9341_height(const ili9341_t *display)
{
    return display->rotation ? ILI9341_WIDTH : ILI9341_HEIGHT;
}

/**
 * Initialize ILI9341 display
 * 
 * @param display Display configuration
 * @return true on success
 */
bool ili9341_init(const ili9341_t *display);

/**
 * Reset display
 * 
 * @param display Display handle
 */
void ili9341_reset(const ili9341_t *display);

/**
 * Fill entire screen with color
 * 
 * @param display Display handle
 * @param color RGB565 color
 */
void ili9341_fill_screen(const ili9341_t *display, uint16_t color);

/**
 * Fill rectangular area with color
 * 
 * @param display Display handle
 * @param x0 Left coordinate
 * @param y0 Top coordinate
 * @param x1 Right coordinate (inclusive)
 * @param y1 Bottom coordinate (inclusive)
 * @param color RGB565 color
 */
void ili9341_fill_rect(const ili9341_t *display, 
                       uint16_t x0, uint16_t y0, 
                       uint16_t x1, uint16_t y1, 
                       uint16_t color);

/**
 * Draw rectangle outline
 * 
 * @param display Display handle
 * @param x0 Left coordinate
 * @param y0 Top coordinate
 * @param x1 Right coordinate
 * @param y1 Bottom coordinate
 * @param color RGB565 color
 * @param thickness Line thickness in pixels
 */
void ili9341_draw_rect(const ili9341_t *display,
                       uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color, uint8_t thickness);

/**
 * Draw horizontal line
 * 
 * @param display Display handle
 * @param x0 Start X
 * @param x1 End X
 * @param y Y coordinate
 * @param color RGB565 color
 * @param thickness Line thickness
 */
void ili9341_draw_h_line(const ili9341_t *display,
                         uint16_t x0, uint16_t x1, uint16_t y,
                         uint16_t color, uint8_t thickness);

/**
 * Draw vertical line
 * 
 * @param display Display handle
 * @param x X coordinate
 * @param y0 Start Y
 * @param y1 End Y
 * @param color RGB565 color
 * @param thickness Line thickness
 */
void ili9341_draw_v_line(const ili9341_t *display,
                         uint16_t x, uint16_t y0, uint16_t y1,
                         uint16_t color, uint8_t thickness);

/**
 * Draw single pixel
 * 
 * @param display Display handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param color RGB565 color
 */
void ili9341_draw_pixel(const ili9341_t *display,
                        uint16_t x, uint16_t y,
                        uint16_t color);

/**
 * Write character at position
 * 
 * @param display Display handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param ch Character to write
 * @param fg_color Foreground color
 * @param bg_color Background color
 * @param scale Font scale (1, 2, 3...)
 */
void ili9341_write_char(const ili9341_t *display,
                        uint16_t x, uint16_t y, char ch,
                        uint16_t fg_color, uint16_t bg_color,
                        uint8_t scale);

/**
 * Write string at position
 * 
 * @param display Display handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param str String to write (null-terminated)
 * @param fg_color Foreground color
 * @param bg_color Background color
 * @param scale Font scale
 * @return Next X position after string
 */
uint16_t ili9341_write_string(const ili9341_t *display,
                              uint16_t x, uint16_t y, const char *str,
                              uint16_t fg_color, uint16_t bg_color,
                              uint8_t scale);

/**
 * Sleep mode (low power)
 * 
 * @param display Display handle
 * @param sleep true = sleep, false = wake
 */
void ili9341_sleep(const ili9341_t *display, bool sleep);

/**
 * Set display brightness
 * 
 * @param display Display handle
 * @param brightness 0-255 (0 = off, 255 = max)
 */
void ili9341_set_brightness(const ili9341_t *display, uint8_t brightness);

#endif // ILI9341_DISPLAY_H
