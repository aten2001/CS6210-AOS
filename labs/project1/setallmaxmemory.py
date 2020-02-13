#!/usr/bin/python

from __future__ import print_function
from vm import VMManager

MAX_MEMORY = 2 * 1024
VM_PREFIX = "aos"

if __name__ == '__main__':
    manager = VMManager()
    manager.setAllVmsMaxMemoryWithFilter(VM_PREFIX,MAX_MEMORY)
