    

## Compilation and Execution steps

 make 
 ./vcpu_scheduler <N> // time out for scheduler
  
  ## Logic Description 
  Our main loop runs until we have a VM running on the host. At each step we collect the workload stats and then compare it with the previous workload stats to decide how to pin the vcpus. Note if we don't have workload stats for the given configuration of VMs, we will first do a dry run with existing pinning to get an estimate of workload. 
  Our scheduler algorithm:
  We sort the VMs based on their workloads in decreasing order, and in first iteration try to assign 1 vcpu to each host cpu. For instance vcpu with highest worload goes to cpu0, then teh next highest goes to cpu1, etc. Once we have assigned 1 vcpu to each host cpu then for the remaining vcpus we run the following logic. 
  At each step, we find which cpu has lowest workload and assign a vcpu to it and repeat this process until all vcpus are assigned.
  Now there might be cases like following:
  vcpu0, vcpu1 -> cpu0 [workload = 50]
  vcpu2 -> cpu1 [workload = 25]
  Here it is possible that we can swap vcpus assigned to cpu0 with cpu1, but that doesn't help us in any manner. To avoid such unnecessary re-assignement, we have implemented another logic. Here before re-assignning, we check it against the existing pinning. If the improvement we are getting after re-pinning is less than a threshold(10% of the workload), we just abort re-pinning. This ensures stablizing and removes redundant fluctuations.