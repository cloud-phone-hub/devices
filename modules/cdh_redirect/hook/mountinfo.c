// SPDX-License-Identifier: GPL-2.0
/*
 * 挂载表与 /mnt 目录项视图收敛（仅对 uid > cdh_max_uid 的普通应用生效）：
 *
 *   1) show_mountinfo / show_vfsmnt / show_vfsstat 返回钩子：
 *      - 过滤载体物理挂载点 /mnt/vendorpatch（及其子挂载）行，回退 m->count；
 *      - 在 show_mountinfo 中按每个 seq_file 会话将可选标签区（" - " 之前）的
 *        shared:<N> / master:<N> / propagate_from:<N> 双射重编号为连续正整数
 *        1..K，消除引导期临时挂载卸载及过滤载体行产生的 peer_group 断号。
 *   2) __arm64_sys_getdents64 返回钩子（ARM64）：
 *      - 当普通应用枚举根目录下的 /mnt 时，从返回的用户态 linux_dirent64 流中
 *        剔除 "vendorpatch" 条目并衔接前驱 d_off。
 *
 * 系统服务进程（init / vold / apexd 等，uid <= cdh_max_uid）在入口直接旁路，
 * 原样读取完整内核挂载表与目录项。
 */
#define pr_fmt(fmt)	"cdh_redirect: " fmt

#include <linux/dcache.h>
#include <linux/dirent.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#ifdef CONFIG_ARM64
#include <asm/memory.h>
#endif

#include "cdh_redirect.h"

#define CDH_MNT_KRETPROBE_MAXACTIVE	64
#define CDH_PEER_MAP_SLOTS		16
#define CDH_PEER_MAX_GROUPS		256

struct cdh_peer_map {
	struct seq_file *m;
	u64 seq;
	u16 count;
	u32 orig_ids[CDH_PEER_MAX_GROUPS];
};

static DEFINE_SPINLOCK(cdh_peer_lock);
static struct cdh_peer_map cdh_peer_maps[CDH_PEER_MAP_SLOTS];
static u64 cdh_peer_clock;

static void cdh_peer_map_reset(struct seq_file *m)
{
	unsigned long flags;
	int i, victim = 0;
	u64 oldest = U64_MAX;

	spin_lock_irqsave(&cdh_peer_lock, flags);
	for (i = 0; i < CDH_PEER_MAP_SLOTS; i++) {
		if (cdh_peer_maps[i].m == m) {
			victim = i;
			break;
		}
		if (!cdh_peer_maps[i].m) {
			victim = i;
			oldest = 0;
		} else if (cdh_peer_maps[i].seq < oldest) {
			oldest = cdh_peer_maps[i].seq;
			victim = i;
		}
	}
	cdh_peer_maps[victim].m = m;
	cdh_peer_maps[victim].seq = ++cdh_peer_clock;
	cdh_peer_maps[victim].count = 0;
	spin_unlock_irqrestore(&cdh_peer_lock, flags);
}

static u32 cdh_peer_map_get(struct seq_file *m, u32 orig_id)
{
	struct cdh_peer_map *slot = NULL;
	unsigned long flags;
	int i, victim = 0;
	u64 oldest = U64_MAX;
	u32 mapped;

	if (!orig_id)
		return 0;

	spin_lock_irqsave(&cdh_peer_lock, flags);
	for (i = 0; i < CDH_PEER_MAP_SLOTS; i++) {
		if (cdh_peer_maps[i].m == m) {
			slot = &cdh_peer_maps[i];
			break;
		}
		if (!cdh_peer_maps[i].m) {
			victim = i;
			oldest = 0;
		} else if (cdh_peer_maps[i].seq < oldest) {
			oldest = cdh_peer_maps[i].seq;
			victim = i;
		}
	}
	if (!slot) {
		slot = &cdh_peer_maps[victim];
		slot->m = m;
		slot->count = 0;
	}
	slot->seq = ++cdh_peer_clock;

	for (i = 0; i < slot->count; i++) {
		if (slot->orig_ids[i] == orig_id) {
			mapped = (u32)(i + 1);
			spin_unlock_irqrestore(&cdh_peer_lock, flags);
			return mapped;
		}
	}
	if (slot->count < CDH_PEER_MAX_GROUPS) {
		slot->orig_ids[slot->count] = orig_id;
		slot->count++;
		mapped = (u32)slot->count;
	} else {
		mapped = orig_id;
	}
	spin_unlock_irqrestore(&cdh_peer_lock, flags);
	return mapped;
}

/* 判断字段内容是否为 /mnt/vendorpatch 或其子路径。 */
static bool cdh_is_carrier_mp(const char *s, size_t len)
{
	if (len < CDH_CARRIER_MOUNT_LEN)
		return false;
	if (memcmp(s, CDH_CARRIER_MOUNT_POINT, CDH_CARRIER_MOUNT_LEN) != 0)
		return false;
	return len == CDH_CARRIER_MOUNT_LEN || s[CDH_CARRIER_MOUNT_LEN] == '/';
}

/* 取单行中第 idx（0 起）个空格分隔字段的起止位置。 */
static const char *cdh_nth_field(const char *line, size_t len, int idx,
				 size_t *out_flen)
{
	const char *end = line + len;
	const char *p = line;
	int cur = 0;

	while (p < end) {
		const char *fstart;

		while (p < end && *p == ' ')
			p++;
		if (p >= end || *p == '\n')
			return NULL;
		fstart = p;
		while (p < end && *p != ' ' && *p != '\n')
			p++;
		if (cur == idx) {
			*out_flen = (size_t)(p - fstart);
			return fstart;
		}
		cur++;
	}
	return NULL;
}

/* 在 [start, end) 范围内查找固定子串 pat（长 plen）。 */
static char *cdh_memfind(char *start, const char *end,
			 const char *pat, size_t plen)
{
	char *p;

	if (!plen || end < start || (size_t)(end - start) < plen)
		return NULL;
	for (p = start; p <= end - plen; p++) {
		if (!memcmp(p, pat, plen))
			return p;
	}
	return NULL;
}

static bool cdh_parse_u32(const char *s, size_t len, u32 *out)
{
	u64 val = 0;
	size_t i;

	if (!len || len > 10)
		return false;
	for (i = 0; i < len; i++) {
		if (s[i] < '0' || s[i] > '9')
			return false;
		val = val * 10 + (u64)(s[i] - '0');
		if (val > U32_MAX)
			return false;
	}
	*out = (u32)val;
	return true;
}

#define CDH_STR_LEN(s) s, (sizeof(s) - 1)

/* 将云机虚块设备源 /dev/block/vda* 按真机分区语义改写，消除 vda* 暴露。 */
static size_t cdh_map_vda_source(const char *mp, size_t mp_len,
				 const char *src, size_t src_len,
				 char *out, size_t out_cap)
{
	static const struct {
		const char *mp;
		size_t mp_len;
		const char *dev;
		size_t dev_len;
	} table[] = {
		{ CDH_STR_LEN("/cust"), CDH_STR_LEN("/dev/block/bootdevice/by-name/cust") },
		{ CDH_STR_LEN("/mnt/rescue"), CDH_STR_LEN("/dev/block/by-name/rescue") },
		{ CDH_STR_LEN("/metadata"), CDH_STR_LEN("/dev/block/sdc14") },
		{ CDH_STR_LEN("/mnt/vendor/persist"), CDH_STR_LEN("/dev/block/sdc15") },
		{ CDH_STR_LEN("/mnt/vendor/nvcfg"), CDH_STR_LEN("/dev/block/sdc6") },
		{ CDH_STR_LEN("/mnt/vendor/nvdata"), CDH_STR_LEN("/dev/block/sdc7") },
		{ CDH_STR_LEN("/mnt/vendor/protect_f"), CDH_STR_LEN("/dev/block/sdc18") },
		{ CDH_STR_LEN("/mnt/vendor/protect_s"), CDH_STR_LEN("/dev/block/sdc19") },
	};
	size_t i;
	int n;

	if (src_len < 14 || memcmp(src, "/dev/block/vda", 14) != 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(table); i++) {
		if (mp_len == table[i].mp_len &&
		    !memcmp(mp, table[i].mp, mp_len)) {
			if (table[i].dev_len >= out_cap)
				return 0;
			memcpy(out, table[i].dev, table[i].dev_len);
			out[table[i].dev_len] = '\0';
			return table[i].dev_len;
		}
	}
	n = scnprintf(out, out_cap, "/dev/block/sdc%.*s",
		      (int)(src_len - 14), src + 14);
	return n > 0 ? (size_t)n : 0;
}

/* 对齐普通 App 无权 stat 的系统/厂商挂载点 maj:min（严禁改动 /cust 等可 stat 分区）。 */
static const char *cdh_map_unstatable_dev(const char *mp, size_t mp_len,
					  size_t *out_len)
{
	static const struct {
		const char *mp;
		size_t mp_len;
		const char *dev;
		size_t dev_len;
	} table[] = {
		{ CDH_STR_LEN("/metadata"),           CDH_STR_LEN("8:46") },
		{ CDH_STR_LEN("/mnt/vendor/persist"), CDH_STR_LEN("8:47") },
		{ CDH_STR_LEN("/mnt/vendor/protect_f"), CDH_STR_LEN("259:2") },
		{ CDH_STR_LEN("/mnt/vendor/protect_s"), CDH_STR_LEN("259:3") },
		{ CDH_STR_LEN("/mnt/vendor/nvdata"),  CDH_STR_LEN("8:39") },
		{ CDH_STR_LEN("/mnt/vendor/nvcfg"),   CDH_STR_LEN("8:38") },
		{ CDH_STR_LEN("/mnt/rescue"),         CDH_STR_LEN("259:41") },
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(table); i++) {
		if (mp_len == table[i].mp_len &&
		    !memcmp(mp, table[i].mp, mp_len)) {
			*out_len = table[i].dev_len;
			return table[i].dev;
		}
	}
	return NULL;
}

static bool cdh_replace_span(struct seq_file *m, char *span,
			     size_t old_len, const char *rep, size_t new_len);

/* 对 /dev/block/dm-* 的 f2fs 挂载在 ,alloc_mode= 前补入 ,inlinecrypt（真机 metadata 加密属性）。 */
static bool cdh_inject_dm_f2fs_inlinecrypt(struct seq_file *m,
					   const char *fstype, size_t ft_len,
					   const char *src, size_t src_len,
					   const char *opts, size_t opts_len)
{
	static const char pat[] = ",alloc_mode=";
	static const char rep[] = ",inlinecrypt,alloc_mode=";
	static const char has[] = ",inlinecrypt";
	char *hit;

	if (!fstype || !src || !opts)
		return true;
	if (ft_len != 4 || memcmp(fstype, "f2fs", 4) != 0)
		return true;
	if (src_len < 14 || memcmp(src, "/dev/block/dm-", 14) != 0)
		return true;
	if (cdh_memfind((char *)opts, opts + opts_len, has, sizeof(has) - 1))
		return true;
	hit = cdh_memfind((char *)opts, opts + opts_len, pat, sizeof(pat) - 1);
	if (!hit)
		return true;
	return cdh_replace_span(m, hit, sizeof(pat) - 1, rep, sizeof(rep) - 1);
}

static bool cdh_replace_span(struct seq_file *m, char *span,
			     size_t old_len, const char *rep, size_t new_len)
{
	char *end = m->buf + m->count;
	size_t tail_len = (size_t)(end - (span + old_len));
	size_t new_count;

	if (new_len == old_len) {
		memcpy(span, rep, new_len);
		return true;
	}
	if (new_len > old_len) {
		size_t grow = new_len - old_len;

		if (grow >= m->size - m->count) {
			/* 交由 seq_file 标准扩容后重放当前行。 */
			m->count = m->size;
			return false;
		}
		new_count = m->count + grow;
	} else {
		new_count = m->count - (old_len - new_len);
	}
	memmove(span + new_len, span + old_len, tail_len);
	memcpy(span, rep, new_len);
	m->count = new_count;
	return true;
}

/*
 * 重写 mountinfo 单行：
 *   - 命中 /mnt/vendorpatch 则回退 m->count 跳过整行；
 *   - 重编号可选标签字段中的 shared:N / master:N / propagate_from:N；
 *   - 将 " - <fstype> <mount_source>" 中的 /dev/block/vda* 映射为真机源名。
 */
static void cdh_process_mountinfo_line(struct seq_file *m, size_t orig)
{
	char *line = m->buf + orig;
	size_t len = m->count - orig;
	const char *mp, *f5;
	size_t mp_len = 0, f5_len = 0;
	char *opt_start, *sep, *line_end;
	char rebuilt[384];
	size_t rlen = 0;
	const char *p;
	bool changed = false;

	mp = cdh_nth_field(line, len, 4, &mp_len);
	if (mp && cdh_is_carrier_mp(mp, mp_len)) {
		m->count = orig;
		atomic64_inc(&cdh_mount_hidden);
		return;
	}

	if (m->read_pos == 0 && orig == 0)
		cdh_peer_map_reset(m);

	f5 = cdh_nth_field(line, len, 5, &f5_len);
	if (!f5)
		return;

	line_end = line + len;
	opt_start = (char *)(f5 + f5_len);
	sep = cdh_memfind(opt_start, line_end, " - ", 3);
	if (!sep || opt_start >= sep)
		return;
	if ((size_t)(sep - opt_start) >= sizeof(rebuilt) - 32)
		return;

	p = opt_start;
	while (p < sep) {
		const char *tok;
		size_t tlen, plen = 0;
		u32 old_id, new_id;
		int n;

		while (p < sep && *p == ' ')
			p++;
		if (p >= sep)
			break;
		tok = p;
		while (p < sep && *p != ' ')
			p++;
		tlen = (size_t)(p - tok);

		if (tlen > 7 && !memcmp(tok, "shared:", 7))
			plen = 7;
		else if (tlen > 7 && !memcmp(tok, "master:", 7))
			plen = 7;
		else if (tlen > 15 && !memcmp(tok, "propagate_from:", 15))
			plen = 15;

		if (plen && cdh_parse_u32(tok + plen, tlen - plen, &old_id)) {
			new_id = cdh_peer_map_get(m, old_id);
			if (new_id != old_id)
				changed = true;
			n = scnprintf(rebuilt + rlen, sizeof(rebuilt) - rlen,
				      " %.*s%u", (int)plen, tok, new_id);
		} else {
			n = scnprintf(rebuilt + rlen, sizeof(rebuilt) - rlen,
				      " %.*s", (int)tlen, tok);
		}
		if (n <= 0 || rlen + (size_t)n >= sizeof(rebuilt) - 1)
			return;
		rlen += (size_t)n;
	}

	if (changed) {
		size_t old_tag_len = (size_t)(sep - opt_start);
		size_t new_line_len = len - old_tag_len + rlen;

		if (orig + new_line_len >= m->size) {
			/* 交由 seq_file 标准扩容后重放当前行（映射表幂等）。 */
			m->count = m->size;
			return;
		}
		memmove(opt_start + rlen, sep, (size_t)(line_end - sep));
		memcpy(opt_start, rebuilt, rlen);
		m->count = orig + new_line_len;
		atomic64_inc(&cdh_peergroup_rewrites);
		sep = opt_start + rlen;
		line = m->buf + orig;
		line_end = m->buf + m->count;
	}

	if (sep + 3 < line_end) {
		size_t right_len = (size_t)(line_end - (sep + 3));
		size_t ft_len = 0, src_len = 0, sopt_len = 0;
		const char *fstype = cdh_nth_field(sep + 3, right_len, 0, &ft_len);
		const char *src = cdh_nth_field(sep + 3, right_len, 1, &src_len);
		const char *sopt = cdh_nth_field(sep + 3, right_len, 2, &sopt_len);

		if (fstype && src && sopt &&
		    !cdh_inject_dm_f2fs_inlinecrypt(m, fstype, ft_len, src,
						    src_len, sopt, sopt_len))
			return;
	}

	line = m->buf + orig;
	len = m->count - orig;
	line_end = line + len;
	mp = cdh_nth_field(line, len, 4, &mp_len);
	sep = cdh_memfind(line, line_end, " - ", 3);
	if (sep && sep + 3 < line_end && mp) {
		size_t src_len = 0;
		const char *src = cdh_nth_field(sep + 3,
						(size_t)(line_end - (sep + 3)),
						1, &src_len);
		char rep[64];
		size_t new_len;

		if (src) {
			new_len = cdh_map_vda_source(mp, mp_len, src, src_len,
						     rep, sizeof(rep));
			if (new_len > 0 &&
			    !cdh_replace_span(m, (char *)src, src_len,
					      rep, new_len))
				return;
		}
	}

	line = m->buf + orig;
	len = m->count - orig;
	mp = cdh_nth_field(line, len, 4, &mp_len);
	if (mp) {
		size_t dev_len = 0, new_dev_len = 0;
		const char *new_dev = cdh_map_unstatable_dev(mp, mp_len,
							     &new_dev_len);
		const char *dev = new_dev ? cdh_nth_field(line, len, 2, &dev_len) : NULL;

		if (new_dev && dev &&
		    !(dev_len == new_dev_len && !memcmp(dev, new_dev, dev_len)))
			cdh_replace_span(m, (char *)dev, dev_len,
					 new_dev, new_dev_len);
	}
}

enum cdh_mnt_hook_kind {
	CDH_MNT_INFO = 0,
	CDH_MNT_VFSMNT = 1,
	CDH_MNT_VFSSTAT = 2,
};

struct cdh_mnt_seq_data {
	struct seq_file *m;
	size_t orig_count;
	int kind;
	bool active;
};

static int cdh_mnt_seq_entry(struct kretprobe_instance *ri,
			     struct pt_regs *regs, int kind)
{
	struct cdh_mnt_seq_data *data = (struct cdh_mnt_seq_data *)ri->data;
	struct seq_file *m;

	if (!cdh_should_skip_uid()) {
		data->active = false;
		return 0;
	}
	m = (struct seq_file *)regs_get_kernel_argument(regs, 0);
	if (!m) {
		data->active = false;
		return 0;
	}
	data->m = m;
	data->orig_count = m->count;
	data->kind = kind;
	data->active = true;
	return 0;
}

static int cdh_mnt_seq_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct cdh_mnt_seq_data *data = (struct cdh_mnt_seq_data *)ri->data;
	struct seq_file *m = data->m;
	size_t orig = data->orig_count;

	if (!data->active || regs_return_value(regs) != 0)
		return 0;
	if (!m || !m->buf || m->count <= orig || seq_has_overflowed(m))
		return 0;

	if (data->kind == CDH_MNT_INFO) {
		cdh_process_mountinfo_line(m, orig);
	} else if (data->kind == CDH_MNT_VFSMNT) {
		char *line = m->buf + orig;
		size_t len = m->count - orig;
		size_t mp_len = 0, src_len = 0;
		const char *mp = cdh_nth_field(line, len, 1, &mp_len);
		const char *src;
		char rep[64];
		size_t new_len;

		if (mp && cdh_is_carrier_mp(mp, mp_len)) {
			m->count = orig;
			atomic64_inc(&cdh_mount_hidden);
			return 0;
		}
		src = cdh_nth_field(line, len, 0, &src_len);
		if (src && mp) {
			new_len = cdh_map_vda_source(mp, mp_len, src, src_len,
						     rep, sizeof(rep));
			if (new_len > 0 &&
			    !cdh_replace_span(m, (char *)src, src_len,
					      rep, new_len))
				return 0;
		}
		line = m->buf + orig;
		len = m->count - orig;
		{
			size_t ft_len = 0, opts_len = 0;
			const char *fstype = cdh_nth_field(line, len, 2, &ft_len);
			const char *opts = cdh_nth_field(line, len, 3, &opts_len);

			src = cdh_nth_field(line, len, 0, &src_len);
			if (src && fstype && opts)
				cdh_inject_dm_f2fs_inlinecrypt(m, fstype, ft_len,
							       src, src_len,
							       opts, opts_len);
		}
	} else if (data->kind == CDH_MNT_VFSSTAT) {
		char *line = m->buf + orig;
		char *end = m->buf + m->count;
		static const char pat[] = " mounted on " CDH_CARRIER_MOUNT_POINT;
		char *hit = cdh_memfind(line, end, pat, sizeof(pat) - 1);

		if (hit) {
			char *after = hit + (sizeof(pat) - 1);

			if (after < end && (*after == ' ' || *after == '/')) {
				m->count = orig;
				atomic64_inc(&cdh_mount_hidden);
				return 0;
			}
		}
		{
			size_t len = (size_t)(end - line);
			size_t src_len = 0, mp_len = 0;
			const char *src = cdh_nth_field(line, len, 1, &src_len);
			const char *mp = cdh_nth_field(line, len, 4, &mp_len);
			char rep[64];
			size_t new_len;

			if (src && mp) {
				new_len = cdh_map_vda_source(mp, mp_len,
							     src, src_len,
							     rep, sizeof(rep));
				if (new_len > 0 &&
				    !cdh_replace_span(m, (char *)src, src_len,
						      rep, new_len))
					return 0;
			}
		}
		line = m->buf + orig;
		{
			size_t len = m->count - orig;
			size_t ft_len = 0, sopt_len = 0, src_len = 0;
			const char *src = cdh_nth_field(line, len, 1, &src_len);
			const char *fstype = cdh_nth_field(line, len, 6, &ft_len);
			const char *sopt = cdh_nth_field(line, len, 7, &sopt_len);

			if (src && fstype && sopt)
				cdh_inject_dm_f2fs_inlinecrypt(m, fstype, ft_len,
							       src, src_len,
							       sopt, sopt_len);
		}
	}
	return 0;
}

static int cdh_mountinfo_entry(struct kretprobe_instance *ri, struct pt_regs *r)
{
	return cdh_mnt_seq_entry(ri, r, CDH_MNT_INFO);
}

static int cdh_vfsmnt_entry(struct kretprobe_instance *ri, struct pt_regs *r)
{
	return cdh_mnt_seq_entry(ri, r, CDH_MNT_VFSMNT);
}

static int cdh_vfsstat_entry(struct kretprobe_instance *ri, struct pt_regs *r)
{
	return cdh_mnt_seq_entry(ri, r, CDH_MNT_VFSSTAT);
}

static struct kretprobe cdh_show_mountinfo_krp = {
	.handler = cdh_mnt_seq_ret,
	.entry_handler = cdh_mountinfo_entry,
	.data_size = sizeof(struct cdh_mnt_seq_data),
	.maxactive = CDH_MNT_KRETPROBE_MAXACTIVE,
	.kp = { .symbol_name = "show_mountinfo" },
};

static struct kretprobe cdh_show_vfsmnt_krp = {
	.handler = cdh_mnt_seq_ret,
	.entry_handler = cdh_vfsmnt_entry,
	.data_size = sizeof(struct cdh_mnt_seq_data),
	.maxactive = CDH_MNT_KRETPROBE_MAXACTIVE,
	.kp = { .symbol_name = "show_vfsmnt" },
};

static struct kretprobe cdh_show_vfsstat_krp = {
	.handler = cdh_mnt_seq_ret,
	.entry_handler = cdh_vfsstat_entry,
	.data_size = sizeof(struct cdh_mnt_seq_data),
	.maxactive = CDH_MNT_KRETPROBE_MAXACTIVE,
	.kp = { .symbol_name = "show_vfsstat" },
};

#ifdef CONFIG_ARM64
static inline void __user *cdh_mnt_untag_user(void __user *ptr)
{
#ifdef untagged_addr
	return untagged_addr(ptr);
#else
	return ptr;
#endif
}

struct cdh_getdents_data {
	void __user *dirent;
	bool active;
};

static int cdh_getdents64_entry(struct kretprobe_instance *ri,
				struct pt_regs *regs)
{
	struct cdh_getdents_data *data = (struct cdh_getdents_data *)ri->data;
	const struct pt_regs *uregs;
	unsigned int fd;
	struct file *file;
	char path_buf[32];
	char *resolved;

	data->active = false;
	if (!cdh_should_skip_uid())
		return 0;

	uregs = (const struct pt_regs *)regs_get_kernel_argument(regs, 0);
	if (!uregs)
		return 0;

	fd = (unsigned int)uregs->regs[0];
	file = fget(fd);
	if (!file)
		return 0;

	resolved = d_path(&file->f_path, path_buf, sizeof(path_buf));
	if (!IS_ERR(resolved) && !strcmp(resolved, "/mnt")) {
		data->dirent = cdh_mnt_untag_user((void __user *)uregs->regs[1]);
		data->active = data->dirent != NULL;
	}
	fput(file);
	return 0;
}

static int cdh_getdents64_ret(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	struct cdh_getdents_data *data = (struct cdh_getdents_data *)ri->data;
	long ret = regs_return_value(regs);
	char *kbuf;
	size_t offset = 0, total;
	struct linux_dirent64 *prev = NULL;
	bool modified = false;

	if (!data->active || ret <= 0 || ret > 65536)
		return 0;

	total = (size_t)ret;
	kbuf = kmalloc(total, GFP_ATOMIC);
	if (!kbuf)
		return 0;

	if (copy_from_user_nofault(kbuf, data->dirent, total))
		goto out;

	while (offset + offsetof(struct linux_dirent64, d_name) < total) {
		struct linux_dirent64 *d = (struct linux_dirent64 *)(kbuf + offset);
		unsigned short reclen = d->d_reclen;

		if (reclen < offsetof(struct linux_dirent64, d_name) + 1 ||
		    offset + reclen > total)
			break;

		if (!strcmp(d->d_name, "vendorpatch") && total > reclen) {
			if (prev)
				prev->d_off = d->d_off;
			memmove(kbuf + offset, kbuf + offset + reclen,
				total - (offset + reclen));
			total -= reclen;
			modified = true;
			atomic64_inc(&cdh_dirent_hidden);
			continue;
		}
		prev = d;
		offset += reclen;
	}

	if (modified && !copy_to_user_nofault(data->dirent, kbuf, total))
		regs_set_return_value(regs, (long)total);

out:
	kfree(kbuf);
	return 0;
}

static struct kretprobe cdh_getdents64_krp = {
	.handler = cdh_getdents64_ret,
	.entry_handler = cdh_getdents64_entry,
	.data_size = sizeof(struct cdh_getdents_data),
	.maxactive = CDH_MNT_KRETPROBE_MAXACTIVE,
	.kp = { .symbol_name = "__arm64_sys_getdents64" },
};

static bool cdh_getdents64_registered;
#endif

static bool cdh_mountinfo_registered;
static bool cdh_vfsmnt_registered;
static bool cdh_vfsstat_registered;

int cdh_mountinfo_hook_init(void)
{
	int ret;

	ret = register_kretprobe(&cdh_show_mountinfo_krp);
	if (ret) {
		pr_warn("注册 show_mountinfo kretprobe 失败（%d）\n", ret);
	} else {
		cdh_mountinfo_registered = true;
	}

	ret = register_kretprobe(&cdh_show_vfsmnt_krp);
	if (ret) {
		pr_warn("注册 show_vfsmnt kretprobe 失败（%d）\n", ret);
	} else {
		cdh_vfsmnt_registered = true;
	}

	ret = register_kretprobe(&cdh_show_vfsstat_krp);
	if (ret) {
		pr_warn("注册 show_vfsstat kretprobe 失败（%d）\n", ret);
	} else {
		cdh_vfsstat_registered = true;
	}

#ifdef CONFIG_ARM64
	ret = register_kretprobe(&cdh_getdents64_krp);
	if (ret) {
		pr_warn("注册 __arm64_sys_getdents64 kretprobe 失败（%d）\n", ret);
	} else {
		cdh_getdents64_registered = true;
	}
#endif

	pr_info("挂载表视图钩子就绪：mountinfo=%d mounts=%d mountstats=%d\n",
		cdh_mountinfo_registered, cdh_vfsmnt_registered,
		cdh_vfsstat_registered);
	return 0;
}

void cdh_mountinfo_hook_exit(void)
{
#ifdef CONFIG_ARM64
	if (cdh_getdents64_registered) {
		unregister_kretprobe(&cdh_getdents64_krp);
		cdh_getdents64_registered = false;
	}
#endif
	if (cdh_vfsstat_registered) {
		unregister_kretprobe(&cdh_show_vfsstat_krp);
		cdh_vfsstat_registered = false;
	}
	if (cdh_vfsmnt_registered) {
		unregister_kretprobe(&cdh_show_vfsmnt_krp);
		cdh_vfsmnt_registered = false;
	}
	if (cdh_mountinfo_registered) {
		unregister_kretprobe(&cdh_show_mountinfo_krp);
		cdh_mountinfo_registered = false;
	}
}
