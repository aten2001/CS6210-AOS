#!/usr/bin/env python
import os
import subprocess

class TestLib:
    @staticmethod
    def getIps(vmNames):
        baseCmd = 'uvt-kvm ip {}'
        return [os.popen(baseCmd.format(vm)).read().strip() for vm in vmNames]

    @staticmethod
    def copyFiles(fileLocation,vmNames):
        ips = TestLib.getIps(vmNames) 
        for ip in ips:
            print('Copy {} to {}.'.format(fileLocation, ip))
            os.popen('scp -r {} ubuntu@{}:~/'.format(fileLocation, ip))

    @staticmethod
    def startTestCase(templateCmd, ipAndValues):
        FNULL = open(os.devnull, 'w')
        pipes = dict()
        for ip,vals in ipAndValues.items():
            sshCmd = "ssh ubuntu@{} '".format(ip)
            sshCmd += templateCmd + "'"
            try:
                pipe = subprocess.Popen(sshCmd.format(vals), stdout=FNULL, shell=True)
                pipes[ip]=pipe
            except OSError:
                raise Exception("Problems when executing the command {} with vals {}".format(sshCmd,vals))
            except ValueError:
                raise Exception("Invalid arguments when executing the command {} with vals {}".format(sshCmd,vals))
        return pipes



