CC=gcc
CFLAGS=-I. -pthread -lvirt

vcpu_scheduler: vcpu_scheduler.c
	$(CC) -o vcpu_scheduler vcpu_scheduler.c $(CFLAGS)
