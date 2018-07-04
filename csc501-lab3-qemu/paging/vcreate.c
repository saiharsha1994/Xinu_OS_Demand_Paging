/* vcreate.c - vcreate */
    
#include <conf.h>
#include <i386.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <paging.h>

/*
static unsigned long esp;
*/

LOCAL	newpid();
/*------------------------------------------------------------------------
 *  create  -  create a process to start running a procedure
 *------------------------------------------------------------------------
 */
SYSCALL vcreate(procaddr,ssize,hsize,priority,name,nargs,args)
	int	*procaddr;		/* procedure address		*/
	int	ssize;			/* stack size in words		*/
	int	hsize;			/* virtual heap size in pages	*/
	int	priority;		/* process priority > 0		*/
	char	*name;			/* name (for debugging)		*/
	int	nargs;			/* number of args that follow	*/
	long	args;			/* arguments (treated like an	*/
					/* array in the code)		*/
{
	STATWORD 	ps;
	disable(ps);
	struct	pentry	*pptr1;
	int avil;
	int back = get_bsm(&avil);
	if(back == SYSERR){
		restore(ps);
		return SYSERR;
	}
	int procid = create(procaddr,ssize,priority,name,nargs,args);
	pptr1 = &proctab[procid];
	pptr1->store = avil;
	pptr1->vhpno = 4096;
	pptr1->vhpnpages = hsize;
	pptr1->vmemlist->mnext = 4096*4096;
	pptr1->vmemlist->mlen = hsize * 4096;
	pptr1->bs_id[pptr1->bs_count] = avil;
	pptr1->bs_type[pptr1->bs_count] = 1;
	pptr1->bs_npages[pptr1->bs_count] = hsize;
	pptr1->bs_count++;
		int back1 = bsm_map(procid, pptr1->vhpno, pptr1->store, pptr1->vhpnpages);
		if(back1 == SYSERR){
			restore(ps);
			return SYSERR;
		}
	bsm_tab[avil].bs_priv = 1;
	restore(ps);
	return procid;
}

/*------------------------------------------------------------------------
 * newpid  --  obtain a new (free) process id
 *------------------------------------------------------------------------
 */
LOCAL	newpid()
{
	int	pid;			/* process id to return		*/
	int	i;

	for (i=0 ; i<NPROC ; i++) {	/* check all NPROC slots	*/
		if ( (pid=nextproc--) <= 0)
			nextproc = NPROC-1;
		if (proctab[pid].pstate == PRFREE)
			return(pid);
	}
	return(SYSERR);
}
