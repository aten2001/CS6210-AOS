### Big Picture

  - In this project, you are going to implement major chunks of a simple distributed service using [gRPC](http://www.grpc.io).
  - Learnings from this project will also help you in the next project as you will become familiar with gRPC and multithreading with threadpool.
  
### Overview
  - You are going to build a store (you can think of Amazon Store), which receives requests from different users, querying the prices offered by the different registered vendors.
  - Your store will be provided with a file containing addresses of vendor servers in the form \<ip address:port\>. On each product query, your server is supposed to request bids for the queried product from all of these vendor servers.
  - Once your store has responses from all the vendors, it is supposed to collate the (bid, vendor_id) from the vendors and send it back to the requesting client.
  
### Learning Outcomes
  - Synchronous and Asynchronous RPC packages
  - Building a multi-threaded store in a distributed service

### Environment Setup

1. Follow the instructions [here](https://github.gatech.edu/cs6210-aos/project3-oncampus/blob/master/setup.md) for a vcpkg and cmake based setup.

### How You Are Going to Implement It ( Step-by-Step Suggestions)

1. Make sure you understand how gRPC synchronous and asynchronous calls work. Understand the [helloworld example](https://github.com/grpc/grpc/tree/master/examples/cpp/helloworld). You will be building your store with asynchronous mechanisms ONLY.
2. Support asynchronous gRPC communication between -
    - Your store and user client. 
    - Your store and the vendors.  
  (We will provide you with the code for the vendors and the user clients)
3. Create your thread pool and use it to support communication between clients/store/vendors.
   Upon receiving a client request, your store will assign a thread from the thread pool to the incoming request for processing.
    - The thread will make async RPC calls to the vendors
    - The thread will await for all results to come back
    - The thread will collate the results
    - The thread will reply to the store client with the results of the call
    - Having completed the work, the thread will return to the thread pool.
    (IMPORTANT: Since you are using a thread pool, you should re-use threads in the pool instead of creating a new thread for every connection. In other words, if your thread pool has only 4 threads then you can only support 4 connections at a time).
4. Use the test harness to test if your server can handle multiple clients concurrently and make sure that your thread handling is correct.

### Keep In Mind
1. Your Server has to handle:
  - Multiple concurrent requests from clients
  - Client connection clean-up: Once a client has been correctly served, be sure to clean up any associated resources with that client (including its connection) and make sure to return its thread to the threadpool.
  - Management of the connections from the client and the associated requests that it makes to the 3rd party vendors.
2.  Server will get the vendor addresses from a file with line separated strings in the form <ip:port>
3.  Your server should be able to accept two command-line parameters: the `address` on which it is going to expose its service and the maximum number of `threads` that its threadpool should have.

### Given to You
  1. run_tests.cc - This will simulate real world users sending concurrent product queries.
  2. client.cc - This provides the ability to connect to the store as a user.
  3. vendor.cc - This acts as the server providing bids for different products. Multiple instances of it will be run listening on different ip address and ports.
  4. `Two .proto files`  
    - store.proto - Communications protocol between user (client) and store (server)  
    - vendor.proto -Communications protocol between store (client) and vendor (server)  

### How To Run The Test Setup
  - Go to the test directory and run the `make` command
  - Two binaries will be created: `run_tests` and `run_vendors`
  - First run the command `./run_vendors ../src/vendor_addresses.txt &` to start a background process which will run multiple servers on different threads listening to (ip_address:ports) from the file given as command line argument.
  - Then start up your store which will read the same address file to determine the vendors' listening addresses. Also, your store should start listening on a port (the clients connect to) given as command line argument.
  - Then finally run the command `./run_tests $IP_and_port_on_which_store_is_listening $max_num_concurrent_client_requests` to start a process which will simulate real world clients sending requests at the same time.
  - This process reads queries from the file `product_query_list.txt`
  - It will send some queries and print back the results, which you can use to verify your whole system's flow.

### Grading
This project is not performance oriented, we will only test the functionality and correctness.  
The Rubric will be:
  - `Async client of your store` - 3 pts.
        - If you do 'Sync client', you only get 1.5 pts.
  - 'Async server of your store' - 3 pts.
    - If you do 'Sync server', you only get 1.5 pts.
  - `Threadpool management` - 4 pts.
  
### Deliverables
Please follow the instructions carefully. The folder you hand in MUST contain the following:
  - `README.md` - text file containing anything about the project that you want to tell the TAs.
  - `CMakeLists.txt` - You might need to change this if you add more source code files.
  - `Store source files (store.cc, ...)` - This contains the source code for store management. If you have added more source files to your implementation, you must include these as well. Ensure all these files compile using the CMakeLists.txt you provided.
  - `Threadpool source files (threadpool.h, ...` - This contains the source code for threadpool management
  - You can add supporting files as well (in addition to above two) if you want to keep your code more structured, clean etc.

**Directory Structure:**

**IMPORTANT: You MUST follow this directory structure for your submission. If you fail to complete this step we will deduct 3 points from your final score**

    FirstName_LastName_p3/
      README.md
      src/CMakeLists.txt
      src/store.cc
      src/threadpool.h
      src/any_additional_supporting_files.*

**Zip** the folder into **FirstName_LastName.zip** using the `zip` utility and submit via Canvas.

# Addendum: Keeping your code up to date
Although you are going to submit your solution through Canvas, we recommend creating a branch and developing your code on that branch:

`git checkout -b develop`

(assuming develop is the name of your branch)

Should the TAs need to push out an update to the assignment, you can commit (or stash if you are more comfortable with git) the changes that are unsaved in your repository:

`git commit -am "<some funny message>"`

Then update the master branch from remote:

`git pull origin master`

This updates your local copy of the master branch. Now try to merge the master branch into your development branch:

`git merge master`

(assuming that you are on your development branch)

There are likely to be merge conflicts during this step. If so, first check what files are in conflict:

`git status`

The files in conflict are the ones that are "Not staged for commit". Open these files using your favourite editor and look for lines containing `<<<<` and `>>>>`. Resolve conflicts as seems best (ask a TA if you are confused!) and then save the file. Once you have resolved all conflicts, stage the files that were in conflict:

`git add -A .`

Finally, commit the new updates to your branch and continue developing:

`git commit -am "<I did it>"`
