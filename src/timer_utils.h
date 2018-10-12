

/* See https://forum.arduino.cc/index.php?topic=425385.0 */
void setup_timer() {
	// Set up the generic clock (GCLK4) used to clock timers
	REG_GCLK_GENDIV = GCLK_GENDIV_DIV(1) |          // Divide the 48MHz clock source by divisor 1: 48MHz/1=48MHz
										GCLK_GENDIV_ID(4);            // Select Generic Clock (GCLK) 4
	while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

	REG_GCLK_GENCTRL = GCLK_GENCTRL_IDC |           // Set the duty cycle to 50/50 HIGH/LOW
										 GCLK_GENCTRL_GENEN |         // Enable GCLK4
										 GCLK_GENCTRL_SRC_DFLL48M |   // Set the 48MHz clock source
										 GCLK_GENCTRL_ID(4);          // Select GCLK4
	while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

	// Feed GCLK4 to TCC2 and TC3
	REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN |         // Enable GCLK4 to TCC2 and TC3
										 GCLK_CLKCTRL_GEN_GCLK4 |     // Select GCLK4
										 GCLK_CLKCTRL_ID_TCC2_TC3;    // Feed the GCLK4 to TCC2 and TC3
	while (GCLK->STATUS.bit.SYNCBUSY);              // Wait for synchronization

	REG_TC3_COUNT16_CC0 = 4166;                     // Set the TC3 CC0 register as the TOP value in match frequency mode
	while (TC3->COUNT16.STATUS.bit.SYNCBUSY);       // Wait for synchronization

	NVIC_SetPriority(TC3_IRQn, 0);    // Set the Nested Vector Interrupt Controller (NVIC) priority for TC3 to 0 (highest)
	NVIC_EnableIRQ(TC3_IRQn);         // Connect TC3 to Nested Vector Interrupt Controller (NVIC)

	REG_TC3_INTFLAG |= TC_INTFLAG_OVF;              // Clear the interrupt flags
	REG_TC3_INTENSET = TC_INTENSET_OVF;             // Enable TC3 interrupts
	// REG_TC3_INTENCLR = TC_INTENCLR_OVF;          // Disable TC3 interrupts

	REG_TC3_CTRLA |= TC_CTRLA_PRESCALER_DIV1 |      // Set prescaler to 1, 48MHz/1 = 48MHz
									 TC_CTRLA_WAVEGEN_MFRQ |        // Put the timer TC3 into match frequency (MFRQ) mode
									 TC_CTRLA_ENABLE;               // Enable TC3
	while (TC3->COUNT16.STATUS.bit.SYNCBUSY);       // Wait for synchronization
}
