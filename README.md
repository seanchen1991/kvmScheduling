# kvmScheduling

VCPU scheduler &amp; memory coordinator for KVM through Libvirt

This is just a experiment to play with the libvirt APIs.

It consists of two projects:

  * A vCPU scheduler which tries to assign the best pCPU to each
  vCPU, based on fairness
  * A memory scheduler based on fairness which keeps the unused memory
  on all active domain within certain boundaries (e.g between 100/300MB)

## Memory scheduler

* Compile by running `make`. It will link using -lvirt.
* The scheduler runs on a period determined by the argument passed (in seconds)
* `./mem_coordinator 3` would run the scheduler every 3 seconds
* You need to have active libvirt domains in order to run the scheduler,
  otherwise the scheduler will simply close on the first cycle without
  active domains.
* Run

The algorithm used to calculate fairness is the following:

* Define a thresholds for starvation and waste of available memory. By default,
  a domain is starved when it has less than 150MB of memory available,
  and it's wasting memory when it has more than 300MB of memory available.

* On every scheduler period, find the most starved and the most wasteful domains.

* If the most starved domain is below the starvation threshold, we have to assign memory to it:
  * If there is a memory wasting memory above the waste threshold, halve
  that domain's memory and assign the same amount to the starved domain.
  * If there is no domain wasting memory (most common case after a few cycles),
  assign more memory to it (it assigns the current memory + the waste threshold).
  The algorithm needs to be generous in this step, as memory intensive processes
  will immediately consume the memory that's assigned in this step.
* Alternatively, if there is no starved but there is a wasteful domain,
  return that memory back to the host by getting
  'wasteful.memory - WASTE_THRESHOLD'
* The last case, if there is no starved domain nor wasteful domains don't do anything.

## vCPU scheduler

In general, vCPUs in libvirt are mapped to all physical CPUs in the hypervisor
by default. Our scheduler decides to pin vCPUs to pCPUs so that pCPU usage
is balanced, while making as few 'pin changes' as possible as these are costly.

* On every scheduler period:
* Find:
  - vCPU usage (%) for all domains
  - pCPU usage (%)
* Check the current mapping to see which pCPUs are mapped to which vCPUs.
* Find 'the best pCPU' to pin each vCPU:
  - vCPU usage has to be balanced across all 4 pCPUs - sum and divide this usage?
  - in order to minimize pin changes use this metric:
     - summation of all vCPU usage
     - summation of all pCPU usage
     - division of vcputotal/pcpus.count = average vCPU load per pCPU
     - keep the same metric within scheduler periods.
     - if the change isn't dramatic enough, do not reassign vCPUs to pCPUs
     - define dramatic via threshold (can be tweaked)
