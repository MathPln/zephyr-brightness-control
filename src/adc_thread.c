#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>

#define ADC_NODE DT_PATH(adc_sensor)
static const struct adc_dt_spec adc_channel = ADC_DT_SPEC_GET(ADC_NODE);

void adc_thread_entry(void *p1, void *p2, void *p3)
{
    if (!adc_is_ready_dt(&adc_channel)) {
        printk("Error : ADC not ready\n");
        return;
    }

    adc_channel_setup_dt(&adc_channel);

    int16_t sample_buffer;
    struct adc_sequence sequence = {
        .buffer = &sample_buffer,
        .buffer_size = sizeof(sample_buffer),
    };

    while (1) {
        adc_sequence_init_dt(&adc_channel, &sequence);
        
        if (adc_read_dt(&adc_channel, &sequence) == 0) {
            printk("LDR ADC raw: %d\n", sample_buffer);
        }
        
        k_msleep(2000); 
    }
}

K_THREAD_DEFINE(adc_thread_id, 1024, adc_thread_entry, NULL, NULL, NULL, 7, 0, 0);