#include "pico/stdlib.h"
#include <stdio.h>

int main(void) {
    stdio_init_all();

    while (true) {
        printf("alive\n");
        sleep_ms(1000);
    }
}
