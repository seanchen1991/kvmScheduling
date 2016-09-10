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

void printStats(struct DomainsList list)
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
			printf("%s - tag -> %s | val -> %lld KB\n",
			       virDomainGetName(list.domains[i]),
			       tagToMeaning(memstats[j].tag),
			       memstats[j].val);
		}
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
	list = active_domains(conn);
	check(list.count > 0, "No active domains available");
	printStats(list);
	virConnectClose(conn);
	return 0;
error:
	return 1;
}
