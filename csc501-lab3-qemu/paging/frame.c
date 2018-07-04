/* frame.c - manage physical frames */
#include <conf.h>
#include <kernel.h>
#include <stdio.h>
#include <proc.h>
#include <paging.h>

/*-------------------------------------------------------------------------
 * init_frm - initialize frm_tab
 *-------------------------------------------------------------------------
 */
 
fr_map_t frm_tab[NFRAMES];
frame_entry frame_tab[NFRAMES];
frame_count = 0;
frame_position = 0;

SYSCALL init_frm()
{
	STATWORD 	ps;
	disable(ps);
	int i = 0;
	for( i = 0; i < NFRAMES; i++){
		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_pid = i;
		frm_tab[i].fr_vpno = -1;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = FR_NONE;
		frm_tab[i].fr_dirty = 0;
		frm_tab[i].fr_count = 0;
	}
	for( i = 0; i < NFRAMES; i++){
		frame_tab[i].frame_ref = -1;
		frame_tab[i].frame_frame_no = -1;
	}
	restore(ps);
	return OK;
}

/*-------------------------------------------------------------------------
 * get_frm - get a free frame according page replacement policy
 *-------------------------------------------------------------------------
 */
SYSCALL get_frm(int* avail)
{
  STATWORD 	ps;
  disable(ps);
  int i = 0, j = 0;
  for(i = 0; i < NFRAMES; i++){
		if(frm_tab[i].fr_status == FRM_UNMAPPED){
			*avail = i;
			insert_frame(i);
			break;
		}
		j++;
  }
  if(j == 1024){
	  if(grpolicy() == SC){
		  int d = sc_replace();
		  if(d == SYSERR){
			  restore(ps);
			  return SYSERR;
		  }
		  else{
			  *avail = d;
			  insert_frame(d);
			  kprintf("\nframe: %d\n",d + 1024);
		  }
	  }
	  else if(grpolicy() == LFU){
		  int d = lfu_replace();
		  if(d == SYSERR){
			  restore(ps);
			  return SYSERR;
		  }
		  else{
			  *avail = d;
			  insert_frame(d);
			  kprintf("\nframe: %d\n",d + 1024);
		  }
	   }
  }
  restore(ps);
  return OK;
}

/*-------------------------------------------------------------------------
 * free_frm - free a frame 
 *-------------------------------------------------------------------------
 */
SYSCALL free_frm(int i)
{
	STATWORD 	ps;
	disable(ps);
	int frame_no_pid, back_store_id, page_id;
	unsigned long vaddr, pdbr;
	virt_addr_t virtual;
	pd_t *glb_pd;
	pt_t *glb_pt;
	if(frm_tab[i].fr_type == FR_PAGE){
		vaddr = frm_tab[i].fr_vpno;
		frame_no_pid = frm_tab[i].fr_pid;
		pdbr = proctab[frame_no_pid].pdbr;
		unsigned long cr22 = (vaddr) * 4096;
		virt_addr_t virtual;
		virtual.pg_offset = ((cr22 & 0x00000fff));
		virtual.pt_offset = ((cr22 & 0x003ff000) >> 12);
		virtual.pd_offset =	((cr22 & 0xffc00000) >> 22);
		glb_pd = (pd_t *)(pdbr + virtual.pd_offset * 4);
		glb_pt = (pt_t *)((glb_pd->pd_base * NBPG) + (virtual.pt_offset * 4));
		if(bsm_lookup(frame_no_pid, vaddr * 4096, &back_store_id, &page_id) != SYSERR){
			write_bs((i + 1024) * 4096, back_store_id, page_id);
		}
		frm_tab[i].fr_status = FRM_UNMAPPED;
		frm_tab[i].fr_pid = -1;
		frm_tab[i].fr_vpno = -1;
		frm_tab[i].fr_refcnt = 0;
		frm_tab[i].fr_type = FR_NONE;
		frm_tab[i].fr_count = 0;
		delete_frame(i);
		glb_pt->pt_pres = 0;
		glb_pt->pt_write = 1;
		glb_pt->pt_user = 0;
		glb_pt->pt_pwt = 0;
		glb_pt->pt_pcd = 0;
		glb_pt->pt_acc = 0;
		glb_pt->pt_dirty = 0;
		glb_pt->pt_mbz = 0;
		glb_pt->pt_global = 0;
		glb_pt->pt_avail = 0;
		glb_pt->pt_base = -1;
		frm_tab[glb_pd->pd_base - 1024].fr_refcnt -= 1;
		if(frm_tab[glb_pd->pd_base - 1024].fr_refcnt == 0){
			frm_tab[glb_pd->pd_base - 1024].fr_status = FRM_UNMAPPED;
			frm_tab[glb_pd->pd_base - 1024].fr_pid = -1;
			frm_tab[glb_pd->pd_base - 1024].fr_vpno = -1;
			frm_tab[glb_pd->pd_base - 1024].fr_type = FR_NONE;
			frm_tab[glb_pd->pd_base - 1024].fr_count = 0;
			delete_frame(glb_pd->pd_base - 1024);
			glb_pd->pd_pres = 0;
			glb_pd->pd_write = 1;
			glb_pd->pd_user = 0;
			glb_pd->pd_pwt = 0;
			glb_pd->pd_pcd = 0;
			glb_pd->pd_acc = 0;
			glb_pd->pd_mbz = 0;
			glb_pd->pd_fmb = 0;
			glb_pd->pd_global = 0;
			glb_pd->pd_avail = 0;
			glb_pd->pd_base = -1;
		}
		restore(ps);
		return OK;
	}
	else{
		restore(ps);
		return SYSERR;
	}
}

int sc_replace(){
	while(1 > 0){
		if(frm_tab[frame_tab[frame_position].frame_frame_no].fr_type == FR_PAGE){
			unsigned long cr22 = (frm_tab[frame_tab[frame_position].frame_frame_no].fr_vpno) * 4096;
			virt_addr_t virtual;
			virtual.pg_offset = ((cr22 & 0x00000fff));
			virtual.pt_offset = ((cr22 & 0x003ff000) >> 12);
			virtual.pd_offset =	((cr22 & 0xffc00000) >> 22);
			pd_t *glb_ptt = (pd_t *)((proctab[frm_tab[frame_tab[frame_position].frame_frame_no].fr_pid].pdbr) + (4 * virtual.pd_offset));
			if(glb_ptt->pd_pres == 1){
				pt_t *glb_pt = 	(pt_t *)((glb_ptt->pd_base * 4096) + (4 * virtual.pt_offset));
				if(glb_pt->pt_pres == 1){
					if(glb_pt->pt_acc == 1){
						glb_pt->pt_acc = 0;
						frame_position++;
						if(frame_position == frame_count)	frame_position = 0;
					}
					else{

						int frame = frame_tab[frame_position].frame_frame_no;
						if(frame_position == frame_count)	frame_position = 0;
						free_frm(frame);
						return frame;
					}
				}
			}
		}
		else{
			frame_position++;
			if(frame_position == frame_count)	frame_position = 0;
		}
	}
}

void insert_frame(int i){
	frame_tab[frame_count].frame_ref = 1;
	frame_tab[frame_count].frame_frame_no = i;
	frame_count++;
}

void delete_frame(int value){
	int i = 0, j = 0;
	for(i = 0; i < frame_count; i++){
		if(frame_tab[i].frame_frame_no == value){
			break;
		}
	}
	if(frame_position > i){
		if(frame_position == 0){
			frame_position = frame_count - 1;
		}
		else{
			frame_position -= 1;
		}
	}
	for(j = i + 1; j < frame_count; j++){
		frame_tab[j - 1].frame_ref = frame_tab[j].frame_ref;
		frame_tab[j - 1].frame_frame_no = frame_tab[j].frame_frame_no;
	}
	frame_count--;
}

int lfu_replace(){
	int pp = 0, frame_no = -1;
	int max_vpno = -100000, max_count = -100000;
	for(pp = 0; pp < 1024; pp++){
		if(frm_tab[pp].fr_status == FRM_MAPPED && frm_tab[pp].fr_type == FR_PAGE){
			if(frm_tab[pp].fr_count >= max_count){
				if(frm_tab[pp].fr_vpno >= max_vpno){
					max_count = frm_tab[pp].fr_count;
					max_vpno = frm_tab[pp].fr_vpno;
					frame_no = pp;
				}
			}
		}
	}
	return frame_no;
}



