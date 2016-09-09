#include <dbg.h>
#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

struct DomainsList {
	virDomainPtr *domains;
	int count;
};

virConnectPtr local_connect(void)
{
	virConnectPtr conn;
	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		perror("virConnectOpen");
		exit(-1);
	}
	return conn;
}

struct DomainsList domains_list(virConnectPtr conn, unsigned int flags)
{
	virDomainPtr *domains;
	int num_domains;
	num_domains = virConnectListAllDomains(conn, &domains, flags);
	if (num_domains < 0) {
		fprintf(stderr, "Failed to list all domains\n");
		exit(1);
	}
	printf("  There are %d domains\n", num_domains);
	printf("  Domain IDs:\n");
	for (int i = 0; i < num_domains; i++) {
	       printf("    - %s\n", virDomainGetName(domains[i]));
	}
	struct DomainsList *list = malloc(sizeof(struct DomainsList));
        list->count = num_domains;
        list->domains = domains;
	return *list;
}

struct DomainsList active_domains(virConnectPtr conn)
{
	unsigned int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE |
		VIR_CONNECT_LIST_DOMAINS_RUNNING;
	printf("****ACTIVE****\n");
	return domains_list(conn, flags);
}

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
