#include <stm32++/spi.hpp>
#include <stm32++/drivers/st756x.hpp>
#include <stm32++/timeutl.hpp>
#include <stm32++/tprintf.hpp>
#include <stm32++/stdfonts.hpp>
#include <stm32++/menu.hpp>

#define PIN_DATACMD GPIO0
#define PIN_RST GPIO1

nsspi::SpiMaster<SPI1> spi;
ST7567<decltype(spi), PinDesc<GPIOB, GPIO1>, PinDesc<GPIOB, GPIO0>> lcd(spi);
bool handler(uint8_t newVal)
{
    tprintf("change to %\n", newVal);
    return true;
}
int main(void)
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    dwt_enable_cycle_counter();
    PinDesc<GPIOC, GPIO13> led;
    led.enableClockAndSetMode(GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL);
    led.set();
    spi.init(nsspi::Baudrate(18000000), nsspi::kDisableInput|nsspi::k8BitFrame|nsspi::kSoftwareNSS);
    //tprintf("SPI baudrate: %, apbFreq: %\n", spi.baudrate(), spi.apbFreq());
    lcd.init();
    lcd.setFont(Font_5x7);
//    lcd.putsCentered(0, "Test message");
    nsmenu::MenuSystem<decltype(lcd)> menu(lcd, "Test Menu", 4);
    menu.addValue<nsmenu::NumValue<uint8_t, 1, &handler>>("test int", 1);
    const char* vals[] = {"one", "two", "three"};
    menu.addValue<nsmenu::EnumValue<2, &handler>>("test enum", 1, vals);
    menu.submenu("SubMenu");
    menu.render();
    lcd.updateScreen();
	return 0;
}
