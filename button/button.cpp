#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <stm32++/tprintf.hpp>
#include <stm32++/timeutl.hpp>
#include <stm32++/utils.hpp>
#include <stm32++/button.hpp>
using namespace btn;
void handler(uint16_t btn, uint8_t event, void* userp)
{
    gpio_toggle(GPIOC, GPIO13);
//    tprintf("button %, event: %\n", button, event);
}

btn::Buttons<GPIOA, GPIO0|GPIO1, GPIO0|GPIO1, kOptActiveLow, NVIC_SYSTICK_IRQ> buttons;

extern "C" void sys_tick_handler()
{
    buttons.poll();
}

int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    dwt_enable_cycle_counter();
    buttons.init(handler, nullptr);
    cm_enable_interrupts();
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    GPIOC_ODR = 0;
    /* 72MHz / 8 => 9000000 counts per second */
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);

    /* 9000000/90000 = 100 overflows per second - every 10ms one interrupt */
    /* SysTick interrupt every N clock pulses: set reload to N-1 */
    systick_set_reload(89999);

    systick_interrupt_enable();

    /* Start counting. */
    systick_counter_enable();
    for (;;)
    {
        buttons.process();
        msDelay(20);
    }
    return 0;
}
