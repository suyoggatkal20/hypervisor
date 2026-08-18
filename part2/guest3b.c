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
void
	__attribute__((noreturn))
	__attribute__((section(".start")))
	_start(void)
{
	// int i = 0;
	int *ptr = (int *)my_malloc(sizeof(int) * 5);
	asm("outl %0,%1" : /* empty */ : "a"(ptr), "Nd"(0xEB) : "memory");

	for (;;)
	{
		asm("outl %0,%1" : /* empty */ : "a"(ptr), "Nd"(0xED) : "memory");
	}

	*(long *)0x400 = 42;

	for (;;)
		asm("hlt" : /* empty */ : "a"(42) : "memory");
}
