// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 *
 * UTS / uname(2) 展示层：从伴随数据文件（默认 /vendor_dlkm/lib/modules/cdh_device_info.uts）
 * 读取目标机型的 uname-version 字段并写入 init_uts_ns.name，卸载时恢复原值。
 *
 * 数据文件由 08_build_vendor_dlkm 从基线清单 rom.vendor.kernel.uname-version 渲染注入，
 * 同时兼容两类内容：
 *   1) 键值行（08 渲染产物）：
 *        sysname  Linux
 *        nodename  localhost
 *        release  6.6.89-android15-8-...
 *        version  #1 SMP PREEMPT Thu Jul 10 06:35:14 UTC 2025
 *        machine  aarch64
 *        domainname  (none)
 *   2) 原始 uname-version 单行文本（`uname -a` 或 `/proc/version` 的 "Linux ..." 原文）。
 */
#define pr_fmt(fmt) "cdh_device_info: uts: " fmt

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kernel_read_file.h>
#include <linux/limits.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/utsname.h>
#include <linux/vmalloc.h>

#include "cdh_device_info.h"

#define CDH_UNAME_SYSNAME_DEFAULT "Linux"
#define CDH_UNAME_NODENAME_DEFAULT "localhost"
#define CDH_UNAME_MACHINE_DEFAULT "aarch64"
#define CDH_UNAME_DOMAINNAME_DEFAULT "(none)"
#define CDH_UTS_FILE_MAX (64 * 1024)

static char *cdh_uts_path;
module_param_named(uts_path, cdh_uts_path, charp, 0444);
MODULE_PARM_DESC(uts_path, "UTS 数据文件路径（缺省 " CDH_DEFAULT_UTS_PATH "）");

static char *cdh_uts_inline;
module_param_named(uts, cdh_uts_inline, charp, 0444);
MODULE_PARM_DESC(uts, "调试用内联 uname-version 字符串（非空时忽略 uts_path）");

static struct new_utsname cdh_original_uts;
static bool cdh_original_uts_saved;

static void cdh_set_uts_field(char *dst, const char *src)
{
	strscpy(dst, src, __NEW_UTS_LEN + 1);
}

static char *cdh_trim_left(char *s)
{
	while (*s == ' ' || *s == '\t' || *s == '\r')
		s++;
	return s;
}

static void cdh_trim_right(char *s)
{
	size_t len = strlen(s);

	while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
			   s[len - 1] == '\r' || s[len - 1] == '\n'))
		s[--len] = '\0';
}

/* 切出开头的第一个非空白 token，并把 *cursor 推到后续非空白字符处。 */
static char *cdh_next_token(char **cursor)
{
	char *start, *walk;

	if (!cursor || !*cursor)
		return NULL;
	start = cdh_trim_left(*cursor);
	if (!*start) {
		*cursor = start;
		return NULL;
	}
	walk = start;
	while (*walk && *walk != ' ' && *walk != '\t' && *walk != '\r')
		walk++;
	if (*walk) {
		*walk++ = '\0';
		walk = cdh_trim_left(walk);
	}
	*cursor = walk;
	return start;
}

static bool cdh_is_known_os_tag(const char *s)
{
	return !strcmp(s, "Toybox") || !strcmp(s, "GNU/Linux") ||
	       !strcmp(s, "Android");
}

static bool cdh_is_known_machine(const char *s)
{
	return !strcmp(s, "aarch64") || !strcmp(s, "arm64") ||
	       !strcmp(s, "armv8l") || !strcmp(s, "armv7l") ||
	       !strcmp(s, "x86_64") || !strcmp(s, "i686") ||
	       !strcmp(s, "riscv64");
}

/* 找字符串最后一个空白分隔的 token 起点（返回指向该 token 前导空白的指针与 token 指针）。 */
static bool cdh_find_last_token(char *s, char **space_pos, char **token_pos)
{
	size_t len = strlen(s);
	size_t i;

	if (!len)
		return false;
	i = len;
	while (i > 0 && s[i - 1] != ' ' && s[i - 1] != '\t')
		i--;
	if (i == 0 || i == len)
		return false;
	*token_pos = s + i;
	while (i > 0 && (s[i - 1] == ' ' || s[i - 1] == '\t'))
		i--;
	if (i == 0)
		return false;
	*space_pos = s + i;
	return true;
}

/*
 * 剥掉 `uname -a` 末尾附带的 "<machine> [<os>]"（如 "aarch64 Toybox" 或 "aarch64"），
 * 同时保留 version 内部原有的空格（如 "Fri Dec  5"）。
 */
static void cdh_strip_trailing_machine_os(char *ver, char *machine_out)
{
	char *sp1 = NULL, *last = NULL;
	char *sp2 = NULL, *prev = NULL;
	char saved;

	cdh_trim_right(ver);
	if (!cdh_find_last_token(ver, &sp1, &last))
		return;

	if (cdh_is_known_os_tag(last)) {
		saved = *sp1;
		*sp1 = '\0';
		if (cdh_find_last_token(ver, &sp2, &prev) &&
		    cdh_is_known_machine(prev)) {
			cdh_set_uts_field(machine_out, prev);
			*sp2 = '\0';
			return;
		}
		*sp1 = saved;
		return;
	}

	if (cdh_is_known_machine(last)) {
		cdh_set_uts_field(machine_out, last);
		*sp1 = '\0';
	}
}

/*
 * 解析原始 uname-version 字符串，支持：
 *   - `uname -a`："Linux localhost 6.6.89-... #1 SMP PREEMPT ... aarch64 Toybox"
 *   - `/proc/version`："Linux version 6.6.66-... (kleaf@...) #1 SMP PREEMPT ..."
 */
static int cdh_uts_parse_raw_uname(char *raw, struct new_utsname *uts,
				   bool *has_release, bool *has_version)
{
	char *cursor = raw;
	char *t0, *t1, *t2;
	char *ver = NULL;
	char *last_paren;

	t0 = cdh_next_token(&cursor);
	t1 = cdh_next_token(&cursor);
	t2 = cdh_next_token(&cursor);
	if (!t0 || !t1 || !t2 || t2[0] == '#' || t2[0] == '(')
		return -EINVAL;

	cdh_set_uts_field(uts->sysname, t0);
	if (strcmp(t1, "version") != 0)
		cdh_set_uts_field(uts->nodename, t1);
	cdh_set_uts_field(uts->release, t2);
	*has_release = true;

	cursor = cdh_trim_left(cursor);
	if (cursor[0] == '#') {
		ver = cursor;
	} else {
		last_paren = strrchr(cursor, ')');
		if (last_paren) {
			char *after = cdh_trim_left(last_paren + 1);

			if (after[0] == '#') {
				ver = after;
			} else {
				after = strstr(last_paren + 1, " #");
				if (after)
					ver = after + 1;
			}
		}
		if (!ver) {
			char *hash = strstr(cursor, " #");

			if (hash)
				ver = hash + 1;
		}
	}

	if (!ver || ver[0] != '#' || !ver[1])
		return -EINVAL;

	cdh_strip_trailing_machine_os(ver, uts->machine);
	if (!ver[0] || !ver[1])
		return -EINVAL;

	cdh_set_uts_field(uts->version, ver);
	*has_version = true;
	return 0;
}

/* 解析 UTS 文本（文件正文与模块参数共用）。 */
static int cdh_uts_parse_text(char *text, const char *origin,
			      struct new_utsname *uts)
{
	char *cursor = text;
	bool has_release = false;
	bool has_version = false;
	int line_no = 0;

	memset(uts, 0, sizeof(*uts));
	cdh_set_uts_field(uts->sysname, CDH_UNAME_SYSNAME_DEFAULT);
	cdh_set_uts_field(uts->nodename, CDH_UNAME_NODENAME_DEFAULT);
	cdh_set_uts_field(uts->machine, CDH_UNAME_MACHINE_DEFAULT);
	cdh_set_uts_field(uts->domainname, CDH_UNAME_DOMAINNAME_DEFAULT);

	while (cursor && *cursor) {
		char *line = cursor;
		char *newline = strchr(cursor, '\n');
		char *rest, *key, *val;

		if (newline) {
			*newline = '\0';
			cursor = newline + 1;
		} else {
			cursor = NULL;
		}
		line_no++;

		line = cdh_trim_left(line);
		cdh_trim_right(line);
		if (!*line || *line == '#')
			continue;

		/* 兼容直接写入整行 "Linux localhost ..." 或 "Linux version ..." */
		if (!strncmp(line, "Linux ", 6) || !strncmp(line, "Linux\t", 6)) {
			if (cdh_uts_parse_raw_uname(line, uts, &has_release,
						    &has_version)) {
				pr_err("%s 第 %d 行：无法解析 uname-version 原文\n",
				       origin, line_no);
				return -EINVAL;
			}
			continue;
		}

		rest = line;
		key = cdh_next_token(&rest);
		val = cdh_trim_left(rest);
		if (!key || !*val) {
			pr_err("%s 第 %d 行：字段格式应为 \"<键> <值>\"\n",
			       origin, line_no);
			return -EINVAL;
		}

		if (!strcmp(key, "uname-version")) {
			if (cdh_uts_parse_raw_uname(val, uts, &has_release,
						    &has_version)) {
				pr_err("%s 第 %d 行：无法解析 uname-version 字段\n",
				       origin, line_no);
				return -EINVAL;
			}
		} else if (!strcmp(key, "sysname")) {
			cdh_set_uts_field(uts->sysname, val);
		} else if (!strcmp(key, "nodename")) {
			cdh_set_uts_field(uts->nodename, val);
		} else if (!strcmp(key, "release")) {
			cdh_set_uts_field(uts->release, val);
			has_release = true;
		} else if (!strcmp(key, "version")) {
			cdh_set_uts_field(uts->version, val);
			has_version = true;
		} else if (!strcmp(key, "machine")) {
			cdh_set_uts_field(uts->machine, val);
		} else if (!strcmp(key, "domainname")) {
			cdh_set_uts_field(uts->domainname, val);
		} else {
			pr_err("%s 第 %d 行：未知字段 %s\n", origin, line_no, key);
			return -EINVAL;
		}
	}

	if (!has_release || !has_version || !uts->release[0] || !uts->version[0]) {
		pr_err("%s 缺少 release 或 version 字段，拒绝装载\n", origin);
		return -EINVAL;
	}
	return 0;
}

static int cdh_uts_load_from_file(const char *path, struct new_utsname *uts)
{
	void *data = NULL;
	size_t size = 0;
	char *text;
	ssize_t read;
	int ret;

	/*
	 * 注意返回值语义：kernel_read_file_from_path() 成功时返回**读到的字节数**
	 * （kernel_read_file() 的 "ret == 0 ? copied : ret"），只有负数才是错误。
	 */
	read = kernel_read_file_from_path(path, 0, &data, INT_MAX, &size,
					  READING_UNKNOWN);
	if (read < 0) {
		pr_err("读不到 UTS 文件 %s（%zd）\n", path, read);
		return (int)read;
	}
	if (!size || size > CDH_UTS_FILE_MAX) {
		pr_err("UTS 文件 %s 大小异常（%zu B）\n", path, size);
		vfree(data);
		return -EINVAL;
	}
	/* 补一个 NUL 再解析：文件正文本身不保证以 NUL 结尾。 */
	text = kmalloc(size + 1, GFP_KERNEL);
	if (!text) {
		vfree(data);
		return -ENOMEM;
	}
	memcpy(text, data, size);
	text[size] = '\0';
	vfree(data);

	ret = cdh_uts_parse_text(text, path, uts);
	kfree(text);
	return ret;
}

static int cdh_uts_load_from_param(const char *param, struct new_utsname *uts)
{
	char *copy;
	int ret;

	copy = kstrdup(param, GFP_KERNEL);
	if (!copy)
		return -ENOMEM;
	ret = cdh_uts_parse_text(copy, "模块参数 uts", uts);
	kfree(copy);
	return ret;
}

static int cdh_uts_load(const char *path, const char *inline_uts,
			struct new_utsname *uts, const char **used_path)
{
	int ret;

	if (inline_uts && *inline_uts) {
		*used_path = "模块参数";
		return cdh_uts_load_from_param(inline_uts, uts);
	}
	if (path && *path) {
		*used_path = path;
		return cdh_uts_load_from_file(path, uts);
	}
	/*
	 * kernel_read_file_from_path() 走 filp_open -> getname_kernel，不经过
	 * cdh_redirect 的 getname_flags 用户态路径重定向；因此缺省先读 patch.img
	 * 载体物理路径 /mnt/vendorpatch/vendor_dlkm/...，-ENOENT 时再回退直挂路径。
	 */
	*used_path = CDH_PATCH_UTS_PATH;
	ret = cdh_uts_load_from_file(CDH_PATCH_UTS_PATH, uts);
	if (ret == -ENOENT) {
		*used_path = CDH_DEFAULT_UTS_PATH;
		ret = cdh_uts_load_from_file(CDH_DEFAULT_UTS_PATH, uts);
	}
	return ret;
}

int cdh_uts_init(void)
{
	const char *used_path = CDH_DEFAULT_UTS_PATH;
	struct new_utsname target_uts;
	int ret;

	ret = cdh_uts_load(cdh_uts_path, cdh_uts_inline, &target_uts, &used_path);
	if (ret) {
		pr_err("UTS 装载失败（%d），模块不加载\n", ret);
		return ret;
	}

	cdh_original_uts = init_uts_ns.name;
	cdh_original_uts_saved = true;
	init_uts_ns.name = target_uts;

	pr_info("patched init_uts_ns (%s) release=%s version=%s\n",
		used_path,
		init_uts_ns.name.release, init_uts_ns.name.version);
	return 0;
}

void cdh_uts_exit(void)
{
	if (cdh_original_uts_saved)
		init_uts_ns.name = cdh_original_uts;
	pr_info("restored init_uts_ns release=%s version=%s\n",
		init_uts_ns.name.release, init_uts_ns.name.version);
}
