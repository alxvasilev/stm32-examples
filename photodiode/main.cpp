#include <stm32++/timeutl.hpp>
#include <stm32++/adc.hpp>
#include <stm32++/drivers/ssd1306.hpp>
#include <stm32++/i2c.hpp>
#include <stm32++/stdfonts.hpp>
#include <stm32++/button.hpp>
#include <math.h>
#include <memory>
#include <libopencm3/stm32/iwdg.h>
#include <stm32++/usart.hpp>

using namespace nsadc;
using namespace nsi2c;

Adc<ADC1> gAdc;
dma::Tx<I2c<I2C1>> i2c;
SSD1306<decltype(i2c), 128, 64> display(i2c, 0x3c);
btn::Buttons<GPIOB, GPIO10, GPIO10> buttons;
nsusart::Usart<USART1> gUsart;

#ifdef NDEBUG
    #define NOLOG
#endif

#ifndef NOLOG
    #define LOG(fmt,...) tprintf(fmt, ##__VA_ARGS__)
#else
    #define LOG(fmt,...)
#endif

static void gpio_setup(void)
{
	/* Enable GPIO clocks. */
    rcc_periph_clock_enable(RCC_GPIOB);
    // latch the power pin
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
        GPIO_CNF_OUTPUT_PUSHPULL, GPIO11);
    gpio_clear(GPIOB, GPIO11);

    /* Setup the LEDs. */
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
        GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

    gpio_set_mode(GPIOB, GPIO_MODE_INPUT,
        GPIO_CNF_INPUT_ANALOG, GPIO1);
}

void dma1_channel1_isr()
{
    gAdc.dmaRxIsr();
}

void dma1_channel6_isr()
{
    i2c.dmaTxIsr();
}

template <uint16_t Cnt, uint8_t W>
class SampleData
{
public:
    enum: uint16_t
    {
        kSampleCount = Cnt,
        kSmoothedCount = W * 2 // * 3 / 2,
    };
    uint8_t mDownsampleFactor = 2;
    uint8_t mHeight = 0;
    uint8_t mTop = 0;
    uint16_t mSize = kSampleCount;
    int16_t mBuf[kSampleCount]; // signed (ADC sets the first 12 bits only), to allow in-place transformations
    int16_t mSmoothedBuf[kSmoothedCount];
    uint16_t mDev;
    int16_t mAvg;
    uint16_t mMin;
    uint16_t mSmoothedMin;
    uint16_t mMax;
    uint16_t mSmoothedMax;
    bool mSmoothScale = false;
    float mYScaleFactor = 1.0;
    void setTopAndHeight(uint8_t top, uint8_t height)
    {
        mTop = top;
        mHeight = height;
    }
    void downsample()
    {
        auto bufEnd = mBuf + mSize;
        auto rptr = mBuf;
        auto wptr = mBuf;
        for (;;)
        {
            auto end = rptr + mDownsampleFactor;
            if (end > bufEnd)
            {
                mSize = wptr - mBuf;
                return;
            }
            uint32_t avg = 0;
            for (; rptr < end; rptr++)
            {
                avg += *rptr;
            }
            avg = (avg + mDownsampleFactor / 2) / mDownsampleFactor; //round
            *wptr++ = avg;
        }
        mSize = wptr - mBuf;
    }
    void findAvgDevMinMax()
    {
        mMax = 0;
        mMin = 0xffff;
        for (int i = 0; i < mSize; i++)
        {
            uint16_t val = mBuf[i];
            if (val > mMax)
                mMax = val;
            if (val < mMin)
                mMin = val;
        }
        uint16_t ptp;
        if (mSmoothScale)
        {
            mSmoothedMax = (mSmoothedMax * 2 + mMax) / 3;
            mSmoothedMin = (mSmoothedMin * 2 + mMin) / 3;

            ptp = abs(mSmoothedMax - mSmoothedMin);
            mAvg = mSmoothedMin + (ptp + 1) / 2;
        }
        else
        {
            ptp = abs(mMax - mMin);
            mAvg = mMin + (ptp + 1) / 2;
        }
        mYScaleFactor = ((float)ptp + 2) / (float)(mHeight);
    }
    uint16_t autocorrelation(int16_t* data, uint16_t size, uint16_t offs)
    {
        auto* end = data + size;
        uint32_t sum = 0;
        for (auto ptr = data + offs; ptr < end; ptr++, data++)
        {
            sum += abs(*ptr - *data);
        }
        auto num = size - offs;
        return (sum + num / 2) / num;
    }
    void transformToDeltaAvg()
    {
        for (auto i = 0; i < mSize; i++)
        {
            mBuf[i] -= mAvg;
        }
    }
    uint16_t detectPeriod(int16_t* data, uint16_t size)
    {
        int32_t minCorr = 0xffff;
        uint16_t minCorrOffs = 0;
        auto endofs = size / 2;
        for (uint16_t ofs = 5; ofs <= endofs; ofs++)
        {
            int32_t corr = autocorrelation(data, size, ofs);
            if (corr < minCorr)
            {
                minCorr = corr;
                minCorrOffs = ofs;
            }
        }
        return (minCorrOffs == 5) ? 0 : minCorrOffs;
    }
    void smooth(int16_t * dest, int destSize, uint8_t factor)
    {
        auto factorMinus1 = factor - 1;
        float avg = mBuf[0];
        int destIdx = -factorMinus1;
        auto srcEnd = mBuf + mSize;
        for (auto src = mBuf + 1; (src < srcEnd) && (destIdx < destSize); src++, destIdx++)
        {
            avg = (avg * factorMinus1 + *src) / factor;
            if (destIdx >= 0)
            {
                dest[destIdx] = round(avg);
            }
        }
    }
    void smooth(uint8_t factor)
    {
        smooth(mSmoothedBuf, kSmoothedCount, factor);
    }
    uint16_t detectPeriodStart(const int16_t* buf)
    {
        enum: uint16_t kLevel = 0;
        // data should be smoothed
        auto prev = buf[0];
        for (uint16_t i = 1; i < W; i++)
        {
            auto curr = buf[i];
            if (prev <= kLevel && curr > kLevel)
            {
                return i;
            }
            prev = curr;
        }
        return 0;
    }
    uint8_t displayGetY(uint16_t offs)
    {
        int8_t y = (mHeight >> 1) - ((float)mBuf[offs] / mYScaleFactor);
        if (y < 0)
        {
            LOG("y < 0: %\n", y);
            y = 0;
        }
        else if (y >= mHeight)
        {
            LOG("y > mHeight: %\n", y);
            y = mHeight - 1;
        }
        return mTop + y;
    }
    void enableSmoothScale()
    {
        mSmoothScale = true;
        mSmoothedMax = mMax;
        mSmoothedMin = mMin;
    }
    void disableSmoothScale()
    {
        mSmoothScale = false;
    }
};

void blinkError(uint8_t code)
{
    for (;;)
    {
        for (int i=0; i < code; i++)
        {
            gpio_clear(GPIOC, GPIO13);
            msDelay(200);
            gpio_set(GPIOC, GPIO13);
            msDelay(200);
        }
        msDelay(1000);
    }
}

void powerOff()
{
    gpio_clear(GPIOB, GPIO11);
    for(;;);
}

void powerOffWithMessage()
{
    // power off
    display.clear();
    display.gotoXY(display.width() / 2 - display.charWidthWithSpacing() * 5,
                   display.height() / 2 - display.font().height / 2);
    display.puts("POWER OFF");
    display.updateScreen();
    msDelay(1000);
    powerOff();
}

SampleData<512, 128> buf;
GenericElapsedTimer<DwtCounter> time;

void buttonCb(uint16_t btn, uint8_t event, void* userp)
{
    xassert(btn = GPIO10);
    time.reset();
    gpio_toggle(GPIOC, GPIO13);
    if (event == btn::kEventHold)
    {
        powerOffWithMessage();
    }
    else if (event == btn::kEventPress)
    {
        if (buf.mSmoothScale)
            buf.disableSmoothScale();
        else
            buf.enableSmoothScale();
    }
}

int main(void)
{
    bool wasResetByWatchdog = (RCC_CSR & RCC_CSR_IWDGRSTF);
    RCC_CSR |= RCC_CSR_RMVF;
    iwdg_set_period_ms(1000);
    iwdg_start();
    rcc_clock_setup_in_hse_8mhz_out_24mhz();

    gpio_setup();

    if (wasResetByWatchdog) {
        powerOff();
    }

    cm_enable_interrupts();
    dwt_enable_cycle_counter();
    msDelay(10);
    gpio_set(GPIOB, GPIO11);
    msDelay(40);

    i2c.init();

    if (!display.init()) {
        blinkError(2);
    }
    display.setFont(Font_5x7);

    ElapsedTimer t;
    auto ticks = t.ticksElapsed();
    tprintf("ticks: %\n", ticks);

    /*
    for (uint16_t x = 0; x < 127; x++)
    {
        display.fill(kColorBlack);
        ElapsedTimer t;
        display.drawLine(0, 0, x, 60);
        tprintf("x = %, time = %us\n", x, t.usElapsed());

        display.updateScreen();
        msDelay(100);
    }

    for(int w = 1; w < 30; w++)
    {
        auto bottom = 63 - w;
        for(int top = 0; top < bottom; top++)
        {
            display.fill(kColorBlack);
            auto left = 63-w/2;
            auto right = 63+w/2-1;
            ElapsedTimer t;
            display.hLine(left, right, top);
            display.vLine(top, top+w-1, left);
            display.vLine(top, top+w-1, right);
            display.hLine(left, right, top+w);
            if (top % 4 == 0)
                tprintf("time: %\n", t.usElapsed());
            display.updateScreen();
            //msDelay(100);
        }
    }
*/
    gAdc.init(kOptContConv|kOptScanMode, 3000000);

    buf.setTopAndHeight(10, 54);
    auto code = gAdc.useSingleChannel(ADC_CHANNEL9, 10000);
    float freq = gAdc.sampleTimeCodeToFreq(code);
    float scanPeriodMs = 1000.0 * buf.kSampleCount / freq;
    (void)scanPeriodMs; //suppress unused warning
    LOG("sample freq: %Hz, sampling should take %ms\n", freq, scanPeriodMs);
    LOG("Min captured freq: %Hz\n", 1000 / scanPeriodMs);
    float avgSignalFreq = 0.0;
    float avgPct = 0.0;
    buttons.init(buttonCb, nullptr);
    buttons.setHoldDelayFor(GPIO10, 2000);
    time.reset();
    gUsart.init(nsusart::kOptEnableTx, 38400);
    gAdc.powerOn();
    for (;;)
    {
        iwdg_reset();
        if (time.msElapsed() > 60000)
        {
            powerOffWithMessage();
        }
        gAdc.dmaRxStart(buf.mBuf, sizeof(buf.mBuf));
        while(gAdc.dmaRxBusy())
        {
            asm ("wfi");
        }

        buttons.poll();
        buttons.process();

        buf.mSize = buf.kSampleCount;
        buf.findAvgDevMinMax();
        buf.transformToDeltaAvg();
        buf.smooth(4);
        auto periodSamples = buf.detectPeriod(buf.mSmoothedBuf, buf.kSmoothedCount);

        buf.downsample();

        display.fill(kColorBlack);
        char msg[64];
        auto pp = buf.mMax - buf.mMin;

        auto pct = pp * 100 / (buf.mMax+buf.mMin);
        avgPct = (avgPct * 9 + pct) / 10;

        float signalFreq = periodSamples
            ? freq / periodSamples
            : 0.0;
        if (avgSignalFreq < 0.0001)
        {
            avgSignalFreq = signalFreq;
        }
        else
        {
            if (signalFreq)
            {
                avgSignalFreq = (avgSignalFreq * 9 + signalFreq) / (float)10;
            }
            else
            {
                avgSignalFreq = 0.0;
            }
        }
        enum: uint16_t { kMaxVal = 2350 };
        if (buf.mMax > kMaxVal)
        {
            tsnprintf(msg, 63, "= Oversaturated =");
        }
        else
        {
            uint8_t sig = (buf.mMax*127 / kMaxVal);
            display.hLine(0, sig, 9);

            tsnprintf(msg, 63, "%% %Hz % B100%", fmtInt(avgPct+0.5, 0, 2), '%',
                fmtInt(avgSignalFreq+0.5, 0, 4), buf.mSmoothScale ? "L":" ");
        }
        display.gotoXY(0, 0);
        display.puts(msg);
        buf.smooth(4);
        auto offs = buf.detectPeriodStart(buf.mSmoothedBuf);
        uint8_t lastY = buf.displayGetY(offs);
        display.setPixel(0, lastY);
        for (uint8_t x = 1; x < 128; x++)
        {
            auto y = buf.displayGetY(x + offs);
            display.drawLine(x-1, lastY, x, y);
            lastY = y;
        }
        buttons.poll();
        display.updateScreen();
        gUsart.sendBlocking("opa\n");
    }
/*
    GPIOC_ODR = 0;
    auto tcode = adc.convertSingle(ADC_CHANNEL_TEMP);
    auto vref = adc.convertSingle(ADC_CHANNEL_VREF);
    auto elapsed = timer.nsElapsed();
    float mv = (1200.0f / vref);
    float vcc = mv * 4.096f;
    auto vsense = tcode * mv;
    float t = ((1430 - vsense) / 4.3f) + 25;
    LOG("temp= %(%), vcc= % (%), ns: %\r\n", fmtFp<1>(t), t, fmtFp<3>(vcc), vref, elapsed);
*/
	return 0;
}
