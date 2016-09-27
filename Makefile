CC=gcc
CFLAGS=-Wall -I. -pthread -lvirt

all: mem_coordinator vcpu_scheduler

mem_coordinator: memory_coordinator/mem_coordinator.c
	$(CC) -o bin/memory_coordinator memory_coordinator/mem_coordinator.c $(CFLAGS)

vcpu_scheduler: vcpu_scheduler/vcpu_scheduler.c
	$(CC) -o bin/vcpu_scheduler vcpu_scheduler/vcpu_scheduler.c $(CFLAGS)

clean:
	rm bin/vcpu_scheduler bin/memory_coordinator
