
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>

/* Message queue defined in adc_thread.c */
extern struct k_msgq adc_msgq;

/* Retrieve button B2 from the DeviceTree overlay. */
#define SW_B2_NODE DT_ALIAS(sw_b2)
static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(SW_B2_NODE, gpios);

/* Retrieve RGB LEDs from the DeviceTree overlay. */
static const struct gpio_dt_spec led_r =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_r), gpios);

static const struct gpio_dt_spec led_g =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_g), gpios);

static const struct gpio_dt_spec led_b =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_b), gpios);

/* Button callback data and semaphore. */
/* FSM States */
typedef enum { MODE_NORMAL, MODE_BLUE } app_mode_t;
static volatile app_mode_t current_mode = MODE_NORMAL;

/* Button callback data and semaphore */
static struct gpio_callback button_cb_data;
K_SEM_DEFINE(button_sem, 0, 1);

/* Set the RGB LED color using logical GPIO states. */
static void set_color(int red, int green, int blue)
{
    gpio_pin_set_dt(&led_r, red);
    gpio_pin_set_dt(&led_g, green);
    gpio_pin_set_dt(&led_b, blue);
}

/* Interrupt Service Routine for button B2. */
static void button_isr(const struct device *dev,
                       struct gpio_callback *cb,
                       uint32_t pins)
{
    k_sem_give(&button_sem);
}

static int system_init(void)
{
    /* Check whether the button is ready. */
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device is not ready\n");
        return -ENODEV;
    }

    /* Configure button input and interrupt. */
    int err = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (err < 0) {
        printk("Button configuration failed: %d\n", err);
        return err;
    }

    gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));

    err = gpio_add_callback(button.port, &button_cb_data);
    if (err < 0) {
        printk("Button callback registration failed: %d\n", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (err < 0) {
        printk("Button interrupt configuration failed: %d\n", err);
        return err;
    }

    /* Check and configure the RGB LEDs. */
    if (!gpio_is_ready_dt(&led_r) ||
        !gpio_is_ready_dt(&led_g) ||
        !gpio_is_ready_dt(&led_b)) {
        printk("Error: RGB GPIO device is not ready\n");
        return -ENODEV;
    }

    err = gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    if (err < 0) return err;

    err = gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    if (err < 0) return err;

    err = gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);
    if (err < 0) return err;

    return 0;
}

static void app_run(void)
{
    printk("=== System started in NORMAL mode ===\n");
    set_color(0, 0, 0);
    int percent;

    while (1) {
        /* Check button presses to toggle FSM state */
        if (k_sem_take(&button_sem, K_NO_WAIT) == 0) {
            if (current_mode == MODE_NORMAL) {
                current_mode = MODE_BLUE;
                set_color(0, 0, 1); // Fix LED to BLUE
                printk("\n--- SWITCH TO MODE BLUE ---\n");
            } else {
                current_mode = MODE_NORMAL;
                printk("\n--- SWITCH TO MODE NORMAL ---\n");
            }
        }

        /* Wait for ADC value (100ms timeout) */
        if (k_msgq_get(&adc_msgq, &percent, K_MSEC(100)) == 0) {
            
            /* Update LED color based on brightness ONLY in NORMAL mode */
            if (current_mode == MODE_NORMAL) {
                if (percent < 33) {
                    set_color(1, 0, 0);       /* Red */
                } else if (percent <= 66) {
                    set_color(1, 1, 0);       /* Yellow */
                } else {
                    set_color(0, 1, 0);       /* Green */
                }
            }
        }
    }
}

int main(void)
{
    int err = system_init();

    if (err < 0) {
        return err;
    }

    app_run();
    return 0;
}
