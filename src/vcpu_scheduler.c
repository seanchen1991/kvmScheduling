#include <src/libvirt_domains.c>
#include <stdio.h>

void vcpus_count(struct DomainsList list)
{
	for (int i = 0; i < list.count; i++) {
	        int vcpus;
		vcpus = virDomainGetVcpusFlags(list.domains[i],
					       VIR_DOMAIN_VCPU_MAXIMUM);
		printf("%s - vCPUs -> %d\n",
		       virDomainGetName(list.domains[i]),
		       vcpus);
	}
}


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
	vcpus_count(list);
	virConnectClose(conn);
	return 0;
error:
	return 1;
}
