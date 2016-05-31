static void CVE_2009_0859_linux2_6_26_shm_get_stat(struct ipc_namespace *ns, unsigned long *rss,
		unsigned long *swp)
{
	int next_id;
	int total, in_use;

	*rss = 0;
	*swp = 0;

	in_use = shm_ids(ns).in_use;

	for (total = 0, next_id = 0; total < in_use; next_id++) {
		struct shmid_kernel *shp;
		struct inode *inode;

		shp = idr_find(&shm_ids(ns).ipcs_idr, next_id);
		if (shp == NULL)
			continue;

		inode = shp->shm_file->f_path.dentry->d_inode;

		if (is_file_hugepages(shp->shm_file)) {
			struct address_space *mapping = inode->i_mapping;
			*rss += (HPAGE_SIZE/PAGE_SIZE)*mapping->nrpages;
		} else {
			struct shmem_inode_info *info = SHMEM_I(inode);
			spin_lock(&info->lock);
			*rss += inode->i_mapping->nrpages;
			*swp += info->swapped;
			spin_unlock(&info->lock);
		}

		total++;
	}
}