# Memory coordinator

This program aims to balance the unused memory in every domain, to
ensure fairness among VMs, and to waste as little hypervisor memory as
possible.

## Demo

![](mem.gif)

## Compile

Run `make` in the root of this project, it will compile `mem_coordinator.c`
into `bin/mem_coordinator`

## Usage

./bin/mem_coordinator [PERIOD IN SECONDS]

* The coordinator runs on a period determined by the argument passed (in seconds)
* `./mem_coordinator 3` would run the coordinator every 3 seconds
* You need to have active libvirt domains in order to run the coordinator.
* The coordinator will stop and close on the first cycle without active domains.

The coordination algorithm works around two basic fairness principles:
  * Give back memory from the wasteful domains to the hypervisor
  * Assign memory from the hypervisor to the starved domains until they are not starved

By default, a domain is starved when it has less than 150MB of memory available,
and is considered to be wasting memory when it has more than 300MB of memory available.

The algorithm used to calculate fairness is the following:

* On every coordination period, find the most starved and the most wasteful domains.
* If the most starved domain is **below the starvation threshold**, we have to assign memory to it:
  * If there is a memory wasting memory **above the waste threshold**, halve that domain's memory
    and assign the same amount to the starved domain.
  * If there is no domain wasting memory (most common case after a few cycles),
    assign more memory to it (it assigns the current memory + the waste threshold).
    The algorithm needs to be generous in this step, as memory intensive processes
    will immediately consume the memory that's assigned in this step.
  * If there is no starved domain but there is a wasteful domain, return memory from the
    wasteful domain back to the host by getting 'wasteful.memory - WASTE_THRESHOLD'
  * The last case, if there is no starved domain nor wasteful domains don't do anything.
* This is repeated until there are no active domains

## Stress testing

1. Create virtual machines with `virt-manager`.
2. Once they have loaded, install [`stress`](https://linux.die.net/man/1/stress)
3. Run `stress -m N` where N is the number of workers you want to have spinning on malloc,
  each of them wasting 256MB.
4. You will notice that once your VMs become memory starved, the coordinator
  will start serving your VMs memory from either the host or 'memory wasteful' VMs
5. You can play with the threshold by modifying the STARVATION_THRESHOLD and
  WASTE_THRESHOLD constants in the source and recompiling the program.
