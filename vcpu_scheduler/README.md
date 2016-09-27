# vCPU scheduler

In general, vCPUs in libvirt are mapped to all physical CPUs in the hypervisor
by default. Our scheduler decides to pin vCPUs to pCPUs so that pCPU usage
is balanced, while making as few 'pin changes' as possible as these are costly.

## Demo

![](vcpu.gif)

## Compile

Run `make` in the root of this project, it will compile `vcpu_scheduler.c`
into `bin/vcpu_scheduler`

## Usage

`./bin/vcpu_scheduler [PERIOD IN SECONDS]`

* The scheduler runs on a period determined by the argument passed (in seconds)
* `./vcpu_scheduler 3` would run the scheduler every 3 seconds
* You need to have active libvirt domains in order to run the scheduler.
* The scheduler will stop and close on the first cycle without active domains.

The scheduling algorithm works around only one fairness principle: ensure the pCPUs
usage is as balanced as possible and below 50% usage (USAGE_THRESHOLD).

To do so, it changes the vCPU affinity and pinning of pCPU to vCPU according to
the following algorithm:

* On every scheduler period it calculates:
  - vCPU usage (%) for all domains
  - Usage (%) for all pCPUs [1]
  - Busiest pCPU (highest usage)
  - Freest pCPU (lowest usage)
* Once we have this information, the fairness algorithm is applied using `pinPcpus`.
  - In order to minimize pin changes, they will only happen when the busiest pCPU
  usage is above the USAGE_THRESHOLD (50% by default)
  - If the busiest pCPU is **above** the USAGE_THRESHOLD
    - Find the vCPUs pinned to the freest pCPU and swap the pins of the
      busiest pCPU with the freest pCPU.
  - If the busiest pCPU is **below** the USAGE_THRESHOLD, don't do anything
  - If all pCPUs are **above** or **below** the USAGE_THRESHOLD, don't do anything
    - There is no room for action in this case

[1] Since libvirt won't give you the %, we have to calculate it by taking samples
of the cputime on every period. Say we have a scheduling period of 10ns:
* When the program starts, take sample of pCPUtime, let's say it's 500ns
* After 10ns, we take another sample - it's 505ns.
* We can use the samples and the period to infer the usage, (505-500)/10 = 0.5 -> 50% usage
* Take into account the number of cpus, usage can go up to 400% with 4 cpus.

### Alternatives

The algorithm could take into account the standard deviation of each pCPU with
regards to the average pCPU usage and decide to change pinning based on that metric.
This is not implemented but was thought as another possibility.

## Stress Testing

1. Create virtual machines with `virt-manager`.
2. Once they have loaded, install [`stress`](https://linux.die.net/man/1/stress)
3. Run `stress -c N` where N is the number of workers you want to have spinning on sqrt,
  each of them wasting as much CPU as possible.
4. I recommend experimenting with lower numbers of N (1 or 2) in a multi-core
  (4) vCPU vm to see how the scheduler pins and unpins.
5. `htop` is one of the tools that I used to ensure the vcpu_scheduler usage numbers were
  correct. Open it in a guest and check that vcpu_scheduler results are the same.
