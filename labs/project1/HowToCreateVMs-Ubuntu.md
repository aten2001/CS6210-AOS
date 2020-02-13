# Creating Test VMs for Project 1

The most convenient way to create test VMs for this project is via Ubuntu's `uvtool` utility. This tool provides pre-built Ubuntu VM images for use within a libvirt/KVM environment.

## Setup

1. Ensure that qemu-kvm and libvirt-bin have been installed and that nested virtualization (or hardware virtualization if you are developing natively) has been enabled in your environment.
2. Install uvtool and uvtool-libvirt
    ``` shell
    sudo apt -y install uvtool
    ```
3. Synchronize the _xenial_ Ubuntu image to your environment.
    ``` shell
    uvt-simplestreams-libvirt sync release=xenial arch=amd64
    ```
4. Create an SSH key for use in accessing the new VMs.
    ``` shell
    ssh-keygen
    ```
5. Create a new virtual machine (**Note: Your project VM names *MUST* start with *aos_***)
    ``` shell
    uvt-kvm create aos_vm1 release=xenial
    ```
6. (Optional) Connect to the running VM
    ``` shell
    uvt-kvm ssh aos_vm1 --insecure
    ```

### References:
[https://help.ubuntu.com/lts/serverguide/virtualization.html](https://help.ubuntu.com/lts/serverguide/virtualization.html)
