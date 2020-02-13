# Test Case 5

In this test case, you will run 8 virtual machines that start with an equal affinity to each pCPU (i.e., the vCPU of each VM is equally like to run on any pCPU of the host). Four of these vCPUs will run a heavy workload and the other four vCPUs will run a light workload.

## Expected Outcome

Each pCPU will exhibit an equal balance of vCPUs given the assigned workloads.
