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

static void gclks_init(void)
{
	OSCCTRL->XOSCCTRL.reg = OSCCTRL_XOSCCTRL_GAIN(0x3);
	// To enable XOSC as an external crystal oscillator, the XTAL Enable bit (XOSCCTRL.XTALEN)
	// must be written to '1'. If XOSCCTRL.XTALEN is zero, the external clock input on XIN will
	// be enabled.
	OSCCTRL->XOSCCTRL.bit.AMPGC = 1;
	OSCCTRL->XOSCCTRL.bit.ONDEMAND = 0;
	OSCCTRL->XOSCCTRL.bit.XTALEN = 1;
	OSCCTRL->XOSCCTRL.bit.ENABLE = 1;
	OSCCTRL->DPLLRATIO.reg = OSCCTRL_DPLLRATIO_LDR(3) | OSCCTRL_DPLLRATIO_LDRFRAC(0);
	while (OSCCTRL->DPLLSYNCBUSY.bit.DPLLRATIO) {
	}
	OSCCTRL->DPLLCTRLB.reg |= OSCCTRL_DPLLCTRLB_REFCLK(0x1U);
	OSCCTRL->DPLLCTRLA.reg = OSCCTRL_DPLLCTRLA_ENABLE;
	while (OSCCTRL->DPLLSYNCBUSY.bit.ENABLE) {
	}
	GCLK->GENCTRL[0].reg = GCLK_GENCTRL_SRC(GCLK_GENCTRL_SRC_DPLL96M) | GCLK_GENCTRL_DIV(1) |
			       GCLK_GENCTRL_GENEN;
}

static int atmel_samc_init(void)
{
	uint32_t key;


	key = irq_lock();

	flash_waitstates_init();
	osc48m_init();
	mclk_init();
	gclks_init();

	/* Install default handler that simply resets the CPU
	 * if configured in the kernel, NOP otherwise
	 */
	NMI_INIT();

	irq_unlock(key);

	return 0;
}

SYS_INIT(atmel_samc_init, PRE_KERNEL_1, 0);
