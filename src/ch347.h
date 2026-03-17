#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CH347_VID        0x1A86
#define CH347_PID        0x55DE
#define CH347_INTERFACE  4
#define CH347_EP_OUT     0x06
#define CH347_EP_IN      0x86

typedef enum {
    CH347_SPI_CLK_60M   = 0,
    CH347_SPI_CLK_30M   = 1,
    CH347_SPI_CLK_15M   = 2,
    CH347_SPI_CLK_7M5   = 3,
    CH347_SPI_CLK_3M75  = 4,
    CH347_SPI_CLK_1M875 = 5,
} ch347_spi_clock_t;

typedef enum {
    CH347_SPI_MODE0 = 0,
    CH347_SPI_MODE1 = 1,
    CH347_SPI_MODE2 = 2,
    CH347_SPI_MODE3 = 3,
} ch347_spi_mode_t;

typedef struct ch347_dev ch347_dev_t;

ch347_dev_t *ch347_open(void);
void ch347_close(ch347_dev_t *dev);

int ch347_spi_init(ch347_dev_t *dev, ch347_spi_mode_t mode, ch347_spi_clock_t clock);
int ch347_spi_write(ch347_dev_t *dev, const uint8_t *data, size_t len);
int ch347_gpio_set_pin(ch347_dev_t *dev, int gpio_num, bool value);

/* Set multiple GPIO pins in a single USB command.
 * mask : bitmask of pins to change (bit N = GPIO N)
 * value: bitmask of output levels  (bit N = HIGH if set)
 * Example: set GPIO 1,2,3 HIGH → ch347_gpio_set_pins(dev, 0x0E, 0x0E)
 */
int ch347_gpio_set_pins(ch347_dev_t *dev, uint8_t mask, uint8_t value);

/* Low-level CS control for batched transactions */
int ch347_spi_cs_assert(ch347_dev_t *dev);    /* assert CS0, drain any queued responses */
int ch347_spi_cs_deassert(ch347_dev_t *dev);  /* deassert CS0, drain any queued responses */
int ch347_spi_write_nocs(ch347_dev_t *dev, const uint8_t *data, size_t len); /* write without CS management */
