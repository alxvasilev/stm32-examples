#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>

#include <stm32++/tprintf.hpp>
#include <stm32++/timeutl.hpp>
#include <stm32++/utils.hpp>
#include <stm32++/usart.hpp>

nsusart::Usart<USART1> uart;

int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    dwt_enable_cycle_counter();
    cm_enable_interrupts();
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    gpio_clear(GPIOC, GPIO13);
    msDelay(2000);
    uart.init(nsusart::kOptEnableTx|nsusart::kOptEnableRx, 115200, USART_PARITY_EVEN, USART_STOPBITS_1);
    uart.powerOn();
    for (;;)
    {
        gpio_toggle(GPIOC, GPIO13);
        msDelay(1000);
        uart.sendBlocking("test message\n");
    }
    return 0;
}
