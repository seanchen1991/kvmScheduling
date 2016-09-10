#include <src/libvirt_domains.c>
#include <stdio.h>

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
		memset(memstats,
		       0,
		       sizeof(virDomainMemoryStatStruct) *
		       VIR_DOMAIN_MEMORY_STAT_NR);
		virDomainMemoryStats(list.domains[i],
				     (virDomainMemoryStatPtr)&memstats,
				     VIR_DOMAIN_MEMORY_STAT_NR,
				     0);
		for(int j = 0; j < VIR_DOMAIN_MEMORY_STAT_NR; j++) {
			printf("%s : %s = %lld KB\n",
			       virDomainGetName(list.domains[i]),
			       tagToMeaning(memstats[j].tag),
			       memstats[j].val);
		}
	}
}

void printHostStats(virConnectPtr conn)
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
		printf("%8s : %lld KB\n",
		       stats[i].field,
		       stats[i].value);
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
		printHostStats(conn);
		printDomainStats(list);
		sleep(10);
	}
	virConnectClose(conn);
	return 0;
error:
	return 1;
}
