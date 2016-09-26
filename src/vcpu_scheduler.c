#include <src/libvirt_domains.c>
#include <stdio.h>
#include <unistd.h>

static const long NANOSECOND = 1000000000;

struct DomainStats {
	virDomainPtr dom;
	int vcpus_count;
	// Contains the vCPU time samples in the same order
	unsigned long long *previous_vcpus, *current_vcpus;
};

int vCPUsCount(struct DomainsList list)
{
	int ret = 0;
	for (int i = 0; i < list.count; i++) {
		int vcpus;
		vcpus = virDomainGetVcpusFlags(list.domains[i],
					       VIR_DOMAIN_VCPU_MAXIMUM);
		ret += vcpus;
		printf("%s - vCPUs -> %d\n",
		       virDomainGetName(list.domains[i]),
		       vcpus);
	}
	return ret;
}

unsigned long long pCpuSample(virConnectPtr conn)
{
	int nr_params = 0;
	int nr_cpus = VIR_NODE_CPU_STATS_ALL_CPUS;
	virNodeCPUStatsPtr params;
	unsigned long long busy_time = 0;

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
	}
	//printf("pCPUs busy time: %llu ns\n", busy_time);
	return busy_time;
error:
	exit(1);
}

void printDomainParams(virDomainStatsRecordPtr record)
{
	for (int i = 0; i < record->nparams; i++) {
		printf("%s %s - %llu\n",
		       virDomainGetName(record->dom),
		       record->params[i].field,
		       record->params[i].value.ul);
	}
}

long long unsigned getVCPUs(virDomainStatsRecordPtr record)
{
	double usage;
	unsigned long long int busy_time = 0, total_time = 0;
	for (int i = 0; i < record->nparams; i++) {
		printf("field %s value %llu\n", record->params[i].field, record->params[i].value.ul);
		if (strcmp(record->params[i].field, "cpu.system") == 0 ||
		    strcmp(record->params[i].field, "cpu.user") == 0) {
			busy_time += record->params[i].value.ul;
		} else if (strcmp(record->params[i].field, "cpu.time")) {
			// Only cpu.time falls here
			total_time += record->params[i].value.ul;
		}
	}
	printf("busytime %llu totaltime %llu\n", busy_time, total_time);
	usage = (double) (busy_time/total_time);
	printf("Domain CPU usage: %f%%\n", usage);
	return usage;
}

virDomainStatsRecordPtr *domainvCPUStats(struct DomainsList list)
{
	unsigned int stats = 0;
	stats = VIR_DOMAIN_STATS_VCPU;
	virDomainStatsRecordPtr *records = NULL;
	check(virDomainListGetStats(list.domains, stats,
				    &records, 0) > 0,
	      "Could not get domains stats");
	//virDomainStatsRecordPtr *next;
	//for (next = records; *next; next++) {
	//printDomainParams(*next); // for debugging
	//}
	return records;
error:
	exit(1);
}



void vCPUUsage(struct DomainsList list)
{
	for (int i = 0; i < list.count; i++) {
		int max_id, nparams = 0;
		virTypedParameterPtr params = NULL;
		max_id = virDomainGetCPUStats(list.domains[i],
					      NULL, 0, 0, 0, 0);
		check(max_id > 0,
		      "Could not retrieve max number of vCPUs in domain");
		nparams = virDomainGetCPUStats(list.domains[i],
					       NULL, 0, 0, 1, 0);
		check(nparams > 0,
		      "Could not retrieve params of vCPUs in domain");
		params = calloc(nparams, sizeof(virTypedParameter));
		check(virDomainGetCPUStats(list.domains[i],
					   params, nparams,
					   -1, 1, 0) > 0,
		      "Failed to get vCPU stats");  //total stats.
		for (i = 0; i < nparams; i++) {
			printf("field %s - value %llu\n",
			       params[i].field, params[i].value.ul);
		}
		virTypedParamsClear(params, nparams * max_id);
	}
	return;
error:
	exit(1);
}

void printCPUmap(virConnectPtr conn)
{
	int cpunum;
	unsigned char *cpumap = NULL;
	unsigned int online;
	cpunum = virNodeGetCPUMap(conn, &cpumap, &online, 0);
	check(cpunum > 0, "Failed when retrieving CPU map");
	printf("Total CPUs %d - online %d\n", cpunum, online);
	printf("CPU map: ");
	for (int cpu = 0; cpu < cpunum; cpu++)
		printf("%c", VIR_CPU_USED(cpumap, cpu) ? 'y' : '-');
	printf("\n");
	free(cpumap);
	return;
error:
	exit(1);
}

double usage(unsigned long long difference, unsigned long long period)
{
	return 100 * ((double) difference / (double) period);
}

struct DomainStats createDomainStats(virDomainStatsRecordPtr record)
{
	// One could sample vCPUs here - with vcpu.0.time etc...
	// We know that values are always unsigned long because
	// we just queried for VCPU info
        struct DomainStats ret;
	int vcpus_count = 0;
	int vcpu_number;
	unsigned long long *previous_vcpus;
	unsigned long long int *current_vcpus;
	for (int i = 0; i < record->nparams; i++) {
		if(strcmp(record->params[i].field, "vcpu.current") == 0) {
			vcpus_count = record->params[i].value.i;
			current_vcpus = (unsigned long long int *)
				calloc(vcpus_count, sizeof(unsigned long long int));
			check(current_vcpus != NULL,
			      "Could not allocate memory for stats struct");
		}

		int field_len = strlen(record->params[i].field);
		if (field_len >= 4) {
			const char *last_four = &record->params[i].field[field_len-4];
			if (strcmp(last_four, "time") == 0) {
				vcpu_number = atoi(&record->params[i].field[field_len-6]); // vCPU number
				current_vcpus[vcpu_number] = record->params[i].value.ul;
				printf("%llu\n", current_vcpus[vcpu_number]);
			}
		}
		printf("%s %s - %llu\n",
		       virDomainGetName(record->dom),
		       record->params[i].field,
		       record->params[i].value.ul);
	}
	return ret;
error:
	exit(1);
}

int main(int argc, char **argv)
{
	check(argc == 2, "ERROR: You need one argument, the time interval in seconds"
	      " the scheduler will trigger.");
	struct DomainsList list;
	struct DomainsStats *domain_stats;
	unsigned long long previous_pcpu, current_pcpu;
	// The main method has a struct DomainStats *domains_stats;
	// This pointer is allocated and freed on every scheduler period.
	virConnectPtr conn;
	virNodeInfo info;
	virDomainStatsRecordPtr *records = NULL;
	printf("- vCPU scheduler - interval: %s\n", argv[1]);
	printf("Connecting to Libvirt... \n");
	conn = local_connect();
	virNodeGetInfo(conn, &info);
	printf("Connected! \n");
	previous_pcpu = pCpuSample(conn);
	while ((list = active_domains(conn)).count > 0) {
		current_pcpu = pCpuSample(conn);
		printf("pCPU usage: %f%%\n",
		       usage(current_pcpu - previous_pcpu,
			     atoi(argv[1]) * NANOSECOND)/info.cpus);
		previous_pcpu = current_pcpu;
		printCPUmap(conn);
		records = domainvCPUStats(list);
		virDomainStatsRecordPtr *next;
		for (next = records; *next; next++) {
			// sampling code for vCPUs goes here
			createDomainStats(*next);
		}
		virDomainStatsRecordListFree(records);
		sleep(atoi(argv[1]));
		clearScreen();
	}
	printf("No active domains - closing. See you next time! \n");
	virConnectClose(conn);
	free(conn);
	return 0;
error:
	return 1;
}
