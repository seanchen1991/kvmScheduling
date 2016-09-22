#include <src/libvirt_domains.c>
#include <stdio.h>

int vCPUsCount(struct DomainsList list)
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

int vCPUCount(virDomainPtr domain)
{
	return virDomainGetVcpusFlags(domain, VIR_DOMAIN_VCPU_MAXIMUM);
}

int pCPUUsage(virConnectPtr conn)
{
	int nr_params = 0;
	int nr_cpus = VIR_NODE_CPU_STATS_ALL_CPUS;
	virNodeCPUStatsPtr params;
	double usage, busy_time, total_time = 0;

	check(virNodeGetCPUStats(conn, nr_cpus, NULL, &nr_params, 0) == 0 &&
	      nr_params != 0, "Could not get pCPU stats 1");
	params = malloc(sizeof(virNodeCPUStats) * nr_params);
	check(params != NULL, "Could not allocate pCPU params");
	memset(params, 0, sizeof(virNodeCPUStats) * nr_params);
	check(virNodeGetCPUStats(conn, nr_cpus, params, &nr_params, 0) == 0,
	      "Could not get pCPU stats 2");

	for (int i = 0; i < nr_params; i++) {
		if (strcmp(params[i].field, VIR_NODE_CPU_STATS_USER) == 0 ||
		    strcmp(params[i].field, VIR_NODE_CPU_STATS_KERNEL) == 0) {
		        busy_time += params[i].value;
		}
		total_time += params[i].value;
	}
        usage = busy_time/total_time * 100;
	printf("CPU usage: %f%\n", usage);
error:
	exit(1);
}

unsigned long long toSeconds(unsigned long long nanoseconds)
{
	return nanoseconds / 1000000000;
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
	pCPUUsage(conn);
//	list = active_domains(conn);
//	check(list.count > 0, "No active domains available");
//	vcpus_count(list);
	virConnectClose(conn);
	return 0;
error:
	return 1;
}
