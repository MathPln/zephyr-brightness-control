
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

/* Message queue and thread ID defined in adc_thread.c */
extern struct k_msgq adc_msgq;
extern const k_tid_t adc_thread_id;

/* Button B2 */
#define SW_B2_NODE DT_ALIAS(sw_b2)
static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(SW_B2_NODE, gpios);

/* RGB LEDs */
static const struct gpio_dt_spec led_r =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_r), gpios);
static const struct gpio_dt_spec led_g =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_g), gpios);
static const struct gpio_dt_spec led_b =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_b), gpios);

/* Finite State Machine */
typedef enum {
    MODE_NORMAL,
    MODE_BLUE,
    MODE_OFF
} app_mode_t;

static app_mode_t current_mode = MODE_NORMAL;
static bool adc_suspended;

/* Button interrupt and semaphore */
static struct gpio_callback button_cb_data;
K_SEM_DEFINE(button_sem, 0, 1);

/* Set RGB color using logical GPIO states */
static void set_color(int red, int green, int blue)
{
    gpio_pin_set_dt(&led_r, red);
    gpio_pin_set_dt(&led_g, green);
    gpio_pin_set_dt(&led_b, blue);
}

/* Suspend ADC only when it is currently running */
static void suspend_adc(void)
{
    if (!adc_suspended) {
        k_thread_suspend(adc_thread_id);
        adc_suspended = true;
    }

    k_msgq_purge(&adc_msgq);
}

/* Resume ADC only when it is currently suspended */
static void resume_adc(void)
{
    if (adc_suspended) {
        k_msgq_purge(&adc_msgq);
        k_thread_resume(adc_thread_id);
        adc_suspended = false;
    }
}

/* GPIO interrupt: notify the main thread */
static void button_isr(const struct device *dev,
                       struct gpio_callback *cb,
                       uint32_t pins)
{
    k_sem_give(&button_sem);
}

/* Handle a short press */
static void handle_short_press(void)
{
    if (current_mode == MODE_NORMAL) {
        current_mode = MODE_BLUE;
        suspend_adc();
        set_color(0, 0, 1);
        printk("\n--- MODE BLUE ---\n");
    } else if (current_mode == MODE_BLUE) {
        current_mode = MODE_NORMAL;
        resume_adc();
        printk("\n--- MODE NORMAL ---\n");
    } else {
        /* A short press in OFF restores NORMAL */
        current_mode = MODE_NORMAL;
        resume_adc();
        printk("\n--- MODE NORMAL ---\n");
    }
}

/* Handle a long press: enter OFF immediately */
static void handle_long_press(void)
{
    if (current_mode != MODE_OFF) {
        current_mode = MODE_OFF;
        suspend_adc();
        set_color(0, 0, 0);
        printk("\nSystem OFF\n");
    }
}

static int system_init(void)
{
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device is not ready\n");
        return -ENODEV;
    }

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

    err = gpio_pin_interrupt_configure_dt(
        &button, GPIO_INT_EDGE_TO_ACTIVE);
    if (err < 0) {
        printk("Button interrupt configuration failed: %d\n", err);
        return err;
    }

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
    bool press_in_progress = false;
    bool long_press_handled = false;
    int64_t press_start_ms = 0;
    int percent;

    printk("System started: NORMAL\n");
    set_color(0, 0, 0);

    while (1) {
        /* Detect a new button press */
        if (k_sem_take(&button_sem, K_NO_WAIT) == 0) {
            if (!press_in_progress) {
                press_in_progress = true;
                long_press_handled = false;
                press_start_ms = k_uptime_get();
            }
        }

        if (press_in_progress) {
            int button_active = gpio_pin_get_dt(&button);

            if (button_active > 0) {
                /* Trigger OFF as soon as the 1-second threshold is reached */
                if (!long_press_handled &&
                    (k_uptime_get() - press_start_ms >= 1000)) {
                    handle_long_press();
                    long_press_handled = true;
                }
            } else if (button_active == 0) {
                /* Release: only short presses trigger a short-press action */
                if (!long_press_handled) {
                    handle_short_press();
                }

                press_in_progress = false;
            }
        }

        /* Receive ADC values; short timeout keeps button detection responsive */
        if (k_msgq_get(&adc_msgq, &percent, K_MSEC(20)) == 0) {
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

