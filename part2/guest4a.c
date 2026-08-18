#include <stddef.h>
#include <stdint.h>

char buffer[10000];
static uint32_t head_offset = 0;
// static void outptr(uint16_t port, uintptr_t value)
// {
// 	uint32_t value1 = value;
// 	asm("outl %0,%1" : /* empty */ : "a"(value1), "Nd"(port) : "memory");
// }

void *my_malloc(uint32_t size)
{
	void *return_val = buffer + head_offset;
	head_offset += size;
	return return_val;
}
struct queue
{
	int *ptr;
	int front;
	int back;
	int size;  
};

int isFull(struct queue *q)
{
	return (q->back + 1) % q->size == q->front;
}
int isEmpty(struct queue *q)
{
	return q->front == -1;
}
int push(struct queue *q, int val)
{
	if (isFull(q))
	{
		return 0;
	}
	else
	{
		if (isEmpty(q))
		{
			q->back = 0;
			q->front = 0;
		}
		else
		{
			q->back = (q->back + 1) % q->size;
		}
		(q->ptr)[q->back] = val;
		return 1;
	}
}
int pop(struct queue *q, int *val)
{
	if (isEmpty(q))
	{
		return 0;
	}
	else
	{
		*val = (q->ptr)[q->front];
		if (q->front == q->back)
		{
			q->back = -1;
			q->front = -1;
		}
		else
		{
			q->front = (q->front + 1) % q->size;
		}
		return 1;
	}
}
struct queue initQueue(int size)
{
	struct queue q;
	q.ptr = (int *)my_malloc(sizeof(int) * size);
	q.front = -1;
	q.back = -1;
	q.size = size;
	return q;
}

uint64_t rdtsc()
{
	uint32_t lo, hi;
	asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32) | lo;
}

void
	__attribute__((noreturn))
	__attribute__((section(".start")))
	_start(void)
{
	int index = 0;
	struct queue q = initQueue(20);
	// outptr(0xE9, (uintptr_t)q.ptr);
	asm("outl %0,%1" : /* empty */ : "a"(q.ptr), "Nd"(0xEA) : "memory");
	volatile int32_t newFront = 0;
	volatile int32_t newBack = 0;
	int *state = (int *)my_malloc(sizeof(int) * 2);
	for (;;)
	{
		asm volatile ("inl %1,%0" : "=a"(newFront) : "Nd"(0xEE));
		asm volatile("inl %1,%0" : "=a"(newBack) : "Nd"(0xEF));

		q.front = newFront;
		q.back = newBack;
		uint64_t rdtsc_out = rdtsc();
		int num = (int)(rdtsc_out % 11);
		for (int i = 0; i < num; i++)
		{
			if (!isFull(&q))
			{
				push(&q, index);
			}
			else
			{
				// goto out;
				// printf("Queue full\n\n");
				break;
			}
			index++;
		}
		state[0]=q.front;
		state[1]=q.back;
		asm("outl %0,%1" : /* empty */ : "a"(state), "Nd"(0xED) : "memory");
		

	}

	*(long *)0x400 = 42;
// out:
	for (;;)
		asm("hlt" : /* empty */ : "a"(42) : "memory");
}
