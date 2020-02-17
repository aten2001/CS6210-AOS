
  

## Compilation and Execution steps

  

make

./memory_coordinator <N> // time out for memory_coordinator

## Logic Description

Our main loop runs until we have a VM running on the host. We first collect the memory stats for each running VM and even the free memory available on the host machine. Then we run our memory coordinator logic based on these stats.

## Our scheduler algorithm:
We have defined two thresholds, LOWER_THRESHOLD and UPPER_THRESHOLD. The logic revolves around these threshold values. If the unused memory of a VM drops below our lower_threshold we need to increase its memory ballon and if a VM has unused memory above upper_threshold we will be assuming it is wasting memory so ask it to return memory to the host.
Note LOWER_THRESHOLD is also the memory unit that we exchange in single iteration. We increase/decrease memory by an amount equal to LOWER_THRESHOLD.

### Now our algorithm:
We loop over reach VM and based on the above described conditions, we assign each VM a state. 0 means it is wasting, 1 means it is starving and 2 means it is doing okay. Then based on the states, if it is wasting(0) we decrease the memory by an ammount equal to lower_threshold. If it is starving(1), we have to increase memory by an amount = lower_threshold, but there might be a case that we don't have enough memory for all starving VMs in this case we ask the host if it can provide the deficit. If the host can't then we can't do much this round. We will try assigning as many starving VMs as we can and rest have to wait till we have enough memory available(either a VM releases or host can provide).
