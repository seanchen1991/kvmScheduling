#include <src/libvirt_domains.c>
#include <stdio.h>
#include <unistd.h>

struct DomainMemory {
	virDomainPtr domain;
	long memory;
};

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

// Returns an array like:
//  0: Contains the DomainMemory struct that wastes the most memory
//  1: Contains the DomainMemory struct that needs memory the most urgently
struct DomainMemory * findRelevantDomains(struct DomainsList list) {
	struct DomainMemory * ret;
	struct DomainMemory wasteful;
	struct DomainMemory starved;
	ret = malloc(sizeof(struct DomainMemory) * 2);
	wasteful.memory = 0;
	starved.memory = 0;
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
		check(nr_stats != -1,
		      "ERROR: Could not collect memory stats for domain %s",
		      virDomainGetName(list.domains[i]));
		printf("%s : %llu MB available \n",
		       virDomainGetName(list.domains[i]),
		       (memstats[5].val)/1024);
		if (memstats[5].val > wasteful.memory) {
			wasteful.domain = list.domains[i];
			wasteful.memory = memstats[5].val;
		}
		if (memstats[5].val < starved.memory ||
		    starved.memory == 0) {
			starved.domain = list.domains[i];
			starved.memory = memstats[5].val;
		}

	}
	printf("%s is the most wasteful domain - %ld MB available\n",
	       virDomainGetName(wasteful.domain),
	       wasteful.memory/1024);
	printf("%s is the domain that needs the most memory - %ld MB available\n",
	       virDomainGetName(starved.domain),
	       starved.memory/1024);

	ret[0] = wasteful;
	ret[1] = starved;
	return ret;
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
		struct DomainMemory * relevantDomains;
		relevantDomains = findRelevantDomains(list);
		struct DomainMemory wasteful = relevantDomains[0];
		struct DomainMemory starved = relevantDomains[1];
		free(relevantDomains);
		if (starved.memory/1024 < 100) {
		// At this point, we must assign more memory to the domain
			if (wasteful.memory/1024 > 100) {
				// The most wasteful domain will get less memory, precisely
				// 'waste/2', and the most starved domain will get
				// removed the same quantity.
				virDomainSetMemory(wasteful.domain,
						   wasteful.memory - wasteful.memory/2);
				virDomainSetMemory(starved.domain,
						   starved.memory + wasteful.memory/2);
			} else {
				// There is not any waste (< 100MB) and a domain is
				// critical (< 100MB). Assign memory from the hypervisor in
				// until the starved host has 100MB available.
				// TODO:
				// Check if the hypervisor has that much free memory,
				// Show an error if it's causing the hypervisor to swap
				printf("Host is very starved %ld \n", starved.memory/1024);

				virDomainSetMemory(starved.domain,
						   100*1024 - starved.memory * 2);
			}
		} else if (starved.memory/1024 > 100 && wasteful.memory > 100) {
			// No domain really need more memory at this point, give
			// it back to the hypervisor
			virDomainSetMemory(wasteful.domain, wasteful.memory - 100);
			virDomainSetMemory(starved.domain, starved.memory - 100);
		}
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
