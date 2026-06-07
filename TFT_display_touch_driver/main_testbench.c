/**
 * @file main_testbench.c
 * @brief Testbench for ILI9341 Display + XPT2046 Touch
 * 
 * Tests:
 * 1. Color fill patterns
 * 2. Rectangle drawing
 * 3. Text rendering
 * 4. Touch coordinate reading
 * 5. Button hit detection
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "ili9341_display.h"
#include "xpt2046_touch.h"

// ============ GPIO PIN CONFIGURATION ============
// Adjust these based on your breadboard setup!

// SPI 0 (Display + AD9102)
#define SPI0_INST       spi0
#define SPI0_CLK        18
#define SPI0_MOSI       19
#define SPI0_MISO       16

// Display pins (ILI9341)
#define DISPLAY_CS      10
#define DISPLAY_DC      13
#define DISPLAY_RST     12

// Touch pins (XPT2046 on SPI1)
#define SPI1_INST       spi1
#define SPI1_CLK        14
#define SPI1_MOSI       15
#define SPI1_MISO       8
#define TOUCH_CS        9
#define TOUCH_IRQ       GP_UNUSED  // Not used in this test (set to high GPIO if not available)

#define GP_UNUSED       99         // Placeholder for unused pins

// ============ TEST STATES ============

typedef enum {
    TEST_COLORS,        // Display color patterns
    TEST_RECTS,         // Rectangle drawing
    TEST_TEXT,          // Text rendering
    TEST_TOUCH,         // Touch input display
    TEST_BUTTONS,       // Button interaction
    TEST_COUNT
} test_state_t;

static test_state_t current_test = TEST_COLORS;
static ili9341_t display;
static xpt2046_t touch;
static uint32_t last_update = 0;

// ============ HELPER FUNCTIONS ============

void print_debug(const char *msg)
{
    printf("[TESTBENCH] %s\n", msg);
}

// Simple button structure
typedef struct {
    uint16_t x0, y0, x1, y1;
    const char *label;
    uint16_t color;
    bool pressed;
} button_t;

void draw_button(button_t *btn, bool highlighted)
{
    uint16_t bg_color = highlighted ? ILI9341_YELLOW : btn->color;
    uint16_t border_color = ILI9341_WHITE;
    
    // Draw background
    ili9341_fill_rect(&display, btn->x0, btn->y0, btn->x1, btn->y1, bg_color);
    
    // Draw border
    ili9341_draw_rect(&display, btn->x0, btn->y0, btn->x1, btn->y1, border_color, 2);
    
    // Draw text (centered approximation)
    uint16_t text_x = btn->x0 + 5;
    uint16_t text_y = btn->y0 + 5;
    ili9341_write_string(&display, text_x, text_y, btn->label, 
                        ILI9341_BLACK, bg_color, 1);
}

bool point_in_rect(uint16_t px, uint16_t py, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    return (px >= x0) && (px <= x1) && (py >= y0) && (py <= y1);
}

// ============ TEST FUNCTIONS ============

void test_colors(void)
{
    static uint32_t color_index = 0;
    static uint32_t last_change = 0;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (now - last_change > 1000) {  // Change color every 1 second
        last_change = now;
        color_index = (color_index + 1) % 7;
        
        uint16_t color = ILI9341_BLACK;
        const char *name = "";
        
        switch (color_index) {
            case 0: color = ILI9341_RED;     name = "RED"; break;
            case 1: color = ILI9341_GREEN;   name = "GREEN"; break;
            case 2: color = ILI9341_BLUE;    name = "BLUE"; break;
            case 3: color = ILI9341_YELLOW;  name = "YELLOW"; break;
            case 4: color = ILI9341_CYAN;    name = "CYAN"; break;
            case 5: color = ILI9341_MAGENTA; name = "MAGENTA"; break;
            case 6: color = ILI9341_WHITE;   name = "WHITE"; break;
        }
        
        ili9341_fill_screen(&display, color);
        
        // Print text in opposite color
        uint16_t text_color = (color_index < 3) ? ILI9341_WHITE : ILI9341_BLACK;
        ili9341_write_string(&display, 50, 150, "COLOR TEST", text_color, color, 2);
        printf("[TEST] Color: %s\n", name);
    }
}

void test_rectangles(void)
{
    static uint32_t rect_index = 0;
    static uint32_t last_change = 0;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (now - last_change > 500) {
        last_change = now;
        
        // Clear screen
        ili9341_fill_screen(&display, ILI9341_BLACK);
        
        // Draw multiple rectangles
        ili9341_draw_rect(&display, 10, 10, 100, 100, ILI9341_RED, 2);
        ili9341_fill_rect(&display, 110, 10, 200, 100, ILI9341_GREEN);
        ili9341_draw_rect(&display, 210, 10, 270, 100, ILI9341_BLUE, 1);
        
        ili9341_fill_rect(&display, 10, 120, 100, 220, ILI9341_YELLOW);
        ili9341_draw_rect(&display, 110, 120, 200, 220, ILI9341_CYAN, 3);
        ili9341_fill_rect(&display, 210, 120, 270, 220, ILI9341_MAGENTA);
        
        // Draw text
        ili9341_write_string(&display, 80, 250, "RECTANGLE TEST", ILI9341_WHITE, ILI9341_BLACK, 1);
        
        printf("[TEST] Drawing rectangles\n");
    }
}

void test_text(void)
{
    static uint32_t last_update_text = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (now - last_update_text > 100) {  // Update every 100ms
        last_update_text = now;
        
        // Clear screen
        ili9341_fill_screen(&display, ILI9341_BLACK);
        
        // Draw text in different sizes and colors
        ili9341_write_string(&display, 10, 10, "TEXT TEST", ILI9341_WHITE, ILI9341_BLACK, 2);
        ili9341_write_string(&display, 10, 40, "Size 1x", ILI9341_CYAN, ILI9341_BLACK, 1);
        ili9341_write_string(&display, 10, 60, "Size 2x", ILI9341_YELLOW, ILI9341_BLACK, 2);
        ili9341_write_string(&display, 10, 100, "0123456789", ILI9341_GREEN, ILI9341_BLACK, 1);
        ili9341_write_string(&display, 10, 130, "ABCDEFGHIJ", ILI9341_RED, ILI9341_BLACK, 1);
        ili9341_write_string(&display, 10, 160, "abcdefghij", ILI9341_MAGENTA, ILI9341_BLACK, 1);
        
        // Display time
        static char time_str[32];
        uint32_t ms = to_ms_since_boot(get_absolute_time());
        snprintf(time_str, sizeof(time_str), "Time: %lu ms", ms);
        ili9341_write_string(&display, 10, 250, time_str, ILI9341_CYAN, ILI9341_BLACK, 1);
    }
}

void test_touch(void)
{
    touch_point_t point;
    
    // Read touch data
    if (xpt2046_read(&touch, &point)) {
        // Clear screen
        ili9341_fill_screen(&display, ILI9341_BLACK);
        
        // Draw crosshair at touch point (guard against uint16_t underflow near edges)
        uint16_t cv_top  = (point.y >= 20) ? (uint16_t)(point.y - 20) : 0;
        uint16_t ch_left = (point.x >= 20) ? (uint16_t)(point.x - 20) : 0;
        uint16_t sq_x0   = (point.x >=  5) ? (uint16_t)(point.x -  5) : 0;
        uint16_t sq_y0   = (point.y >=  5) ? (uint16_t)(point.y -  5) : 0;
        ili9341_draw_v_line(&display, point.x, cv_top, point.y + 20, ILI9341_RED, 2);
        ili9341_draw_h_line(&display, ch_left, point.x + 20, point.y, ILI9341_RED, 2);
        ili9341_fill_rect(&display, sq_x0, sq_y0, point.x + 5, point.y + 5, ILI9341_GREEN);
        
        // Display coordinates
        static char coord_str[64];
        snprintf(coord_str, sizeof(coord_str), "X: %u  Y: %u  Z: %u", 
                point.x, point.y, point.z);
        ili9341_write_string(&display, 10, 10, coord_str, ILI9341_WHITE, ILI9341_BLACK, 1);
        
        // Display status
        ili9341_write_string(&display, 10, 30, "TOUCH TEST - PRESS SCREEN", ILI9341_YELLOW, ILI9341_BLACK, 1);
    } else {
        // No touch, show message
        ili9341_fill_screen(&display, ILI9341_BLACK);
        ili9341_write_string(&display, 30, 150, "TOUCH TEST", ILI9341_CYAN, ILI9341_BLACK, 2);
        ili9341_write_string(&display, 20, 180, "PRESS THE DISPLAY", ILI9341_WHITE, ILI9341_BLACK, 1);
    }
}

void test_buttons(void)
{
    static button_t buttons[3] = {
        {30, 50, 130, 100, "RED", ILI9341_RED, false},
        {150, 50, 250, 100, "GREEN", ILI9341_GREEN, false},
        {30, 130, 130, 180, "BLUE", ILI9341_BLUE, false}
    };
    
    static uint32_t last_touch_read = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Read touch every 50ms
    if (now - last_touch_read > 50) {
        last_touch_read = now;
        
        touch_point_t point;
        
        // Clear previous button states
        for (int i = 0; i < 3; i++) {
            buttons[i].pressed = false;
        }
        
        // Check which button is pressed
        if (xpt2046_read(&touch, &point)) {
            for (int i = 0; i < 3; i++) {
                if (point_in_rect(point.x, point.y, 
                                 buttons[i].x0, buttons[i].y0, 
                                 buttons[i].x1, buttons[i].y1)) {
                    buttons[i].pressed = true;
                    printf("[BUTTON] Button %d pressed at (%u, %u)\n", i, point.x, point.y);
                }
            }
        }
    }
    
    // Draw screen
    ili9341_fill_screen(&display, ILI9341_BLACK);
    
    ili9341_write_string(&display, 50, 10, "BUTTON TEST", ILI9341_WHITE, ILI9341_BLACK, 2);
    
    for (int i = 0; i < 3; i++) {
        draw_button(&buttons[i], buttons[i].pressed);
    }
    
    ili9341_write_string(&display, 10, 250, "Press any button", ILI9341_CYAN, ILI9341_BLACK, 1);
}

// ============ MAIN LOOP ============

void run_testbench(void)
{
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Change test every 10 seconds
    if (now - last_update > 10000) {
        last_update = now;
        current_test = (current_test + 1) % TEST_COUNT;
        ili9341_fill_screen(&display, ILI9341_BLACK);
        printf("[TEST] Switching to test %d\n", current_test);
    }
    
    // Run current test
    switch (current_test) {
        case TEST_COLORS:
            test_colors();
            break;
        case TEST_RECTS:
            test_rectangles();
            break;
        case TEST_TEXT:
            test_text();
            break;
        case TEST_TOUCH:
            test_touch();
            break;
        case TEST_BUTTONS:
            test_buttons();
            break;
        default:
            break;
    }
}

// ============ ENTRY POINT ============

int main(void)
{
    // Initialize stdio
    stdio_init_all();
    sleep_ms(1000);
    print_debug("=== ILI9341 + XPT2046 TESTBENCH ===");
    
    // Configure display
    display.spi = (spi_device_t){
        .spi = SPI0_INST,
        .clk_pin = SPI0_CLK,
        .mosi_pin = SPI0_MOSI,
        .miso_pin = SPI0_MISO,
        .cs_pin = DISPLAY_CS,
        .baud_rate = 10000000,  // 10 MHz
        .cs_active_low = true
    };
    display.reset_pin = DISPLAY_RST;
    display.dc_pin = DISPLAY_DC;
    display.rotation = false;
    
    if (!ili9341_init(&display)) {
        print_debug("ERROR: Display init failed!");
        return 1;
    }
    print_debug("Display initialized");
    
    // Configure touch
    touch.spi = (spi_device_t){
        .spi = SPI1_INST,
        .clk_pin = SPI1_CLK,
        .mosi_pin = SPI1_MOSI,
        .miso_pin = SPI1_MISO,
        .cs_pin = TOUCH_CS,
        .baud_rate = 2000000,   // 2 MHz
        .cs_active_low = true
    };
    touch.irq_pin = TOUCH_IRQ;
    touch.z_threshold = 500;
    
    if (!xpt2046_init(&touch)) {
        print_debug("ERROR: Touch init failed!");
        return 1;
    }
    print_debug("Touch initialized");
    
    // Show splash screen
    ili9341_fill_screen(&display, ILI9341_BLUE);
    ili9341_write_string(&display, 40, 100, "TESTBENCH", ILI9341_WHITE, ILI9341_BLUE, 2);
    ili9341_write_string(&display, 30, 130, "ILI9341 + XPT2046", ILI9341_YELLOW, ILI9341_BLUE, 1);
    ili9341_write_string(&display, 50, 160, "INITIALIZED", ILI9341_GREEN, ILI9341_BLUE, 1);
    sleep_ms(2000);
    
    print_debug("Starting test loop...");
    
    // Main test loop
    while (true) {
        run_testbench();
        sleep_ms(10);
    }
    
    return 0;
}
