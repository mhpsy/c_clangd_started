#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "ch347.h"

/* GPIO 1-7: bitmask 0b11111110 = 0xFE */
#define GPIO_MASK 0xFE

int main(void)
{
    printf("%s v%s\n", APP_NAME, APP_VERSION);

    ch347_dev_t *dev = ch347_open();
    if (!dev) return 1;

    uint8_t state = 0x00;
    while (1) {
        ch347_gpio_set_pins(dev, GPIO_MASK, state);
        printf("[gpio] pins 1-7 -> %s\n", state ? "HIGH" : "LOW");
        state = state ? 0x00 : 0xFF;
        sleep(1);
    }

    ch347_close(dev);
    return 0;
}
