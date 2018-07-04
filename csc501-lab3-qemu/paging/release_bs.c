#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

SYSCALL release_bs(bsd_t bs_id) {
	STATWORD 	ps;
	disable(ps);
	
	if(bsm_tab[bs_id].bs_priv == 1){
		if(bsm_tab[bs_id].bs_pid == currpid){
			write_back_frame(currpid, bs_id);
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
			if(bsm_tab[bs_id].bs_share_id[i] == currpid){
				j = 1;
				break;
			}
		}
		if(j == 1){
			if(bsm_tab[bs_id].bs_share_no == 1){
				kprintf("Value qqq");
				//write_back_frame(currpid, bs_id);
				free_bsm(bs_id);
				restore(ps);
				return OK;
			}
			else{
				write_back_frame(currpid, bs_id);
				int k = 0, l =0;
				for(k = 0; k < bsm_tab[bs_id].bs_share_no; k++){
					if(bsm_tab[bs_id].bs_share_id[k] == currpid){
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
				for(k = 0; k < proctab[currpid].bs_count; k++){
					if(proctab[currpid].bs_id[k] == bs_id){
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
		}
		else{
			restore(ps);
			return SYSERR;
		}
	}
}

void write_back_frame(int pid, int bs_id){
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

