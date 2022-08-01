#include <stm32++/tprintf.hpp>
#include <stm32++/timeutl.hpp>
#include <stm32++/gpio.hpp>
#include <stm32++/utils.hpp>
#include <stm32++/xassert.hpp>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/systick.h>
#include <memory.h>
struct MyProtocol
{
    enum { kBitCount = 32, kPreambleTime = 2000, kShortTime = 400,
           kLongTime = 1200, kRepeatAfter = 10000 };
};
const uint32_t kButtonACode = 0b11011001011101000110100111101010;

nsgpio::Pin<GPIOC, GPIO13> ledPin;
nsgpio::Pin<GPIOB, GPIO15> inputPin;
nsgpio::Pin<GPIOB, GPIO10> outPin;
TimeClockNoWrap<true, DwtCounter> timer;

template <class Proto>
struct PulseDecoder
{
    enum State: uint8_t { kDisabled = 0, kWaitPreamble, kRecvPreambleHigh,
                 kRecvPreambleLow, kRecvData, kDataReady };
    enum { kTimingTolerance = 200 };
    enum Error: uint8_t { kErrPreambleHigh=0, kErrPreambleLow, kErrLevelDur, kErrBitDur, kErrLast=kErrBitDur };
    static const char** errNames() {
        static const char* ret[] = { "PreambleHigh", "PreambleLow", "LevelDur", "BitDur" };
        return ret;
    }

    volatile State mState = kDisabled;
    uint64_t mData;
    uint64_t mCurrBit;
    bool mLastEdgeWasRising;
    uint64_t mTsLastEdge;
    uint64_t mHighDur;
    uint32_t mDecodeFails;
    uint64_t mTsDecodeFailCtrReset;
    uint16_t mPreambleHighDur;
    uint16_t mPreambleLowDur;
    uint16_t mErrors[kErrLast + 1];
    void restart()
    {
        mLastEdgeWasRising = false;
        mState = kWaitPreamble;
    }
    template <typename T>
    void errorRestart(Error code, T val)
    {
        mErrors[code]++;
        restart();
        //tprintf("%: %\n", errNames()[code], val);
    }
    void onEdge(bool isRising)
    {
        if (mState == kDisabled || mState == kDataReady) {
            return;
        }
        auto now = timer.ticks();
        if (isRising) {
            ledPin.clear();
        } else {
            ledPin.set();
        }

        if (mState == kWaitPreamble) {
            // first pulse
            if (isRising) { // first pulse can't be falling edge
                mTsLastEdge = now;
                mLastEdgeWasRising = true;
                mState = kRecvPreambleHigh;
            }
            return;
        }

        uint64_t dur = timer.ticksToUs(now - mTsLastEdge);

        switch (mState) {
        case kRecvPreambleHigh: {
            if (dur < Proto::kPreambleTime - kTimingTolerance || dur > Proto::kPreambleTime + kTimingTolerance) {
                errorRestart(kErrPreambleHigh, dur);
                return;
            }
            mPreambleHighDur = dur;
            // preamble high period received
            mState = kRecvPreambleLow;
            break;
        }
        case kRecvPreambleLow: {
            if (dur < Proto::kShortTime - kTimingTolerance || dur > Proto::kShortTime + kTimingTolerance) {
                errorRestart(kErrPreambleLow, dur);
                return;
            }
            mData = 0;
            mCurrBit = 1 << (Proto::kBitCount - 2); //FIXME
            mState = kRecvData;
            mPreambleLowDur = dur;
            break;
        }
        case kRecvData: {
            if (dur < 10 || dur > Proto::kLongTime * 2) {
                errorRestart(kErrLevelDur, dur);
                return;
            }
            if (mLastEdgeWasRising) { // receivd high period
                mHighDur = dur;
            } else { // received low period, validate both periods and decode data
                auto bitDur = dur + mHighDur;
                if (bitDur < (Proto::kShortTime + Proto::kLongTime) / 10 ||
                    bitDur > (Proto::kShortTime + Proto::kLongTime) * 2) {
                    errorRestart(kErrBitDur, bitDur);
                    return;
                }
                if (mHighDur > dur) { // one
                    mData |= mCurrBit;
                }
                mCurrBit >>= 1;
                if (!mCurrBit) {
                    /*if (Proto::kBitCount < 64) {
                        mData >>= 64 - Proto::kBitCount;
                    }*/
                    mState = kDataReady;
                }
            }
            break;
        }
        default: break;
        }
        mLastEdgeWasRising = !mLastEdgeWasRising;
        mTsLastEdge = now;
    }
    uint64_t data() const { return mData; }
    void resetDecodeFails()
    {
        memset(mErrors, 0, sizeof(mErrors));
        mTsDecodeFailCtrReset = timer.ticks();
    }
    void printDecodeFailsPerSec()
    {
        auto ms100 = timer.ticksTo100Ms(timer.ticks() - mTsDecodeFailCtrReset);
        for (int err = 0; err <= kErrLast; err++)
        {
            tprintf("%/sec: %\n", errNames()[err], fmtFp<2>((float)mErrors[err] * 10 / ms100));
        }
    }
};

PulseDecoder<MyProtocol> pulseDecoder;

volatile uint32_t decodeFails = 0;
uint64_t tsDecodeFailCtrReset = 0;
/*
void printPulses()
{
    tprintf("========== pulses: % ===========\n", lastPulse.idx);
    for (int i = 0; i < lastPulse.idx;) {
        auto t1 = pulses[i++];
        auto t0 = pulses[i++];
        auto len = t1 + t0;
        tprintf("bit %: t1= %, t0= %, len= %, ratio= %%\n", i/2, t1, t0, len, t1*100/len, '%');
    }
    tprintf("================================\n");
}

uint32_t decodePulses()
{
    uint32_t data = 0;
    uint32_t bit = 0x80000000;
    for (int i = 0; i<lastPulse.idx; bit >>= 1)
    {
        auto t1 = pulses[i++];
        auto t0 = pulses[i++];
        if (t1 > t0) { // one
            data |= bit;
        }
    }
    return data;
}
*/

template <class Proto=MyProtocol>
void transmitCode(uint32_t code)
{
    for (;;)
    {
        usDelay(Proto::kRepeatAfter);
        // preamble
        if (Proto::kPreambleTime != 0)
        {
            outPin.set();
            usDelay(Proto::kPreambleTime);
            outPin.clear();
            usDelay(Proto::kShortTime);
        }
        for (uint32_t mask = 1 << (Proto::kBitCount-1); mask; mask >>= 1)
        {
            outPin.set();
            if (code & mask)
            {
                usDelay(Proto::kLongTime);
                outPin.clear();
                usDelay(Proto::kShortTime);
            }
            else {
                usDelay(Proto::kShortTime);
                outPin.clear();
                usDelay(Proto::kLongTime);
            }
        }
    }
}


int main()
{
    rcc_clock_setup_in_hse_8mhz_out_24mhz();
    dwt_enable_cycle_counter();
    ledPin.setMode(GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL);
    ledPin.set();
    outPin.setMode(GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL);
    outPin.clear();
    if (0) {
        transmitCode(kButtonACode);
        return 0;
    }

    inputPin.setModeInputPuPd(false); //pull down
    inputPin.configInterrupt(EXTI_TRIGGER_BOTH);
/*
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    systick_set_reload(89999);
    systick_interrupt_enable();
    systick_counter_enable();
*/
    cm_enable_interrupts();
    nvic_enable_irq(inputPin.irqn());
    tprintf("Button A code: %\n", fmtBin(kButtonACode));
    for (;;)
    {
        pulseDecoder.resetDecodeFails();
        pulseDecoder.restart();
        inputPin.enableInterrupt();

        while(pulseDecoder.mState != pulseDecoder.kDataReady)
        {
            asm("wfi");
        }
        inputPin.disableInterrupt();
        tprintf("===================\n");
        tprintf("preamble: high: %, low: %\n", pulseDecoder.mPreambleHighDur, pulseDecoder.mPreambleLowDur);
        tprintf("data = % (%)\n", fmtHex(pulseDecoder.data()),
                fmtBin(pulseDecoder.data()));
        if (pulseDecoder.data() != (kButtonACode >> 1)) {
            tprintf("\n\n       ============= \033[1;31mDATA DOES NOT MATCH ================\033[0;0m\n");
        } else {
            tprintf("\n\n       ============= \033[1;32mDATA MATCHES ================\033[0;0m\n");
        }
        pulseDecoder.printDecodeFailsPerSec();
    }
}
/*
extern "C" void sys_tick_handler()
{
    IntrDisable disable;
    if (!busy || lastPulse.idx < 0) {
         return;
    }
    auto now = timer.ticks();
    if (timer.ticksToMs(now - lastPulse.tsStart) > 100) {
        busy = false;
    }
}
*/
extern "C" void exti15_10_isr(void)
{
    //tprintf("intr\n");
    exti_reset_request(EXTI15);
    pulseDecoder.onEdge(inputPin.get() ? true : false);
}
