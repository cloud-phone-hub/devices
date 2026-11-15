// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025-2026 fei_cong(https://github.com/feicong/feicong-course)
 *
 * 输入设备身份配置解析器：从伴随数据文件（默认
 * /vendor_dlkm/lib/modules/cdh_input_identity.devices）读取行式文本并解析为
 * struct cdh_input_config。数据文件由 08_build_vendor_dlkm 从基线清单
 * modules.input_identity 渲染注入，格式（键值两空格分隔，# 注释）：
 *
 *   touch_major  1
 *   goodix_fp  1
 *   dev  uinput-fpc
 *   id  6 1638 2184 0
 *   key  103 105 106 108
 *   abs  2 0 255 0 0
 *   dev  mt6833-mt6359 Headset Jack
 *   id  25 0 0 0
 *   phys  ALSA
 *   key  172 257 258 226
 *   sw  0 1 2 4
 *   align  mtk-kpd
 *   key  114
 *
 * dev/align 起一条记录（取整行剩余为名，容忍空格），后续 id/phys/key/sw/msc/abs
 * 子行归属当前记录。能力码已在 Go 渲染期转为数字，内核只解析数字。
 * 缺文件/解析失败 → -EINVAL，不加载（无内置缺省设备集）。
 */
#define pr_fmt(fmt) "cdh_input_identity: config: " fmt

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kernel_read_file.h>
#include <linux/limits.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "config.h"

static char *cdh_trim_left(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r')
    s++;
  return s;
}

static void cdh_trim_right(char *s) {
  size_t len = strlen(s);

  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                     s[len - 1] == '\r' || s[len - 1] == '\n'))
    s[--len] = '\0';
}

/* 切出开头的第一个非空白 token，并把 *cursor 推到后续非空白字符处。 */
static char *cdh_next_token(char **cursor) {
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

/* 把一行空格分隔的十进制无符号整数解析为动态数组（每个码校验 ≤ max_code）。
 * 成功返回 0 并填 arr 与 n（调用方负责 kfree）；空行、非法、超范围返回 -EINVAL。 */
static int cdh_parse_uint_line(char *rest, unsigned int max_code,
                               unsigned int **arr, size_t *n, int line_no,
                               const char *origin) {
  char *cursor = rest;
  char *tok;
  unsigned int *tmp = NULL;
  size_t count = 0, cap = 0;

  while ((tok = cdh_next_token(&cursor)) != NULL) {
    unsigned int v;

    if (kstrtouint(tok, 10, &v)) {
      pr_err("%s 第 %d 行：非法能力码 %s\n", origin, line_no, tok);
      kfree(tmp);
      return -EINVAL;
    }
    if (v > max_code) {
      pr_err("%s 第 %d 行：能力码 %u 超出范围 [0, %u]\n", origin, line_no, v,
             max_code);
      kfree(tmp);
      return -EINVAL;
    }
    if (count == cap) {
      unsigned int *grown;

      cap = cap ? cap * 2 : 8;
      grown = krealloc(tmp, cap * sizeof(*tmp), GFP_KERNEL);
      if (!grown) {
        kfree(tmp);
        return -ENOMEM;
      }
      tmp = grown;
    }
    tmp[count++] = v;
  }
  if (count == 0) {
    pr_err("%s 第 %d 行：能力码行不能为空\n", origin, line_no);
    kfree(tmp);
    return -EINVAL;
  }
  if (count > CDH_INPUT_MAX_CAPS) {
    pr_err("%s 第 %d 行：能力码数量 %zu 超过上限 %d\n", origin, line_no, count,
           CDH_INPUT_MAX_CAPS);
    kfree(tmp);
    return -EINVAL;
  }
  *arr = tmp;
  *n = count;
  return 0;
}

/* 把 src 追加到 *dst 动态数组尾部（krealloc 扩容）。成功后 src 由调用方释放。 */
static int cdh_append_uints(unsigned int **dst, size_t *n_dst,
                            const unsigned int *src, size_t n_src) {
  unsigned int *grown;

  if (n_src == 0)
    return 0;
  if (*n_dst + n_src > CDH_INPUT_MAX_CAPS)
    return -EINVAL;
  grown = krealloc(*dst, (*n_dst + n_src) * sizeof(**dst), GFP_KERNEL);
  if (!grown)
    return -ENOMEM;
  memcpy(grown + *n_dst, src, n_src * sizeof(*src));
  *dst = grown;
  *n_dst += n_src;
  return 0;
}

/* 取整行剩余为名（trim 后），校验非空/长度，kstrdup 到动态字符串。 */
static int cdh_parse_name(char *rest, char **out, int line_no,
                          const char *origin) {
  char *name = cdh_trim_left(rest);
  char *dup;

  cdh_trim_right(name);
  if (!*name) {
    pr_err("%s 第 %d 行：设备/对齐名称不能为空\n", origin, line_no);
    return -EINVAL;
  }
  if (strlen(name) > CDH_INPUT_NAME_MAX) {
    pr_err("%s 第 %d 行：名称超过 %d 字节\n", origin, line_no,
           CDH_INPUT_NAME_MAX);
    return -EINVAL;
  }
  dup = kstrdup(name, GFP_KERNEL);
  if (!dup)
    return -ENOMEM;
  *out = dup;
  return 0;
}

/* 起一条 dev 记录：扩容 devices 数组、清零新槽、解析名称。 */
static int cdh_begin_dev(struct cdh_input_config *cfg, char *rest, int line_no,
                         const char *origin, struct cdh_input_dev_spec **out) {
  struct cdh_input_dev_spec *grown, *dev;
  int ret;

  if (cfg->n_devs >= CDH_INPUT_MAX_DEVICES) {
    pr_err("%s 第 %d 行：设备数超过上限 %d\n", origin, line_no,
           CDH_INPUT_MAX_DEVICES);
    return -EINVAL;
  }
  grown = krealloc(cfg->devices, (cfg->n_devs + 1) * sizeof(*grown), GFP_KERNEL);
  if (!grown)
    return -ENOMEM;
  cfg->devices = grown;
  dev = &cfg->devices[cfg->n_devs];
  memset(dev, 0, sizeof(*dev));
  ret = cdh_parse_name(rest, &dev->name, line_no, origin);
  if (ret)
    return ret;
  cfg->n_devs++;
  *out = dev;
  return 0;
}

/* 起一条 align 记录：扩容 aligns 数组、清零新槽、解析名称。 */
static int cdh_begin_align(struct cdh_input_config *cfg, char *rest, int line_no,
                           const char *origin,
                           struct cdh_input_align_spec **out) {
  struct cdh_input_align_spec *grown, *al;
  int ret;

  if (cfg->n_aligns >= CDH_INPUT_MAX_ALIGNS) {
    pr_err("%s 第 %d 行：对齐声明数超过上限 %d\n", origin, line_no,
           CDH_INPUT_MAX_ALIGNS);
    return -EINVAL;
  }
  grown = krealloc(cfg->aligns, (cfg->n_aligns + 1) * sizeof(*grown), GFP_KERNEL);
  if (!grown)
    return -ENOMEM;
  cfg->aligns = grown;
  al = &cfg->aligns[cfg->n_aligns];
  memset(al, 0, sizeof(*al));
  ret = cdh_parse_name(rest, &al->name, line_no, origin);
  if (ret)
    return ret;
  cfg->n_aligns++;
  *out = al;
  return 0;
}

/* 解析 id 子行：恰好 4 个数字（bus vendor product version），各 ≤ 0xffff。 */
static int cdh_parse_id(char *rest, struct cdh_input_dev_spec *dev, int line_no,
                        const char *origin) {
  char *cursor = rest;
  unsigned int v[4];
  int i;

  for (i = 0; i < 4; i++) {
    char *tok = cdh_next_token(&cursor);

    if (!tok || kstrtouint(tok, 10, &v[i]) || v[i] > 0xffff) {
      pr_err("%s 第 %d 行：id 应为 4 个 [0,65535] 数字（bus vendor product "
             "version）\n",
             origin, line_no);
      return -EINVAL;
    }
  }
  if (cdh_next_token(&cursor)) {
    pr_err("%s 第 %d 行：id 只能有 4 个数字\n", origin, line_no);
    return -EINVAL;
  }
  dev->bustype = (__u16)v[0];
  dev->vendor = (__u16)v[1];
  dev->product = (__u16)v[2];
  dev->version = (__u16)v[3];
  return 0;
}

/* 解析 abs 子行：恰好 5 个数字（axis min max fuzz flat），axis ≤ ABS_MAX，
 * min ≤ max、fuzz/flat ≥ 0；追加到当前设备 abs 动态数组。 */
static int cdh_parse_abs(char *rest, struct cdh_input_dev_spec *dev, int line_no,
                         const char *origin) {
  char *cursor = rest;
  unsigned int axis;
  int vals[4];
  struct cdh_input_abs_spec *grown, a;
  char *tok;
  int i;

  tok = cdh_next_token(&cursor);
  if (!tok || kstrtouint(tok, 10, &axis) || axis > CDH_INPUT_ABS_MAX) {
    pr_err("%s 第 %d 行：abs axis 非法（应 ≤ %u）\n", origin, line_no,
           CDH_INPUT_ABS_MAX);
    return -EINVAL;
  }
  for (i = 0; i < 4; i++) {
    tok = cdh_next_token(&cursor);
    if (!tok || kstrtoint(tok, 10, &vals[i])) {
      pr_err("%s 第 %d 行：abs 应为 5 个数字（axis min max fuzz flat）\n",
             origin, line_no);
      return -EINVAL;
    }
  }
  if (cdh_next_token(&cursor)) {
    pr_err("%s 第 %d 行：abs 只能有 5 个数字\n", origin, line_no);
    return -EINVAL;
  }
  a.axis = axis;
  a.min = vals[0];
  a.max = vals[1];
  a.fuzz = vals[2];
  a.flat = vals[3];
  if (a.min > a.max) {
    pr_err("%s 第 %d 行：abs min (%d) 大于 max (%d)\n", origin, line_no, a.min,
           a.max);
    return -EINVAL;
  }
  if (a.fuzz < 0 || a.flat < 0) {
    pr_err("%s 第 %d 行：abs fuzz/flat 不能为负\n", origin, line_no);
    return -EINVAL;
  }
  if (dev->n_abs >= CDH_INPUT_MAX_ABS) {
    pr_err("%s 第 %d 行：abs 轴数超过上限 %d\n", origin, line_no,
           CDH_INPUT_MAX_ABS);
    return -EINVAL;
  }
  grown = krealloc(dev->abs, (dev->n_abs + 1) * sizeof(*grown), GFP_KERNEL);
  if (!grown)
    return -ENOMEM;
  dev->abs = grown;
  dev->abs[dev->n_abs++] = a;
  return 0;
}

/* 解析顶层布尔标志（touch_major/goodix_fp）：单个 0/1。 */
static int cdh_parse_flag(char *rest, bool *out, int line_no,
                          const char *origin) {
  char *tok = cdh_next_token(&rest);
  unsigned int v;

  if (!tok || kstrtouint(tok, 10, &v) || v > 1 || cdh_next_token(&rest)) {
    pr_err("%s 第 %d 行：标志应为单个 0 或 1\n", origin, line_no);
    return -EINVAL;
  }
  *out = (v == 1);
  return 0;
}

/* 解析配置文本（文件正文与模块参数共用）。cfg 由调用方清零，失败时内部释放。 */
static int cdh_input_config_parse(char *text, const char *origin,
                                  struct cdh_input_config *cfg) {
  char *cursor = text;
  int line_no = 0;
  /* 当前记录：0=无、1=dev、2=align。子行必须归属某条记录。 */
  int rec = 0;
  struct cdh_input_dev_spec *cur_dev = NULL;
  struct cdh_input_align_spec *cur_align = NULL;
  int ret = 0;

  while (cursor && *cursor) {
    char *line = cursor;
    char *newline = strchr(cursor, '\n');
    char *rest, *key;

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

    rest = line;
    key = cdh_next_token(&rest);
    if (!key)
      continue;

    if (!strcmp(key, "touch_major")) {
      ret = cdh_parse_flag(rest, &cfg->touch_major, line_no, origin);
    } else if (!strcmp(key, "goodix_fp")) {
      ret = cdh_parse_flag(rest, &cfg->goodix_fp, line_no, origin);
    } else if (!strcmp(key, "dev")) {
      ret = cdh_begin_dev(cfg, rest, line_no, origin, &cur_dev);
      if (!ret) {
        rec = 1;
        cur_align = NULL;
      }
    } else if (!strcmp(key, "align")) {
      ret = cdh_begin_align(cfg, rest, line_no, origin, &cur_align);
      if (!ret) {
        rec = 2;
        cur_dev = NULL;
      }
    } else if (!strcmp(key, "id")) {
      if (rec != 1) {
        pr_err("%s 第 %d 行：id 子行须归属 dev 记录\n", origin, line_no);
        ret = -EINVAL;
      } else {
        ret = cdh_parse_id(rest, cur_dev, line_no, origin);
      }
    } else if (!strcmp(key, "phys")) {
      if (rec != 1) {
        pr_err("%s 第 %d 行：phys 子行须归属 dev 记录\n", origin, line_no);
        ret = -EINVAL;
      } else {
        kfree(cur_dev->phys);
        cur_dev->phys = NULL;
        ret = cdh_parse_name(rest, &cur_dev->phys, line_no, origin);
      }
    } else if (!strcmp(key, "key") || !strcmp(key, "sw") ||
               !strcmp(key, "msc")) {
      unsigned int *codes = NULL;
      size_t n_codes = 0;
      unsigned int max_code;
      unsigned int **dst;
      size_t *n_dst;

      if (rec == 0) {
        pr_err("%s 第 %d 行：%s 子行须归属 dev/align 记录\n", origin, line_no,
               key);
        ret = -EINVAL;
        goto out;
      }
      if (!strcmp(key, "key")) {
        max_code = CDH_INPUT_KEY_MAX;
        if (rec == 1) {
          dst = &cur_dev->keys;
          n_dst = &cur_dev->n_keys;
        } else {
          dst = &cur_align->keys;
          n_dst = &cur_align->n_keys;
        }
      } else if (!strcmp(key, "sw")) {
        if (rec != 1) {
          pr_err("%s 第 %d 行：sw 子行须归属 dev 记录\n", origin, line_no);
          ret = -EINVAL;
          goto out;
        }
        max_code = CDH_INPUT_SW_MAX;
        dst = &cur_dev->switches;
        n_dst = &cur_dev->n_switches;
      } else {
        if (rec != 1) {
          pr_err("%s 第 %d 行：msc 子行须归属 dev 记录\n", origin, line_no);
          ret = -EINVAL;
          goto out;
        }
        max_code = CDH_INPUT_MSC_MAX;
        dst = &cur_dev->msc;
        n_dst = &cur_dev->n_msc;
      }
      ret = cdh_parse_uint_line(rest, max_code, &codes, &n_codes, line_no,
                                origin);
      if (!ret) {
        ret = cdh_append_uints(dst, n_dst, codes, n_codes);
        kfree(codes);
      }
    } else if (!strcmp(key, "abs")) {
      if (rec != 1) {
        pr_err("%s 第 %d 行：abs 子行须归属 dev 记录\n", origin, line_no);
        ret = -EINVAL;
      } else {
        ret = cdh_parse_abs(rest, cur_dev, line_no, origin);
      }
    } else {
      pr_err("%s 第 %d 行：未知字段 %s\n", origin, line_no, key);
      ret = -EINVAL;
    }
    if (ret)
      goto out;
  }

out:
  if (ret)
    cdh_input_config_free(cfg);
  return ret;
}

static int cdh_input_config_load_from_file(const char *path,
                                           struct cdh_input_config *cfg) {
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
    pr_err("读不到输入设备文件 %s（%zd）\n", path, read);
    return (int)read;
  }
  if (!size || size > CDH_INPUT_DEVICES_FILE_MAX) {
    pr_err("输入设备文件 %s 大小异常（%zu B）\n", path, size);
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

  ret = cdh_input_config_parse(text, path, cfg);
  kfree(text);
  return ret;
}

static int cdh_input_config_load_from_param(const char *param,
                                            struct cdh_input_config *cfg) {
  char *copy;
  int ret;

  copy = kstrdup(param, GFP_KERNEL);
  if (!copy)
    return -ENOMEM;
  ret = cdh_input_config_parse(copy, "模块参数 devices", cfg);
  kfree(copy);
  return ret;
}

int cdh_input_config_load(const char *path, const char *inline_text,
                          struct cdh_input_config *cfg, const char **used_path) {
  int ret;

  memset(cfg, 0, sizeof(*cfg));
  if (inline_text && *inline_text) {
    *used_path = "模块参数";
    return cdh_input_config_load_from_param(inline_text, cfg);
  }
  if (path && *path) {
    *used_path = path;
    return cdh_input_config_load_from_file(path, cfg);
  }
  /*
   * kernel_read_file_from_path() 走 filp_open -> getname_kernel，不经过
   * cdh_redirect 的 getname_flags 用户态路径重定向；因此缺省先读 patch.img
   * 载体物理路径 /mnt/vendorpatch/vendor_dlkm/...，-ENOENT 时再回退直挂路径。
   */
  *used_path = CDH_PATCH_INPUT_DEVICES_PATH;
  ret = cdh_input_config_load_from_file(CDH_PATCH_INPUT_DEVICES_PATH, cfg);
  if (ret == -ENOENT) {
    *used_path = CDH_DEFAULT_INPUT_DEVICES_PATH;
    ret = cdh_input_config_load_from_file(CDH_DEFAULT_INPUT_DEVICES_PATH, cfg);
  }
  return ret;
}

void cdh_input_config_free(struct cdh_input_config *cfg) {
  size_t i;

  if (!cfg)
    return;
  for (i = 0; i < cfg->n_devs; i++) {
    struct cdh_input_dev_spec *d = &cfg->devices[i];

    kfree(d->name);
    kfree(d->phys);
    kfree(d->keys);
    kfree(d->switches);
    kfree(d->msc);
    kfree(d->abs);
  }
  kfree(cfg->devices);
  for (i = 0; i < cfg->n_aligns; i++) {
    struct cdh_input_align_spec *a = &cfg->aligns[i];

    kfree(a->name);
    kfree(a->keys);
  }
  kfree(cfg->aligns);
  memset(cfg, 0, sizeof(*cfg));
}