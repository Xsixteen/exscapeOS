#include <kernel/console.h>
#include <string.h>
#include <stdlib.h>
#include <kernel/kernutil.h>
#include <stdio.h>
#include <stdarg.h>
#include <kernel/kheap.h>
#include <kernel/task.h>

/* A character representing empty space on the screen */
static const uint16 blank = (0x7 << 8 /* grey on black */) | 0x20 /* space */;

static uint16 *videoram = (uint16 *) 0xb8000;

/* Used for double buffering when scrolling (due to a lack of memmove()) */
uint16 *vram_buffer = NULL;

/* task.c */
extern volatile task_t *current_task;
extern task_t kernel_task;

/* TODO: mutexes! */

volatile console_t *current_console;

console_t kernel_console = {
	.task = &kernel_task,
	.active = true,
	.cursor = { .x = 0, .y = 0},
	.prev_console = NULL,
};

void console_switch(console_t *new) {
	assert(new != NULL);

	if (new == current_console)
		return;

	/* Copy the video memory of the new console to the actual video RAM, to display it */
	memcpy(videoram, new->videoram, 80*25*2);
	//memcpy(

	/* Switch the consoles */
	current_console->active = false;
	new->active = true;
	current_console = new;
}

/* Creates a new console for the specified task */
console_t *console_create(task_t *owning_task) {
	assert(owning_task != NULL);
	assert(owning_task != &kernel_task);

	console_t *new = kmalloc(sizeof(console_t));
	new->task = owning_task;
	new->active = false;

	/* Copy the screen content and cursor position from the currently displayed console */
	memcpy(new->videoram, ((console_t *)current_console)->videoram, 80*25*2);
	memcpy(& new->cursor, & ((console_t *)current_console)->cursor, sizeof(Point));

	new->prev_console = (console_t *)current_console;

	return new;
}

/* Destroy a console (free its memory, etc.) and switch to the previous one */
void console_destroy(console_t *con) {
	assert(con->prev_console != NULL);
	console_t *prev = con->prev_console;

	/* Copy the new display back to the previous console */
	memcpy(prev->videoram, con->videoram, 80*25*2);
	memcpy(&prev->cursor, &con->cursor, sizeof(Point));

	if (current_console == con) {
		console_switch(con->prev_console);
	}

	kfree(con);
}

/* Syscall test function */
int print(const char *s) {
	size_t len = strlen(s);

	for (size_t j = 0; j < len; j++) {
		putchar(s[j]);
	}

	update_cursor();

	return len;
}
/*
void print_time(const Time *t) {
	// Prints the time in the bottom corner.

	// Save the cursor (since printk will modify it)
	Point p = cursor;

	cursor.y = 24;
	cursor.x = 0;

	// clear the area
	memsetw(videoram + 24*80, blank, 80);

	printk("%d-%02d-%02d %02d:%02d:%02d", 
			t->year, t->month, t->day, t->hour, t->minute, t->second);

	// Restore the cursor
	cursor = p;
}
*/

/* Called by kmain() on boot. Creates the VRAM buffer (used in scroll() and maybe others). */
void init_video(void) {
	kernel_task.console = &kernel_console;
	current_console = &kernel_console;

	if (vram_buffer == NULL)
		vram_buffer = (uint16 *)kmalloc(80*25*2);

	clrscr();
}

void clrscr(void) {
	memsetw( ((console_t *)current_console)->videoram, blank, 80*25);
	if (current_console->task == current_task) {
		/* If the task that's calling clrscr() has its console on display, also update the screen at once */
		memsetw(videoram, blank, 80*25);
	}

	Point *cursor = &current_task->console->cursor;
	cursor->x = 0;
	cursor->y = 0;
	update_cursor();
}

void cursor_left(void) {
	Point *cursor = &current_task->console->cursor;
	if (cursor->x != 0)
		cursor->x--;
	update_cursor();
}

void cursor_right(void) {
	Point *cursor = &current_task->console->cursor;
	if (cursor->x < 79) /* TODO: is this correct? I'm slightly unsure about the indexing... */
		cursor->x++;
	else {
		cursor->x = 0;
		cursor->y++;
		scroll();
	}

	update_cursor();
}

void scroll(void) {
	Point *cursor = &current_task->console->cursor;
	if (cursor->y < 25)
		return;

	/* Copy the entire screen to the buffer */
	memcpy(vram_buffer, current_task->console->videoram, 80*25*2);
	
	/* Copy back the lower 24 lines
	 * Note that we add 80, not 80*2, due to pointer arithmetic! (vram_buffer is a uint16 *) */
	memcpy(current_task->console->videoram, vram_buffer + 80, 80*24*2);

	/* Blank the last line */
	memsetw(current_task->console->videoram + 24*80, blank, 80);

	/* Also update the screen, if this console is currently displayed */
	if (current_console->task == current_task) {
		assert(current_console->active == true);
		memcpy(videoram, current_task->console->videoram, 80*25*2);
	}

	/* Move the cursor */
	cursor->y = 24;
//	update_cursor();
}

int putchar(int c) {
	Point *cursor = &current_task->console->cursor;
	if (c == '\n') {
		// c == newline
		cursor->x = 0;
		cursor->y++;
	}
	else if (c == 0x08) {
		// Backspace
		if (cursor->x > 0)
			cursor->x--;
	}
	else if (c >= 0x20) {
		// 0x20 is the lowest printable character (space)
		// Write the character
		const unsigned int offset = cursor->y*80 + cursor->x;
		current_task->console->videoram[offset] = ( ((unsigned char)c)) | (0x07 << 8); /* grey on black */
		if (current_console->task == current_task) {
			/* Also update the actual video ram if this console is currently displayed */
			assert(current_console->active == true);
			videoram[offset] = ( ((unsigned char)c)) | (0x07 << 8); /* grey on black */
		}

		if (cursor->x + 1 == 80) {
			// Wrap to the next line
			cursor->y++;
			cursor->x = 0;
		}
		else {
			// Don't wrap
			cursor->x++;
		}
	}

	scroll(); // Scroll down, if need be
//	update_cursor();

	return c;
}

void update_cursor(void) {
	// Moves the hardware cursor to the current position specified by the cursor struct
	Point *cursor = &current_task->console->cursor; /* TODO: use current_console instead? */
	uint16 loc = cursor->y * 80 + cursor->x;

	uint8 high = (uint8)((loc >> 8) & 0xff);
	uint8 low  = (uint8)(loc & 0xff);
	outb(0x3d4, 0xe);
	outb(0x3d5, high);
	outb(0x3d4, 0xf);
	outb(0x3d5, low);
}

/* The buffer used by printk */
static char buf[1024];

size_t printk(const char *fmt, ...) {
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf, fmt, args);
	va_end(args);

	if (i > 0) {
		size_t len = strlen(buf);
		for (size_t j = 0; j < len; j++) {
			putchar(buf[j]);
		}
	}
	update_cursor();

	return i;
}

int sprintf(char *sprintf_buf, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(sprintf_buf,fmt,args);
	va_end(args);

	return i;
}