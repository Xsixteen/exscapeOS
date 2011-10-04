#include <kernel/kernutil.h>
#include <kernel/interrupts.h>
#include <kernel/monitor.h>
#include <kernel/task.h>

/* This is enough to not wrap for 497 days at 100 Hz. */
uint32 timer_ticks = 0;
const uint16 TIMER_DIVISOR = 11932;
const uint16 TIMER_HZ = 100;

uint32 gettickcount(void) {
	/* Returns the number of ticks that have passed since reboot. */
	return timer_ticks;
}

uint32 uptime(void) {
	/* Returns the system uptime, in seconds. */
	return timer_ticks / TIMER_HZ;
}

void timer_handler(uint32 esp __attribute__((unused))) {
	/* Increase the tick count */
	timer_ticks++;
}

/*
 * I *think* the error from timer drift comes to ~1.3 seconds per 24 hours, but
 * I don't feel too certain about the calculations...
 * Those 1.3 seconds (if correct) come from the fact that we assume the timer frequency to be 100.00 Hz, but
 * it in realy is sliightly less at something about 99.9985 Hz.
 * Additional error will of course be added due to PIT inaccurary - or, if we're lucky, they cancel out
 * so that the accuracy in in fact improved! Needless to say, we can't know or rely on either of these, though.
 */

void timer_install(void) {
	/* 
     * Set the timer frequency
     * If the oscillator works at 1 193 182 Hz, dividing by 11932 gives
     * 99.9985 Hz, the closest possible to 100 (with an integer divisor, of course).
     */
	outb(0x43, 0x36);
	outb(0x40, TIMER_DIVISOR & 0xff); /* set low byte */
	outb(0x40, (TIMER_DIVISOR >> 8) & 0xff); /* set high byte */

	/* Install the timer handler */
	register_interrupt_handler(IRQ0, timer_handler);
}
