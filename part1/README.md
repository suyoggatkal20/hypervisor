# CS695 Assignment 2 - Part 1

simple-kvm is a very simple example program to demonstrate the use of the KVM API provided by the Linux kernel. Tested in Intel processors with the VMX hardware virtualization extensions and AMD processors with AMD-V hardware virtualization extensions.

## How to build and run

## How to run

Performing ```make``` or ```make run``` will compile the program and run the program all modes. Otherwise he program can be run in 4 modes specifically using the following commands:

````bash
make simple-kvm
./simple-kvm
./simple-kvm -s
./simple-kvm -p
./simple-kvm -l
````

## Expected output on my machine
```
./simple-kvm
Testing real mode
./simple-kvm -s
Testing protected mode
Hello 695!    // String printed by guest byte by byte
2048          // 32 bit value print
4294967295    // 32 bit value print
CS695 Assignment 2  
              // String printed by guest at a time
14            // no of hyoercalls
16            // no of hyoercalls
4055892992    // hva curresponding to gva
4055891967    // hva curresponding to gva
IO in:2       // no of input hypercalls
IO out:17     // no of output hypercalls
IO in:2       // no of input hypercalls
IO out:22     // no of output hypercalls
./simple-kvm -p    // similar output for 32 bit paging mode
Testing 32-bit paging
Hello 695!
2048
4294967295
CS695 Assignment 2
14
16
3825206272
Invalid GVA
0
IO in:2
IO out:17
IO in:2
IO out:22
./simple-kvm -l // similar output for 64 bit mode
Testing 64-bit mode
Hello 695!
2048
4294967295
CS695 Assignment 2
14
16
681575424
Invalid GVA
0
IO in:2
IO out:17
IO in:2
IO out:22

```


## hypercall count
Following is the discription of hypercall count
```
for (p = "Hello 695!\n"; *p; ++p)  // 11 out exit
		HC_print8bit(*p);
	HC_print32bit(2048);           // 1 out exit
	HC_print32bit(4294967295);     // 1 out exit

	uint32_t num_exits_a, num_exits_b;
	num_exits_a = HC_numExits();   // 1 in exit

	char *str = "CS695 Assignment 2\n";
	HC_printStr(str);             // 1 out exit

	num_exits_b = HC_numExits();  // 1 in exit

	HC_print32bit(num_exits_a);   // 1 out exit
	HC_print32bit(num_exits_b);   // 1 out exit

	char *firststr = HC_numExitsByType();  // 1 out exit 
                                            //Total till Now
                                            // IN=2 out=17
                                            //(including current exit)
	uint32_t hva;
	hva = HC_gvaToHva(1024);        // 1 out exit
	HC_print32bit(hva);             // 1 out exit
	hva = HC_gvaToHva(4294967295);  // 1 out exit
	HC_print32bit(hva);             // 1 out exit
	char *secondstr = HC_numExitsByType();  
                                    // 1 out exit 
                                    // Total tillNow
                                    // IN=2 out=22
                                    // (including current exit)

	HC_printStr(firststr);
	HC_printStr(secondstr);

```

## How to clean
```
make clean
```
Performing a clean will remove the executable and the object files. You should perform this while updating the code and recompiling.

### A couple of aspects are worth noting

Note that the Intel VMX extensions did not initially implement support for real mode.  In fact, they restricted VMX guests to paged protected
mode.  VMM / Hypervisor were expected to emulate the unsupported modes in software, only employing VMX when a guest had entered paged protected mode.  Later VMX implementations include *Unrestricted Guest Mode*: support for virtualization of all x86 modes in hardware.

The code run in the VM code exits with a HLT instruction.  There are many ways to cause a VM exit, so why use a HLT instruction?  The most obvious way might be the VMCALL (or VMMCALL on AMD) instruction, which it specifically intended to call out to the hypervisor.  But it turns out the KVM reserves VMCALL/VMMCALL for its internal hypercall mechanism, without notifying the userspace VM host program of the VM exits caused by these instructions.  So we need some other way to trigger a VM exit.  HLT is convenient because it is a single-byte instruction.


