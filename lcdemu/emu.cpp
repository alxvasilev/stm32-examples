#include <stm32++/tprintf.hpp>
#include <stm32++/emu/lcdemu.hpp>
#include <stm32++/stdfonts.hpp>
#include <stm32++/menu.hpp>
#include <wx/wxprec.h>

class MyApp: public ButtonApp<MyApp, 128, 64, 63, 63>
{
public:
    typedef nsmenu::MenuSystem<decltype(*lcd)> Menu;
    Menu* menu;
    void onStart()
    {
        lcd->setFont(Font_5x7);
        menu = new Menu(*lcd, "Test menu");
        menu->addValue<nsmenu::NumValue<uint8_t, 1, &handler>>("test int", 1);
        menu->addEnum<2, &handler>("test enum", 1, {"one", "two", "three"});
        menu->submenu("SubMenu");
        menu->addValue<nsmenu::NumValue<uint8_t, 1, &handler>>("test int", 4);
        menu->addValue<nsmenu::NumValue<uint8_t, 1, &handler>>("test int", 6);
        menu->addValue<nsmenu::NumValue<int16_t, 1>>("test int", -800);
//        menu->addValue<nsmenu::NumValue<float, 1, &handler>>("test float", 1234.567);
        menu->addEnum<2, &handler>("test bool(enum)", 1, {"yes", "no"});

        menu->addValue<nsmenu::BoolValue<1, &handler>>("test bool", 1);
        menu->render();
        lcd->updateScreen();
        startTimer(10);
    }
    static bool handler(uint8_t newVal)
    {
        tprintf("change to %\n", newVal);
        return true;
    }
    static void buttonHandler(uint8_t btn, uint8_t event, void* userp)
    {
        auto self = static_cast<MyApp*>(userp);
        static int ctr = 0;
        printf("%.6d: handler: btn = %u, event = %u\n", ctr++, btn, event);
        if (event == btn::kEventDown || event ==  btn::kEventRepeat) {
            if (btn == 1) {
                self->menu->onButtonUp();
            } else if (btn == 2) {
                self->menu->onButtonDown();
            }
        }
    }
};

wxIMPLEMENT_APP(MyApp);
