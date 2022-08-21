#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

#include "opt-A1.h"
#include <mips/trapframe.h>
#include <clock.h>

#include "opt-A3.h"
#include "test.h"
#include "vfs.h"
#include <kern/fcntl.h>
#include <vm.h>




  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

#if OPT_A1
  while (array_num(p->p_children)) {
	  struct proc *temp_child;
	  temp_child = (struct proc *)array_get(p->p_children, 0);
	  array_remove(p->p_children, 0);
	  spinlock_acquire(&temp_child->p_lock);
	  if (temp_child->p_exitstatus == 1) {
		  spinlock_release(&temp_child->p_lock);
		  proc_destroy(temp_child);
	  }
	  else {
		  temp_child->p_parent = NULL;
		  spinlock_release(&temp_child->p_lock);
	  }
  }
#endif /* OPT_A1 */

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

#if OPT_A1
  spinlock_acquire(&p->p_lock);
  if (p->p_parent == NULL) {
	  spinlock_release(&p->p_lock);
	  proc_destroy(p);
  }
  else {
	  p->p_exitstatus = 1;
	  p->p_exitcode = exitcode;
	  spinlock_release(&p->p_lock);
  }
#else
  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
#endif /*OPT_A1 */

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */

#if OPT_A1
	*retval = curproc->p_pid;
	return(0);

#else
	*retval = 1;
	return(0);
#endif /* OPT_A1 */
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

    if (options != 0) {
    return(EINVAL);
  }

#if OPT_A1
  struct proc *p = curproc;

  int len = array_num(p->p_children);

  struct proc *temp_child = NULL;
  for(int i = 0; i < len; i++) {
	  struct proc *tmp = (struct proc *)array_get(p->p_children, i);
	  if (tmp->p_pid == pid) {
		  temp_child = tmp;
		  array_remove(p->p_children, i);
		  break;
	  }
  }
  if (temp_child == NULL) {
  	return(ESRCH);
  }

  spinlock_acquire (&temp_child->p_lock);

  while (!temp_child->p_exitstatus) {
	  spinlock_release (&temp_child->p_lock);
	  clocksleep (1);
	  spinlock_acquire (&temp_child->p_lock);

  }

  spinlock_release (&temp_child->p_lock);

  exitstatus = temp_child->p_exitcode;

  proc_destroy(temp_child);

  exitstatus = _MKWAIT_EXIT(exitstatus);

#else

  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;

#endif /* OPT_A1 */

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A1
int
sys_fork(pid_t *retval, struct trapframe *tf) {

	// create child proc structure
	struct proc *child = proc_create_runprogram("child");

	// sys_exit
	child->p_parent = curproc;

	unsigned int data1 = 0;
	array_add(child->p_parent->p_children, child, &data1);
	
	// copy addrspace of curproc to child process
	as_copy(curproc_getas(), &child->p_addrspace);
	
	// create trapframe for child and copy trapframe sent in
	struct trapframe *trapframe_for_child;
	trapframe_for_child = kmalloc(sizeof(struct trapframe));
	*trapframe_for_child = *tf;

	// create new thread
	thread_fork("child_thread", child, enter_forked_process, trapframe_for_child, 0);

	// return p_pid of child
	*retval = child->p_pid;
	
	clocksleep(1);
	return(0);
}
#endif /* OPT_A1 */


#if OPT_A3

char **
args_alloc() 
{
	char **args = kmalloc(sizeof(char *)*17);
	for (int i = 0; i <= 16; ++i) {
		args[i] = kmalloc(sizeof(char)*129);
	}
	return args;
}

void
args_free(char **args) {
	for (int i = 0; i <= 16; ++i) {
		kfree(args[i]);
	}
	kfree(args);
}

int
argcopy_in(char **argv, char **args) {
	char **tmp = argv;
	int i = 0;
	while (*tmp != NULL) {
		int len = strlen(*tmp)+1;
		copyinstr((userptr_t)*tmp, args[i], len, NULL);
		++i;
		++tmp;
	}
	return i;
}

int
sys_execv(char *progname, char **argv)
{
  	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Allocate arguments array and copy in arguments */
	char **args = args_alloc();
	int nargs = argcopy_in(argv, args);

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* Save old address space */
	struct addrspace *old = curproc_getas();

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Destroy old address space */
	as_destroy(old);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

	vaddr_t argv2[nargs+1];
	stackptr = ROUNDUP(stackptr, 4);
	argv2[nargs] = stackptr;

	for (int i = nargs-1; i >= 0; --i) {
		argv2[i] = argcopy_out(&stackptr, args[i]);
	}

	/* Free arguments array */
	args_free(args);

	for (int i = nargs; i >= 0; --i) {
		stackptr -= sizeof(vaddr_t);
		if (i == nargs) {
			copyout((void *)NULL, (userptr_t)stackptr, (size_t)4);
		}
		else {
			copyout(&argv2[i], (userptr_t)stackptr, sizeof(vaddr_t));
		}
	}

	/* Warp to user mode. */
	enter_new_process(nargs, (userptr_t)stackptr /*userspace addr of argv*/,
				stackptr, entrypoint);

	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}
#endif /* OPT_A3 */
