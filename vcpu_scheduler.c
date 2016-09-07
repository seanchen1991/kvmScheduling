#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>

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

virDomainPtr * domains_list(virConnectPtr conn, unsigned int flags)
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
	return domains;
}

virDomainPtr active_domains(virConnectPtr conn)
{
	virDomainPtr *domains;
	unsigned int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE |
		VIR_CONNECT_LIST_DOMAINS_RUNNING;
	printf("****ACTIVE****\n");
	domains = domains_list(conn, flags);
	return *domains;
}

virDomainPtr inactive_domains(virConnectPtr conn)
{
	virDomainPtr *domains;
	unsigned int flags = VIR_CONNECT_LIST_DOMAINS_INACTIVE;
	printf("****INACTIVE****\n");
	domains = domains_list(conn, flags);
	return *domains;
}

int main (int argc, char** argv)
{
	printf("- vCPU scheduler - \n");
	printf("Connecting to Libvirt... \n");
	virConnectPtr conn = local_connect();
	active_domains(conn);
	inactive_domains(conn);
	printf("Connected! \n");
	virConnectClose(conn);
	return 0;
}
