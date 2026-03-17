#include "st7789.h"
#include "ch347.h"
#include <unistd.h>
#include <string.h>

/* GPIO bitmasks
 * CH347F pin mapping (from schematic):
 *   SCS0 (HW SPI CS) = dedicated pin 13, independent of GPIO numbering
 *   GPIO1 = plain GPIO
 *   GPIO2 = DTR0/TNOW0  - used as software CS
 *   GPIO4 = TCK/SWDCLK  - JTAG pin, no SPI conflict
 *   GPIO7 = TMS/SWDIO   - JTAG pin, no SPI conflict
 */
#define PIN_BL  (1u << 1)   /* GPIO1 - Backlight */
#define PIN_CS  (1u << 2)   /* GPIO2 - Chip Select (software, DTR0 pin) */
#define PIN_DC  (1u << 4)   /* GPIO4 - Data/Command (TCK/SWDCLK pin, no SPI conflict) */
#define PIN_RST (1u << 7)   /* GPIO7 - Reset (TMS/SWDIO pin, no SPI conflict) */

/* ST7789 commands */
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVON    0x21
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static void pin_set(ch347_dev_t *dev, uint8_t pins, bool high)
{
    ch347_gpio_set_pins(dev, pins, high ? pins : 0x00);
}

/*
 * CS (GPIO2) is managed manually. We use spi_write_nocs throughout so the
 * CH347 SPI hardware never auto-toggles its own SCS0 pin, leaving our
 * software CS and DC fully under GPIO control.
 */
static void write_cmd(ch347_dev_t *dev, uint8_t cmd)
{
    pin_set(dev, PIN_DC, false);            /* DC LOW  = command */
    ch347_spi_write_nocs(dev, &cmd, 1);
}

static void write_data(ch347_dev_t *dev, const uint8_t *data, size_t len)
{
    pin_set(dev, PIN_DC, true);             /* DC HIGH = data */
    ch347_spi_write_nocs(dev, data, len);
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void st7789_init(ch347_dev_t *dev)
{
    /* SPI mode 0, 15 MHz */
    ch347_spi_init(dev, CH347_SPI_MODE0, CH347_SPI_CLK_15M);

    /* Backlight on */
    pin_set(dev, PIN_BL, true);

    /* Hardware reset: pull RST low 10 ms, then release */
    pin_set(dev, PIN_RST, false);
    usleep(10 * 1000);
    pin_set(dev, PIN_RST, true);
    usleep(120 * 1000);

    /* Assert CS for the entire init sequence */
    pin_set(dev, PIN_CS, false);

    /* Software reset */
    write_cmd(dev, ST7789_SWRESET);
    usleep(150 * 1000);

    /* Sleep out */
    write_cmd(dev, ST7789_SLPOUT);
    usleep(500 * 1000);

    /* 16-bit color (RGB565) */
    write_cmd(dev, ST7789_COLMOD);
    uint8_t colmod = 0x55;
    write_data(dev, &colmod, 1);

    /* Memory access control - row/col order, RGB */
    write_cmd(dev, ST7789_MADCTL);
    uint8_t madctl = 0x00;
    write_data(dev, &madctl, 1);

    /* Most ST7789 modules require display inversion */
    write_cmd(dev, ST7789_INVON);

    /* Normal display on */
    write_cmd(dev, ST7789_NORON);
    usleep(10 * 1000);

    /* Display on */
    write_cmd(dev, ST7789_DISPON);
    usleep(10 * 1000);
}

void st7789_fill(ch347_dev_t *dev, uint16_t color)
{
    /* Column address: 0 .. WIDTH-1 */
    write_cmd(dev, ST7789_CASET);
    uint8_t caset[4] = {
        0x00, 0x00,
        (uint8_t)((ST7789_WIDTH  - 1) >> 8),
        (uint8_t)((ST7789_WIDTH  - 1) & 0xFF),
    };
    write_data(dev, caset, sizeof(caset));

    /* Row address: 0 .. HEIGHT-1 */
    write_cmd(dev, ST7789_RASET);
    uint8_t raset[4] = {
        0x00, 0x00,
        (uint8_t)((ST7789_HEIGHT - 1) >> 8),
        (uint8_t)((ST7789_HEIGHT - 1) & 0xFF),
    };
    write_data(dev, raset, sizeof(raset));

    /* Start memory write, then stream pixel data */
    write_cmd(dev, ST7789_RAMWR);
    pin_set(dev, PIN_DC, true);     /* DC HIGH = data, stays HIGH through all chunks */

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    /* 4096-byte chunk buffer (reused, matches spi_write_nocs internal limit) */
    static uint8_t chunk[4096];
    for (int i = 0; i < (int)sizeof(chunk); i += 2) {
        chunk[i]     = hi;
        chunk[i + 1] = lo;
    }

    size_t remaining = (size_t)ST7789_WIDTH * ST7789_HEIGHT * 2;
    while (remaining > 0) {
        size_t n = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
        ch347_spi_write_nocs(dev, chunk, n);
        remaining -= n;
    }
}
