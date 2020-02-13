# Test Case 1

In this test case, you will run 8 virtual machines that all start pinned to pCPU0. The vCPU of each VM will process the same workload.

## Expected Outcome

Each pCPU will exhibit an equal balance of vCPUs given the assigned workloads (e.g., if there are 4 pCPUs and 8 vCPUs, then there would be 2 vCPUs per pCPU).
