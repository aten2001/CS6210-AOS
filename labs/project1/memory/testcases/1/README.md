### The first stage

1. The first virtual machine consumes memory gradually, while others stay inactive.
2. All virtual machines start from 512MB.
3. __Expected outcome__: The first virtual machine gains more and more memory, and others give out some. 

### The second stage

1. The first virtual machine start to free the memory gradually, while others stay inactive.
2. __Expected outcome__: The first virtual machine gives out memory resource to host, and up to policy others may or may not gain memory.
