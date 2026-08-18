#include <stddef.h>
#include <stdint.h>

char buffer[10000];
static uint32_t head_offset = 0;

void *my_malloc(uint32_t size)
{
	void *return_val = buffer + head_offset;
	head_offset += size;
	return return_val;
}
static void outb(uint16_t port, uint8_t value)
{
	asm("outb %0,%1" : /* empty */ : "a"(value), "Nd"(port) : "memory");
}
static void outl(uint16_t port, uint32_t value)
{
	asm("outl %0,%1" : /* empty */ : "a"(value), "Nd"(port) : "memory");
}

static void outptr(uint16_t port, uintptr_t value)
{
	uint32_t value1 = value;
	asm("outl %0,%1" : /* empty */ : "a"(value1), "Nd"(port) : "memory");
}

// static uint8_t inb(uint16_t port)
// {
// 	uint8_t value;
// 	asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
// 	return value;
// }

static uint32_t inl(uint16_t port)
{
	uint32_t value;
	asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

void HC_print8bit(uint8_t val)
{
	outb(0xE9, val);
}

void HC_print32bit(uint32_t val)
{
	outl(0xEA, val);
}

uint32_t HC_numExits()
{
	uint32_t val = 0;
	val = inl(0xEB);
	return val;
}

void HC_printStr(char *str)
{
	outptr(0xEC, (uintptr_t)str);
}

char *HC_numExitsByType()
{
	char *output = my_malloc(20);
	outptr(0xED, (uintptr_t)output);
	return output;
}

uint32_t HC_gvaToHva(uint32_t gva)
{
	uint32_t hva = 0;
	uint32_t *output = (uint32_t *)my_malloc(sizeof(uint32_t));
	*output = gva;
	outptr(0xEF, (uintptr_t)output);
	hva = *(uint32_t *)output;
	return hva;
}

void
	__attribute__((noreturn))
	__attribute__((section(".start")))
	_start(void)
{
	const char *p;

	for (p = "Hello 695!\n"; *p; ++p)  // 11 out hypercalls
		HC_print8bit(*p);
	/*----------Don't modify this section. We will use grading script---------*/
	/*---Your submission will fail the testcases if you modify this section---*/
	HC_print32bit(2048);
	HC_print32bit(4294967295);

	uint32_t num_exits_a, num_exits_b;
	num_exits_a = HC_numExits();

	char *str = "CS695 Assignment 2\n";
	HC_printStr(str);

	num_exits_b = HC_numExits();

	HC_print32bit(num_exits_a);
	HC_print32bit(num_exits_b);

	char *firststr = HC_numExitsByType();
	uint32_t hva;
	hva = HC_gvaToHva(1024);
	HC_print32bit(hva);
	hva = HC_gvaToHva(4294967295);
	HC_print32bit(hva);
	char *secondstr = HC_numExitsByType();

	HC_printStr(firststr);
	HC_printStr(secondstr);
	/*------------------------------------------------------------------------*/

	*(long *)0x400 = 42;

	for (;;)
		asm("hlt" : /* empty */ : "a"(42) : "memory");
}
