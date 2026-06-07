/**
 * @file xpt2046_touch.h
 * @brief XPT2046 Capacitive Touch Controller Driver
 */

#ifndef XPT2046_TOUCH_H
#define XPT2046_TOUCH_H

#include "spi_interface.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * Touch point data
 */
typedef struct {
    uint16_t x;          // X coordinate (0 to 279 for 280px display)
    uint16_t y;          // Y coordinate (0 to 319 for 320px display)
    bool pressed;        // true if touch detected
    uint16_t z;          // Pressure (0-4095), 0 = not pressed
} touch_point_t;

/**
 * XPT2046 driver configuration
 */
typedef struct {
    spi_device_t spi;       // SPI device config
    uint32_t irq_pin;       // Interrupt pin (GPIO, optional)
    
    // Calibration constants
    float x_scale;           // Raw X ADC to pixel X
    float x_offset;          // Raw X ADC offset (in ADC units, after >> 3 shift)
    float y_scale;           // Raw Y ADC to pixel Y
    float y_offset;          // Raw Y ADC offset (in ADC units, after >> 3 shift)
    uint16_t z_threshold;    // Z1 threshold for touch detection (0-4095 after ADC shift)
    uint16_t display_width;  // Display width in pixels (used for coordinate clamping)
    uint16_t display_height; // Display height in pixels
} xpt2046_t;

/**
 * Initialize XPT2046 touch controller
 * 
 * @param touch Touch controller configuration
 * @return true on success
 */
bool xpt2046_init(const xpt2046_t *touch);

/**
 * Read raw touch data from XPT2046
 * 
 * @param touch Touch controller
 * @param point Pointer to touch_point_t to store result
 * @return true if touch detected, false otherwise
 */
bool xpt2046_read_raw(const xpt2046_t *touch, touch_point_t *point);

/**
 * Read and calibrate touch data
 * 
 * @param touch Touch controller
 * @param point Pointer to touch_point_t to store result
 * @return true if touch detected, false otherwise
 */
bool xpt2046_read(const xpt2046_t *touch, touch_point_t *point);

/**
 * Perform touch calibration
 * User must touch 3 corner points displayed on screen
 * 
 * @param touch Touch controller
 * @param display_width Display width in pixels
 * @param display_height Display height in pixels
 * @return true on successful calibration
 */
bool xpt2046_calibrate(xpt2046_t *touch, uint16_t display_width, uint16_t display_height);

/**
 * Set calibration manually
 * 
 * @param touch Touch controller
 * @param x_scale Pixel per ADC unit in X
 * @param x_offset ADC offset for X
 * @param y_scale Pixel per ADC unit in Y
 * @param y_offset ADC offset for Y
 */
void xpt2046_set_calibration(xpt2046_t *touch,
                             float x_scale, float x_offset,
                             float y_scale, float y_offset);

#endif // XPT2046_TOUCH_H
