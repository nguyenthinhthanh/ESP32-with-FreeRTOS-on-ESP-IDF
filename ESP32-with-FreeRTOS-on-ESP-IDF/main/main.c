#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "../esp-idf-lib/components/bme680/bme680.h"

void app_main(void)
{
    while (true) {
        printf("Hello from app_main!\n");
        sleep(1);
    }
}
