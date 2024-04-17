# System Environment
Operating System: Ubuntu 22.04.2 LTS
Processor: Intel Core i7-1255U
Installing XV6:

# Install Ubuntu and WSL using the command: wsl --install.
Install necessary packages: sudo apt-get update and sudo apt-get install qemu-system-x86.
Clone the XV6 repository: sudo apt-get install git and git clone https://github.com/mit-pdos/xv6-public.git.
Compile XV6 using the make command: cd xv6-public and make.
Run XV6 using QEMU: make qemu-nox.

# Implementation Approach:
Understanding XV6: Familiarize yourself with the XV6 operating system architecture and system calls.
Setting Up Makefile: Configure the Makefile to ensure the build process runs on a single CPU core.
Adding New System Calls: Modify relevant files to incorporate new system calls for setting priority, getting statistics, and calculating average times.
Implementation of Priority Scheduling Algorithm: Update scheduler() method in proc.c to prioritize processes based on their priority values.
Modifications for Priority Scheduler: Implement functions in sysproc.c and proc.c to set and retrieve process priorities, track running time, and calculate average times.
Running the Code: Execute commands in the XV6 terminal to test priority scheduling with various scenarios.

# Steps to Run the Code:
Open the XV6 terminal and navigate to the XV6 directory.
Clean previous build artifacts using make clean.
Build XV6 using make.
Run XV6 in QEMU using make qemu-nox.

# Testing Priority Scheduling:
Execute the test command followed by the number of processes and their priorities to observe the scheduling behavior.
Various scenarios are provided to demonstrate different priority configurations and their impact on process execution.
