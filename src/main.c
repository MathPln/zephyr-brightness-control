#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
    printk("\n");
    printk("===============================\n");
    printk(" Brightness Control System\n");
    printk(" Zephyr RTOS - UPM\n");
    printk("===============================\n");

    while (1) {
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
