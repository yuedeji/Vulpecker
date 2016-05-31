
static int
CVE_2011_1171_linux2_6_12_6_do_ipt_get_ctl(struct sock *sk, int cmd, void __user *user, int *len)
{
	int ret;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	switch (cmd) {
	case IPT_SO_GET_INFO: {
		char name[IPT_TABLE_MAXNAMELEN];
		struct ipt_table *t;

		if (*len != sizeof(struct ipt_getinfo)) {
			duprintf("length %u != %u\n", *len,
				 sizeof(struct ipt_getinfo));
			ret = -EINVAL;
			break;
		}

		if (copy_from_user(name, user, sizeof(name)) != 0) {
			ret = -EFAULT;
			break;
		}
		name[IPT_TABLE_MAXNAMELEN-1] = '\0';

		t = try_then_request_module(find_table_lock(name),
					    "iptable_%s", name);
		if (t && !IS_ERR(t)) {
			struct ipt_getinfo info;

			info.valid_hooks = t->valid_hooks;
			memcpy(info.hook_entry, t->private->hook_entry,
			       sizeof(info.hook_entry));
			memcpy(info.underflow, t->private->underflow,
			       sizeof(info.underflow));
			info.num_entries = t->private->number;
			info.size = t->private->size;
			memcpy(info.name, name, sizeof(info.name));

			if (copy_to_user(user, &info, *len) != 0)
				ret = -EFAULT;
			else
				ret = 0;
			up(&ipt_mutex);
			module_put(t->me);
		} else
			ret = t ? PTR_ERR(t) : -ENOENT;
	}
	break;

	case IPT_SO_GET_ENTRIES: {
		struct ipt_get_entries get;

		if (*len < sizeof(get)) {
			duprintf("get_entries: %u < %u\n", *len, sizeof(get));
			ret = -EINVAL;
		} else if (copy_from_user(&get, user, sizeof(get)) != 0) {
			ret = -EFAULT;
		} else if (*len != sizeof(struct ipt_get_entries) + get.size) {
			duprintf("get_entries: %u != %u\n", *len,
				 sizeof(struct ipt_get_entries) + get.size);
			ret = -EINVAL;
		} else
			ret = get_entries(&get, user);
		break;
	}

	case IPT_SO_GET_REVISION_MATCH:
	case IPT_SO_GET_REVISION_TARGET: {
		struct ipt_get_revision rev;
		int (*revfn)(const char *, u8, int *);

		if (*len != sizeof(rev)) {
			ret = -EINVAL;
			break;
		}
		if (copy_from_user(&rev, user, sizeof(rev)) != 0) {
			ret = -EFAULT;
			break;
		}

		if (cmd == IPT_SO_GET_REVISION_TARGET)
			revfn = target_revfn;
		else
			revfn = match_revfn;

		try_then_request_module(find_revision(rev.name, rev.revision,
						      revfn, &ret),
					"ipt_%s", rev.name);
		break;
	}

	default:
		duprintf("CVE_2011_1171_linux2_6_12_6_do_ipt_get_ctl: unknown request %i\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}