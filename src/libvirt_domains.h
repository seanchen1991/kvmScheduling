#ifndef __libvirt_domains__
#define __libvirt_domains__

#include <libvirt/libvirt.h>

/* DomainsList: struct to store just a list of domains and the array length */
struct DomainsList {
	virDomainPtr *domains; /* pointer to array of Libvirt domains */
	int count;             /* number of domains in the *domains array */
};

/* local_connect:
 *   initiates a connection to local qemu libvirt, returns a
 *   pointer to that connection */
virConnectPtr local_connect(void);

/* domains_list:
 *   given a pointer to a libvirt connection and flags, it retrieves
 *   a list of domains that match the flags */
struct DomainsList domains_list(virConnectPtr conn, unsigned int flags);

/* active_domains:
 *   calls domains_list with the flags needed to return all active domains */
struct DomainsList active_domains(virConnectPtr conn);

#endif
