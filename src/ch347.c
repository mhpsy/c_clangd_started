/**
 * ch347.c - CH347F USB SPI/GPIO driver using libusb-1.0
 *
 * CH347F (VID=0x1A86, PID=0x55DE) operates in SPI+I2C+GPIO mode.
 * USB communication uses Interface 4 (vendor specific):
 *   EP6 OUT (0x06) - host-to-device
 *   EP6 IN  (0x86) - device-to-host
 *
 * NOTE: You may need to run as root or add a udev rule:
 *   echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="55de", MODE="0666"' \
 *     | sudo tee /etc/udev/rules.d/99-ch347.rules
 *   sudo udevadm control --reload-rules && sudo udevadm trigger
 */

#include "ch347.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* USB transfer timeout in milliseconds */
#define USB_TIMEOUT_MS      100   /* SPI write response: ~2ms for 4KB at 15MHz */
#define USB_INIT_TIMEOUT_MS  50   /* CS control / GPIO responses */

/* Maximum SPI payload per single write command */
#define SPI_MAX_CHUNK_SIZE  4096

/* Command bytes */
#define CMD_SPI_INIT    0xC0
#define CMD_SPI_WRITE   0xC4
#define CMD_SPI_CS_CTRL 0xC1  /* manual CS control */
#define CMD_GPIO_CTRL   0xCC

/* CS control values */
#define CS0_ASSERT      0x00  /* CS0 active (low) */
#define CS0_DEASSERT    0x40  /* CS0 inactive (high) */

struct ch347_dev {
    libusb_context       *ctx;
    libusb_device_handle *handle;
};

/* --------------------------------------------------------------------------
 * Low-level USB helpers
 * -------------------------------------------------------------------------- */

/**
 * usb_write - send data to the CH347 bulk OUT endpoint
 * Returns number of bytes transferred, or negative on error.
 */
static int usb_write(ch347_dev_t *dev, const uint8_t *buf, int len)
{
    int transferred = 0;
    int r = libusb_bulk_transfer(dev->handle,
                                  CH347_EP_OUT,
                                  (unsigned char *)buf,
                                  len,
                                  &transferred,
                                  USB_TIMEOUT_MS);
    if (r < 0) {
        fprintf(stderr, "[ch347] usb_write error: %s\n", libusb_error_name(r));
        return r;
    }
    return transferred;
}

/* Max USB HS bulk packet size - always use this to avoid LIBUSB_ERROR_OVERFLOW */
#define USB_READ_BUF_SIZE 512

/**
 * usb_read - receive data from the CH347 bulk IN endpoint
 *
 * Always reads into a 512-byte staging buffer (HS max packet size) to prevent
 * LIBUSB_ERROR_OVERFLOW when the device pads responses to packet boundaries.
 * Copies min(received, len) bytes to caller's buf.
 *
 * Returns number of bytes copied to buf, or negative on error.
 */
static int usb_read(ch347_dev_t *dev, uint8_t *buf, int len, int timeout_ms)
{
    static uint8_t staging[USB_READ_BUF_SIZE];
    int transferred = 0;
    int r = libusb_bulk_transfer(dev->handle,
                                  CH347_EP_IN,
                                  staging,
                                  sizeof(staging),
                                  &transferred,
                                  timeout_ms);
    if (r < 0 && r != LIBUSB_ERROR_TIMEOUT) {
        /* Suppress noisy overflow messages - just return 0 and continue */
        return 0;
    }
    if (r == LIBUSB_ERROR_TIMEOUT || transferred == 0) {
        return 0;
    }
    int copy = transferred < len ? transferred : len;
    memcpy(buf, staging, copy);
    return copy;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * ch347_open - open the CH347 device and claim the SPI/GPIO interface
 *
 * Detaches any kernel driver that may have claimed the interface,
 * then claims interface 4 for our exclusive use.
 *
 * Returns pointer to device context, or NULL on failure.
 */
ch347_dev_t *ch347_open(void)
{
    ch347_dev_t *dev = calloc(1, sizeof(ch347_dev_t));
    if (!dev) {
        perror("calloc");
        return NULL;
    }

    int r = libusb_init(&dev->ctx);
    if (r < 0) {
        fprintf(stderr, "[ch347] libusb_init failed: %s\n", libusb_error_name(r));
        free(dev);
        return NULL;
    }

#if LIBUSB_API_VERSION >= 0x01000106
    libusb_set_option(dev->ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
#else
    libusb_set_debug(dev->ctx, LIBUSB_LOG_LEVEL_WARNING);
#endif

    dev->handle = libusb_open_device_with_vid_pid(dev->ctx, CH347_VID, CH347_PID);
    if (!dev->handle) {
        fprintf(stderr, "[ch347] Cannot open device %04X:%04X.\n"
                        "  Make sure the device is connected.\n"
                        "  Try: sudo chmod 666 /dev/bus/usb/XXX/YYY\n"
                        "  Or add a udev rule for VID=1A86 PID=55DE.\n",
                CH347_VID, CH347_PID);
        libusb_exit(dev->ctx);
        free(dev);
        return NULL;
    }

    /* Detach kernel driver if active on interface 4 */
    if (libusb_kernel_driver_active(dev->handle, CH347_INTERFACE) == 1) {
        r = libusb_detach_kernel_driver(dev->handle, CH347_INTERFACE);
        if (r < 0) {
            fprintf(stderr, "[ch347] Cannot detach kernel driver: %s\n",
                    libusb_error_name(r));
            /* Non-fatal: try to continue */
        } else {
            printf("[ch347] Detached kernel driver from interface %d\n",
                   CH347_INTERFACE);
        }
    }

    r = libusb_claim_interface(dev->handle, CH347_INTERFACE);
    if (r < 0) {
        fprintf(stderr, "[ch347] Cannot claim interface %d: %s\n",
                CH347_INTERFACE, libusb_error_name(r));
        libusb_close(dev->handle);
        libusb_exit(dev->ctx);
        free(dev);
        return NULL;
    }

    printf("[ch347] Device %04X:%04X opened, interface %d claimed.\n",
           CH347_VID, CH347_PID, CH347_INTERFACE);
    return dev;
}

/**
 * ch347_close - release interface and close the device
 */
void ch347_close(ch347_dev_t *dev)
{
    if (!dev) return;
    libusb_release_interface(dev->handle, CH347_INTERFACE);
    libusb_close(dev->handle);
    libusb_exit(dev->ctx);
    free(dev);
}

/**
 * ch347_spi_init - initialise the SPI controller
 *
 * Packet layout (29 bytes total):
 *   [0]    = 0xC0  (CMD_SPI_INIT)
 *   [1]    = 26    (LEN_LOW  = 26 data bytes)
 *   [2]    = 0     (LEN_HIGH)
 *   [3..28]= 26-byte SPI configuration block
 *
 * Returns 0 on success, negative on hard failure.
 */
int ch347_spi_init(ch347_dev_t *dev, ch347_spi_mode_t mode, ch347_spi_clock_t clock)
{
    uint8_t buf[29];
    memset(buf, 0, sizeof(buf));

    buf[0] = CMD_SPI_INIT;
    buf[1] = 26;   /* LEN_LOW  */
    buf[2] = 0;    /* LEN_HIGH */

    /* 26-byte SPI config block (buf[3..28]) */
    buf[3] = (uint8_t)mode;     /* SPI mode: 0=CPOL0/CPHA0 */
    buf[4] = (uint8_t)clock;    /* Clock index */
    buf[5] = 0;                 /* Bit order: 0=MSB first */
    buf[6] = 0;                 /* Write-read interval (uint16 LE, low byte) */
    buf[7] = 0;                 /* Write-read interval (uint16 LE, high byte) */
    buf[8] = 0xFF;              /* Default output data byte */
    /* buf[9..28] = 0: CS config and reserved, defaults are fine */

    int r = usb_write(dev, buf, sizeof(buf));
    if (r < 0) {
        fprintf(stderr, "[ch347] spi_init: write failed\n");
        return -1;
    }

    /* Read 4-byte response; use a short timeout - some devices don't respond */
    uint8_t resp[4] = {0};
    int n = usb_read(dev, resp, sizeof(resp), USB_INIT_TIMEOUT_MS);
    if (n >= 4) {
        if (resp[0] != CMD_SPI_INIT) {
            fprintf(stderr, "[ch347] spi_init: unexpected response byte 0x%02X\n",
                    resp[0]);
            /* Non-fatal: device may still work */
        } else if (resp[3] != 0) {
            fprintf(stderr, "[ch347] spi_init: device returned status 0x%02X\n",
                    resp[3]);
        } else {
            printf("[ch347] SPI initialised (mode=%d, clock_idx=%d)\n",
                   mode, clock);
        }
    } else {
        /* No response or short response - many CH347 don't send one */
        printf("[ch347] SPI init command sent (no response - OK)\n");
    }

    return 0;
}

/**
 * ch347_spi_write - write bytes over SPI
 *
 * For len <= SPI_MAX_CHUNK_SIZE: single CMD_SPI_WRITE with cs_byte=0x80
 * (hardware CS0 automatically asserted/de-asserted around the transfer).
 *
 * For len > SPI_MAX_CHUNK_SIZE: split into multiple chunks.
 * The CH347 CMD_SPI_CS_CTRL (0xC1) is used to manually assert/de-assert CS
 * around multi-chunk transfers so CS stays low throughout.
 *
 * Packet format for CMD_SPI_WRITE (0xC4):
 *   [0]   = 0xC4
 *   [1]   = (3 + data_len) & 0xFF   total payload length, low byte
 *   [2]   = (3 + data_len) >> 8     total payload length, high byte
 *   [3]   = cs_byte  (0x80 = auto CS0, 0x00 = CS already asserted)
 *   [4]   = data_len & 0xFF
 *   [5]   = data_len >> 8
 *   [6..] = data
 *
 * Returns 0 on success, negative on failure.
 */
/* --------------------------------------------------------------------------
 * Internal: send data chunks without CS management
 * Returns bytes sent (== len on success), or < len on failure
 * -------------------------------------------------------------------------- */
static size_t spi_send_chunks(ch347_dev_t *dev, const uint8_t *data, size_t len)
{
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > (size_t)SPI_MAX_CHUNK_SIZE) chunk = (size_t)SPI_MAX_CHUNK_SIZE;

        size_t pkt_len = 6 + chunk;
        uint8_t *pkt = malloc(pkt_len);
        if (!pkt) { perror("malloc"); return offset; }

        uint16_t payload = (uint16_t)(3 + chunk);
        pkt[0] = CMD_SPI_WRITE;
        pkt[1] = payload & 0xFF;
        pkt[2] = payload >> 8;
        pkt[3] = 0x00;   /* CS already controlled manually */
        pkt[4] = (uint8_t)(chunk & 0xFF);
        pkt[5] = (uint8_t)(chunk >> 8);
        memcpy(pkt + 6, data + offset, chunk);

        int r = usb_write(dev, pkt, (int)pkt_len);
        free(pkt);
        if (r < 0) break;

        /* Read response to keep the device's response queue drained */
        uint8_t resp[4] = {0};
        usb_read(dev, resp, sizeof(resp), USB_TIMEOUT_MS);

        offset += chunk;
    }
    return offset;
}

/* --------------------------------------------------------------------------
 * Internal: send CS control command, drain queued responses
 * -------------------------------------------------------------------------- */
static int spi_cs_ctrl(ch347_dev_t *dev, uint8_t cs_val)
{
    uint8_t cs_buf[4] = { CMD_SPI_CS_CTRL, 1, 0, cs_val };
    if (usb_write(dev, cs_buf, sizeof(cs_buf)) < 0) return -1;
    /* Drain all queued responses from IN endpoint (up to 16 × 512 bytes) */
    uint8_t drain[4];
    for (int i = 0; i < 16; i++) {
        int n = usb_read(dev, drain, sizeof(drain), USB_TIMEOUT_MS);
        if (n == 0) break;
    }
    return 0;
}

int ch347_spi_cs_assert(ch347_dev_t *dev)
{
    if (!dev) return -1;
    if (spi_cs_ctrl(dev, CS0_ASSERT) < 0) {
        fprintf(stderr, "[ch347] CS assert failed\n");
        return -1;
    }
    return 0;
}

int ch347_spi_cs_deassert(ch347_dev_t *dev)
{
    if (!dev) return -1;
    if (spi_cs_ctrl(dev, CS0_DEASSERT) < 0) {
        fprintf(stderr, "[ch347] CS deassert failed\n");
        return -1;
    }
    return 0;
}

int ch347_spi_write_nocs(ch347_dev_t *dev, const uint8_t *data, size_t len)
{
    if (!dev || !data || len == 0) return -1;
    return (spi_send_chunks(dev, data, len) == len) ? 0 : -1;
}

int ch347_spi_write(ch347_dev_t *dev, const uint8_t *data, size_t len)
{
    if (!dev || !data || len == 0) return -1;

    if (ch347_spi_cs_assert(dev) < 0) return -1;
    size_t sent = spi_send_chunks(dev, data, len);
    ch347_spi_cs_deassert(dev);

    return (sent == len) ? 0 : -1;
}

/**
 * ch347_gpio_set_pin - set a single GPIO pin direction and level
 *
 * gpio_num : 0..7
 * value    : true = HIGH, false = LOW
 *
 * Packet format (11 bytes):
 *   [0]    = 0xCC  (CMD_GPIO_CTRL)
 *   [1]    = 8     (LEN_LOW = 8 GPIO bytes always)
 *   [2]    = 0     (LEN_HIGH)
 *   [3..10]= GPIO0..GPIO7 control bytes
 *             0x00 = don't change this GPIO
 *             0xF0 = configure as output, set LOW
 *             0xF8 = configure as output, set HIGH
 *
 * Response: 11 bytes [0xCC, 8, 0, GPIO0_state..GPIO7_state]
 *
 * Returns 0 on success, negative on failure.
 */
int ch347_gpio_set_pin(ch347_dev_t *dev, int gpio_num, bool value)
{
    if (!dev || gpio_num < 0 || gpio_num > 7) return -1;

    uint8_t buf[11];
    memset(buf, 0, sizeof(buf));

    buf[0] = CMD_GPIO_CTRL;
    buf[1] = 8;   /* LEN_LOW  = 8 (always 8 GPIO control bytes) */
    buf[2] = 0;   /* LEN_HIGH */

    /* buf[3..10] = GPIO0..GPIO7 control bytes; 0x00 = no change */
    /* Only set the target GPIO; leave all others as 0x00 */
    buf[3 + gpio_num] = value ? 0xF8 : 0xF0;

    int r = usb_write(dev, buf, sizeof(buf));
    if (r < 0) {
        fprintf(stderr, "[ch347] gpio_set_pin(%d, %d): write failed\n",
                gpio_num, (int)value);
        return -1;
    }

    /* No response read - GPIO command is fire-and-forget for speed */
    return 0;
}
