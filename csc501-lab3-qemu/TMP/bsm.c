/* bsm.c - manage the backing store mapping*/

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>

/*-------------------------------------------------------------------------
 * init_bsm- initialize bsm_tab
 *-------------------------------------------------------------------------
 */
 
bs_map_t bsm_tab[NO_BSM];
SYSCALL init_bsm()
{
	STATWORD 	ps;
	disable(ps);
	 int i = 0;
	 for( i = 0; i < NO_BSM; i++){
		 bsm_tab[i].bs_id = i;
		 bsm_tab[i].bs_status = BSM_UNMAPPED;
		 bsm_tab[i].bs_pid = 1;
		 bsm_tab[i].bs_vpno = 4096;
		 bsm_tab[i].bs_npages = 128;
		 bsm_tab[i].bs_sem = 1;
		 bsm_tab[i].bs_priv = 0;
		 bsm_tab[i].bs_share_no = 0;
		 int j = 0;
		 for(j = 0; j < NPROC; j++){
			 bsm_tab[i].bs_share_id[j] = -1;
			 bsm_tab[i].bs_share_npages[j] = 0; 
			 bsm_tab[i].bs_share_vpno[j] = -1;
		 }
	 }
	 restore(ps);
	 return OK;
}

/*-------------------------------------------------------------------------
 * get_bsm - get a free entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL get_bsm(int* avail)
{
	STATWORD 	ps;
	disable(ps);
	int i = 0;
	for(i = 0; i < 16; i++){
		if(bsm_tab[i].bs_status == BSM_UNMAPPED){
			*avail = i;
			restore(ps);
			return OK;
		}
	}
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * free_bsm - free an entry from bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL free_bsm(int i)
{
	STATWORD 	ps;
	disable(ps);
	bsm_tab[i].bs_id = i;
	bsm_tab[i].bs_status = BSM_UNMAPPED;
	bsm_tab[i].bs_pid = 1;
	bsm_tab[i].bs_vpno = 4096;
	bsm_tab[i].bs_npages = 128;
	bsm_tab[i].bs_sem = 1;
	bsm_tab[i].bs_priv = 0;
	bsm_tab[i].bs_share_no = 0;
	int j = 0;
	for(j = 0; j < NPROC; j++){
		bsm_tab[i].bs_share_id[j] = -1;
		bsm_tab[i].bs_share_npages[j] = 0; 
		bsm_tab[i].bs_share_vpno[j] = -1;
	}
	int k = 0, l = 0;
	for(k = 0; k < proctab[currpid].bs_count; k++){
		if(proctab[currpid].bs_id[k] == i){
			break;
		}
	}
	for(l = k + 1; l < proctab[currpid].bs_count; l++){
		proctab[currpid].bs_id[l - 1] = proctab[currpid].bs_id[l];
		proctab[currpid].bs_type[l - 1] = proctab[currpid].bs_type[l];
		proctab[currpid].bs_npages[l - 1] = proctab[currpid].bs_npages[l];
	}
	proctab[currpid].bs_count -= 1;
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * bsm_lookup - lookup bsm_tab and find the corresponding entry
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_lookup(int pid, long vaddr, int* store, int* pageth)
{
	STATWORD 	ps;
	disable(ps);
	int i = 0;
	for(i = 0; i < proctab[pid].bs_count; i++){
		int j = 0;
		if(bsm_tab[proctab[pid].bs_id[i]].bs_priv == 1){
			int value = bsm_tab[proctab[pid].bs_id[i]].bs_vpno;
			int page = ((vaddr >> 12)& 0x000fffff);
			if(value <= page && (value + bsm_tab[proctab[pid].bs_id[i]].bs_npages) >= page){
				*store = proctab[pid].bs_id[i];
				*pageth = page - value;
				restore(ps);
				return OK;
			}
		}
		else{
			for( j = 0; j < bsm_tab[proctab[pid].bs_id[i]].bs_share_no; j++){
				if(bsm_tab[proctab[pid].bs_id[i]].bs_share_id[j] == pid){
					int value = bsm_tab[proctab[pid].bs_id[i]].bs_share_vpno[j]; 
					int page = ((vaddr >> 12)& 0x000fffff);
					if(value <= page && (value + bsm_tab[proctab[pid].bs_id[i]].bs_share_npages[j]) >= page){
						*store = proctab[pid].bs_id[i];
						*pageth = page - value;
						restore(ps);
						return OK;
					}
				}
			}
		}
	}
	restore(ps);
	return SYSERR;
}


/*-------------------------------------------------------------------------
 * bsm_map - add an mapping into bsm_tab 
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_map(int pid, int vpno, int source, int npages)
{
	STATWORD 	ps;
	disable(ps);
	if(bsm_tab[source].bs_status == BSM_MAPPED){
		restore(ps);
		return SYSERR;
	}
	else{
		bsm_tab[source].bs_status = BSM_MAPPED;
		bsm_tab[source].bs_pid = pid;
		bsm_tab[source].bs_vpno = vpno;
		bsm_tab[source].bs_npages = npages;
		restore(ps);
		return OK;
	}
}



/*-------------------------------------------------------------------------
 * bsm_unmap - delete an mapping from bsm_tab
 *-------------------------------------------------------------------------
 */
SYSCALL bsm_unmap(int pid, int vpno, int flag)
{
	STATWORD 	ps;
	disable(ps);
	
	int u = 0, bs_id, page;
	for(u = 0; u < proctab[pid].bs_count; u++){
		if( bsm_lookup(pid, vpno * 4096, &bs_id, &page) == OK)	break;
	}
	
	if(bsm_tab[bs_id].bs_status == BSM_UNMAPPED){
		restore(ps);
		return SYSERR;
	}
	if(bsm_tab[bs_id].bs_priv == 1){
		if(bsm_tab[bs_id].bs_pid == pid){
			write_back_frames(pid, vpno, bs_id, page);
			free_bsm(bs_id);
			restore(ps);
			return OK;
		}
		else{
			restore(ps);
			return SYSERR;
		}
	}
	else{
		int i = 0, j = 0;
		for(i = 0; i < bsm_tab[bs_id].bs_share_no; i++){
			if(bsm_tab[bs_id].bs_share_id[i] == pid){
				j = 1;
				break;
			}
		}
		if(j == 1){
			if(bsm_tab[bs_id].bs_share_no == 1){
				write_back_frames(pid, vpno, bs_id, page);
				free_bsm(bs_id);
				restore(ps);
				return OK;
			}
			else{
				write_back_frames(pid, vpno, bs_id, page);
				int k = 0, l =0;
				for(k = 0; k < bsm_tab[bs_id].bs_share_no; k++){
					if(bsm_tab[bs_id].bs_share_id[k] == pid){
						break;
					}
				}
				for(l = k + 1; l < bsm_tab[bs_id].bs_share_no; l++){
					bsm_tab[bs_id].bs_share_id[l - 1] = bsm_tab[bs_id].bs_share_id[l];
					bsm_tab[bs_id].bs_share_npages[l - 1] = bsm_tab[bs_id].bs_share_npages[l];
					bsm_tab[bs_id].bs_share_vpno[l - 1] = bsm_tab[bs_id].bs_share_vpno[l];
				}
				bsm_tab[bs_id].bs_share_no -= 1;
				int max = -1000000;
				for(k = 0; k < bsm_tab[bs_id].bs_share_no; k++){
					if(max < bsm_tab[bs_id].bs_share_npages[k]){
						max = bsm_tab[bs_id].bs_share_npages[k];
					}
				}
				bsm_tab[bs_id].bs_npages = max;
				for(k = 0; k < proctab[pid].bs_count; k++){
					if(proctab[pid].bs_id[k] == bs_id){
						break;
					}
				}
				for(l = k + 1; l < proctab[pid].bs_count; l++){
					proctab[pid].bs_id[l - 1] = proctab[pid].bs_id[l];
					proctab[pid].bs_type[l - 1] = proctab[pid].bs_type[l];
					proctab[pid].bs_npages[l - 1] = proctab[pid].bs_npages[l];
				}
				proctab[pid].bs_count -= 1;
				restore(ps);
				return OK;
			}
		}
		else{
			restore(ps);
			return SYSERR;
		}
	}
}

void write_back_frames(int pid, int vpno, int bs_id, int page){
	int k = 0, back_store_id, page_id;
	for(k = 0; k < 1024; k++){
		if(frm_tab[k].fr_pid == pid && frm_tab[k].fr_type == FR_PAGE){
			if(bsm_lookup(pid, frm_tab[k].fr_vpno * 4096, &back_store_id, &page_id) != SYSERR){
				if(back_store_id == bs_id){
					free_frm(k);
				}
			}
		}
	}
	for(k = 0; k < 1024; k++){
		if(frm_tab[k].fr_pid == pid && frm_tab[k].fr_type == FR_TBL && bsm_tab[bs_id].bs_share_no > 1){
			unsigned long vaddr, pdbr;
			int frame_no_pid;
			pd_t *glb_pd;
			pt_t *glb_pt;
			vaddr = frm_tab[k].fr_vpno;
			frame_no_pid = frm_tab[k].fr_pid;
			pdbr = proctab[frame_no_pid].pdbr;
			unsigned long cr22 = (frm_tab[k].fr_vpno) * 4096;
			virt_addr_t virtual;
			virtual.pg_offset = ((cr22 & 0x00000fff));
			virtual.pt_offset = ((cr22 & 0x003ff000) >> 12);
			virtual.pd_offset =	((cr22 & 0xffc00000) >> 22);
			glb_pd = (pd_t *)(pdbr + (virtual.pd_offset * 4));
			glb_pt = (pt_t *)((glb_pd->pd_base * NBPG) + (virtual.pt_offset * 4));
			frm_tab[k].fr_status = FRM_UNMAPPED;
			frm_tab[k].fr_pid = -1;
			frm_tab[k].fr_vpno = -1;
			frm_tab[k].fr_type = FR_NONE;
			frm_tab[k].fr_count = 0;
			frm_tab[k].fr_refcnt = 0;
			delete_frame(k);
		}
	}
}


