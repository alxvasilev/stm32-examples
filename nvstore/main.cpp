#define STM32PP_FLASH_DEBUG 1
#include <stm32++/flash.hpp>
#include <stm32++/tprintf.hpp>
#include <string.h>
#include <stm32++/semihosting.hpp>
#include <stm32++/timeutl.hpp>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/desig.h>

using namespace flash;

KeyValueStore<> store;
bool setString(uint8_t key, const char* str)
{
    auto size = strlen(str) + 1;
    store.setRawValue(key, str, size);
    uint8_t readSize;
    auto ptr = store.getRawValue(key, readSize);
    if (!ptr || readSize != size)
    {
        return false;
    }
    if (memcmp(str, ptr, size))
    {
        tprintf("write NOT ok (%): %\n", size, str);
        return false;
    }
    else
    {
        tprintf("write ok (%): %\n", size, str);
        return true;
    }
}

void dumpPage(uint8_t* page)
{
    enum { kLineLen = 32 };
    auto lastLine = page + 1024 - kLineLen;
    for (uint8_t* line = page; line <= lastLine; line += kLineLen)
    {
        auto lineEnd = line + kLineLen;
        char lineBuf[kLineLen * 3 + 8];
        char* writePtr = lineBuf;
        for (auto ptr = line; ptr < lineEnd; ptr++)
        {
            auto ch = *ptr;
            if (ch >= 32 && ch < 129)
            {
                *(writePtr++) = ch;
                *(writePtr++) = ' ';
            }
            else
            {
                writePtr = toString(writePtr, 4, fmtHex8<kDontNullTerminate|kNoPrefix>(ch));
            }
            *(writePtr++) = ' ';
        }
        *writePtr = 0;
        tprintf("% %\n", lineBuf, line - page + kLineLen);
    }
}
const char* values[] = {
    "this is a test message",
    "new value",
    "I am having a huge list of items (15000) to be populated on the items drop down in the front end.",
    "Hence I have made an AJAX call (triggered upon a selection of a Company) and this AJAX call to made",
    "to an action method in the Controller and this action method populates the list of service items and",
    "returns it back to the AJAX call via response. This is where my AJAX call is failing.",
    "If i have about 100 - 500 items, the ajax call works. How do I fix this issue?",
    "Here, you specify the length as an int argument to printf(), which treats the '*' in the format as a request to get the length from an argument.",
    "shouldn't be necessary unless the compiler is far more broken than not implicitly converting char arguments to int.",
    "suggests that it is in fact not doing the conversion, and picking up the other 8 bits from trash on the stack or left over in a register",
    "Incorrect. Since printf is a variadic function, arguments of type char or unsigned char are promoted to int",
    "unsigned char gets promoted to int because printf() is a variadic function (assuming <stdio.h> is included). If the header isn't included, then (a) it should be and (b) you don't have a prototype in scope so the unsigned char will still be promoted to int",
    "Here, you specify the length as an int argument to printf(), which treats the '*' in the format as a request to get the length from an argument.",
    "shouldn't be necessary unless the compiler is far more broken than not implicitly converting char arguments to int.",

    "to an action method in the Controller and this action method populates the list of service items and",
    "returns it back to the AJAX call via response. This is where my AJAX call is failing.",
    "If i have about 100 - 500 items, the ajax call works. How do I fix this issue?",
    "Here, you specify the length as an int argument to printf(), which treats the '*' in the format as a request to get the length from an argument.",
    "shouldn't be necessary unless the compiler is far more broken than not implicitly converting char arguments to int.",
    "suggests that it is in fact not doing the conversion, and picking up the other 8 bits from trash on the stack or left over in a register",

};

size_t page1 = 0x0801F800;
size_t page2 = 0x0801FC00;

int main()
{
    rcc_clock_setup_in_hse_8mhz_out_72mhz();
    dwt_enable_cycle_counter();
//  cm_enable_interrupts();
    tprintf("Page1 before init\n");
    dumpPage((uint8_t*)page1);
    tprintf("initializing\n");
    store.init(page1, page2);

    tprintf("initialized\n");
    for (auto i = 0; i< 4; i++)
    {
        setString(0x0b, values[i]);
    }
    dumpPage((uint8_t*)page1);
    for (;;);
}
