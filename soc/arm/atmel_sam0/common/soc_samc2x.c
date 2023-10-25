/*
 * Copyright (c) 2022 Kamil Serwus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Atmel SAMC MCU series initialization code
 */

#include <zephyr/arch/cpu.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <soc.h>

static void flash_waitstates_init(void)
{
	/* One wait state at 48 MHz. */
	NVMCTRL->CTRLB.bit.RWS = NVMCTRL_CTRLB_RWS_HALF_Val;
}

static void osc48m_init(void)
{
	/* Turn off the prescaler */
	OSCCTRL->OSC48MDIV.bit.DIV = 0;
	while (OSCCTRL->OSC48MSYNCBUSY.bit.OSC48MDIV) {
	}
	while (!OSCCTRL->STATUS.bit.OSC48MRDY) {
	}
}

static void mclk_init(void)
{
	MCLK->CPUDIV.reg = MCLK_CPUDIV_CPUDIV_DIV1_Val;
}

static void enable_watchdog()
{
    MCLK->APBAMASK.bit.WDT_ = 1;
    WDT->CONFIG.bit.WINDOW = WDT_CONFIG_PER_CYC1024_Val;
    WDT->CTRLA.bit.ENABLE = 1;
    // Loop and check for synchronization only for a fixed amount of times, don't want to boot lock in any scenario
   while(!WDT->SYNCBUSY.bit.ENABLE)
   {}
}

static void disable_watchdog()
{
    MCLK->APBAMASK.bit.WDT_ = 0;
    WDT->CTRLA.bit.ENABLE = 0;
}

// Returns true when the XOSC and DPLL96M were enabled
static bool osc_init(void)
{
    // Before starting with setting up the XOSC, we check the reason of the last reset
    // Cannot use the bit field, it's from HAL and it overlaps with a defined WDT base address define
    const bool was_watchdog_reset = RSTC->RCAUSE.reg & (1<<6);
    if(was_watchdog_reset) {
        return false;
    }
	////////////////
	/// XOSCCTRL ///
	////////////////

	// To enable XOSC as an external crystal oscillator, the XTAL Enable bit must be written
	// to 1.
	//
	// When in crystal oscillator mode (XOSCCTRL.XTALEN = 1), the XOSCCTRL.GAIN must be set
	// to match the external crystal oscillator frequency.
	// If the XOSCCTRL.AMPGC = 1, the oscillator amplitude will be automatically adjusted,
	// and in most cases result in lower power consumption.
	//
	// The XOSC is enabled by writing a 1 to the Enable bit in the XOSCCTRL register.

	OSCCTRL->XOSCCTRL.reg = OSCCTRL_XOSCCTRL_GAIN(0x3);
	OSCCTRL->XOSCCTRL.bit.AMPGC = 1;


    // The prescaler will be used by CFD to divide the CLK_OSC48M, so that it matches the XOSC freq
    // freq of CLK_XOSC in case of recovery will be CLK_OSC48M / (2^CFDPRESC)
    // We use a 16MHz XOSC, so we need to divide by 4, value of CFDPRESC is 2
    OSCCTRL->CFDPRESC.bit.CFDPRESC = 2;

    // Enable XOSC failure detection (Clock Failure Detection).
    OSCCTRL->XOSCCTRL.bit.CFDEN = 1;
//    The clock switch back is enabled. This bit is reset once the XOSC output clock is switched back to the
//    external clock or crystal oscillator.
    OSCCTRL->XOSCCTRL.bit.SWBEN = 1;

	OSCCTRL->XOSCCTRL.bit.ONDEMAND = 0;
	OSCCTRL->XOSCCTRL.bit.XTALEN = 1;
	OSCCTRL->XOSCCTRL.bit.ENABLE = 1;

	// After a hard reset, or when waking up from a sleep mode where the XOSC was disabled,
	// the XOSC will need time to stabilize on the correct frequency, depending on the external
	// crystal specification. This start-up time can be configured by changing the Oscillator
	// Start-Up Time bit group (XOSCCTRL.STARTUP). During the start-up time, the oscillator
	// output is masked to ensure that no unstable clock propagates to the digital logic.

	// The STATUS.XOSCRDY bit is set once the external clock or crystal oscillator is stable
	// and ready to be used as a clock source.
	while (!OSCCTRL->STATUS.bit.XOSCRDY) {
	}

	////////////
	/// DPLL ///
	////////////

	// The task of the DPLL is to maintain coherence between the input (reference) signal
	// and the respective output frequency, CLK_DPLL, via phase comparison.

	OSCCTRL->DPLLCTRLA.bit.ONDEMAND = 0;
	OSCCTRL->DPLLCTRLA.bit.RUNSTDBY = 0;
	OSCCTRL->DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDR((5)) | OSCCTRL_DPLLRATIO_LDRFRAC(0U);
	OSCCTRL->DPLLCTRLB.reg = OSCCTRL_DPLLCTRLB_REFCLK(0x1U) | OSCCTRL_DPLLCTRLB_FILTER(0x0U);
	OSCCTRL->DPLLPRESC.reg = OSCCTRL_DPLLPRESC_PRESC(0x0U);
	// The DPLLC is enabled by writing a 1 to DPLLCTRLA.ENABLE.
	OSCCTRL->DPLLCTRLA.bit.ENABLE = 1;

	// The DPLLSYNCBUSY.ENABLE is set when the DPLLCTRLA.ENABLE bit is modified.
	// It is cleared when the DPLL output clock CK has sampled the bit at the high level after
	// enabling the DPLL. When disabling the DPLL, DPLLSYNCBUSY.ENABLE is cleared when the
	// output clock is no longer running.
	while (OSCCTRL->DPLLSYNCBUSY.bit.ENABLE) {
	}

	// The frequency of the DPLL output clock CK is stable when the module is enabled
	// and when the Lock bit in the DPLL Status register (DPLLSTATUS.LOCK) is set.
	while (!OSCCTRL->DPLLSTATUS.bit.LOCK) {
	}

	// Output clock ready
	while (!OSCCTRL->DPLLSTATUS.bit.CLKRDY) {
	}

    return true;
}

static void gclks_init(bool useDPLL96M)
{
	// Before a Generator is enabled, the corresponding clock source must be enabled
	// (already done in osc_init()).
    if(useDPLL96M)
    {
	    GCLK->GENCTRL[0].bit.SRC = GCLK_GENCTRL_SRC_DPLL96M_Val;
    } else {
        GCLK->GENCTRL[0].bit.SRC = GCLK_GENCTRL_SRC_OSC48M_Val;
    }

	// 1. The Generator must be enabled (GENCTRL.GENEN = 1) and the division factor must
	// be set (GENCTRLn.DIVSEL and GENCTRLn.DIV) by performing a single 32-bit write to the
	// Generator Control register (GENCTRLn).
	GCLK->GENCTRL[0].bit.DIVSEL = 0;
	GCLK->GENCTRL[0].bit.DIV = 1;
	GCLK->GENCTRL[0].bit.GENEN = 1;

    while (GCLK->SYNCBUSY.bit.GENCTRL0){
    }

	// 2. The Generic Clock for a peripheral must be configured by writing to the respective
	// PCHCTRLm register. The Generator used as the source for the Peripheral Clock
	// must be written to GEN bit field in the PCHCTRLm.GEN register.
}

static int atmel_samc_init(void)
{
	uint32_t key;

	key = irq_lock();

	flash_waitstates_init();

    enable_watchdog();

	osc48m_init();
	mclk_init();
    bool dpll_now_used = osc_init();
	gclks_init(dpll_now_used);

    disable_watchdog();
	/* Install default handler that simply resets the CPU
	 * if configured in the kernel, NOP otherwise
	 */
	NMI_INIT();

	irq_unlock(key);

	return 0;
}

SYS_INIT(atmel_samc_init, PRE_KERNEL_1, 0);
