# kvmScheduling

VCPU scheduler &amp; memory coordinator for KVM through Libvirt

This is just a experiment to play with the libvirt APIs and
have a benchmarking tool for scheduling problems.

It consists of two projects:

  * A vCPU scheduler which tries to assign the best pCPU to each
  vCPU, based on fairness. It triggers only after a fixed usage
  percentage on one of the CPUs (defined in the source).

  * A memory scheduler based on fairness which keeps the unused memory
  on all active domain within certain boundaries (e.g between 100/300MB)

## Compile

Run `make` from the project's root to generate both programs.
You can also run `make` in each of the subfolders to only generate
one or the other program.

Find the binaries on `bin/memory_coordinator` and `bin/vcpu_scheduler`.

To compile this, make sure you have the packages `libvirt-dev` and `qemu-kvm`.

## Usage

### [Memory Coordinator](memory_coordinator/README.md)
### [vCPU Scheduler](vcpu_scheduler/README.md)
