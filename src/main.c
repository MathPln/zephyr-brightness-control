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

/* Button callback data and semaphore */
static struct gpio_callback button_cb_data;
K_SEM_DEFINE(button_sem, 0, 1);
static int color_state = 0;

/* Helper: Set RGB color */
static void set_color(int r, int g, int b)
{
    gpio_pin_set_dt(&led_r, r);
    gpio_pin_set_dt(&led_g, g);
    gpio_pin_set_dt(&led_b, b);
}

/* Interrupt Service Routine for Button B2 */
void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_sem_give(&button_sem);
}

void system_init(void)
{
    /* Check and configure the button */
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device is not ready\n");
        return;
    }
    gpio_pin_configure_dt(&button, GPIO_INPUT);

    /* Configure button interrupt */
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    /* Check and configure the RGB LEDs */
    if (gpio_is_ready_dt(&led_r)) gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    if (gpio_is_ready_dt(&led_g)) gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    if (gpio_is_ready_dt(&led_b)) gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
}

void app_run(void)
{
    while (1) {
        /* Wait for button press via semaphore instead of polling */
        if (k_sem_take(&button_sem, K_FOREVER) == 0) {
            
            color_state = (color_state + 1) % 4;

            /* Turn off all LEDs first */
            set_color(0, 0, 0);

            /* Switch colors based on state */
            switch (color_state) {
                case 1:
                    set_color(1, 0, 0);
                    printk("RGB LED -> RED\n");
                    break;
                case 2:
                    set_color(0, 1, 0);
                    printk("RGB LED -> GREEN\n");
                    break;
                case 3:
                    set_color(0, 0, 1);
                    printk("RGB LED -> BLUE\n");
                    break;
                default:
                    printk("RGB LED -> OFF\n");
                    break;
            }
        }
    }
}

void main(void)
{
    system_init();
    app_run();
}