#include <shared/libvirt_domains.c>
#include <stdio.h>
#include <unistd.h>

// Define an available memory threshold below which a domain can be considered
// memory starved (in MB)
static const int STARVATION_THRESHOLD = 150 * 1024;

// Define an available memory threshold above which a domain can be
// considered to be wasting memory (in MB)
static const int WASTE_THRESHOLD = 300 * 1024;

struct DomainMemory {
	virDomainPtr domain;
	long memory;
};

char *tagToMeaning(int tag)
{
	char *meaning;
	switch (tag) {
	case VIR_DOMAIN_MEMORY_STAT_SWAP_IN:
		meaning = "SWAP IN";
		break;
	case VIR_DOMAIN_MEMORY_STAT_SWAP_OUT:
		meaning = "SWAP OUT";
		break;
	case VIR_DOMAIN_MEMORY_STAT_MAJOR_FAULT:
		meaning = "MAJOR FAULT";
		break;
	case VIR_DOMAIN_MEMORY_STAT_MINOR_FAULT:
		meaning = "MINOR FAULT";
		break;
	case VIR_DOMAIN_MEMORY_STAT_UNUSED:
		meaning = "UNUSED";
		break;
	case VIR_DOMAIN_MEMORY_STAT_AVAILABLE:
		meaning = "AVAILABLE";
		break;
	case VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON:
		meaning = "CURRENT BALLOON";
		break;
	case VIR_DOMAIN_MEMORY_STAT_RSS:
		meaning = "RSS (Resident Set Size)";
		break;
	case VIR_DOMAIN_MEMORY_STAT_NR:
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
		for (int j = 0; j < nr_stats; j++) {
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
	printf("Hypervisor memory:\n");
	for (int i = 0; i < nparams; i++) {
		printf("%8s : %lld MB\n",
		       stats[i].field,
		       stats[i].value/1024);
	}
}

// Returns an array like:
//  0: Contains the DomainMemory struct that wastes the most memory
//  1: Contains the DomainMemory struct that needs memory the most urgently
struct DomainMemory *findRelevantDomains(struct DomainsList list)
{
	struct DomainMemory *ret;
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
		printf("%s : %llu MB available\n",
		       virDomainGetName(list.domains[i]),
		       (memstats[VIR_DOMAIN_MEMORY_STAT_AVAILABLE].val)/1024);
		if (memstats[VIR_DOMAIN_MEMORY_STAT_AVAILABLE].val > wasteful.memory) {
			wasteful.domain = list.domains[i];
			wasteful.memory = memstats[VIR_DOMAIN_MEMORY_STAT_AVAILABLE].val;
		}
		if (memstats[VIR_DOMAIN_MEMORY_STAT_AVAILABLE].val < starved.memory ||
		    starved.memory == 0) {
			starved.domain = list.domains[i];
			starved.memory = memstats[VIR_DOMAIN_MEMORY_STAT_AVAILABLE].val;
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

int main(int argc, char **argv)
{
	check(argc == 2, "ERROR: You need one argument, the time interval in seconds"
	      " the scheduler will trigger.");
	struct DomainsList list;
	printf("- vCPU scheduler - interval: %s\n", argv[1]);
	printf("Connecting to Libvirt...\n");
	virConnectPtr conn = local_connect();
	printf("Connected!\n");
	// Loop until program halts or no active domains available
	// Naive implementation, as setting memory in this scenario may
	// not be optimal due to race conditions.
	while ((list = active_domains(conn)).count > 0) {
		struct DomainMemory *relevantDomains;
		relevantDomains = findRelevantDomains(list);
		struct DomainMemory wasteful = relevantDomains[0];
		struct DomainMemory starved = relevantDomains[1];
		free(relevantDomains);
		// Uncomment this to see all guests stats (helpful when debugging)
		// printDomainStats(list);
		// Uncomment this to see host stats
		// printHostMemoryStats(conn);
		if (starved.memory/1024 <= STARVATION_THRESHOLD) {
		// At this point, we must assign more memory to the domain
			if (wasteful.memory/1024 >= WASTE_THRESHOLD) {
				// The most wasteful domain will get less memory, precisely
				// 'waste/2', and the most starved domain will get
				// removed the same quantity.
				virDomainSetMemory(wasteful.domain,
						   wasteful.memory - wasteful.memory/2);
				virDomainSetMemory(starved.domain,
						   starved.memory + wasteful.memory/2);
			} else {
				// There is not any waste (< WASTE_THRESHOLD) and a domain is
				// critical (< STARVATION_THRESHOLD).
				// Assign memory from the hypervisor until the starved host
				// has STARVATION_THRESHOLD available.
				//
				// You need to be generous assigning memory,
				// otherwise it's consumed immediately (in
				// between coordinator periods)
				virDomainSetMemory(starved.domain,
						   starved.memory + WASTE_THRESHOLD);
			}
		} else if (wasteful.memory/1024 >= WASTE_THRESHOLD) {
			// No domain really need more memory at this point, give
			// it back to the hypervisor
			printf("Returning memory back to host\n");
			virDomainSetMemory(wasteful.domain,
					   wasteful.memory - WASTE_THRESHOLD);
			printf("DONE\n");
		}
		sleep(atoi(argv[1]));
		clearScreen();
	}
	printf("No active domains - closing. See you next time!\n");
	virConnectClose(conn);
	free(conn);
	return 0;
error:
	return 1;
}
