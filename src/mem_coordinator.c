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
		meaning = "ACTUAL BALLOON";
		break;
	case 7:
		meaning = "RSS";
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
	for (int i = 0; i < list.count; i++) {
		virDomainMemoryStatPtr minfo;
		minfo = malloc (VIR_DOMAIN_MEMORY_STAT_NR *
				sizeof (virDomainMemoryStatStruct));
		virDomainMemoryStats(list.domains[i],
				     minfo,
				     VIR_DOMAIN_MEMORY_STAT_NR,
				     0);
		printf("%s - tag -> %s | val -> %lld\n",
		       virDomainGetName(list.domains[i]),
		       tagToMeaning(minfo->tag),
		       minfo->val);
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
