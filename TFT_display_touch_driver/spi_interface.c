/**
 * @file spi_interface.c
 * @brief SPI abstraction layer implementation
 */

#include "spi_interface.h"
#include "hardware/gpio.h"

bool spi_device_init(const spi_device_t *device)
{
    if (!device || !device->spi) {
        return false;
    }

    // Initialize SPI instance
    spi_init(device->spi, device->baud_rate);

    // Set pin function to SPI (skip MISO for write-only devices)
    gpio_set_function(device->clk_pin, GPIO_FUNC_SPI);
    gpio_set_function(device->mosi_pin, GPIO_FUNC_SPI);
    if (device->miso_pin != SPI_MISO_UNUSED) {
        gpio_set_function(device->miso_pin, GPIO_FUNC_SPI);
    }

    // CS pin is managed by user (GPIO output)
    gpio_init(device->cs_pin);
    gpio_set_dir(device->cs_pin, GPIO_OUT);
    gpio_put(device->cs_pin, device->cs_active_low ? 1 : 0);

    return true;
}

void spi_write(const spi_device_t *device, const uint8_t *data, uint32_t length)
{
    if (!device || !data || length == 0) {
        return;
    }

    // CS active
    gpio_put(device->cs_pin, device->cs_active_low ? 0 : 1);
    
    // Write data
    spi_write_blocking(device->spi, data, length);
    
    // CS inactive
    gpio_put(device->cs_pin, device->cs_active_low ? 1 : 0);
}

void spi_read(const spi_device_t *device, uint8_t *data, uint32_t length)
{
    if (!device || !data || length == 0) {
        return;
    }

    // CS active
    gpio_put(device->cs_pin, device->cs_active_low ? 0 : 1);
    
    // Read data (write 0xFF as dummy)
    spi_read_blocking(device->spi, 0xFF, data, length);
    
    // CS inactive
    gpio_put(device->cs_pin, device->cs_active_low ? 1 : 0);
}

void spi_write_read(const spi_device_t *device, const uint8_t *tx_data, 
                    uint8_t *rx_data, uint32_t length)
{
    if (!device || length == 0) {
        return;
    }

    // CS active
    gpio_put(device->cs_pin, device->cs_active_low ? 0 : 1);
    
    if (tx_data && rx_data) {
        // Full duplex
        spi_write_read_blocking(device->spi, tx_data, rx_data, length);
    } else if (tx_data) {
        // Write only
        spi_write_blocking(device->spi, tx_data, length);
    } else if (rx_data) {
        // Read only
        spi_read_blocking(device->spi, 0xFF, rx_data, length);
    }
    
    // CS inactive
    gpio_put(device->cs_pin, device->cs_active_low ? 1 : 0);
}

void spi_write_byte(const spi_device_t *device, uint8_t byte)
{
    spi_write(device, &byte, 1);
}

uint8_t spi_read_byte(const spi_device_t *device)
{
    uint8_t byte;
    spi_read(device, &byte, 1);
    return byte;
}

void spi_write_word_be(const spi_device_t *device, uint16_t word)
{
    uint8_t data[2] = {
        (uint8_t)((word >> 8) & 0xFF),
        (uint8_t)(word & 0xFF)
    };
    spi_write(device, data, 2);
}

uint16_t spi_read_word_be(const spi_device_t *device)
{
    uint8_t data[2];
    spi_read(device, data, 2);
    return ((uint16_t)data[0] << 8) | data[1];
}

void spi_cs_assert(const spi_device_t *device)
{
    if (!device) return;
    gpio_put(device->cs_pin, device->cs_active_low ? 0 : 1);
}

void spi_cs_deassert(const spi_device_t *device)
{
    if (!device) return;
    gpio_put(device->cs_pin, device->cs_active_low ? 1 : 0);
}

void spi_write_raw(const spi_device_t *device, const uint8_t *data, uint32_t length)
{
    if (!device || !data || length == 0) return;
    spi_write_blocking(device->spi, data, length);
}
