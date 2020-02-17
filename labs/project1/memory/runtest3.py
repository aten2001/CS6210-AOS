#!/usr/bin/python

from __future__ import print_function
import os, sys
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from vm import VMManager
from testLibrary import TestLib

VM_PREFIX="aos"

if __name__ == '__main__':
    manager = VMManager()
    manager.setAllVmsMemoryWithFilter(VM_PREFIX,512)
    vms = manager.getRunningVMNames(VM_PREFIX)
    print('Start testcase 3')
    ips = TestLib.getIps(vms)
    ipsAndVals = dict()
    tmp=0
    for ip in ips:
        if tmp == 0 or tmp == 1:
            ipsAndVals[ip]="A"
        else:
            ipsAndVals[ip]="B"
        tmp+=1
    TestLib.startTestCase("~/testcases/3/run {}",ipsAndVals)

        

