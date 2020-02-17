#!/usr/bin/python
from __future__ import print_function
import argparse
import os, sys
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from vm import VMManager
from testLibrary import TestLib
import sched, time

VM_PREFIX="aos"
def print_line(x):
    for i in range(x):
        print('-', end = "")
    print("")

def run(sc,vmobjlist,machineParseable):
    i = 0
    print_line(50)
    for vm in vmobjlist:
        stats = vm.memoryStats()
        if machineParseable:
            print("memory,{},{},{}"
                    .format(vm.name(), 
                        stats['actual'] / 1024.0,
                        stats['unused'] / 1024.0))
        else:
            print("Memory (VM: {})  Actual [{}], Unused: [{}]"
                    .format(vm.name(), 
                        stats['actual'] / 1024.0,
                        stats['unused'] / 1024.0))

        i+=1
    sc.enter(2, 1, run, (sc,vmobjlist,machineParseable))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-m","--machine",action="store_true",help="outputs a machine parseable format")
    args = parser.parse_args()
    machineParseable = args.machine
    s = sched.scheduler(time.time, time.sleep)
    manager = VMManager()
    vmlist = manager.getRunningVMNames(VM_PREFIX)
    vmobjlist = [manager.getVmObject(name) for name in vmlist]   
    
    for vm in vmobjlist:
        vm.setMemoryStatsPeriod(1)   
 
    s.enter(2, 1, run, (s,vmobjlist,machineParseable,))
    s.run()
