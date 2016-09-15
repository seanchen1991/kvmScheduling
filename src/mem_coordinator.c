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

// Used to debug what memory stats do we have available in the domain
void printDomainStats(struct DomainsList list)
{
	printf("------------------------------------------------\n");
	printf("%d memory stat types supported by this hypervisor\n",
	       VIR_DOMAIN_MEMORY_STAT_NR);
	printf("------------------------------------------------\n");
	for (int i = 0; i < list.count; i++) {
		virDomainMemoryStatStruct memstats[VIR_DOMAIN_MEMORY_STAT_NR];
		unsigned int nr_stats;
		unsigned int flags = VIR_DOMAIN_AFFECT_CURRENT;
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

virDomainPtr * findWastefulDomain(struct DomainsList list) {
	int mostWastefulDomain;
	long mostWastedMemory = 0;
	int mostCriticalDomain;
	long mostCriticalMemory = 0;
	virDomainPtr * wastefulAndCritical;
	wastefulAndCritical = malloc(sizeof(virDomainPtr) * 2);
	for (int i = 0; i < list.count; i++) {
		virDomainMemoryStatStruct memstats[VIR_DOMAIN_MEMORY_STAT_NR];
		unsigned int nr_stats;
		unsigned int flags = VIR_DOMAIN_AFFECT_CURRENT;
		unsigned int period_enabled;
		period_enabled = virDomainSetMemoryStatsPeriod(list.domains[i],
							       1,
							       flags);
		check(period_enabled >= 0,
		      "ERROR: Could not change balloon collecting period");
		nr_stats = virDomainMemoryStats(list.domains[i], memstats,
						VIR_DOMAIN_MEMORY_STAT_NR, 0);
		check(nr_stats != -1, "ERROR: Could not collect memory stats for domain %s",
		      virDomainGetName(list.domains[i]));
		printf("%s : Available = %llu MB\n",
		       virDomainGetName(list.domains[i]),
		       (memstats[5].val)/1024);
                if (memstats[5].val > mostWastedMemory) {
			mostWastefulDomain = i;
			mostWastedMemory = memstats[5].val;
		}
                if (memstats[5].val < mostCriticalMemory ||
		    mostCriticalMemory == 0) {
			mostCriticalDomain = i;
			mostCriticalMemory = memstats[5].val;
		}

	}
	printf("%s is the most wasteful domain - mem available %ld MB \n",
	       virDomainGetName(list.domains[mostWastefulDomain]),
	       mostWastedMemory/1024);
	printf("%s is the domain that needs the most memory - mem available %ld MB \n",
	       virDomainGetName(list.domains[mostCriticalDomain]),
	       mostCriticalMemory/1024);


	wastefulAndCritical[0] = list.domains[mostWastefulDomain];
	wastefulAndCritical[1] = list.domains[mostCriticalDomain];
	return wastefulAndCritical;
error:
	exit(1);
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
		// This should return an array with the most wasteful and
		// the domain that needs the most memory;
		findWastefulDomain(list);
                // if 'critical < 100MB' {
		//   At this point, we must assign more memory to the domain
		//
		//   if (waste > 50MB) {
		//     The most wasteful domain will get less memory, precisely
		//     'waste/2', and the most starved domain will get the same
		//      quantity.
		//   } else {
		//     There is not a lot of waste (< 50MB) and a domain is
		//     critical (< 100MB). Assign memory from the hypervisor in
		//     100MB chunks.
		//     (if the hypervisor does not have that much free memory,
		//     show an error saying it's causing the hypervisor to swap)
		//   }
		// } else if (critical > 100MB && waste > 50MB)
		//   free(waste);  so host can get back that memory,
		//   no one 'really needs it'
		// }
		//     otherwise free it so host can have it
		//
		// Uncomment this to see all guests stats (helpful when debugging)
	        //printDomainStats(list);
	        printHostMemoryStats(conn);
		sleep(atoi(argv[1]));
	}
	printf("No active domains - closing. See you next time! \n");
	virConnectClose(conn);
        free(conn);
	return 0;
error:
	return 1;
}
