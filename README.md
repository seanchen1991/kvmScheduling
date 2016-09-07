# kvmScheduling

VCPU scheduler &amp; memory coordinator for KVM through Libvirt

This is just a experiment to test the libvirt APIs and apply scheduling
algorithms to vCPUs manually.

A virtual CPU equates to 1 physical core, but when your VM attempts to process something, it can potentially run on any of the cores that happen to be available at that moment. The scheduler handles this, and the VM is not aware of it. You can assign multiple vCPUs to a VM which allows it to run concurrently across several cores.

Cores are shared between all VMs as needed, so you could have a 4-core system, and 10 VMs running on it with 2 vCPUs assigned to each. VMs share all the cores in your system quite efficiently as determined by the scheduler. This is one of the main benefits of virtualization - making the most use of under-subscribed resources to power multiple OS instances.
>>>>>>> Connect to local Libvirt
