# LiteVM with KVM

This repository contains the implementation of a custom x86 hypervisor and CPU scheduling models built using the Linux Kernel-based Virtual Machine (KVM) API. The project is divided into two parts.

## Project Structure

* README.md - Root project documentation.
* Makefile - Root build script that compiles both sub-projects.
* .gitignore - Git ignore configurations.
* part1a.pdf - Diagram and theoretical explanations for Part 1.
* part1/ - Source code and documentation for the DIY Hypervisor.
  * guest.c - Single-threaded guest code containing implementations of various hypercalls.
  * simple-kvm.c - The userspace hypervisor controlling real mode, protected mode, 32-bit paging, and 64-bit long mode execution.
  * Makefile - Local build configurations for Part 1.
  * guest.ld - Linker script for the guest code.
  * payload.ld - Linker script mapping payloads into payload.o.
* part2/ - Source code and documentation for the Emu VM Scheduler.
  * emu.c - Concurrent VM execution using POSIX threads (Warmup 2.0).
  * emu1.c - Single-threaded CPU sharing alternating between producer and consumer (Task 2.1).
  * emu2.c - Bursty CPU scheduling (Task 2.2).
  * emu3.c - Mass production transferring 5 elements at a time in protected mode (Task 2.3).
  * emu4.c - Non-deterministic queue sharing via a virtual shared buffer (Task 2.4).
  * guest*.s / guest*.c - Guest programs corresponding to the scheduler tasks.
  * Makefile - Local build configurations for Part 2.
  * sched1.txt / sched2.txt - Scheduler input files specifying VM schedule order.

## Part 1: DIY Hypervisor and Custom Hypercalls

The `simple-kvm` hypervisor creates a virtual machine and virtual CPU (vCPU), sets up memory, and runs the guest code. The guest utilizes special port IO instructions to execute hardware-assisted hypercalls:

1. HC_print8bit (Port 0xE9): Writes a single character to standard output.
2. HC_print32bit (Port 0xEA): Prints a 32-bit value.
3. HC_numExits (Port 0xEB): Queries and returns the total exits made from the start of execution.
4. HC_printStr (Port 0xEC): Prints a null-terminated string by resolving the Guest Virtual Address (GVA) to Host Virtual Address (HVA).
5. HC_numExitsByType (Port 0xED): Returns a string showing the count of exits partitioned by input and output directions.
6. HC_gvaToHva (Port 0xEF): Translates a Guest Virtual Address (GVA) to Host Virtual Address (HVA) using KVM translation APIs.

## Part 2: Emu CPU Scheduling

Part 2 simulates concurrent and coordinated CPU scheduling of multiple virtual machines:

* Task 2.0 (Concurrent Threads): Emulates concurrent VM execution of two real-mode guests using pthreads.
* Task 2.1 (Single Thread Sharing): Executes VMs sequentially on a single thread. The hypervisor coordinates the CPU time, alternating between VM1 (producer) and VM2 (consumer twice).
* Task 2.2 (Bursty CPU Sharing): Introduces bursty scheduling where VM1 produces 3 numbers sequentially before VM2 is scheduled to consume them.
* Task 2.3 (Mass Production): Executes VMs in protected mode, transferring 5 values in a single hypercall by copying memory arrays directly between the guest address spaces via the hypervisor.
* Task 2.4 (Non-Deterministic shared buffer): Coordinates a virtual shared ring-buffer of size 20. VMs randomly choose to produce or consume 0-10 elements using rdtsc. The hypervisor reads scheduling priorities from an input trace file, manages queue pointers, and copies values to avoid overflow or underflow.

## Build and Execution

To clean and compile all targets:
```sh
make clean
make all
```

### Running Part 1
To test the DIY hypervisor in different modes:
```sh
cd part1
./simple-kvm       # Real mode
./simple-kvm -s    # Protected mode
./simple-kvm -p    # 32-bit paging mode
./simple-kvm -l    # 64-bit long mode
```

### Running Part 2
To run the schedulers:
```sh
cd part2
./emu              # Concurrent threads
./emu1             # Single-thread CPU sharing
./emu2             # Bursty CPU sharing
./emu3             # Mass production (protected mode)
./emu4 sched1.txt  # Non-deterministic queue sharing
```
