/**
 * @file xpt2046_touch.c
 * @brief XPT2046 Touch Controller Implementation
 */

#include "xpt2046_touch.h"
#include "hardware/gpio.h"
#include <stdio.h>

// XPT2046 Command byte structure
#define XPT2046_X_READ      0xD0    // Read X position
#define XPT2046_Y_READ      0x90    // Read Y position
#define XPT2046_Z1_READ     0xB0    // Read Z1 (pressure)
#define XPT2046_Z2_READ     0xC0    // Read Z2 (pressure)

// Averaging filter depth
#define TOUCH_SAMPLES       5

// Global touch handle for static functions
static const xpt2046_t *_touch = NULL;

// ============ Helper Functions ============

static uint16_t xpt2046_read_adc(uint8_t cmd)
{
    if (!_touch) return 0;

    uint8_t tx[3] = {cmd, 0, 0};
    uint8_t rx[3] = {0, 0, 0};

    spi_write_read(&_touch->spi, tx, rx, 3);

    // XPT2046 returns 12-bit result left-aligned in 15 bits:
    // rx[1] carries bits[11:5], rx[2] carries bits[4:0] in the upper 5 bits.
    // Shift right by 3 to get the true 12-bit value (0-4095).
    return (((uint16_t)rx[1] << 8) | rx[2]) >> 3;
}

static uint16_t xpt2046_read_adc_avg(uint8_t cmd, uint8_t samples)
{
    uint32_t sum = 0;
    
    for (uint8_t i = 0; i < samples; i++) {
        sum += xpt2046_read_adc(cmd);
    }

    return (uint16_t)(sum / samples);
}

// ============ Public API ============

bool xpt2046_init(const xpt2046_t *touch)
{
    if (!touch) {
        return false;
    }

    _touch = touch;

    // Initialize SPI
    if (!spi_device_init(&touch->spi)) {
        return false;
    }

    // Initialize IRQ pin if provided
    if (touch->irq_pin < 32) {
        gpio_init(touch->irq_pin);
        gpio_set_dir(touch->irq_pin, GPIO_IN);
    }

    // Default calibration values — approximate for a 280x320 display.
    // ADC values are 0-4095 (12-bit) after the >> 3 shift applied in xpt2046_read_adc.
    // x_offset / y_offset are the raw ADC values at the top-left touch corner.
    ((xpt2046_t *)touch)->x_scale = 280.0f / 3600.0f;
    ((xpt2046_t *)touch)->x_offset = 200.0f;
    ((xpt2046_t *)touch)->y_scale = 320.0f / 3600.0f;
    ((xpt2046_t *)touch)->y_offset = 200.0f;
    ((xpt2046_t *)touch)->z_threshold = 200;
    ((xpt2046_t *)touch)->display_width = 280;
    ((xpt2046_t *)touch)->display_height = 320;

    return true;
}

bool xpt2046_read_raw(const xpt2046_t *touch, touch_point_t *point)
{
    if (!touch || !point) {
        return false;
    }

    _touch = touch;

    bool pressed;

    if (touch->irq_pin < 32) {
        // PENIRQ is available: active-low signal is the most reliable touch indicator.
        pressed = !gpio_get(touch->irq_pin);
        point->z = pressed ? (touch->z_threshold + 1u) : 0u;
    } else {
        // No PENIRQ: use Z1 measurement.
        // Z1 rises from ~0 to several hundred when the screen is touched.
        // Z2 alone or Z1-Z2 are less reliable due to floating-input variation.
        uint16_t z1 = xpt2046_read_adc_avg(XPT2046_Z1_READ, TOUCH_SAMPLES);
        point->z = z1;
        pressed = (z1 > touch->z_threshold);
    }

    point->pressed = pressed;

    if (pressed) {
        point->x = xpt2046_read_adc_avg(XPT2046_X_READ, TOUCH_SAMPLES);
        point->y = xpt2046_read_adc_avg(XPT2046_Y_READ, TOUCH_SAMPLES);
    } else {
        point->x = 0;
        point->y = 0;
    }

    return pressed;
}

bool xpt2046_read(const xpt2046_t *touch, touch_point_t *point)
{
    if (!touch || !point) {
        return false;
    }

    // Read raw data
    if (!xpt2046_read_raw(touch, point)) {
        return false;
    }

    // Apply calibration
    if (point->pressed) {
        // Convert raw ADC to pixel coordinates: pixel = (adc - offset) * scale.
        // Use signed 32-bit intermediates so underflow wraps safely before clamping.
        int32_t x_raw = (int32_t)point->x - (int32_t)touch->x_offset;
        int32_t y_raw = (int32_t)point->y - (int32_t)touch->y_offset;

        if (x_raw < 0) x_raw = 0;
        if (y_raw < 0) y_raw = 0;

        int32_t px = (int32_t)(x_raw * touch->x_scale);
        int32_t py = (int32_t)(y_raw * touch->y_scale);

        uint16_t max_x = touch->display_width  > 0 ? touch->display_width  - 1 : 279;
        uint16_t max_y = touch->display_height > 0 ? touch->display_height - 1 : 319;

        point->x = (uint16_t)(px <= max_x ? px : max_x);
        point->y = (uint16_t)(py <= max_y ? py : max_y);
    }

    return point->pressed;
}

bool xpt2046_calibrate(xpt2046_t *touch, uint16_t display_width, uint16_t display_height)
{
    if (!touch) {
        return false;
    }

    // Calibration requires external display interaction
    // This is a placeholder that sets default values
    // In real implementation, this would:
    // 1. Display calibration points on screen
    // 2. Read touch coordinates at each point
    // 3. Calculate scale and offset
    // 4. Store calibration data

    _touch = touch;

    touch->x_scale = (float)display_width  / 3600.0f;
    touch->x_offset = 200.0f;
    touch->y_scale = (float)display_height / 3600.0f;
    touch->y_offset = 200.0f;
    touch->display_width  = display_width;
    touch->display_height = display_height;

    return true;
}

void xpt2046_set_calibration(xpt2046_t *touch,
                             float x_scale, float x_offset,
                             float y_scale, float y_offset)
{
    if (!touch) {
        return;
    }

    touch->x_scale = x_scale;
    touch->x_offset = x_offset;
    touch->y_scale = y_scale;
    touch->y_offset = y_offset;
}
