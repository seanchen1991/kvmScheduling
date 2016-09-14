# kvmScheduling

VCPU scheduler &amp; memory coordinator for KVM through Libvirt

This is just a experiment to test the libvirt APIs and apply scheduling
algorithms to vCPUs manually.

A virtual CPU equates to 1 physical core, but when your VM attempts to process something, it can potentially run on any of the cores that happen to be available at that moment. The scheduler handles this, and the VM is not aware of it. You can assign multiple vCPUs to a VM which allows it to run concurrently across several cores.

Cores are shared between all VMs as needed, so you could have a 4-core system, and 10 VMs running on it with 2 vCPUs assigned to each. VMs share all the cores in your system quite efficiently as determined by the scheduler. This is one of the main benefits of virtualization - making the most use of under-subscribed resources to power multiple OS instances.
>>>>>>> Connect to local Libvirt

Launch up to 4 VMs, each with 4 vCPU. Or, 4 VMs for memory part.
Run a simple program in user space. (This is a different one than the program that you will submit.)
The program could be either CPU-intensive or memory-intensive. Use 'stress' to generate workload
  e.g.) ./workload_example
Run your program with an argument, the time interval(in seconds) the scheduler will trigger.
  e.g.) ./your_scheduler  12
Check the resource usage. The usage numbers across VMs should be balanced.
We don’t expect the perfectly balanced numbers, but definitely you need
to make sure that work is not overloaded in some VMs only.
For vCPU scheduler, one option to check the usage is doing “virt-top” from the hypervisor.


## Memory coordinator algorithm

On an 'arg1' period:

1. Calculate the free memory for every domain by substracting 'available' - 'used'.
  - Store the domain with the most free memory (it's wasting that memory in the balloon)
  - Store the domain with the least free memory (it might need more memory)
1. Free enough memory from the domain that wastes memory until 'free mem' =< 100MB
1. Assign enough memory to the domain that needs more memory until 'free mem' >= 100MB
1. Rinse and repeat
