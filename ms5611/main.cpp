#include <stm32++/i2c.hpp>
#include <stm32++/drivers/ms5611.hpp>
#include <stm32++/drivers/ssd1306.hpp>
#include <stm32++/usart.hpp>
#include <stm32++/font.hpp>
#include <math.h>
#include <stm32++/semihosting.hpp>

using namespace nsusart;
extern const Font font5x7;
Usart<UsartTxDma<Usart1>> usart;
nsi2c::I2cDma<I2C1> i2c1;

void dma1_channel6_isr()
{
    i2c1.dmaTxIsr();
}

void dma1_channel4_isr()
{
    usart.dmaTxIsr();
}

int main()
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    dwt_enable_cycle_counter();

    nsi2c::I2c<I2C2> i2c2;
    i2c1.init();
    i2c2.init();
    usart.init(kEnableSend, 38400);
//    usart.setDmaPrintSink();
    MS5611<nsi2c::I2c<I2C2>> sens(i2c2);
    SSD1306<nsi2c::I2cDma<I2C1>, 128, 32> display(i2c1);
    display.init();
    sens.init();
    usDelay(1000);
    sens.sample();
    float pres = sens.pressure();
    float temp = sens.temp();
    for(uint32_t i=0;;i++)
    {
        sens.sample();
        pres = round((pres*49 + sens.pressure())) / 50;
        temp = round((temp*49 + sens.temp())) / 50;
        if (i % 50)
        {
            usDelay(10);
            continue;
        }
        display.fill(kColorBlack);
        char buf[32];
        tsnprintf(buf, 32, "temp: % C", fmtFp<2>(float((sens.temp()))/100));
        display.gotoXY(0, 0);
        display.puts(buf, font5x7);
        display.gotoXY(0, 12);
        char buf2[32];
        tsnprintf(buf2, 32, "Patm: % mbar", fmtFp<2>(pres/100));
        display.puts(buf2, font5x7);
        display.updateScreen();
        msDelay(20);
    }
}
