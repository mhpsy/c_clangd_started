#include <stdio.h>
#include "config.h"

int main(void) {
    printf("%s v%s\n", APP_NAME, APP_VERSION);
    printf("%s\n", GREETING);
    return 0;
}
