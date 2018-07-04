/* vfreemem.c - vfreemem */

#include <conf.h>
#include <kernel.h>
#include <mem.h>
#include <proc.h>

extern struct pentry proctab[];
/*------------------------------------------------------------------------
 *  vfreemem  --  free a virtual memory block, returning it to vmemlist
 *------------------------------------------------------------------------
 */
SYSCALL	vfreemem(block, size)
	struct	mblock	*block;
	unsigned size;
{
	STATWORD ps;    
	struct	mblock	*p, *q;
	unsigned top;
	struct pentry *pptr;
	
	size = (unsigned)roundmb(size);
	disable(ps);
	if (size==0 || ((unsigned)block)<((unsigned) &end) || (((unsigned)block+size) > (proctab[currpid].vmemlist)->mnext) ){
		restore(ps);
		return(SYSERR);
	}
	else if((((unsigned)block+size) == (proctab[currpid].vmemlist)->mnext)){
		proctab[currpid].vmemlist->mnext = (unsigned)block;
		proctab[currpid].vmemlist->mlen += size; 
		restore(ps);
		return OK;
	}
	else{
		proctab[currpid].vmemlist->mnext = proctab[currpid].vmemlist->mnext - size;
		proctab[currpid].vmemlist->mlen += size; 
		restore(ps);
		return OK;
	}
}
