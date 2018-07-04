#include <conf.h>
#include <kernel.h>
#include <proc.h>
#include <paging.h>

int get_bs(bsd_t bs_id, unsigned int npages) {

  /* requests a new mapping of npages with ID map_id */
    STATWORD 	ps;
	disable(ps);
	int pages;
	if(bsm_tab[bs_id].bs_status == BSM_MAPPED && bsm_tab[bs_id].bs_priv == 1){
		restore(ps);
		return SYSERR;
	}
	else if(npages > 128 || npages <= 0){
		restore(ps);
		return SYSERR;
	}
	else if(bsm_tab[bs_id].bs_status == BSM_MAPPED){
		bsm_tab[bs_id].bs_share_id[bsm_tab[bs_id].bs_share_no] = currpid;
		bsm_tab[bs_id].bs_share_no++;
		restore(ps);
		return bsm_tab[bs_id].bs_npages;
	}
	else{
		bsm_tab[bs_id].bs_status = BSM_MAPPED;
		bsm_tab[bs_id].bs_share_id[bsm_tab[bs_id].bs_share_no] = currpid;
		bsm_tab[bs_id].bs_npages = npages;
		bsm_tab[bs_id].bs_share_no++;
		restore(ps);
		return npages;
	}
}


