CC=gcc
CFLAGS=-Wall -I. -pthread -lvirt

vcpu_scheduler: vcpu_scheduler.c
	$(CC) -o vcpu_scheduler vcpu_scheduler.c $(CFLAGS)
