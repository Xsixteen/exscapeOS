#include <string.h>
#include <kernel/kheap.h>
#include <kernel/interrupts.h>
#include <kernel/kernutil.h>
#include <kernel/pmm.h>

#define PAGE_SIZE 4096

/* The bitset of free/used frames */
uint32 *used_frames;
uint32 nframes;

uint32 mem_end_page;

extern uint32 placement_address;

void pmm_init(uint32 upper_mem) {
	/* upper_mem is provided by GRUB; it's the number of *continuous* kilobytes of memory starting at 1MB (0x100000). */
	mem_end_page = 0x100000 + (uint32)upper_mem*1024;

	/* Ignore the last few bytes of RAM to align, if necessary */
	mem_end_page &= 0xfffff000;

	/* The size of the bitmap is one bit per page */
	nframes = mem_end_page / PAGE_SIZE;

	/* allocate and initialize the bitmap */
	used_frames = (uint32 *)kmalloc((nframes / 32 + 1) * sizeof(uint32));
	memset(used_frames, 0, (nframes / 32 + 1) * sizeof(uint32));
}

/* Bitmap macros */
/* 32 == sizeof(uint32) in bits, so these simply calculate which dword a bit belongs to,
 * and the number of bits to shift that dword to find it, respectively. */
#define INDEX_FROM_BIT(a) (a / 32)
#define OFFSET_FROM_BIT(a) (a % 32)

/* Set a bit in the used_frames bitmap */
static void _pmm_set_frame(uint32 phys_addr) {
	uint32 frame_index = phys_addr / PAGE_SIZE;
	uint32 index = INDEX_FROM_BIT(frame_index);
	assert (index <= nframes/32 - 1);
	uint32 offset = OFFSET_FROM_BIT(frame_index);
	assert((used_frames[index] & (1 << offset)) == 0);
	used_frames[index] |= (1 << offset);
}

/* Clear a bit in the used_frames bitmap */
static void _pmm_clear_frame(uint32 phys_addr) {
	uint32 frame_index = phys_addr / PAGE_SIZE;
	uint32 index = INDEX_FROM_BIT(frame_index);
	assert (index <= nframes/32 - 1);
	uint32 offset = OFFSET_FROM_BIT(frame_index);
	assert((used_frames[index] & (1 << offset)) != 0);
	used_frames[index] &= ~(1 << offset);
}

/* Test whether a bit is set in the used_frames bitmap */
static bool _pmm_test_frame(uint32 phys_addr) {
	uint32 frame_index = phys_addr / PAGE_SIZE;
	uint32 index = INDEX_FROM_BIT(frame_index);
	assert (index <= nframes/32 - 1);
	uint32 offset = OFFSET_FROM_BIT(frame_index);
	if ((used_frames[index] & (1 << offset)) != 0)
		return true;
	else
		return false;
}

/* Returns the first free frame, roughly after (or at) /start_addr/ */
static uint32 _pmm_first_free_frame(uint32 start_addr) {
	uint32 index = start_addr / PAGE_SIZE / 32;
	if (index != 0)
		index -= 1; // TODO: fix this - this is to be on the safe side by wasting time instead of getting bad results during the initial implementation phase

	for (; index < nframes / 32; index++) {
		if (used_frames[index] == 0xffffffff) {
			/* No bits are free among the 32 tested; try the next index */
			continue;
		}

		/* Since we're still here, at least one bit among these 32 is zero... Let's find the first. */
		// Offset starts at 0 which means we *may* return something earlier than start_addr,
		// but that is only indended as a rough guide, not a hard rule.
		for (uint32 offset = 0; offset < 32; offset++) {
			if ((used_frames[index] & (1 << offset)) == 0) {
				/* Found it! Return the frame address. */
				return (index * 32 + offset) * PAGE_SIZE;
			}
		}
	}

	/* If this is reached, there were no free frames! */
	return 0xffffffff;
}

uint32 pmm_alloc(void) {
	INTERRUPT_LOCK;
	// TODO: keep track of the first free frame for quicker access
	uint32 phys_addr = _pmm_first_free_frame(0);
	if (phys_addr == 0xffffffff) {
		panic("pmm_alloc: no free frames (out of memory)!");
	}

	_pmm_set_frame(phys_addr); // also tests that it's actually free
	INTERRUPT_UNLOCK;
	return phys_addr;
}

// Allocates /num_frames/ continuous physical frames
uint32 pmm_alloc_continuous(uint32 num_frames) {
	if (num_frames < 2)
		return pmm_alloc();

	uint32 last;
   	if (num_frames < (placement_address / PAGE_SIZE)) {
		// This is a regular allocation: there won't be stuff free below the placement address, so don't bother trying
		last = placement_address + PAGE_SIZE;
	}
	else {
		// This is (most likely!) the very FIRST "allocation" where we identity map the lower addresses. Always start at zero here.
		last = 0;
	}

	uint32 start = _pmm_first_free_frame(last);
	bool success = false;

	INTERRUPT_LOCK;

	/*
	 * The idea behind this (naïve but relatively simple) algorithm is:
	 * 1) Find the first free frame
	 * 2) Are (frame+1), (frame+2), (frame+...), (frame + (num_frames - 1)) also free?
	 *	3) If yes: we're done, allocate them and return
	 *  4) If no: find the next free frame; start looking *after* the used one we found in step 2
	 */

	while (!success) {
		success = true; // if set when the for loop breaks, we're done
		if (start + (num_frames - 1) * PAGE_SIZE > mem_end_page)
			panic("pmm_alloc_continuous: no large enough continuous region found");

		for (uint32 i=1; i < num_frames; i++) { // we know that start + 0 is free, so start looking at 1
			if (_pmm_test_frame(start + (i * PAGE_SIZE)) != 0) {
				// We found a non-free frame! D'oh!
				// Start over at the next possibly free address.
				last = start + ((i+1) * PAGE_SIZE);
				success = false;
				break;
			}
		}
		// if the for loop didn't break because of finding a page, success == true and we'll exit
	}

	// Phew! /num_frames/ starting at (and including) /start/ ought to be free now.
	for(uint32 i=0; i < num_frames; i++) {
		_pmm_set_frame(start + i * PAGE_SIZE);
	}

	INTERRUPT_UNLOCK;

	return start;
}

void pmm_free(uint32 phys_addr) {
	INTERRUPT_LOCK;
	assert(_pmm_test_frame(phys_addr) != 0);
	_pmm_clear_frame(phys_addr);
	INTERRUPT_UNLOCK;
}

/* Returns the amount of *physical* RAM that it still unused, i.e. unused_frame_count * 4096
 * Note that this function is simple, not fast! It should NOT be called often, e.g. in loops! */
uint32 pmm_bytes_free(void) {
	uint32 unused = 0;
	INTERRUPT_LOCK;

	for (uint32 index = 0; index < nframes/32; index++) {
		if (used_frames[index] == 0) {
			/* All 32 frames in this bitmap chunk are free */
			unused += PAGE_SIZE * 32;
			continue;
		}
		else if (used_frames[index] == 0xffffffff) {
			/* All 32 frames in this bitmap chunk are used */
			continue;
		}

		/* We're somewhere in between all used and all free; let's check a bit closer */
		for (uint32 offset = 0; offset < 32; offset++) {
			if ( (used_frames[index] & (1 << offset)) == 0 ) {
				unused += PAGE_SIZE;
			}
		}
	}

	INTERRUPT_UNLOCK;
	assert(unused % 4096 == 0);
	return unused;
}