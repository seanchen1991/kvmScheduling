CC=gcc
CFLAGS=-Wall -I. -pthread -lvirt

all: mem_coordinator vcpu_scheduler

mem_coordinator: src/mem_coordinator.c
	$(CC) -o mem_coordinator src/mem_coordinator.c $(CFLAGS)

vcpu_scheduler: src/vcpu_scheduler.c
	$(CC) -o vcpu_scheduler src/vcpu_scheduler.c $(CFLAGS)

clean:
	rm vcpu_scheduler mem_coordinator
