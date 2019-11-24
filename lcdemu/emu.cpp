#include <stm32++/tprintf.hpp>
#include <stm32++/emu/lcdemu.hpp>
#include <stm32++/stdfonts.hpp>
#include <stm32++/menu.hpp>
#include <wx/wxprec.h>

class MyApp: public ButtonApp<MyApp, 128, 64, 63, 63>
{
public:
    void onStart()
    {
        startTimer(10);
        nsmenu::MenuSystem<decltype(*lcd)> menu(*lcd, "Test menu");
        menu.addValue<nsmenu::NumValue<uint8_t, 1, &handler>>("test int", 1);
        const char* vals[] = {"one", "two", "three"};
        menu.addValue<nsmenu::EnumValue<2, &handler>>("test enum", 1, vals);
        menu.submenu("SubMenu");
        menu.render();
        lcd->updateScreen();
    }
    static bool handler(uint8_t newVal)
    {
        tprintf("change to %\n", newVal);
        return true;
    }
    static void buttonHandler(uint8_t btn, uint8_t event, void* userp)
    {
        auto self = static_cast<Self*>(userp);
        static int ctr = 0;
        printf("%.6d: handler: btn = %u, event = %u\n", ctr++, btn, event);

        if (btn == 1 && event == btn::kEventDown) {
            for (int i = 0; i< 55; i++) {
                self->lcd->putsCentered(i, "Test message");
                self->lcd->updateScreen();
                wxMilliSleep(10);
                self->lcd->clear();
                wxMilliSleep(10);
            }
        }
    }
};

wxIMPLEMENT_APP(MyApp);
