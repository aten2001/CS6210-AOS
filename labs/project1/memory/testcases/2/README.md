### The first stage

1. All virtual machines consume memory gradually.
2. All virtual machines start from 512MB
3. __Expected outcome__: all virtual machines gain more and more memory. At the end each virtual machine should have similar memory balloon size.

### The second stage

1. All virtual machines free memory gradually.
2. __Expected outcome__: all virtual machines give memory resources to host.
