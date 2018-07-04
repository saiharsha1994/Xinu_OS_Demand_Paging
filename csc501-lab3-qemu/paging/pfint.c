/* pfint.c - pfint */

#include <conf.h>
#include <kernel.h>
#include <paging.h>
#include <proc.h>


/*-------------------------------------------------------------------------
 * pfint - paging fault ISR
 *-------------------------------------------------------------------------
 */
SYSCALL pfint()
{
	STATWORD 	ps;
	disable(ps);
	unsigned long cr2 = read_cr2();
	virt_addr_t virtual;
	int bs_store, bs_page;
	int a = bsm_lookup(currpid, cr2, &bs_store, &bs_page);
	if(a == SYSERR){
		kill(currpid);
		restore(ps);
		return SYSERR;
	}
	int back = shared_frame(bs_store, bs_page, cr2);
	virtual.pg_offset = ((cr2 & 0x00000fff));
	virtual.pt_offset = ((cr2 & 0x003ff000) >> 12);
	virtual.pd_offset =	((cr2 & 0xffc00000) >> 22);
	unsigned long pdbr_value = proctab[currpid].pdbr;
	pt_t *glb_pt;
	pd_t *glb_ptt = (pd_t *)(pdbr_value + (4 * virtual.pd_offset));
	int free_frame;
	if(glb_ptt->pd_pres == 1){
		frm_tab[glb_ptt->pd_base - 1024].fr_refcnt += 1;
		if(back < 0){
			get_frm(&free_frame);
			frm_tab[free_frame].fr_status = FRM_MAPPED;
			frm_tab[free_frame].fr_pid = currpid;
			frm_tab[free_frame].fr_vpno = ((cr2 >> 12)& 0x000fffff);
			frm_tab[free_frame].fr_type = FR_PAGE;
			if(frm_tab[free_frame].fr_refcnt < 0)	frm_tab[free_frame].fr_refcnt = 0;
			frm_tab[free_frame].fr_refcnt += 1;
			frm_tab[free_frame].fr_count = 1;
			read_bs((1024 + free_frame) * 4096, bs_store, bs_page);
		}
		else{
			free_frame = back;
			frm_tab[free_frame].fr_refcnt += 1;
		}
		glb_pt = (pt_t *)((glb_ptt->pd_base * 4096) + (4 * virtual.pt_offset));
		glb_pt->pt_user = 0;
		glb_pt->pt_pwt = 0;
		glb_pt->pt_pcd = 0;
		glb_pt->pt_acc = 0;
		glb_pt->pt_dirty = 0;
		glb_pt->pt_mbz = 0;
		glb_pt->pt_global = 0;
		glb_pt->pt_avail = 0;
		glb_pt->pt_pres = 1;
		glb_pt->pt_write = 1;
		glb_pt->pt_base = free_frame + 1024;
	}
	else{
		int pt_frame_no;
		get_frm(&pt_frame_no);
		frm_tab[pt_frame_no].fr_status = FRM_MAPPED;
		frm_tab[pt_frame_no].fr_pid = currpid;
		frm_tab[pt_frame_no].fr_vpno = 1024 + pt_frame_no;
		frm_tab[pt_frame_no].fr_refcnt = 0;
		frm_tab[pt_frame_no].fr_refcnt += 1;
		frm_tab[pt_frame_no].fr_type = FR_TBL;
		frm_tab[pt_frame_no].fr_count = 1;
		glb_pt = (pt_t *)((1024 + pt_frame_no) * NBPG);
		int k = 0;
		for( k  = 0; k < 1024; k++){
			glb_pt->pt_pres = 0;
			glb_pt->pt_write = 0;
			glb_pt->pt_user = 0;
			glb_pt->pt_pwt = 0;
			glb_pt->pt_pcd = 0;
			glb_pt->pt_acc = 0;
			glb_pt->pt_dirty = 0;
			glb_pt->pt_mbz = 0;
			glb_pt->pt_global = 0;
			glb_pt->pt_avail = 0;
			glb_pt->pt_base = 0;
			glb_pt++;
		}
		glb_ptt->pd_pres = 1;
		glb_ptt->pd_write = 1;
		glb_ptt->pd_user = 0;
		glb_ptt->pd_pwt = 0;
		glb_ptt->pd_pcd = 0;
		glb_ptt->pd_acc = 0;
		glb_ptt->pd_mbz = 0;
		glb_ptt->pd_fmb = 0;
		glb_ptt->pd_global = 0;
		glb_ptt->pd_avail = 0;
		glb_ptt->pd_base = 1024 + pt_frame_no;
		if(back < 0){
			get_frm(&free_frame);
			frm_tab[free_frame].fr_status = FRM_MAPPED;
			frm_tab[free_frame].fr_pid = currpid;
			frm_tab[free_frame].fr_vpno = ((cr2 >> 12)& 0x000fffff);
			frm_tab[free_frame].fr_type = FR_PAGE;
			if(frm_tab[free_frame].fr_refcnt < 0)	frm_tab[free_frame].fr_refcnt = 0;
			frm_tab[free_frame].fr_refcnt += 1;
			frm_tab[free_frame].fr_count = 1;
			read_bs((1024 + free_frame) * 4096, bs_store, bs_page);
		}
		else{
			free_frame = back;
			frm_tab[free_frame].fr_refcnt += 1;
		}
		glb_ptt = (pd_t *)(pdbr_value + (4 * virtual.pd_offset));
		glb_pt = (pt_t *)(((1024 + pt_frame_no) * 4096) + (4 * virtual.pt_offset));
		glb_pt->pt_user = 0;
		glb_pt->pt_pwt = 0;
		glb_pt->pt_pcd = 0;
		glb_pt->pt_acc = 0;
		glb_pt->pt_dirty = 0;
		glb_pt->pt_mbz = 0;
		glb_pt->pt_global = 0;
		glb_pt->pt_avail = 0;
		glb_pt->pt_pres = 1;
		glb_pt->pt_write = 1;
		glb_pt->pt_base = free_frame + 1024;
	}
	restore(ps);
	return OK;
}

int shared_frame(int bs_store, int bs_page, unsigned long cr2){
	if(bsm_tab[bs_store].bs_priv != 1){
		if(bsm_tab[bs_store].bs_share_no > 1 && (cr2 > 0x10000000)){
			int i = 0;
			for(i = 0; i < bsm_tab[bs_store].bs_share_no; i++){
				if(bsm_tab[bs_store].bs_share_id[i] != currpid){
					if(bsm_tab[bs_store].bs_share_npages[i] >= bs_page){
						unsigned long cr22 = (bsm_tab[bs_store].bs_share_vpno[i] + (bs_page)) * 4096;
						virt_addr_t virtual;
						virtual.pg_offset = ((cr22 & 0x00000fff));
						virtual.pt_offset = ((cr22 & 0x003ff000) >> 12);
						virtual.pd_offset =	((cr22 & 0xffc00000) >> 22);
						pd_t *glb_ptt = (pd_t *)((proctab[bsm_tab[bs_store].bs_share_id[i]].pdbr) + (4 * virtual.pd_offset));
						if(glb_ptt->pd_pres == 1){
							pt_t *glb_pt = 	(pt_t *)((glb_ptt->pd_base * 4096) + (4 * virtual.pt_offset));
							if(glb_pt->pt_pres == 1){
								return (glb_pt->pt_base - 1024);
							}
						}
					}
				}
			}
		}
		else{
			return -1;
		}
	}
	else{
		return -1;
	}
	return -1;
}


