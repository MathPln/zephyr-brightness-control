#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include <stdbool.h>

/* Get the ADC configuration from the DeviceTree alias. */
#define ADC_NODE DT_PATH(zephyr_user)

static const struct adc_dt_spec adc_channel =
    ADC_DT_SPEC_GET(ADC_NODE);

/* RGB LED GPIO specifications. */
static const struct gpio_dt_spec led_r =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_r), gpios);

static const struct gpio_dt_spec led_g =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_g), gpios);

static const struct gpio_dt_spec led_b =
    GPIO_DT_SPEC_GET(DT_ALIAS(led_b), gpios);

/* Set the RGB LED color using logical GPIO states. */
static void set_color(bool red, bool green, bool blue)
{
    gpio_pin_set_dt(&led_r, red);
    gpio_pin_set_dt(&led_g, green);
    gpio_pin_set_dt(&led_b, blue);
}

/* Thread responsible for ADC acquisition and brightness-based RGB control. */
void adc_thread_entry(void *p1, void *p2, void *p3)
{
    /* Check that the ADC and RGB GPIO devices are ready. */
    if (!adc_is_ready_dt(&adc_channel) ||
        !gpio_is_ready_dt(&led_r) ||
        !gpio_is_ready_dt(&led_g) ||
        !gpio_is_ready_dt(&led_b)) {
        printk("Error: ADC or RGB GPIO not ready\n");
        return;
    }

    /* Configure the RGB LED pins as inactive outputs. */
    gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);

    /* Initialize the ADC channel. */
    int err = adc_channel_setup_dt(&adc_channel);
    if (err < 0) {
        printk("ADC setup failed: %d\n", err);
        return;
    }

    printk("Brightness-based RGB control started\n");

    while (1) {
        int16_t sample_buffer;

        /* Prepare the ADC sequence for one sample. */
        struct adc_sequence sequence = {
            .buffer = &sample_buffer,
            .buffer_size = sizeof(sample_buffer),
        };

        adc_sequence_init_dt(&adc_channel, &sequence);
        err = adc_read_dt(&adc_channel, &sequence);

        if (err < 0) {
            printk("ADC read failed: %d\n", err);
        } else {
            /* Convert the ADC reading into a percentage. */
            int32_t raw = sample_buffer;
            int32_t max_raw =
                ((int32_t)1 << adc_channel.resolution) - 1;

            /* Invert the percentage according to the observed LDR behavior. */
            int percent = 100 - (raw * 100 / max_raw);

            /* Keep the brightness percentage within 0–100%. */
            if (percent < 0) {
                percent = 0;
            } else if (percent > 100) {
                percent = 100;
            }

            /* Select the RGB color according to the brightness level. */
            if (percent < 33) {
                set_color(true, false, false);   /* RED: below 33% */
            } else if (percent <= 66) {
                set_color(true, true, false);    /* YELLOW: 33–66% */
            } else {
                set_color(false, true, false);   /* GREEN: above 66% */
            }

            /* Display the raw ADC value and calculated brightness. */
            printk("LDR raw: %d | Brightness: %d%%\n",
                   (int)raw, percent);
        }

        /* Repeat the measurement every two seconds. */
        k_msleep(2000);
    }
}

/* Create the ADC thread with a 1024-byte stack and priority 7. */
K_THREAD_DEFINE(adc_thread_id, 1024, adc_thread_entry,
                NULL, NULL, NULL, 7, 0, 0);