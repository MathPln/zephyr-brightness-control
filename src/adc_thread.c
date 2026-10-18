#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>

K_MSGQ_DEFINE(adc_msgq, sizeof(int), 10, 4);

/* Use the ADC channel declared in zephyr,user in the overlay. */
#define ADC_NODE DT_PATH(zephyr_user)
static const struct adc_dt_spec adc_channel =
    ADC_DT_SPEC_GET(ADC_NODE);

void adc_thread_entry(void *p1, void *p2, void *p3)
{
    if (!adc_is_ready_dt(&adc_channel)) {
        printk("Error: ADC not ready\n");
        return;
    }

    int err = adc_channel_setup_dt(&adc_channel);
    if (err < 0) {
        printk("ADC setup failed: %d\n", err);
        return;
    }

    int16_t sample_buffer;

    struct adc_sequence sequence = {
        .buffer = &sample_buffer,
        .buffer_size = sizeof(sample_buffer),
    };

    while (1) {
        adc_sequence_init_dt(&adc_channel, &sequence);
        err = adc_read_dt(&adc_channel, &sequence);

        if (err == 0) {
            int32_t raw = sample_buffer;

            /* Calculate the maximum value for the ADC resolution. */
            int32_t max_raw =
                ((int32_t)1 << adc_channel.resolution) - 1;

            /* Convert the raw measurement into an inverted percentage. */
            int percent = 100 - (raw * 100 / max_raw);

            /* Keep the percentage within the 0–100 range. */
            if (percent < 0) {
                percent = 0;
            } else if (percent > 100) {
                percent = 100;
            }

            printk("LDR ADC raw: %d | Brightness: %d%%\n",
                   (int)raw, percent);

            /* Send the brightness value to the message queue. */
            err = k_msgq_put(&adc_msgq, &percent, K_NO_WAIT);
            if (err < 0) {
                printk("Warning: ADC message queue is full\n");
            }
        } else {
            printk("ADC read failed: %d\n", err);
        }

        k_msleep(2000);
    }
}

K_THREAD_DEFINE(adc_thread_id, 1024, adc_thread_entry,
                NULL, NULL, NULL, 7, 0, 0);