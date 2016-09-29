#include <shared/libvirt_domains.c>
#include <stdio.h>
#include <unistd.h>

static const long NANOSECOND = 1000000000;
// The following values are used to determine when a vCPU
// should be pinned to a different pCPU. If the usage
// standard deviation for any of the pCPUs is larger than this
// (between periods), it will be reassigned to another vCPU.
static const double USAGE_THRESHOLD = 50.0;

struct DomainStats {
	virDomainPtr domain;
	int vcpus_count;
	double *usage;
	double avg_usage;
	// Contains the vCPU time samples in the same order
	// as the vCPUs, e.g vcpus[0] for vCPU0
	unsigned long long int *vcpus;
};

// Samples the global CPU time
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
		//printf("pCPUs %s: %llu ns\n", params[i].field, params[i].value);
	}
	free(params);
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

virDomainStatsRecordPtr *domainvCPUStats(struct DomainsList list)
{
	unsigned int stats = 0;
	virDomainStatsRecordPtr *records = NULL;

	stats = VIR_DOMAIN_STATS_VCPU;
	check(virDomainListGetStats(list.domains, stats,
				    &records, 0) > 0,
	      "Could not get domains stats");
	// These lines are for debugging
	//virDomainStatsRecordPtr *next;
	//for (next = records; *next; next++) {
	//printDomainParams(*next);
	//}
	return records;
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
	int vcpu_number, field_len;
	unsigned long long int *current_vcpus;
	const char *last_four;

	for (int i = 0; i < record->nparams; i++) {
		if (strcmp(record->params[i].field, "vcpu.current") == 0) {
			vcpus_count = record->params[i].value.i;
			current_vcpus = (unsigned long long int *)
				calloc(vcpus_count, sizeof(unsigned long long int));
			check(current_vcpus != NULL,
			      "Could not allocate memory for stats struct");
		}
		field_len = strlen(record->params[i].field);
		if (field_len >= 4) {
			last_four = &record->params[i].field[field_len-4];
			if (strcmp(last_four, "time") == 0) {
				vcpu_number = atoi(&record->params[i].field[field_len-6]); // vCPU number
				current_vcpus[vcpu_number] = record->params[i].value.ul;
			}
		}
	}
	ret.domain = record->dom;
	ret.vcpus_count = vcpus_count;
	ret.vcpus = current_vcpus;
	return ret;
error:
	exit(1);
}

// Populates previous_domain_stats with all the stats in
// the struct DomainStats.
void calculateDomainUsage(struct DomainStats *previous_domain_stats,
			  struct DomainStats *current_domain_stats,
			  int domains_count,
			  long period)
{
	double avg_usage;

	// i represents number of domains
	for (int i = 0; i < domains_count; i++) {
		current_domain_stats[i].usage =
			calloc(current_domain_stats[i].vcpus_count,
			       sizeof(double));
		avg_usage = 0.0;
		printf("Domain: %s:\n",
		       virDomainGetName(current_domain_stats[i].domain));
		// j represents vcpu number
		for (int j = 0; j < current_domain_stats[i].vcpus_count; j++) {
			current_domain_stats[i].usage[j] =
				usage(current_domain_stats[i].vcpus[j] -
				      previous_domain_stats[i].vcpus[j],
				      period);
			printf("  - vCPU %d usage: %f%%\n",
			       j,
			       current_domain_stats[i].usage[j]);
			avg_usage += current_domain_stats[i].usage[j];

		}
		current_domain_stats[i].avg_usage =
			avg_usage/current_domain_stats[i].vcpus_count;
		printf("  - Average vCPU usage: %f%%\n",
		       current_domain_stats[i].avg_usage);
	}
	memcpy(previous_domain_stats, current_domain_stats,
	       domains_count * sizeof(struct DomainStats));
}

// Populates an array in cpu_usage with the % of usage of
// each pCPU
void setCpuUsage(struct DomainStats *domain_stats,
		 int nr_domains,
		 int maxcpus,
		 double *cpu_usage)
{
	virVcpuInfoPtr cpuinfo;
	unsigned char *cpumaps;
	size_t cpumaplen;
	int vcpus_per_cpu[maxcpus];

	// Stores the usage of a CPU by adding the usage of
	// the vCPUs it uses in all domains and divides it by the
	// vcpus it uses.
	// double cpu_usage[maxcpus];
	memset(vcpus_per_cpu, 0, sizeof(int) * maxcpus);

	for (int i = 0; i < nr_domains; i++) {
		printf("Domain: %s:\n",
		       virDomainGetName(domain_stats[i].domain));
		cpuinfo = calloc(domain_stats[i].vcpus_count,
				 sizeof(virVcpuInfo));
		cpumaplen = VIR_CPU_MAPLEN(maxcpus);
		cpumaps = calloc(domain_stats[i].vcpus_count,
				 cpumaplen);
		check(virDomainGetVcpus(domain_stats[i].domain,
					cpuinfo,
					domain_stats[i].vcpus_count,
					cpumaps, cpumaplen) > 0,
		      "Could not retrieve vCpus affinity info");
		for (int j = 0; j < domain_stats[i].vcpus_count; j++) {
			cpu_usage[cpuinfo[j].cpu] += domain_stats[i].usage[j];
			vcpus_per_cpu[cpuinfo[j].cpu] += 1;
			printf("  - CPUmap: 0x%x", cpumaps[j]);
			printf(" - CPU: %d", cpuinfo[j].cpu);
			printf(" - vCPU %d affinity: ", j);
			for (int m = 0; m < maxcpus; m++) {
				printf("%c",
				       VIR_CPU_USABLE(cpumaps, cpumaplen, j, m) ?
				       'y' : '-');
			}
			printf("\n");
		}
		free(cpuinfo);
		free(cpumaps);
	}
	printf("--------------------------------\n");
	for (int i = 0; i < maxcpus; i++) {
		if (vcpus_per_cpu[i] != 0) {
			cpu_usage[i] = cpu_usage[i]/((double)vcpus_per_cpu[i]);
			printf("CPU %d - # vCPUs assigned %d - usage %f%%\n",
			       i,
			       vcpus_per_cpu[i],
			       cpu_usage[i]);
		}
	}
	return;
error:
	exit(1);
}

// Initially pin all vCPUs to their correspondent CPU number, e.g vCPU 0
// is pinned to CPU 0 mod #CPUs, vCPU 1 to CPU 1 mod #CPUs, etc...
void setInitialVcpuPinning(struct DomainStats *domain_stats,
			   int domains_count,
			   int nr_cpus)
{
	unsigned char map;
	size_t cpumaplen = VIR_CPU_MAPLEN(nr_cpus);
	unsigned char nr_cpus_mask = 0x1;
	unsigned char temp = 0x1;

	for (int i = 0; i < nr_cpus; i++) {
		temp <<= 0x1;
		nr_cpus_mask ^= temp;
	}

	for (int i = 0; i < domains_count; i++) {
		map = 0x1;
		for (int j = 0; j < domain_stats[i].vcpus_count; j++) {
			printf("  - CPUmap: 0x%x\n", map);
			virDomainPinVcpu(domain_stats[i].domain,
					 j,
					 &map,
					 cpumaplen);
			map <<= 0x1;
			map %= nr_cpus_mask;  // equivalent to map % # of pCPUs
		}
	}
}

void pinPcpus(double *usage, int nr_cpus,
	      struct DomainStats *domain_stats, int domains_count)
{
	char do_nothing = 1;
	int freest;
	double freest_usage = 100.0;
	int busiest;
	double busiest_usage = 0.0;
	// Do not pin anything if all pCpus are above the threshold,
	// no room available to change pinnings
	for (int i = 0; i < nr_cpus; i++) {
		do_nothing &= (usage[i] < USAGE_THRESHOLD);
		if (usage[i] > busiest_usage) {
			busiest_usage = usage[i];
			busiest = i;
		}
		if (usage[i] < freest_usage) {
			freest_usage = usage[i];
			freest = i;
		}
	}
	if (do_nothing) {
		printf("Cannot or should not change pinnings\n");
		printf("Busiest CPU: %d - Freest CPU: %d\n", busiest, freest);
		return;
	}
	printf("Busiest CPU: %d - Freest CPU: %d\n", busiest, freest);
	printf("Busiest CPU above usage threshold of %f%%\n", USAGE_THRESHOLD);
	printf("Changing pinnings...\n");
	virVcpuInfoPtr cpuinfo;
	unsigned char *cpumaps;
	size_t cpumaplen;
	unsigned char freest_map = 0x1 << freest;
	unsigned char busiest_map = 0x1 << busiest;

	// To do so iterate over all domains, over all vcpus and change
	// the busiest CPU vcpus for the freest ones and viceversa
	for (int i = 0; i < domains_count; i++) {
		cpuinfo = calloc(domain_stats[i].vcpus_count,
				 sizeof(virVcpuInfo));
		cpumaplen = VIR_CPU_MAPLEN(nr_cpus);
		cpumaps = calloc(domain_stats[i].vcpus_count,
				 cpumaplen);
		check(virDomainGetVcpus(domain_stats[i].domain,
					cpuinfo,
					domain_stats[i].vcpus_count,
					cpumaps, cpumaplen) > 0,
		      "Could not retrieve vCpus affinity info");
		for (int j = 0; j < domain_stats[i].vcpus_count; j++) {
			if (cpuinfo[j].cpu == busiest) {
				printf("%s vCPU %d is one of the busiest\n",
				       virDomainGetName(domain_stats[i].domain),
				       j);
				virDomainPinVcpu(domain_stats[i].domain,
						 j,
						 &freest_map,
						 cpumaplen);
			} else if (cpuinfo[j].cpu == freest) {
				printf("%s vCPU %d is one of the freest\n",
				       virDomainGetName(domain_stats[i].domain),
				       j);
				virDomainPinVcpu(domain_stats[i].domain,
						 j,
						 &busiest_map,
						 cpumaplen);

			}
		}
		free(cpuinfo);
		free(cpumaps);
	}
	return;
error:
	exit(1);

}

int main(int argc, char **argv)
{
	check(argc == 2, "ERROR: You need one argument, the time interval in seconds"
	      " the scheduler will trigger.");
	struct DomainsList list;
	struct DomainStats *previous_domain_stats, *current_domain_stats;
	int domain_counter = 0, previous_count = 0, maxcpus;
	unsigned long long previous_pcpu, current_pcpu;
	double *pcpu_usage;
	// The main method has a struct DomainStats *domains_stats;
	// This pointer is allocated and freed on every scheduler period.
	virConnectPtr conn;
	virNodeInfo info;
	virDomainStatsRecordPtr *domains = NULL;

	printf("- vCPU scheduler - interval: %s\n", argv[1]);
	printf("Connecting to Libvirt...\n");
	conn = local_connect();
	virNodeGetInfo(conn, &info);
	printf("Connected!\n");
	previous_pcpu = pCpuSample(conn);
	maxcpus = virNodeGetCPUMap(conn, NULL, NULL, 0);
	while ((list = active_domains(conn)).count > 0) {
		// pCPU usage calculation
		current_pcpu = pCpuSample(conn);
		printf("pCPU usage: %f%%\n",
		       usage(current_pcpu - previous_pcpu,
			     atoi(argv[1]) * NANOSECOND)/info.cpus);
		previous_pcpu = current_pcpu;

		// vCPU usage calculation
		domains = domainvCPUStats(list);
		current_domain_stats = (struct DomainStats *)
			calloc(list.count, sizeof(struct DomainStats));
		virDomainStatsRecordPtr *next;

		for (next = domains; *next; next++) {
			current_domain_stats[domain_counter] = createDomainStats(*next);
			domain_counter++;
		}

		// Check if the number of VMs changed, if so, don't calculate
		// stats during this period and set initial vCPU pinning
		if (previous_count != list.count) {
			// Do not calculate usage or change pinning, we don't have stats yet
			previous_domain_stats = (struct DomainStats *)
				calloc(list.count, sizeof(struct DomainStats));
			memcpy(previous_domain_stats, current_domain_stats,
			       list.count * sizeof(struct DomainStats));
			setInitialVcpuPinning(current_domain_stats,
					      list.count,
					      maxcpus);
		} else {
			calculateDomainUsage(previous_domain_stats,
					     current_domain_stats,
					     list.count,
					     atoi(argv[1]) * NANOSECOND);
			pcpu_usage = calloc(maxcpus, sizeof(double));
			setCpuUsage(current_domain_stats,
				    list.count,
				    maxcpus,
				    pcpu_usage);
			pinPcpus(pcpu_usage, maxcpus,
				 current_domain_stats, list.count);
			free(pcpu_usage);
		}

		domain_counter = 0;
		free(current_domain_stats);
		virDomainStatsRecordListFree(domains);
		previous_count = list.count;
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
