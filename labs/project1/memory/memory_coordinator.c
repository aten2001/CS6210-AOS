#include<stdio.h>
#include<libvirt/libvirt.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>

//#define DEBUG 1

#ifdef DEBUG
    #define debug(fmt, ...) printf("%d: " fmt,  __LINE__, __VA_ARGS__);
#else
    #define debug(fmt, ...) //nothing
#endif

// Note UPPER_THRESHOLD > 2*LOWER_THRESHOLD based on our policy
// We grab LOWER_THRESHOLD or assign LOWER_THRESHOLD
// If above condition is not met we will land in infinite loop of exchange
// If available mem is below this we need to assign more
unsigned long long LOWER_THRESHOLD = 100 * 1024;
// If avialble mem is more than this value just take back memory
unsigned long long UPPER_THRESHOLD = 210 * 1024;

struct domainStats{
    int id;
    unsigned long long available, unused, actual;
}typedef domainStats;

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

void getMemoryStats(virDomainPtr *domains, int nVMs, domainStats* memstats){
    for(int i=0; i<nVMs; i++){
        virDomainMemoryStatStruct stats[VIR_DOMAIN_MEMORY_STAT_NR];
        if(virDomainSetMemoryStatsPeriod(domains[i], 1, VIR_DOMAIN_AFFECT_LIVE) < 0)
            error("set Stats Period Failed", 1); 
 
        if(virDomainMemoryStats(domains[i], stats, VIR_DOMAIN_MEMORY_STAT_NR, 0) < 0)
            error("cannot find mem stats", 0);
        
        memstats[i].id = i;
        for (int j = 0; j < VIR_DOMAIN_MEMORY_STAT_NR; j++){
            if(stats[j].tag == VIR_DOMAIN_MEMORY_STAT_AVAILABLE)
                memstats[i].available = stats[j].val;
            else if(stats[j].tag == VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON)
                memstats[i].actual = stats[j].val;
            else if(stats[j].tag == VIR_DOMAIN_MEMORY_STAT_UNUSED)
                memstats[i].unused = stats[j].val;
        }

        debug("name:%s aval: %llu MB actual: %llu MB unused: %lluMB\n", virDomainGetName(domains[i]), 
                memstats[i].available/1024, memstats[i].actual/1024, memstats[i].unused/1024);
    }
}

void runMemoryManager(virDomainPtr *domains, domainStats* mem, int nVMs, unsigned long long freemem){
    /* states[i] = 
     * 0 means it is wasting 
     * 1 means it is starving
     * 2 means it is doing okay*/

    int states[nVMs];
    unsigned long long required = 0;
    unsigned long long available = 0;
    int rcount = 0 ;
    for(int i=0; i<nVMs; i++){
        if(mem[i].unused < LOWER_THRESHOLD){
            states[i] = 1; 
            required += LOWER_THRESHOLD;
            rcount ++;
            debug("Starving VM: %s, Memory = %llu MB\n", virDomainGetName(domains[i]), mem[i].unused/1024);
        }
        else if(mem[i].unused > UPPER_THRESHOLD){
            states[i] = 0;
            available += LOWER_THRESHOLD;
            debug("Excess VM = %s, Memory = %lluMB\n", virDomainGetName(domains[i]), mem[i].unused/1024);
        }
        else
            states[i] = 2;
    }
    debug("required %llu aval %llu\n", required, available);
    
    if (required > available){
        // We won't give mem from host; 
        //Try getting from the host, but if can't that is it for this round
        if(freemem > (required - available + 10*LOWER_THRESHOLD))
            available = required;
    }
    
   
    // Assign all the available memory among the VMs
    if(rcount && available >= LOWER_THRESHOLD * rcount){
        for(int i=0; i<nVMs; i++){
            if(states[i] == 0){
                virDomainSetMemory(domains[i], mem[i].actual - LOWER_THRESHOLD);
                debug("%s exchanges %llu MB with other VM\n", virDomainGetName(domains[i]), LOWER_THRESHOLD / 1024);
            }
            else if(states[i]==1){
                virDomainSetMemory(domains[i], mem[i].actual + available/rcount);
                debug("%s gets %llu MB from VM\n", virDomainGetName(domains[i]), (available/rcount) / 1024);
            }
        }
    }
    else if(rcount){
        //Greedily assign LOWER_THRESHOLD Mem to each VM and atleast one will suffer this round
        for(int i=0; i< nVMs; i++){
            if(states[i] == 0){
                virDomainSetMemory(domains[i], mem[i].actual - LOWER_THRESHOLD);
                debug("%s exchanges %llu MB with other VM\n", virDomainGetName(domains[i]), LOWER_THRESHOLD / 1024);
                }
            else if(states[i] == 1){
                if(available >= LOWER_THRESHOLD){
                    virDomainSetMemory(domains[i], mem[i].actual + LOWER_THRESHOLD);
                    available -= LOWER_THRESHOLD;
                    debug("%s exchanges %llu MB with other VM\n", virDomainGetName(domains[i]), LOWER_THRESHOLD / 1024);
                }
                else{
                    virDomainSetMemory(domains[i], mem[i].actual + available);
                    debug("%s exchanges %llu MB with other VM\n", virDomainGetName(domains[i]), available / 1024);
                    available = 0;
                }
            }
        }
    }
    else if (!rcount && available){
        //return wasted memory to host
         for(int i=0; i<nVMs; i++){
            if(states[i] == 0){
                virDomainSetMemory(domains[i], mem[i].actual - LOWER_THRESHOLD);
                debug("%s returns %llu MB to host\n", virDomainGetName(domains[i]), LOWER_THRESHOLD / 1024);
            }
        }
    }
}

int main(int argc, char *argv[]){
    int period = 12;
    if(argc == 2){
        period = atoi(argv[1]);
    }
    debug("%d\n", period);
    virConnectPtr conn;

    conn = virConnectOpen("qemu:///system");
	if(conn == NULL){
		error("Failed to open connection to hypervisor", 1);
	}
	//Grab active running vms
	virDomainPtr *domains;
	int nVMs = getActiveVMs(conn, &domains);
	if(nVMs < 0)
		error("Cannot get VMs", 1);
    unsigned long long freemem = virNodeGetFreeMemory(conn)/1024;
    debug("Host Memory: %llu\n", freemem / 1024);
        
    while(nVMs){
        domainStats *mem = (domainStats *)malloc(nVMs*sizeof(domainStats));
        getMemoryStats(domains, nVMs, mem);
        freemem = virNodeGetFreeMemory(conn)/1024;
        runMemoryManager(domains, mem, nVMs, freemem);
        debug("Host Memory: %llu\n", freemem / 1024);
        sleep(period);
        free(mem);
        for(int i=0; i<nVMs; i++)
		    virDomainFree(domains[i]);	
        nVMs = getActiveVMs(conn, &domains); 
    }
    for(int i=0; i<nVMs; i++)
		virDomainFree(domains[i]);
    free(domains);
    virConnectClose(conn);
}

