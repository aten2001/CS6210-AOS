#include <stdio.h>
#include <stdlib.h>
#include <libvirt/libvirt.h>
#include <string.h>
#include <unistd.h>

//#define DEBUG 1

#ifdef DEBUG
    #define debug(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__);
#else
    #define debug(fmt, ...) //nothing
#endif

struct workloadNode{
    double workload;
    int id;
} typedef workloadNode;

void error(char *str, int doexit)
{
    fprintf(stderr, "%s\n", str);
	if(doexit)
		exit(1);
	return;
}

int getActiveVMs(virConnectPtr conn, virDomainPtr ** domains){
	unsigned int flags = VIR_CONNECT_LIST_DOMAINS_ACTIVE | VIR_CONNECT_LIST_DOMAINS_RUNNING;
	int ret = virConnectListAllDomains(conn, domains, flags);
       return ret;	
}

void getVCPUWorkloadStats(virDomainPtr* domains, int nVMs, int npCPUs, double *workload){
   
    for(int i=0; i<nVMs; i++){
		//Grab VM info
		virDomainInfo dominfo;
		if(virDomainGetInfo(domains[i], &dominfo) < 0) error("Dom info failed", 1);
		virTypedParameterPtr params;
		int nparams = virDomainGetCPUStats(domains[i], NULL, 0, -1, 1, 0); 
		if(nparams < 0) 
			error("Get nparams failed",1);
		params = (virTypedParameterPtr)calloc(nparams, sizeof(virTypedParameter));
		if(virDomainGetCPUStats(domains[i], params, nparams, -1, 1, 0) < 0){
			error("Get CPU Stats failed", 1);
		}
		// CPU usage
		unsigned long long cpu_time = 0;
		virTypedParamsGetULLong(params, nparams, VIR_DOMAIN_CPU_STATS_CPUTIME, &cpu_time);
		
        workload[i] = cpu_time;

        free(params);

	}
}

void doDefaultPinning(virDomainPtr* domains, int nVMs, int npCPUs){
    /* Assign CPUs in roundrobin manner starting from 0
     * */
    unsigned char map = 0x1;
    int maplen = VIR_CPU_MAPLEN(npCPUs);
    for(int i=0; i<nVMs; i++){
        //Assuming we just have one cpu/vm
        virDomainPinVcpu(domains[i], 0, &map, maplen);
        map <<= 1;
        if(i == npCPUs-1)
            map = 0x1;
    }
    return;
}

int compare (const  void *a1, const void * b1){
/*
 * qsort compare function
 * */
    workloadNode * a = (workloadNode *)a1;
    workloadNode * b = (workloadNode *)b1;
    if ((a->workload) > (b->workload)) return 1;
    else if ((a->workload) < (b->workload)) return -1;
    else return 0;
}
int cmp (const void * a, const void * b)
{
  if (*(double*)a > *(double*)b) return 1;
  else if (*(double*)a < *(double*)b) return -1;
  else return 0;  
}

void runScheduler(virDomainPtr* domains, double *currWorkload, double* prevWorkload, int nVMs, int npCPUs){
    /*Pinning done based on decreasing workload 0 - npCPUs-1;
     * Once all cpus are occupied, we then we start assigning
     * work to processor with minimum current load
     * */
    double workload[nVMs];
    workloadNode vmToWorkloadMap[nVMs];
    for(int i=0; i<nVMs; i++){
        workload[i] = (currWorkload[i] - prevWorkload[i])/1000000000;
        vmToWorkloadMap[i].workload = workload[i];
        vmToWorkloadMap[i].id = i;
    }

    qsort(vmToWorkloadMap, nVMs, sizeof(workloadNode), compare);
    for(int i=0; i<nVMs; i++){
        debug("workload[%s] = %f\t", virDomainGetName(domains[vmToWorkloadMap[i].id]), vmToWorkloadMap[i].workload);
    }
    
    /*Get previous pinning*/
    double prevHostCpuWorkload[npCPUs];
    for (int i = 0; i < npCPUs; i++)
        prevHostCpuWorkload[i] = 0.0;
    for (int i = 0; i < nVMs; i++)
    {
        virVcpuInfo *cpuinfo = calloc(1, sizeof(virVcpuInfo));
        int cpumaplen = VIR_CPU_MAPLEN(npCPUs);
        unsigned char *cpumaps = calloc(1, cpumaplen);
        virDomainGetVcpus(domains[i], cpuinfo, 1, cpumaps, cpumaplen);
        for (int j = 0; j < npCPUs; j++){
            if (((*cpumaps) >> j)&1){
                debug("pin %s = %d\n", virDomainGetName(domains[i]), j);
                prevHostCpuWorkload[j] += workload[i];
            }
        }
    }

    int count =0;
    int pininfo[nVMs];
    double runningWorkload[npCPUs];
    for (int i = 0; i < npCPUs; i++)
        runningWorkload[i] = 0.0;
    for (int i = nVMs - 1; i >= 0; i--){
        if(count < npCPUs){
            pininfo[vmToWorkloadMap[i].id] = count;
            runningWorkload[count++] = vmToWorkloadMap[i].workload;
        }
        else{
            // find the minimum workload cpu
            int min = 0;
            for (int j = 1; j < npCPUs; j++)
                if(runningWorkload[min] > runningWorkload[j])
                    min = j;
            debug("min = %d\t", min);
            pininfo[vmToWorkloadMap[i].id] = min;
            runningWorkload[min] += vmToWorkloadMap[i].workload;
        }
    }
  
    for (int i = 0; i < nVMs; i++)
        debug("\npininfo[%s] = %d\t", virDomainGetName(domains[i]), pininfo[i]);

    for (int i = 0; i < npCPUs; i++)
        debug("runningwl %f; prevwl %f\t", runningWorkload[i], prevHostCpuWorkload[i]);
    
    qsort(runningWorkload, npCPUs, sizeof(double), cmp);
    qsort(prevHostCpuWorkload, npCPUs, sizeof(double), cmp);
    //if delta is small dont change pinning
    double delta = 0.0;
    double total = 0.0;
    for (int i = 0; i < npCPUs; i++){
        delta += abs(prevHostCpuWorkload[i] - runningWorkload[i]);
        total += prevHostCpuWorkload[i];
    }
    debug("delta= %f", delta);
    if (delta < 0.1*total)
        return;
    unsigned char map = 0x1;
    int maplen = VIR_CPU_MAPLEN(npCPUs);
   
    for (int i = 0; i < nVMs; i++){
        map = 0x1 << pininfo[i];
        virDomainPinVcpu(domains[i], 0, &map, maplen);
    }
    return;
}

int main(int argc, char *argv[]){
	virConnectPtr conn;
    int period = 12;
    if(argc == 2){
        period = atoi(argv[1]);
    }
    debug("%d\n", period);
    
	conn = virConnectOpen("qemu:///system");
	if(conn == NULL){
		error("Failed to open connection to hypervisor", 1);
		return 1;
	}
	//Grab node info
	virNodeInfo nodeinfo;
	if(virNodeGetInfo(conn, &nodeinfo) < 0 ) error("Node Info failed", 1);
	int npCPUs = VIR_NODEINFO_MAXCPUS(nodeinfo);
	//Grab active running vms
	virDomainPtr *domains;
	int nVMs = getActiveVMs(conn, &domains);
	int prevVMs = 0;
    if(nVMs < 0)
		error("Cannot get VMs", 1);

    //Here I'm assuming that we just have one CPU per VM so just one stat value/VM

    while(nVMs){
        double workloadStats[nVMs];
        memset(workloadStats, 0.0, nVMs*sizeof(double));
        
        //If prevVM count doesn't match current count, then we don't have stats yet for this
        //setup so will need to sample them before we can run scheduler
        if(prevVMs != nVMs){
            getVCPUWorkloadStats(domains, nVMs, npCPUs, workloadStats);
            //doDefaultPinning(domains, nVMs, npCPUs);    
        }
        else{
            double currentWorkloadStats[nVMs];
            memset(currentWorkloadStats, 0.0, nVMs*sizeof(double));
            getVCPUWorkloadStats(domains, nVMs, npCPUs, currentWorkloadStats);
            runScheduler(domains, currentWorkloadStats, workloadStats, nVMs, npCPUs);
            for(int i=0; i<nVMs; i++)
                workloadStats[i] = currentWorkloadStats[i];
           // workloadStats = currentWorkloadStats;
        }
        prevVMs = nVMs;
        sleep(period);
        for(int i=0; i<nVMs; i++)
		    virDomainFree(domains[i]);	
	
        nVMs = getActiveVMs(conn, &domains);
    }
	for(int i=0; i<nVMs; i++)
		virDomainFree(domains[i]);	
    free(domains);
	virConnectClose(conn);
}
