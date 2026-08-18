# CS695 Assignment 2 - Part 2

emu.c is a example program to demonstrate running of two VM using KVM API. Tested in Intel processors with the VMX hardware virtualization extensions and AMD processors with AMD-V hardware virtualization extensions.

## How to build and run

### To run Task 2.0
````bash
make
./emu
````

### To run Task 2.1
````bash
make
./emu1
````

### To run Task 2.2
````bash
make
./emu2
````

### To run Task 2.3
````bash
make
./emu3
````
### To run Task 2.4
````bash
make
./emu4
````
Note: When you run make it will compile all the files in the folder so it is not necessory to compile saperatly for each task.

## How to clean
```
make clean
```
Performing a clean will remove the executable and the object files. You should perform this while updating the code and recompiling.
