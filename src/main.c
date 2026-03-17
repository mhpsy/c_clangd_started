#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "ch347.h"
#include "st7789.h"

int main(void)
{
    printf("%s v%s\n", APP_NAME, APP_VERSION);

    ch347_dev_t *dev = ch347_open();
    if (!dev) return 1;

    st7789_init(dev);

    while (1) {
        printf("fill RED\n");
        st7789_fill(dev, ST7789_RED);
        sleep(1);

        printf("fill BLUE\n");
        st7789_fill(dev, ST7789_BLUE);
        sleep(1);
    }

    ch347_close(dev);
    return 0;
}
