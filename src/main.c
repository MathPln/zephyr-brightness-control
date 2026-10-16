#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* Retrieve button B2 */
#define SW_B2_NODE DT_ALIAS(sw_b2)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW_B2_NODE, gpios, {0});

/* Retrieve RGB LEDs via aliases */
static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led_r), gpios, {0});
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led_g), gpios, {0});
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led_b), gpios, {0});

void main(void)
{
    printk("=== Step: Add RGB LED controller ===\n");

    /* Check and configure the button */
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device is not ready\n");
        return;
    }
    gpio_pin_configure_dt(&button, GPIO_INPUT);

    /* Check and configure the RGB LEDs */
    if (gpio_is_ready_dt(&led_r)) gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    if (gpio_is_ready_dt(&led_g)) gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    if (gpio_is_ready_dt(&led_b)) gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);

    int color_state = 0;

    while (1) {
        int val = gpio_pin_get_dt(&button);

        /* If button B2 is pressed, change the LED color */
        if (val > 0) {
            color_state = (color_state + 1) % 4;
            
            /* Turn off all LEDs first */
            gpio_pin_set_dt(&led_r, 0);
            gpio_pin_set_dt(&led_g, 0);
            gpio_pin_set_dt(&led_b, 0);

            switch (color_state) {
                case 1:
                    gpio_pin_set_dt(&led_r, 1);
                    printk("RGB LED -> RED\n");
                    break;
                case 2:
                    gpio_pin_set_dt(&led_g, 1);
                    printk("RGB LED -> GREEN\n");
                    break;
                case 3:
                    gpio_pin_set_dt(&led_b, 1);
                    printk("RGB LED -> BLUE\n");
                    break;
                default:
                    printk("RGB LED -> OFF\n");
                    break;
            }
            /* Simple debounce delay to prevent multiple triggers on a single click */
            k_msleep(300);
        }

        k_msleep(100);
    }
}
