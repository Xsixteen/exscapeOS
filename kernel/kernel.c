#include <types.h>

// TODO: Proper makefile for .s -> .o and linking

static unsigned char *videoram = (unsigned char *) 0xb8000;
static Point cursor;

typedef struct Time {
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
} Time;


int putchar(int c);
void *memset(void *addr, int c, size_t n);
size_t strlen(const char *str);
void panic(const char *str);
void print(const char *str);
void clrscr(void);
void kmain(void* mbd, unsigned int magic);
void get_time(Time *);

void *memset(void *addr, int c, size_t n) {
	unsigned char *p = addr;

	for (size_t i = 0; i < n; i++) {
		*p++ = (unsigned char)c;
	}

	return addr;
}

size_t strlen(const char *str) {
	size_t len = 0;

	while (*str++ != 0) {
		len++;
	}

	return len;
}

void clrscr(void) {
	memset(videoram, 0, 80*25*2);
	cursor.x = 0;
	cursor.y = 0;
}

int putchar(int c) {
	char out[2] = {0};
	out[0] = (unsigned char)c;
	print(out);

	return c;
}

void print(const char *str) {
	size_t len = strlen(str);

	for (size_t i = 0; i < len; i++) {
		const unsigned int offset = cursor.y*80*2 + cursor.x*2;

		if (str[i] != '\n') {
			videoram[2*i +   offset] = (unsigned char)str[i]; // FIXME: does this work as expected?
			videoram[2*i+1 + offset] = 0x07;
		}
		else {
			cursor.x = 0;
			cursor.y++;
			continue;
		}
	}

	// Remove newline characters from the calculations
	for (size_t i = len-1; i >= 0; i--) {
		if (i == (size_t)-1)
			break;
		if (str[i] == '\n')
			len--;
	}

	if (cursor.x + len > 80) {
		cursor.y += (cursor.x+len)/80;
	}
	
	cursor.x += len;

	if (cursor.x > 80)
		cursor.x = cursor.x % 80;

	videoram[cursor.y*80*2 + cursor.x*2] = 178;
	videoram[cursor.y*80*2 + cursor.x*2 + 1] = 0x7;

	// TODO:
	// Wrapping!
	// Track cursor position, w/ wrapping
}

void panic(const char *str) {
	clrscr();
	print("PANIC: ");
	print(str);
	asm("hangloop: hlt ; jmp hangloop");
	// FIXME: halt, somehow
}

void get_time(Time *t) {
	t->year = 2011;

	unsigned char sec = 0;

	asm(
		".ll:"
		"movb $10, %%al;"
		"outb %%al, $0x70;"
		"inb $0x71, %%al;"
		"testb $0x80, %%al;"
		"jne .ll;"

		"movb $0x02, %%al;"
		"outb %%al, $0x70;"
		"inb $0x71, %%al;"
		"movb %%al, %0;"
		: "=r"(sec) : : "%al");

	// TODO: Check if BCD or not
	sec = ((sec / 16) * 10) + (unsigned char)(sec & (unsigned char)0xf);

	print("Break here");
}

void kmain(void* mbd, unsigned int magic) {
   if ( magic != 0x2BADB002 )
   {
      /* Something went not according to specs. Print an error */
      /* message and halt, but do *not* rely on the multiboot */
      /* data structure. */
   }
 
   mbd = mbd; // silence warning

   /* You could either use multiboot.h */
   /* (http://www.gnu.org/software/grub/manual/multiboot/multiboot.html#multiboot_002eh) */
   /* or do your offsets yourself. The following is merely an example. */ 
   //char * boot_loader_name =(char*) ((long*)mbd)[16];
   clrscr();

   // Initialize cursor
   cursor.x = 0;
   cursor.y = 0;

   print("Hello world! Printing works, and keeping track of the cursor as well! Blah blah");
   print("\nThis is on a new line.\n");
   print("This is also on a new line.\n this too!");

   Time t;
   memset(&t, 0, sizeof(t));
   get_time(&t);
}
