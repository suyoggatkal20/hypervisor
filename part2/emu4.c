#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>

/* CR0 bits */
#define CR0_PE 1u
#define CR0_MP (1U << 1)
#define CR0_EM (1U << 2)
#define CR0_TS (1U << 3)
#define CR0_ET (1U << 4)
#define CR0_NE (1U << 5)
#define CR0_WP (1U << 16)
#define CR0_AM (1U << 18)
#define CR0_NW (1U << 29)
#define CR0_CD (1U << 30)
#define CR0_PG (1U << 31)

/* CR4 bits */
#define CR4_VME 1
#define CR4_PVI (1U << 1)
#define CR4_TSD (1U << 2)
#define CR4_DE (1U << 3)
#define CR4_PSE (1U << 4)
#define CR4_PAE (1U << 5)
#define CR4_MCE (1U << 6)
#define CR4_PGE (1U << 7)
#define CR4_PCE (1U << 8)
#define CR4_OSFXSR (1U << 8)
#define CR4_OSXMMEXCPT (1U << 10)
#define CR4_UMIP (1U << 11)
#define CR4_VMXE (1U << 13)
#define CR4_SMXE (1U << 14)
#define CR4_FSGSBASE (1U << 16)
#define CR4_PCIDE (1U << 17)
#define CR4_OSXSAVE (1U << 18)
#define CR4_SMEP (1U << 20)
#define CR4_SMAP (1U << 21)

#define EFER_SCE 1
#define EFER_LME (1U << 8)
#define EFER_LMA (1U << 10)
#define EFER_NXE (1U << 11)

/* 32-bit page directory entry bits */
#define PDE32_PRESENT 1
#define PDE32_RW (1U << 1)
#define PDE32_USER (1U << 2)
#define PDE32_PS (1U << 7)

/* 64-bit page * entry bits */
#define PDE64_PRESENT 1
#define PDE64_RW (1U << 1)
#define PDE64_USER (1U << 2)
#define PDE64_ACCESSED (1U << 5)
#define PDE64_DIRTY (1U << 6)
#define PDE64_PS (1U << 7)
#define PDE64_G (1U << 8)

char buffer[10000];
static uint32_t head_offset = 0;
void *my_malloc(uint32_t size)
{
	void *return_val = buffer + head_offset;
	head_offset += size;
	return return_val;
}
int *vm1queue;
int *vm2queue;
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
void printQueue(struct queue *q)
{
	printf("HYPVSR: [");
	if (q->front != -1)
	{
		int idx = q->front - 1;
		while (idx != q->back)
		{
			idx = (idx + 1) % q->size;
			printf(" %d", q->ptr[idx]);
		}
	}
	printf("]\n");
}
struct queue initQueue(int size)
{
	struct queue q;
	q.ptr = (int *)my_malloc(sizeof(int) * size);
	q.back = -1;
	q.front = -1;
	q.size = size;
	return q;
}

uint64_t rdtsc()
{
	uint32_t lo, hi;
	asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32) | lo;
}

struct vm
{
	int dev_fd;
	int vm_fd;
	char *mem;
};

struct vcpu
{
	int vcpu_fd;
	struct kvm_run *kvm_run;
};

/* Data from sched.txt */
char sched_order[100];

void vm_init(struct vm *vm, size_t mem_size)
{
	int kvm_version;
	struct kvm_userspace_memory_region memreg;

	vm->dev_fd = open("/dev/kvm", O_RDWR);
	if (vm->dev_fd < 0)
	{
		perror("open /dev/kvm");
		exit(1);
	}

	kvm_version = ioctl(vm->dev_fd, KVM_GET_API_VERSION, 0);
	if (kvm_version < 0)
	{
		perror("KVM_GET_API_VERSION");
		exit(1);
	}

	if (kvm_version != KVM_API_VERSION)
	{
		fprintf(stderr, "Got KVM api version %d, expected %d\n",
				kvm_version, KVM_API_VERSION);
		exit(1);
	}

	vm->vm_fd = ioctl(vm->dev_fd, KVM_CREATE_VM, 0);
	if (vm->vm_fd < 0)
	{
		perror("KVM_CREATE_VM");
		exit(1);
	}

	if (ioctl(vm->vm_fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0)
	{
		perror("KVM_SET_TSS_ADDR");
		exit(1);
	}

	vm->mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (vm->mem == MAP_FAILED)
	{
		perror("mmap mem");
		exit(1);
	}

	madvise(vm->mem, mem_size, MADV_MERGEABLE);

	memreg.slot = 0;
	memreg.flags = 0;
	memreg.guest_phys_addr = 0;
	memreg.memory_size = mem_size;
	memreg.userspace_addr = (unsigned long)vm->mem;
	if (ioctl(vm->vm_fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0)
	{
		perror("KVM_SET_USER_MEMORY_REGION");
		exit(1);
	}
}

void vcpu_init(struct vm *vm, struct vcpu *vcpu)
{
	int vcpu_mmap_size;

	vcpu->vcpu_fd = ioctl(vm->vm_fd, KVM_CREATE_VCPU, 0);
	if (vcpu->vcpu_fd < 0)
	{
		perror("KVM_CREATE_VCPU");
		exit(1);
	}

	vcpu_mmap_size = ioctl(vm->dev_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
	if (vcpu_mmap_size <= 0)
	{
		perror("KVM_GET_VCPU_MMAP_SIZE");
		exit(1);
	}

	vcpu->kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE,
						 MAP_SHARED, vcpu->vcpu_fd, 0);
	if (vcpu->kvm_run == MAP_FAILED)
	{
		perror("mmap kvm_run");
		exit(1);
	}
}

/*	Modify this function to complete part 2.4 */
/*	sched_order holds the scheduling order for the VMs
	each VM when "scheduled" should read producer-consumer
	buffer state, act on it and convey the new state back
	to the hypervisor.
*/
int run_vm(struct vm *vm1, struct vm *vm2, struct vcpu *vcpu1, struct vcpu *vcpu2, size_t sz)
{
	struct kvm_regs regs;
	struct vm *vm = NULL;
	struct vcpu *vcpu = NULL;
	uint64_t memval = 0;
	struct queue q = initQueue(20);
	int init = 0;
	int schedule = 0;
	int turn = 1;

	for (; schedule < 100;)
	{

		if (init)
		{

			turn = sched_order[schedule++] - '1';
		}
		else
		{
			turn = !turn;
		}
		if (turn)
		{
			vm = vm2;
			vcpu = vcpu2;
		}
		else
		{
			vm = vm1;
			vcpu = vcpu1;
		}
		sleep(1);
		for (;;)
		{
			if (ioctl(vcpu->vcpu_fd, KVM_RUN, 0) < 0)
			{
				perror("KVM_RUN");
				exit(1);
			}
			switch (vcpu->kvm_run->exit_reason)
			{
			case KVM_EXIT_HLT:
				goto check;

			case KVM_EXIT_IO:
				if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT && vcpu->kvm_run->io.port == 0xE9)
				{
					char *p = (char *)vcpu->kvm_run;
					fwrite(p + vcpu->kvm_run->io.data_offset,
						   vcpu->kvm_run->io.size, 1, stdout);
					fflush(stdout);
					continue;
				}
				else if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT && vcpu->kvm_run->io.port == 0xEA)
				{
					char *p = (char *)vcpu->kvm_run;
					char *p1 = p + vcpu->kvm_run->io.data_offset; // p1 will store location where pointer to string is stored
					unsigned long pp = *(uint32_t *)p1;			  // pp will store location at which string is stored
					vm1queue = (int *)(vm->mem + pp);
					goto nextSchedule;
				}
				else if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT && vcpu->kvm_run->io.port == 0xEB)
				{
					char *p = (char *)vcpu->kvm_run;
					char *p1 = p + vcpu->kvm_run->io.data_offset; // p1 will store location where pointer to string is stored
					unsigned long pp = *(int32_t *)p1;			  // pp will store location at which string is stored
					vm2queue = (int *)(vm->mem + pp);
					init = 1;
					goto nextSchedule;
				}
				else if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT && vcpu->kvm_run->io.port == 0xEC) // update front (consuming data)
				{

					char *p = (char *)vcpu->kvm_run;
					char *p1 = p + vcpu->kvm_run->io.data_offset; // p1 will store location where pointer to string is stored
					unsigned long pp = *(uint32_t *)p1;			  // pp will store location at which string is stored
					int32_t *finalLocation = (int32_t *)(vm->mem + pp);

					int newFront = finalLocation[0]; // pp will store location at which string is stored
					// int newBack = finalLocation[1];
					int consumedElements;
					// printf("old back: %d new back: %d\n", q.back, newBack);
					// printf("old front: %d new front: %d\n", q.front, newFront);
					if (newFront != -1)
					{
						if (newFront >= q.front)
						{
							consumedElements = newFront - q.front;
						}
						else
						{
							consumedElements = q.size - q.front + newFront;
						}
						printf("VMFD: %d Consumed %d Values:", vm->vm_fd, consumedElements);
						for (int l = 0; l < consumedElements; l++)
						{
							int popVal = 0;
							pop(&q, &popVal);
							printf(" %d", popVal);
						}
						printf("\n");
					}
					else
					{
						if (q.back == -1 && q.front == -1)
						{
							consumedElements = 0;
						}
						else if (q.back >= q.front)
						{
							consumedElements = q.back - q.front + 1;
						}
						else
						{
							consumedElements = q.size - q.front + q.back + 1;
						}
						printf("VMFD: %d Consumed %d Values:", vm->vm_fd, consumedElements);
						for (int l = 0; l < consumedElements; l++)
						{
							int popVal = 0;
							pop(&q, &popVal);
							printf(" %d", popVal);
						}
						printf("\n");
					}
					goto nextSchedule;
				}
				else if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT && vcpu->kvm_run->io.port == 0xED) // update back(Producing data)
				{
					printQueue(&q);
					char *p = (char *)vcpu->kvm_run;
					char *p1 = p + vcpu->kvm_run->io.data_offset; // p1 will store location where pointer to string is stored
					unsigned long pp = *(uint32_t *)p1;			  // pp will store location at which string is stored
					int32_t *finalLocation = (int32_t *)(vm->mem + pp);

					// int newFront = finalLocation[0]; // pp will store location at which string is stored
					int newBack = finalLocation[1];
					int producedElements;
					// printf("old back: %d new back: %d\n", q.back, newBack);
					// printf("old front: %d new front: %d\n", q.front, newFront);
					if (newBack >= q.back)
					{
						producedElements = newBack - q.back;
					}
					else
					{
						producedElements = q.size - q.back + newBack;
					}
					printf("VMFD: %d Produced %d Values:", vm->vm_fd, producedElements);
					for (int l = 0; l < producedElements; l++)
					{
						printf(" %d", vm1queue[(q.back + 1) % q.size]);
						push(&q, vm1queue[(q.back + 1) % q.size]);
					}
					printf("\n");
					goto nextSchedule;
				}
				else if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_IN && vcpu->kvm_run->io.port == 0xEE) // get front
				{
					char *p = (char *)vcpu->kvm_run;
					char *p1 = p + vcpu->kvm_run->io.data_offset; // p1 will store location where pointer to string is stored
					*(int32_t *)p1 = q.front;
					continue;
				}
				else if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_IN && vcpu->kvm_run->io.port == 0xEF) // get back
				{
					char *p = (char *)vcpu->kvm_run;
					char *p1 = p + vcpu->kvm_run->io.data_offset; // p1 will store location where pointer to string is stored
					// printf("reading back: %d\n", q.back);
					*(int32_t *)p1 = q.back;
					continue;
				}
				printf("VMFD: %d KVM_EXIT_DEBUG1\n", vm->vm_fd);
				setbuf(stdout, NULL);
				continue;
				/* fall through */
			default:
				fprintf(stderr, "Got exit_reason %d,"
								" expected KVM_EXIT_HLT (%d)\n",
						vcpu->kvm_run->exit_reason, KVM_EXIT_HLT);
				exit(1);
			}
		}

	nextSchedule:
	}

check:
	if (ioctl(vcpu->vcpu_fd, KVM_GET_REGS, &regs) < 0)
	{
		perror("KVM_GET_REGS");
		exit(1);
	}

	if (regs.rax != 42)
	{
		printf("Wrong result: {E,R,}AX is %lld\n", regs.rax);
		return 0;
	}

	memcpy(&memval, &vm->mem[0x400], sz);
	if (memval != 42)
	{
		printf("Wrong result: memory at 0x400 is %lld\n",
			   (unsigned long long)memval);
		return 0;
	}

	return 1;
}

static void setup_protected_mode(struct kvm_sregs *sregs)
{
	struct kvm_segment seg = {
		.base = 0,
		.limit = 0xffffffff,
		.selector = 1 << 3,
		.present = 1,
		.type = 11, /* Code: execute, read, accessed */
		.dpl = 0,
		.db = 1,
		.s = 1, /* Code/data */
		.l = 0,
		.g = 1, /* 4KB granularity */
	};

	sregs->cr0 |= CR0_PE; /* enter protected mode */

	sregs->cs = seg;

	seg.type = 3; /* Data: read/write, accessed */
	seg.selector = 2 << 3;
	sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

extern const unsigned char guest4a[], guest4a_end[];
extern const unsigned char guest4b[], guest4b_end[];

int run_protected_mode1(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	if (ioctl(vcpu->vcpu_fd, KVM_GET_SREGS, &sregs) < 0)
	{
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_protected_mode(&sregs);

	if (ioctl(vcpu->vcpu_fd, KVM_SET_SREGS, &sregs) < 0)
	{
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;
	regs.rsp = 2 << 20;

	if (ioctl(vcpu->vcpu_fd, KVM_SET_REGS, &regs) < 0)
	{
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest4a, guest4a_end - guest4a);
	printf("VMFD: %d Loaded Program with size: %ld\n", vm->vm_fd, guest4a_end - guest4a);
	return 0;
}

int run_protected_mode2(struct vm *vm, struct vcpu *vcpu)
{
	struct kvm_sregs sregs;
	struct kvm_regs regs;

	if (ioctl(vcpu->vcpu_fd, KVM_GET_SREGS, &sregs) < 0)
	{
		perror("KVM_GET_SREGS");
		exit(1);
	}

	setup_protected_mode(&sregs);

	if (ioctl(vcpu->vcpu_fd, KVM_SET_SREGS, &sregs) < 0)
	{
		perror("KVM_SET_SREGS");
		exit(1);
	}

	memset(&regs, 0, sizeof(regs));
	/* Clear all FLAGS bits, except bit 1 which is always set. */
	regs.rflags = 2;
	regs.rip = 0;
	regs.rsp = 2 << 20;

	if (ioctl(vcpu->vcpu_fd, KVM_SET_REGS, &regs) < 0)
	{
		perror("KVM_SET_REGS");
		exit(1);
	}

	memcpy(vm->mem, guest4b, guest4b_end - guest4b);
	printf("VMFD: %d Loaded Program with size: %ld\n", vm->vm_fd, guest4b_end - guest4b);
	return 0;
}

void read_sched_file(char *filename)
{
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
	{
		fprintf(stderr, "Error in opening file");
		exit(EXIT_FAILURE);
	}
	int rc = read(fd, sched_order, 100);
	if (rc < 0)
	{
		fprintf(stderr, "Error in opening file");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv)
{
	struct vm vm1, vm2;
	struct vcpu vcpu1, vcpu2;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s filename\n",
				argv[0]);
		return 1;
	}
	read_sched_file(argv[1]);

	vm_init(&vm1, 0x200000);
	vm_init(&vm2, 0x200000);
	vcpu_init(&vm1, &vcpu1);
	vcpu_init(&vm2, &vcpu2);
	run_protected_mode1(&vm1, &vcpu1);
	run_protected_mode2(&vm2, &vcpu2);
	return run_vm(&vm1, &vm2, &vcpu1, &vcpu2, 4);
}
