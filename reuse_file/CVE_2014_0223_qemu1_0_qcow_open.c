
static int CVE_2014_0223_qemu1_0_qcow_open(BlockDriverState *bs, int flags)
{
    BDRVQcowState *s = bs->opaque;
    int len, i, shift;
    QCowHeader header;

    if (bdrv_pread(bs->file, 0, &header, sizeof(header)) != sizeof(header))
        goto fail;
    be32_to_cpus(&header.magic);
    be32_to_cpus(&header.version);
    be64_to_cpus(&header.backing_file_offset);
    be32_to_cpus(&header.backing_file_size);
    be32_to_cpus(&header.mtime);
    be64_to_cpus(&header.size);
    be32_to_cpus(&header.crypt_method);
    be64_to_cpus(&header.l1_table_offset);

    if (header.magic != QCOW_MAGIC || header.version != QCOW_VERSION)
        goto fail;
    if (header.size <= 1 || header.cluster_bits < 9)
        goto fail;
    if (header.crypt_method > QCOW_CRYPT_AES)
        goto fail;
    s->crypt_method_header = header.crypt_method;
    if (s->crypt_method_header)
        bs->encrypted = 1;
    s->cluster_bits = header.cluster_bits;
    s->cluster_size = 1 << s->cluster_bits;
    s->cluster_sectors = 1 << (s->cluster_bits - 9);
    s->l2_bits = header.l2_bits;
    s->l2_size = 1 << s->l2_bits;
    bs->total_sectors = header.size / 512;
    s->cluster_offset_mask = (1LL << (63 - s->cluster_bits)) - 1;

    /* read the level 1 table */
    shift = s->cluster_bits + s->l2_bits;
    s->l1_size = (header.size + (1LL << shift) - 1) >> shift;

    s->l1_table_offset = header.l1_table_offset;
    s->l1_table = g_malloc(s->l1_size * sizeof(uint64_t));
    if (!s->l1_table)
        goto fail;
    if (bdrv_pread(bs->file, s->l1_table_offset, s->l1_table, s->l1_size * sizeof(uint64_t)) !=
        s->l1_size * sizeof(uint64_t))
        goto fail;
    for(i = 0;i < s->l1_size; i++) {
        be64_to_cpus(&s->l1_table[i]);
    }
    /* alloc L2 cache */
    s->l2_cache = g_malloc(s->l2_size * L2_CACHE_SIZE * sizeof(uint64_t));
    if (!s->l2_cache)
        goto fail;
    s->cluster_cache = g_malloc(s->cluster_size);
    if (!s->cluster_cache)
        goto fail;
    s->cluster_data = g_malloc(s->cluster_size);
    if (!s->cluster_data)
        goto fail;
    s->cluster_cache_offset = -1;

    /* read the backing file name */
    if (header.backing_file_offset != 0) {
        len = header.backing_file_size;
        if (len > 1023)
            len = 1023;
        if (bdrv_pread(bs->file, header.backing_file_offset, bs->backing_file, len) != len)
            goto fail;
        bs->backing_file[len] = '\0';
    }

    /* Disable migration when qcow images are used */
    error_set(&s->migration_blocker,
              QERR_BLOCK_FORMAT_FEATURE_NOT_SUPPORTED,
              "qcow", bs->device_name, "live migration");
    migrate_add_blocker(s->migration_blocker);

    qemu_co_mutex_init(&s->lock);
    return 0;

 fail:
    g_free(s->l1_table);
    g_free(s->l2_cache);
    g_free(s->cluster_cache);
    g_free(s->cluster_data);
    return -1;
}