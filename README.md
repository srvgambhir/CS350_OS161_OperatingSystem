# CS350 OS161 Operating System

This is a course long project completed during CS 350 - Operating Systems

OS/161 is a simple operating system kernel. The baseline OS/161 has limited functionality. Over the course of CS 350, we added additional functionality to this baseline. These improvements include:
- Customizing boot output and adding menu commands
- Adding process related system calls (getpid, fork, _exit, waitpid, and execv)
- Adding synchronization primitizes (lo cks and condition variables)
- Expanding TLB functionality, and implementing a physical page allocator

Sys/161 is a machine simulator which emulates the physical hardware that OS/161 runs on.

To run the completed OS/161 kernel, navigate to the root directory and run the following command:

  sys161 kernel
  
Note: Sys/161 needs to be downloaded prior to running the kernel
