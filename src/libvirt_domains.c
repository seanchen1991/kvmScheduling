#include <stdlib.h>
#include <src/dbg.h>
#include <src/libvirt_domains.h>

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
	printf("Domain IDs:\n");
	for (int i = 0; i < num_domains; i++) {
		printf("  %s\n", virDomainGetName(domains[i]));
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
