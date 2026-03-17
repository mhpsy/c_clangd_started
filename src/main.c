#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "ch347.h"

/* GPIO pins to toggle */
static const int PINS[] = {1, 2, 3, 4, 5, 6, 7};
#define PIN_COUNT ((int)(sizeof(PINS) / sizeof(PINS[0])))

int main(void)
{
    printf("%s v%s\n", APP_NAME, APP_VERSION);

    ch347_dev_t *dev = ch347_open();
    if (!dev) return 1;

    bool state = false;
    while (1) {
        for (int i = 0; i < PIN_COUNT; i++) {
            ch347_gpio_set_pin(dev, PINS[i], state);
        }
        printf("[gpio] pins 1-7 -> %s\n", state ? "HIGH" : "LOW");
        state = !state;
        sleep(1);
    }

    ch347_close(dev);
    return 0;
}
