/**
 * @file spi_interface.h
 * @brief SPI abstraction layer for Raspberry Pi Pico
 * 
 * Supports multiple SPI instances and devices
 * Used by: ILI9341 display, XPT2046 touch, AD9102 (future)
 */

#ifndef SPI_INTERFACE_H
#define SPI_INTERFACE_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * Pass as miso_pin for write-only devices (display without readback).
 * spi_device_init skips GPIO_FUNC_SPI configuration for this pin.
 */
#define SPI_MISO_UNUSED 0xFFU

/**
 * SPI device configuration
 */
typedef struct {
    spi_inst_t *spi;        // SPI instance (spi0 or spi1)
    uint32_t clk_pin;       // Clock pin
    uint32_t mosi_pin;      // Master Out Slave In
    uint32_t miso_pin;      // Master In Slave Out (set to SPI_MISO_UNUSED if not needed)
    uint32_t cs_pin;        // Chip Select (managed by user)
    uint32_t baud_rate;     // SPI clock speed (Hz)
    bool cs_active_low;     // CS polarity (true = active low)
} spi_device_t;

/**
 * Initialize SPI device
 * 
 * @param device Pointer to device configuration
 * @return true on success, false on failure
 */
bool spi_device_init(const spi_device_t *device);

/**
 * Write data to SPI device (without reading)
 * 
 * @param device SPI device
 * @param data Pointer to data buffer
 * @param length Number of bytes to write
 */
void spi_write(const spi_device_t *device, const uint8_t *data, uint32_t length);

/**
 * Read data from SPI device (without writing)
 * 
 * @param device SPI device
 * @param data Pointer to receive buffer
 * @param length Number of bytes to read
 */
void spi_read(const spi_device_t *device, uint8_t *data, uint32_t length);

/**
 * Write and read simultaneously (full duplex)
 * 
 * @param device SPI device
 * @param tx_data Data to transmit (NULL for dummy writes)
 * @param rx_data Buffer for received data (NULL to discard)
 * @param length Number of bytes
 */
void spi_write_read(const spi_device_t *device, const uint8_t *tx_data, 
                    uint8_t *rx_data, uint32_t length);

/**
 * Write single byte
 * 
 * @param device SPI device
 * @param byte Byte to write
 */
void spi_write_byte(const spi_device_t *device, uint8_t byte);

/**
 * Read single byte
 * 
 * @param device SPI device
 * @return Received byte
 */
uint8_t spi_read_byte(const spi_device_t *device);

/**
 * Write 16-bit word (big-endian)
 * 
 * @param device SPI device
 * @param word 16-bit word to write
 */
void spi_write_word_be(const spi_device_t *device, uint16_t word);

/**
 * Read 16-bit word (big-endian)
 *
 * @param device SPI device
 * @return 16-bit word received
 */
uint16_t spi_read_word_be(const spi_device_t *device);

/**
 * Assert chip select (CS active).
 * Use together with spi_write_raw / spi_cs_deassert for streaming bulk writes.
 */
void spi_cs_assert(const spi_device_t *device);

/**
 * Deassert chip select (CS inactive).
 */
void spi_cs_deassert(const spi_device_t *device);

/**
 * Write data without touching CS — caller must bracket with spi_cs_assert / spi_cs_deassert.
 */
void spi_write_raw(const spi_device_t *device, const uint8_t *data, uint32_t length);

#endif // SPI_INTERFACE_H
