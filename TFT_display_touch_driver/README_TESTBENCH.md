# ILI9341 Display + XPT2046 Touch Testbench

Complete driver implementation and testbench for Fasizi 2.8" TFT Display (280×320) with touch support on Raspberry Pi Pico 2 / RP2350.

## 📋 Files Overview

| File | Purpose |
|------|---------|
| `spi_interface.h/c` | SPI abstraction layer (supports multiple SPI buses) |
| `ili9341_display.h/c` | ILI9341 TFT display driver |
| `xpt2046_touch.h/c` | XPT2046 resistive touch controller driver |
| `main_testbench.c` | Comprehensive test application |
| `CMakeLists.txt` | Build configuration |

## 🔧 GPIO Pin Configuration

### Breadboard Setup (Adjustable)

**SPI0 (Display)**
```
GP18  → Display CLK
GP19  → Display MOSI
GP16  → Display MISO
GP10  → Display CS
GP13  → Display DC (Data/Command)
GP12  → Display RST (Reset)
```

**SPI1 (Touch)**
```
GP14  → Touch CLK
GP15  → Touch MOSI
GP8   → Touch MISO
GP9   → Touch CS
(IRQ) → not connected — GP_UNUSED sentinel (99) in code, no physical pin needed
```

**⚠️ CUSTOMIZE THESE PINS:**
Edit GPIO definitions in `main_testbench.c` lines 22–41 to match your breadboard layout.
See [`../docs/PIN_ASSIGNMENT.md`](../docs/PIN_ASSIGNMENT.md) for the full pin reference.

## 🏗️ Building the Testbench

### Prerequisites

**Optie A — Pico VS Code Extension (aanbevolen)**
Installeer de officiële [Raspberry Pi Pico VS Code extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico).
De extension installeert SDK, toolchain, cmake en ninja automatisch onder `~/.pico-sdk/`.

**Optie B — Handmatig**
```bash
sudo apt install git build-essential
git clone https://github.com/raspberrypi/pico-sdk ~/pico/pico-sdk
cd ~/pico/pico-sdk && git submodule update --init
export PICO_SDK_PATH=~/pico/pico-sdk
```

### Build Steps

```bash
cd TFT_display_touch_driver
mkdir -p build && cd build

# --- Voorkeur: Ninja (via VS Code extension tools) ---
export PATH="$HOME/.pico-sdk/cmake/v3.31.5/bin:$HOME/.pico-sdk/ninja/v1.12.1:$HOME/.pico-sdk/toolchain/14_2_Rel1/bin:$PATH"
cmake -G Ninja -DPICO_BOARD=pico2_w ..
ninja

# --- Alternatief: make (handmatige SDK installatie) ---
# cmake -DPICO_BOARD=pico2_w ..
# make -j4

# Output binaries:
# build/testbench.uf2  → via USB flashen (sleep naar Pico schijf)
# build/testbench.elf  → via debug probe (SWD/OpenOCD)
```

### Flashing to Pico

1. **Connect Pico to PC in bootloader mode** (hold BOOT, press RESET)
2. **Drag-and-drop** `testbench.uf2` to the Pico disk
3. **Connect serial console** to monitor debug output:
   ```bash
   screen /dev/ttyACM0 115200
   # or
   picocom -b 115200 /dev/ttyACM0
   ```

## 🧪 Test Modes

The testbench cycles through 5 test modes (auto-switches every 10 seconds):

### 1. **Color Test** (TEST_COLORS)
- Displays full-screen color patterns
- Cycles: RED → GREEN → BLUE → YELLOW → CYAN → MAGENTA → WHITE
- Tests basic fill_screen functionality

### 2. **Rectangle Test** (TEST_RECTS)
- Draws multiple rectangles (outlined and filled)
- Tests `ili9341_fill_rect()` and `ili9341_draw_rect()`
- Various thicknesses and colors

### 3. **Text Test** (TEST_TEXT)
- Renders characters at different scales (1x and 2x)
- Tests 5×7 bitmap font rendering
- Displays ASCII numbers, uppercase, and lowercase letters
- Shows elapsed time

### 4. **Touch Test** (TEST_TOUCH)
- Displays crosshair at touch position
- Shows X, Y, Z (pressure) coordinates
- Tests `xpt2046_read()` functionality

### 5. **Button Test** (TEST_BUTTONS)
- Three interactive buttons (RED, GREEN, BLUE)
- Highlights button when touched
- Tests button hit-detection logic

## 📊 Display Functions Used

### Core API
```c
// Initialization
bool ili9341_init(const ili9341_t *display);

// Filling rectangles (main primitive)
void ili9341_fill_rect(const ili9341_t *display, 
                       uint16_t x0, uint16_t y0, 
                       uint16_t x1, uint16_t y1, 
                       uint16_t color);

// Drawing
void ili9341_draw_rect(...)      // Rectangle outline
void ili9341_draw_h_line(...)    // Horizontal line
void ili9341_draw_v_line(...)    // Vertical line
void ili9341_fill_screen(...)    // Fill entire screen

// Text rendering
uint16_t ili9341_write_string(const ili9341_t *display,
                              uint16_t x, uint16_t y, 
                              const char *str,
                              uint16_t fg_color, 
                              uint16_t bg_color,
                              uint8_t scale);
```

### Color Definitions (RGB565)
```c
#define ILI9341_BLACK   0x0000
#define ILI9341_RED     0xF800
#define ILI9341_GREEN   0x07E0
#define ILI9341_BLUE    0x001F
#define ILI9341_WHITE   0xFFFF
#define ILI9341_YELLOW  0xFFE0
#define ILI9341_CYAN    0x07FF
#define ILI9341_MAGENTA 0xF81F
```

## 📍 Touch Functions Used

### Core API
```c
// Initialization
bool xpt2046_init(const xpt2046_t *touch);

// Read touch with calibration
bool xpt2046_read(const xpt2046_t *touch, touch_point_t *point);

// Read raw ADC data (no calibration)
bool xpt2046_read_raw(const xpt2046_t *touch, touch_point_t *point);

// Set calibration manually
void xpt2046_set_calibration(xpt2046_t *touch,
                             float x_scale, float x_offset,
                             float y_scale, float y_offset);
```

### Touch Point Structure
```c
typedef struct {
    uint16_t x;          // Pixel X (0-279)
    uint16_t y;          // Pixel Y (0-319)
    bool pressed;        // true if touched
    uint16_t z;          // Pressure (0-4095)
} touch_point_t;
```

## 🔌 Wiring Diagram

```
                    RP2350/Pico 2
                    
    SPI0 (Display)              SPI1 (Touch)
    ════════════════            ═════════════════
    GP18 ◄────────────CLK       GP14 ◄──────────CLK
    GP19 ◄────────────MOSI      GP15 ◄──────────MOSI
    GP16 ◄────────────MISO      GP8  ◄──────────MISO
    GP10 ◄────────────CS        GP9  ◄──────────CS
    GP13 ◄────────────DC
    GP12 ◄────────────RST
    
    
    ILI9341 Display             XPT2046 Touch
    ════════════════            ════════════════
    CLK  ─────────────►  GP18   CLK  ─────────────► GP14
    MOSI ─────────────►  GP19   MOSI ─────────────► GP15
    MISO ◄──────────────  GP16   MISO ◄──────────────  GP8
    CS   ─────────────►  GP10   CS   ─────────────► GP9
    DC   ─────────────►  GP13
    RST  ─────────────►  GP12
    
    GND  ─────────────►  GND
    +5V  ─────────────►  VCC
    +3V3 ─────────────►  VCC (via level shifter if needed)
```

## 🐛 Troubleshooting

### Display Not Appearing
1. **Check GPIO pins** - Verify all pins match your wiring
2. **Check SPI speed** - Reduce to 5 MHz: `baud_rate = 5000000`
3. **Check reset** - Ensure GP12 (RST) is connected and functioning
4. **Check DC pin** - Ensure GP13 (DC) switches properly

### Touch Not Responding
1. **Check calibration** - Default calibration assumes 280×320 display
2. **Increase z_threshold** - Try `z_threshold = 1000` for less sensitivity
3. **Check SPI speed** - XPT2046 supports up to 2 MHz

### Garbled Display
1. **SPI clock too fast** - Reduce `baud_rate` for ILI9341
2. **Noise/crosstalk** - Keep SPI wires short and twisted
3. **Power supply** - Add 100µF capacitor near display VCC

## 🚀 Next Steps

Once testbench works, you can:

1. **Integrate into main project** - Use drivers as-is for Bode plot application
2. **Customize colors** - Add your own RGB565 color palette
3. **Add more fonts** - Expand font_5x7 with larger character sets
4. **Implement UI framework** - Build menu system on top of drivers

## 📝 License

Free to use and modify for embedded projects.

## 📚 References

- [ILI9341 Datasheet](https://www.ilitek.com/products/Semicon_products/LCD_Driver_IC_detail/ILI9341)
- [XPT2046 Datasheet](http://www.xptek.com.cn/Download/XPT2046.pdf)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
