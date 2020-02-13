#!/usr/bin/env bash
import libvirt
import time

class VMManager:
    def __init__(self,uri='qemu:///system'):
        self.conn = libvirt.open(uri)

    def __del__(self):
        self.conn.close()

    def getPhysicalCpus(self):
        hostinfo = self.conn.getInfo()
        return hostinfo[4] * hostinfo[5] * hostinfo[6] * hostinfo[7]

    def getRunningVMNames(self,filterPrefix=None):
        domainIDs = self.conn.listDomainsID()
        vms = [self.conn.lookupByID(id).name() for id in domainIDs]
        if filterPrefix:
            return [vm for vm in vms if vm.startswith(filterPrefix)]
        return vms

    def getAllVMNames(self):
        return [domain.name() for domain in self.conn.listAllDomains(0)]

    def getVmObject(self,vmName):
        try:
            vm = self.conn.lookupByName(vmName)
        except libvirt.libvirtError:
            print("Name used for starting vm is invalid [{}]".format(vmName))
            raise Exception("Name used for vm is invalid, please check the configuration for [{}] and/or if the VM was already created".format(vmName))
        return vm

    def startVM(self,name):
        vm = self.getVmObject(name)
        if vm:
            try:
                vm.create()
            except libvirt.libvirtError:
                print("Issues starting vm [{}], most likely it was already running".format(vmName))
        else:
            print("Error happened starting vm [{}]".format(name))

    def destroyVM(self,name):
        vm = self.getVmObject(name)
        if vm:
            vm.destroy()
        else:
            print("Error happened destroying vm [{}]".format(name))

    def shutdownVM(self,name):
        vm = self.getVmObject(name)
        if vm:
            vm.shutdown()
        else:
            print("Error happened shutting down vm [{}]".format(name))

    def getPinTupleToOneCpu(self,pCpu):
        pinlist = [False] * self.getPhysicalCpus()
        pinlist[pCpu]=True
        return tuple(pinlist)

    def pinVCpuToPCpu(self,vmName,vCpu,pCpu):
        vm = self.getVmObject(vmName)
        pinTuple = self.getPinTupleToOneCpu(pCpu)
        if vm:
            vm.pinVcpu(vCpu,pinTuple)
        else:
            print("Error happened pinning a vcpu for vm [{}]".format(name))

    def upinVCpu(self,vmName,vCpu):
        vm = self.getVmObject(vmName)
        pinlist = [True] * self.getPhysicalCpus()
        if vm:
            vm.pinVcpu(vCpu,tuple(pinlist))
        else:
            print("Error happened pinning a vcpu for vm [{}]".format(name))

    def setMemory(self,vmName,memory):
        vm = self.getVmObject(vmName)
        if vm:
            vm.setMemory(memory*1024)
        else:
            print("Error happened setting memory for vm [{}]".format(name))

    def setMaxMemory(self,vmName,memory):
        vm = self.getVmObject(vmName)
        if vm:
            vm.setMaxMemory(memory*1024)
        else:
            print("Error happened setting memory for vm [{}]".format(name))

    def getFilteredVms(self,filterPrefix):
        vms=self.getAllVMNames()
        filtered = [vm for vm in vms if vm.startswith(filterPrefix)]
        return filtered

    def setAllVmsMemoryWithFilter(self,filterPrefix,newMemory):
        filtered = self.getFilteredVms(filterPrefix)
        for vmName in filtered:
            self.setMemory(vmName,newMemory)

    def setAllVmsMaxMemoryWithFilter(self,filterPrefix,newMemory):
        filtered = self.getFilteredVms(filterPrefix)
        for vmName in filtered:
            self.setMaxMemory(vmName,newMemory)

    def startAllVMsWithFilter(self,filterPrefix, waitTime=5):
        filtered = self.getFilteredVms(filterPrefix)
        for vm in filtered:
            self.startVM(vm)
        time.sleep(waitTime)
