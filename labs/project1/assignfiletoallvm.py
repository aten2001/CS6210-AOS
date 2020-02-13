#!/usr/bin/python
from __future__ import print_function
from vm import VMManager
from testLibrary import TestLib
import sys

VM_PREFIX = "aos"

def main(args):
    if len(args) != 1:
        print('Usage: ./assignfiletoallvm.py [file]')
        exit(-1)
    filename = sys.argv[1]
    manager = VMManager()
    vms = manager.getRunningVMNames()
    filteredVms = [vm for vm in vms if vm.startswith(VM_PREFIX)]
    if not filteredVms:
        print("No VMs exist with 'aos' prefix. Exiting...")
        exit(-1)
    TestLib.copyFiles(filename,filteredVms)

if __name__ == '__main__':
    main(sys.argv[1:])
