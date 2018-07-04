/* kill.c - kill */

#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <sem.h>
#include <mem.h>
#include <io.h>
#include <q.h>
#include <stdio.h>
#include <paging.h>

/*------------------------------------------------------------------------
 * kill  --  kill a process and remove it from the system
 *------------------------------------------------------------------------
 */
SYSCALL kill(int pid)
{
	STATWORD ps;    
	struct	pentry	*pptr;		/* points to proc. table for pid*/
	int	dev;

	disable(ps);
	if (isbadpid(pid) || (pptr= &proctab[pid])->pstate==PRFREE) {
		restore(ps);
		return(SYSERR);
	}
	int k = 0, l = 0;
	for(k = 0; k < proctab[pid].bs_count; k++){
		int s = 0, v = 0;
		if(bsm_tab[proctab[pid].bs_id[k]].bs_priv == 1){
			bsm_unmap(pid, bsm_tab[proctab[pid].bs_id[k]].bs_vpno, bsm_tab[proctab[pid].bs_id[k]].bs_priv);
		}
		else{
			for(s = 0; s < bsm_tab[proctab[pid].bs_id[k]].bs_share_no; s++){
				if(bsm_tab[proctab[pid].bs_id[k]].bs_share_id[s] == pid){
					bsm_unmap(pid, bsm_tab[proctab[pid].bs_id[k]].bs_share_vpno[s], bsm_tab[proctab[pid].bs_id[k]].bs_priv);
					break;
				}
			}
		}
	}
	int direct = -1;
	for(l = 0; l < NFRAMES; l++){
		if(frm_tab[l].fr_pid == pid && frm_tab[l].fr_type == FR_PAGE){
			free_frm(l);
		}
		else if(frm_tab[l].fr_pid == pid && frm_tab[l].fr_type == FR_DIR){
			direct = l;
		}
	}
	if(direct != -1){
		frm_tab[direct].fr_status = FRM_UNMAPPED;
		frm_tab[direct].fr_pid = -1;
		frm_tab[direct].fr_vpno = -1;
		frm_tab[direct].fr_refcnt -= 1;
		frm_tab[direct].fr_type = FR_NONE;
		delete_frame(direct);
		pd_t *glb_ptt = (pd_t *)((NFRAMES + direct) * NBPG);
		int y = 0;
		proctab[pid].pdbr = 0;
	}
	
	if (--numproc == 0)
		xdone();

	dev = pptr->pdevs[0];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->pdevs[1];
	if (! isbaddev(dev) )
		close(dev);
	dev = pptr->ppagedev;
	if (! isbaddev(dev) )
		close(dev);
	
	send(pptr->pnxtkin, pid);

	freestk(pptr->pbase, pptr->pstklen);
	switch (pptr->pstate) {

	case PRCURR:	pptr->pstate = PRFREE;	/* suicide */
			resched();

	case PRWAIT:	semaph[pptr->psem].semcnt++;

	case PRREADY:	dequeue(pid);
			pptr->pstate = PRFREE;
			break;

	case PRSLEEP:
	case PRTRECV:	unsleep(pid);
						/* fall through	*/
	default:	pptr->pstate = PRFREE;
	}
	restore(ps);
	return(OK);
}
