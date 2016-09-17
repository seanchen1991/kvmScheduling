#include <stdlib.h>
#include <src/dbg.h>
#include <src/libvirt_domains.h>

virConnectPtr local_connect(void)
{
	virConnectPtr conn;
	conn = virConnectOpen("qemu:///system");
	check(conn != NULL, "ERROR: When calling virConnectOpen");
	return conn;
error:
	exit(1);
}

struct DomainsList domains_list(virConnectPtr conn, unsigned int flags)
{
	virDomainPtr *domains;
	int num_domains;
	num_domains = virConnectListAllDomains(conn, &domains, flags);
	check(num_domains > 0, "Failed to list all domains\n");
	struct DomainsList *list = malloc(sizeof(struct DomainsList));
	list->count = num_domains;
	list->domains = domains;
	return *list;
error:
	exit(1);
}

struct DomainsList active_domains(virConnectPtr conn)
{
	unsigned int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE |
		VIR_CONNECT_LIST_DOMAINS_RUNNING;
	return domains_list(conn, flags);
}
