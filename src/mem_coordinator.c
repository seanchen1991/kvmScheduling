#include <src/libvirt_domains.c>
#include <stdio.h>

 int main (int argc, char **argv)
{
	check(argc == 2, "ERROR: You need one argument, the time interval in seconds"
	      " the scheduler will trigger.");
	struct DomainsList list;
	printf("- vCPU scheduler - interval: %s\n", argv[1]);
	printf("Connecting to Libvirt... \n");
	virConnectPtr conn = local_connect();
	printf("Connected! \n");
	list = active_domains(conn);
	check(list.count > 0, "No active domains available");
	virConnectClose(conn);
	return 0;
error:
	return 1;
}
