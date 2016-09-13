#include <src/libvirt_domains.c>
#include <stdio.h>
#include <unistd.h>

char * tagToMeaning(int tag) {
	char * meaning;
	switch(tag)
	{
	case 0:
		meaning = "SWAP IN";
		break;
	case 1:
		meaning = "SWAP OUT";
		break;
	case 2:
		meaning = "MAJOR FAULT";
		break;
	case 3:
		meaning = "MINOR FAULT";
		break;
	case 4:
		meaning = "UNUSED";
		break;
	case 5:
		meaning = "AVAILABLE";
		break;
	case 6:
		meaning = "CURRENT BALLOON";
		break;
	case 7:
		meaning = "RSS (Resident Set Size)";
		break;
	case 8:
		meaning = "USABLE";
		break;
	case 9:
		meaning = "LAST UPDATE";
		break;
	case 10:
		meaning = "NR";
		break;
	}

	return meaning;
}

void printDomainStats(struct DomainsList list)
{
	printf("------------------------------------------------\n");
	printf("%d memory stat types supported by this hypervisor\n",
	       VIR_DOMAIN_MEMORY_STAT_NR);
	printf("------------------------------------------------\n");
	for (int i = 0; i < list.count; i++) {
		virDomainMemoryStatStruct memstats[VIR_DOMAIN_MEMORY_STAT_NR];
                unsigned int nr_stats;
		unsigned int flags = VIR_DOMAIN_AFFECT_CURRENT |
			VIR_DOMAIN_AFFECT_CONFIG |
			VIR_DOMAIN_AFFECT_LIVE;
                virDomainSetMemoryStatsPeriod(list.domains[i], 1, flags);
		nr_stats = virDomainMemoryStats(list.domains[i],
						memstats,
						VIR_DOMAIN_MEMORY_STAT_NR,
						0);
		for(int j = 0; j < nr_stats; j++) {
			printf("%s : %s = %llu MB\n",
			       virDomainGetName(list.domains[i]),
			       tagToMeaning(memstats[j].tag),
			       memstats[j].val/1024);
		}

	}
}

void printHostMemoryStats(virConnectPtr conn)
{
	int nparams = 0;
	virNodeMemoryStatsPtr stats = malloc(sizeof(virNodeMemoryStats));
	if (virNodeGetMemoryStats(conn,
				  VIR_NODE_MEMORY_STATS_ALL_CELLS,
				  NULL,
				  &nparams,
				  0) == 0 && nparams != 0) {
		stats = malloc(sizeof(virNodeMemoryStats) * nparams);
		memset(stats, 0, sizeof(virNodeMemoryStats) * nparams);
		virNodeGetMemoryStats(conn,
				      VIR_NODE_MEMORY_STATS_ALL_CELLS,
				      stats,
				      &nparams,
				      0);
	}
	printf("Hypervisor memory: \n");
	for (int i = 0; i < nparams; i++) {
		printf("%8s : %lld MB\n",
		       stats[i].field,
		       stats[i].value/1024);
	}
}

long wastedMemory(virDomainPtr domain) {

}

virDomainPtr findWastefulDomain(struct DomainsList list) {
	for (int i = 0; i < list.count; i++) {
		virDomainMemoryStatStruct memstat;
		memset (&memstat, '\0', sizeof (virDomainMemoryStatStruct));
		virDomainMemoryStats(list.domains[i],
				     &memstat,
				     VIR_DOMAIN_MEMORY_STAT_NR,
				     0);
		printf("%s : %s = %lld KB\n",
		       virDomainGetName(list.domains[i]),
		       tagToMeaning(memstat.tag),
                       memstat.val);
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
	// Loop until program halts or no active domains available
	// Naive implementation, as setting memory in this scenario may
	// not be optimal due to race conditions.
	while((list = active_domains(conn)).count > 0)
	{
		// look for the highest (available - used) memory out of all domains
		//   - this looks for memory that's WASTED in the balloon
		//
		// look for the lowest (available - used) memory out of all domains
		//   - this looks for domains that DESPERATELY NEED memory
		//
		// deflate the balloon for the highest result and allocate it
		// for the lowest result
		//   - use a threshold, e.g:
		//     only allocate to lowest VM if 100MB away from exhaustion
		//     otherwise free it so host can have it
		printHostMemoryStats(conn);
		//findWastefulDomain(list)
		printDomainStats(list);
		sleep(atoi(argv[1]));
	}
	virConnectClose(conn);
	return 0;
error:
	return 1;
}
