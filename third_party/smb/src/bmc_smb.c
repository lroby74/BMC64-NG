#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "compat.h"
#include "smb2.h"
#include "libsmb2.h"
#include "libsmb2-raw.h"
#include "libsmb2-private.h"

#include "ff.h"
#include "bmc_smb.h"

#define BS_SUCCESS                 0x00000000u
#define BS_PENDING                 0x00000103u
#define BS_NOTIFY_CLEANUP          0x0000010Bu
#define BS_BUFFER_OVERFLOW         0x80000005u
#define BS_NO_MORE_FILES           0x80000006u
#define BS_INVALID_INFO_CLASS      0xC0000003u
#define BS_INFO_LENGTH_MISMATCH    0xC0000004u
#define BS_INVALID_HANDLE          0xC0000008u
#define BS_INVALID_PARAMETER       0xC000000Du
#define BS_NO_SUCH_FILE            0xC000000Fu
#define BS_INVALID_DEVICE_REQUEST  0xC0000010u
#define BS_END_OF_FILE             0xC0000011u
#define BS_ACCESS_DENIED           0xC0000022u
#define BS_BUFFER_TOO_SMALL        0xC0000023u
#define BS_OBJECT_NAME_INVALID     0xC0000033u
#define BS_OBJECT_NAME_NOT_FOUND   0xC0000034u
#define BS_OBJECT_NAME_COLLISION   0xC0000035u
#define BS_OBJECT_PATH_NOT_FOUND   0xC000003Au
#define BS_SHARING_VIOLATION       0xC0000043u
#define BS_DELETE_PENDING          0xC0000056u
#define BS_DISK_FULL               0xC000007Fu
#define BS_INSUFFICIENT_RESOURCES  0xC000009Au
#define BS_MEDIA_WRITE_PROTECTED   0xC00000A2u
#define BS_FILE_IS_A_DIRECTORY     0xC00000BAu
#define BS_NOT_SUPPORTED           0xC00000BBu
#define BS_NETWORK_NAME_DELETED    0xC00000C9u
#define BS_BAD_NETWORK_NAME        0xC00000CCu
#define BS_DIRECTORY_NOT_EMPTY     0xC0000101u
#define BS_NOT_A_DIRECTORY         0xC0000103u
#define BS_CANCELLED               0xC0000120u
#define BS_CANNOT_DELETE           0xC0000121u
#define BS_FILE_CLOSED             0xC0000128u
#define BS_IO_DEVICE_ERROR         0xC0000185u
#define BS_FS_DRIVER_REQUIRED      0xC000019Cu
#define BS_NOT_A_REPARSE_POINT     0xC0000275u

#define AC_READ_DATA        0x00000001u
#define AC_WRITE_DATA       0x00000002u
#define AC_APPEND_DATA      0x00000004u
#define AC_READ_EA          0x00000008u
#define AC_WRITE_EA         0x00000010u
#define AC_EXECUTE          0x00000020u
#define AC_DELETE_CHILD     0x00000040u
#define AC_READ_ATTRIBUTES  0x00000080u
#define AC_WRITE_ATTRIBUTES 0x00000100u
#define AC_DELETE           0x00010000u
#define AC_READ_CONTROL     0x00020000u
#define AC_WRITE_DAC        0x00040000u
#define AC_WRITE_OWNER      0x00080000u
#define AC_SYNCHRONIZE      0x00100000u
#define AC_MAXIMUM_ALLOWED  0x02000000u
#define AC_GENERIC_ALL      0x10000000u
#define AC_GENERIC_EXECUTE  0x20000000u
#define AC_GENERIC_WRITE    0x40000000u
#define AC_GENERIC_READ     0x80000000u

#define AC_FULL_FILE        0x001F01FFu

#define DISP_SUPERSEDE      0
#define DISP_OPEN           1
#define DISP_CREATE         2
#define DISP_OPEN_IF        3
#define DISP_OVERWRITE      4
#define DISP_OVERWRITE_IF   5

#define ACT_SUPERSEDED      0
#define ACT_OPENED          1
#define ACT_CREATED         2
#define ACT_OVERWRITTEN     3

#define OPT_DIRECTORY_FILE      0x00000001u
#define OPT_NON_DIRECTORY_FILE  0x00000040u
#define OPT_DELETE_ON_CLOSE     0x00001000u
#define OPT_OPEN_BY_FILE_ID     0x00002000u

#define FA_ATTR_READONLY    0x00000001u
#define FA_ATTR_HIDDEN      0x00000002u
#define FA_ATTR_SYSTEM      0x00000004u
#define FA_ATTR_DIRECTORY   0x00000010u
#define FA_ATTR_ARCHIVE     0x00000020u
#define FA_ATTR_NORMAL      0x00000080u

#define FSCTL_DFS_GET_REFERRALS        0x00060194u
#define FSCTL_DFS_GET_REFERRALS_EX     0x000601B0u
#define FSCTL_GET_REPARSE_POINT        0x000900A8u
#define FSCTL_SRV_REQUEST_RESUME_KEY   0x00140078u
#define FSCTL_SRV_COPYCHUNK            0x001440F2u
#define FSCTL_SRV_COPYCHUNK_WRITE      0x001480F2u
#define FSCTL_QUERY_NETWORK_INTERFACE  0x001401FCu
#define FSCTL_PIPE_TRANSCEIVE_CODE     0x0011C017u
#define FSCTL_PIPE_WAIT_CODE           0x00110018u

#define MAX_PATH_BYTES 640
#define MAX_HANDLES    64
#define MAX_FILES      24
#define MAX_TREES      16
#define MAX_CONNS      8
#define MAX_PATTERN    260
#define CTX_REPLY_CAP  256
#define FID_KEY        0xB3C64A5A5A5A5A5AULL

struct bs_file {
	int used;
	char path[MAX_PATH_BYTES];
	FIL fil;
	int writable;
	int refs;
	FSIZE_t phys;
	FSIZE_t eof;
	int delete_pending;
	int time_pending;
	WORD fdate;
	WORD ftime;
	int attr_pending;
	BYTE attr;
	int failed;
};

struct bs_handle {
	int used;
	uint16_t gen;
	struct smb2_context *owner;
	uint32_t tree_id;
	int dir;
	char path[MAX_PATH_BYTES];
	int file;
	uint32_t access;
	int delete_on_close;
	DIR dj;
	int dj_open;
	uint16_t pattern[MAX_PATTERN];
	int pattern_len;
	int pattern_set;
	int pattern_wild;
	int dir_stage;
	int dir_any;
	int dir_ended;
	int held;
	int held_kind;
	FILINFO held_fno;
	uint64_t position;
	int notify_pending;
	uint64_t notify_mid;

	int pipe;
	uint16_t rpc_ctx;
	uint8_t *rpc_buf;
	uint32_t rpc_len;
	uint32_t rpc_pos;
};

struct bs_tree {
	int used;
	struct smb2_context *owner;
	uint32_t id;
	int ipc;
};

struct bs_conn {
	int used;
	struct smb2_context *ctx;
	uint8_t last_fid[SMB2_FD_SIZE];
	int have_last;
	uint32_t last_status;
};

struct bs_info {
	int exists;
	int dir;
	BYTE attr;
	WORD fdate;
	WORD ftime;
	FSIZE_t size;
	char name[FF_LFN_BUF + 1];
};

static struct bmc_smb_config g_cfg;
static char g_volume[16];
static char g_share[32];
static char g_hostname[32];
static char g_user[64];
static char g_password[64];
static struct bs_file g_files[MAX_FILES];
static struct bs_handle g_handles[MAX_HANDLES];
static struct bs_tree g_trees[MAX_TREES];
static struct bs_conn g_conns[MAX_CONNS];
static uint32_t g_next_tree = 1;
static uint16_t g_next_gen = 1;
static uint32_t g_cluster_bytes;
static uint32_t g_serial;
static uint64_t g_root_time;
static uint8_t g_ctx_reply[CTX_REPLY_CAP];
static FILINFO g_fno;
static FILINFO g_iter_fno;
static struct smb2_server_request_handlers g_handlers;

static void bs_log(const char *fmt, ...)
{
	char buf[640];
	va_list ap;
	if (g_cfg.log == NULL) {
		return;
	}
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	g_cfg.log(buf);
}

static void put16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t) v;
	p[1] = (uint8_t) (v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
	put16(p, (uint16_t) v);
	put16(p + 2, (uint16_t) (v >> 16));
}

static void put64(uint8_t *p, uint64_t v)
{
	put32(p, (uint32_t) v);
	put32(p + 4, (uint32_t) (v >> 32));
}

static uint16_t get16(const uint8_t *p)
{
	return (uint16_t) (p[0] | (p[1] << 8));
}

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t) get16(p) | ((uint32_t) get16(p + 2) << 16);
}

static uint64_t get64(const uint8_t *p)
{
	return (uint64_t) get32(p) | ((uint64_t) get32(p + 4) << 32);
}

static int utf8_next(const unsigned char **s, uint32_t *cp)
{
	const unsigned char *p = *s;
	uint32_t c = p[0];
	if (c == 0) {
		return 0;
	}
	if (c < 0x80) {
		*cp = c;
		*s = p + 1;
		return 1;
	}
	if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
		*cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
		*s = p + 2;
		return *cp >= 0x80 ? 1 : -1;
	}
	if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
		*cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
		*s = p + 3;
		return *cp >= 0x800 ? 1 : -1;
	}
	if ((c & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
		*cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
		*s = p + 4;
		return *cp >= 0x10000 ? 1 : -1;
	}
	return -1;
}










static uint32_t fat_cp(const unsigned char **s)
{
	const unsigned char *p = *s;
	uint32_t cp;
	if (utf8_next(&p, &cp) > 0) {
		*s = p;
		return cp;
	}
	cp = **s;
	(*s)++;
	return cp;
}

static uint32_t fat_upper(uint32_t cp)
{
	return cp < 0x10000 ? (uint32_t) ff_wtoupper(cp) : cp;
}


static int fat_name_ok(const char *name)
{
	const unsigned char *p = (const unsigned char *) name;
	uint32_t cp;
	int r;
	while ((r = utf8_next(&p, &cp)) != 0) {
		if (r < 0 || cp == '?') {
			return 0;
		}
	}
	return 1;
}

static int put_utf16(uint8_t *out, size_t n, size_t room, uint32_t cp)
{
	if (cp >= 0x10000) {
		if (n + 4 > room) {
			return -1;
		}
		cp -= 0x10000;
		put16(out + n, (uint16_t) (0xD800 | (cp >> 10)));
		put16(out + n + 2, (uint16_t) (0xDC00 | (cp & 0x3FF)));
		return 4;
	}
	if (n + 2 > room) {
		return -1;
	}
	put16(out + n, (uint16_t) cp);
	return 2;
}

static int fat_to_utf16(const char *in, uint8_t *out, size_t room)
{
	size_t n = 0;
	const unsigned char *p = (const unsigned char *) in;
	while (*p) {
		int k = put_utf16(out, n, room, fat_cp(&p));
		if (k < 0) {
			return -1;
		}
		n += (size_t) k;
	}
	return (int) n;
}

static int bad_name_char(uint32_t cp)
{
	return cp < 0x20 || cp == '"' || cp == '*' || cp == ':' || cp == '<' || cp == '>' || cp == '?' || cp == '|';
}

static uint32_t smb_path_to_fat(const char *name, char *out, size_t room)
{
	const unsigned char *s = (const unsigned char *) (name ? name : "");
	char comp[FF_MAX_LFN + 1];
	size_t n;
	size_t clen;
	uint32_t cp;
	int r;
	size_t len = strlen((const char *) s);
	char *tmp = NULL;

	if (len >= 7 && strcmp((const char *) s + len - 7, "::$DATA") == 0) {
		tmp = strdup((const char *) s);
		if (tmp == NULL) {
			return BS_INSUFFICIENT_RESOURCES;
		}
		tmp[len - 7] = 0;
		s = (const unsigned char *) tmp;
	}
	n = (size_t) snprintf(out, room, "%s/", g_volume);
	for (;;) {
		while (*s == '\\' || *s == '/') {
			s++;
		}
		if (*s == 0) {
			break;
		}
		clen = 0;
		while (*s && *s != '\\' && *s != '/') {
			const unsigned char *s0 = s;
			r = utf8_next(&s, &cp);
			if (r <= 0 || bad_name_char(cp) || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
				free(tmp);
				return BS_OBJECT_NAME_INVALID;
			}
			if (clen + (size_t) (s - s0) > FF_LFN_BUF) {
				free(tmp);
				return BS_OBJECT_NAME_INVALID;
			}
			memcpy(comp + clen, s0, (size_t) (s - s0));
			clen += (size_t) (s - s0);
		}
		comp[clen] = 0;
		if (clen == 1 && comp[0] == '.') {
			continue;
		}
		if (clen == 2 && comp[0] == '.' && comp[1] == '.') {
			free(tmp);
			return BS_OBJECT_NAME_INVALID;
		}
		if (n + clen + 2 > room) {
			free(tmp);
			return BS_OBJECT_NAME_INVALID;
		}
		if (out[n - 1] != '/') {
			out[n++] = '/';
		}
		memcpy(out + n, comp, clen);
		n += clen;
		out[n] = 0;
	}
	free(tmp);
	return BS_SUCCESS;
}

static int is_root(const char *path)
{
	size_t v = strlen(g_volume);
	return strncmp(path, g_volume, v) == 0 && path[v] == '/' && path[v + 1] == 0;
}

static const char *rel_path(const char *path)
{
	return path + strlen(g_volume) + 1;
}

static int path_equal(const char *a, const char *b)
{
	const unsigned char *pa = (const unsigned char *) a;
	const unsigned char *pb = (const unsigned char *) b;
	for (;;) {
		if (*pa == 0 || *pb == 0) {
			return *pa == *pb;
		}
		if (fat_upper(fat_cp(&pa)) != fat_upper(fat_cp(&pb))) {
			return 0;
		}
	}
}

static int path_under(const char *path, const char *dir)
{
	size_t n = strlen(dir);
	char tmp[MAX_PATH_BYTES];
	if (strlen(path) <= n || path[n] != '/' || n >= sizeof(tmp)) {
		return 0;
	}
	memcpy(tmp, path, n);
	tmp[n] = 0;
	return path_equal(tmp, dir);
}

static uint64_t path_id(const char *path)
{
	uint64_t h = 1469598103934665603ULL;
	const unsigned char *p = (const unsigned char *) rel_path(path);
	while (*p) {
		uint32_t c = fat_upper(fat_cp(&p));
		h ^= (uint64_t) (c & 0xFF);
		h *= 1099511628211ULL;
		h ^= (uint64_t) ((c >> 8) & 0xFF);
		h *= 1099511628211ULL;
		if (c > 0xFFFF) {
			h ^= (uint64_t) (c >> 16);
			h *= 1099511628211ULL;
		}
	}
	h &= 0x0000FFFFFFFFFFFFULL;
	return h == 0 ? 1 : h;
}

static int64_t days_from_civil(int64_t y, int m, int d)
{
	int64_t era;
	int64_t yoe;
	int64_t doy;
	int64_t doe;
	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = y - era * 400;
	doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

static void civil_from_days(int64_t z, int *y, int *m, int *d)
{
	int64_t era;
	int64_t doe;
	int64_t yoe;
	int64_t doy;
	int64_t mp;
	z += 719468;
	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	*d = (int) (doy - (153 * mp + 2) / 5 + 1);
	*m = (int) (mp < 10 ? mp + 3 : mp - 9);
	*y = (int) (yoe + era * 400 + (*m <= 2));
}







static int fuso_minuti(void)
{
	return g_cfg.tz_offset ? g_cfg.tz_offset() : g_cfg.tz_offset_minutes;
}

static uint64_t fat_to_filetime(WORD fdate, WORD ftime)
{
	int y = 1980 + (fdate >> 9);
	int m = (fdate >> 5) & 15;
	int d = fdate & 31;
	int64_t secs;
	if (fdate == 0 || m < 1 || m > 12 || d < 1) {
		y = 1980;
		m = 1;
		d = 1;
		ftime = 0;
	}
	secs = days_from_civil(y, m, d) * 86400 + (int64_t) (ftime >> 11) * 3600 + (int64_t) ((ftime >> 5) & 63) * 60
	     + (int64_t) (ftime & 31) * 2;
	secs -= (int64_t) fuso_minuti() * 60;
	return (uint64_t) (secs + 11644473600LL) * 10000000ULL;
}

static int filetime_to_fat(uint64_t ft, WORD *fdate, WORD *ftime)
{
	int64_t secs;
	int64_t days;
	int64_t rem;
	int y;
	int m;
	int d;
	if (ft == 0 || ft == 0xFFFFFFFFFFFFFFFFULL || ft == 0xFFFFFFFFFFFFFFFEULL) {
		return 0;
	}
	secs = (int64_t) (ft / 10000000ULL) - 11644473600LL + (int64_t) fuso_minuti() * 60;
	days = secs >= 0 ? secs / 86400 : -((-secs + 86399) / 86400);
	rem = secs - days * 86400;
	civil_from_days(days, &y, &m, &d);
	if (y < 1980 || y > 2107) {
		return 0;
	}
	*fdate = (WORD) (((y - 1980) << 9) | (m << 5) | d);
	*ftime = (WORD) (((rem / 3600) << 11) | (((rem / 60) % 60) << 5) | ((rem % 60) / 2));
	return 1;
}

static uint32_t fat_attr_to_smb(BYTE attr, int dir)
{
	uint32_t a = attr & (AM_RDO | AM_HID | AM_SYS | AM_ARC);
	if (dir) {
		a |= FA_ATTR_DIRECTORY;
	}
	if (a == 0) {
		a = FA_ATTR_NORMAL;
	}
	return a;
}

static uint32_t fr_to_status(FRESULT fr)
{
	switch (fr) {
	case FR_OK:
		return BS_SUCCESS;
	case FR_NO_FILE:
		return BS_OBJECT_NAME_NOT_FOUND;
	case FR_NO_PATH:
		return BS_OBJECT_PATH_NOT_FOUND;
	case FR_INVALID_NAME:
		return BS_OBJECT_NAME_INVALID;
	case FR_DENIED:
		return BS_ACCESS_DENIED;
	case FR_EXIST:
		return BS_OBJECT_NAME_COLLISION;
	case FR_WRITE_PROTECTED:
		return BS_MEDIA_WRITE_PROTECTED;
	case FR_LOCKED:
		return BS_SHARING_VIOLATION;
	case FR_NOT_ENOUGH_CORE:
	case FR_TOO_MANY_OPEN_FILES:
		return BS_INSUFFICIENT_RESOURCES;
	default:
		return BS_IO_DEVICE_ERROR;
	}
}

static void volume_geometry(void)
{
	DWORD nclst;
	FATFS *fs = NULL;
	char vol[20];
	if (g_cluster_bytes != 0) {
		return;
	}
	snprintf(vol, sizeof(vol), "%s/", g_volume);
	if (f_getfree(vol, &nclst, &fs) == FR_OK && fs != NULL) {
		g_cluster_bytes = (uint32_t) fs->csize * 512u;
	}
	if (g_cluster_bytes == 0) {
		g_cluster_bytes = 32768;
	}
}

static uint64_t alloc_size(FSIZE_t size, int dir)
{
	uint64_t c;
	if (dir) {
		return 0;
	}
	volume_geometry();
	c = g_cluster_bytes;
	return ((uint64_t) size + c - 1) / c * c;
}

static struct bs_file *open_file_of(const char *path)
{
	int i;
	for (i = 0; i < MAX_FILES; i++) {
		if (g_files[i].used && path_equal(g_files[i].path, path)) {
			return &g_files[i];
		}
	}
	return NULL;
}

static int handles_on_path(const char *path, const struct bs_handle *except)
{
	int i;
	int n = 0;
	for (i = 0; i < MAX_HANDLES; i++) {
		if (g_handles[i].used && &g_handles[i] != except && path_equal(g_handles[i].path, path)) {
			n++;
		}
	}
	return n;
}

static void stat_path(const char *path, struct bs_info *info)
{
	struct bs_file *f;
	FRESULT fr;
	memset(info, 0, sizeof(*info));
	if (is_root(path)) {
		info->exists = 1;
		info->dir = 1;
		info->attr = AM_DIR;
		return;
	}
	fr = f_stat(path, &g_fno);
	if (fr != FR_OK) {
		return;
	}
	info->exists = 1;
	info->dir = (g_fno.fattrib & AM_DIR) != 0;
	info->attr = g_fno.fattrib;
	info->fdate = g_fno.fdate;
	info->ftime = g_fno.ftime;
	info->size = g_fno.fsize;
	snprintf(info->name, sizeof(info->name), "%s", g_fno.fname);
	f = open_file_of(path);
	if (f != NULL) {
		info->size = f->eof;
		if (f->time_pending) {
			info->fdate = f->fdate;
			info->ftime = f->ftime;
		}
		if (f->attr_pending) {
			info->attr = (BYTE) ((info->attr & AM_DIR) | f->attr);
		}
	}
}

static uint64_t info_time(const struct bs_info *info)
{
	if (info->fdate == 0) {
		return g_root_time;
	}
	return fat_to_filetime(info->fdate, info->ftime);
}

static struct bs_conn *conn_of(struct smb2_context *smb2)
{
	return (struct bs_conn *) smb2_get_opaque(smb2);
}

static uint32_t current_tree(struct smb2_context *smb2)
{
	return smb2->hdr.sync.tree_id;
}

static struct bs_tree *find_tree(struct smb2_context *smb2, uint32_t id)
{
	int i;
	for (i = 0; i < MAX_TREES; i++) {
		if (g_trees[i].used && g_trees[i].owner == smb2 && g_trees[i].id == id) {
			return &g_trees[i];
		}
	}
	return NULL;
}

static void make_fid(const struct bs_handle *h, uint8_t *fid)
{
	uint64_t p = (uint64_t) ((h - g_handles) + 1) | ((uint64_t) h->gen << 16);
	put64(fid, p);
	put64(fid + 8, p ^ FID_KEY);
}

static int fid_all_ff(const uint8_t *fid)
{
	int i;
	for (i = 0; i < SMB2_FD_SIZE; i++) {
		if (fid[i] != 0xFF) {
			return 0;
		}
	}
	return 1;
}

static struct bs_handle *find_handle(struct smb2_context *smb2, const uint8_t *fid_in)
{
	uint8_t fid[SMB2_FD_SIZE];
	uint64_t p;
	uint64_t v;
	unsigned slot;
	struct bs_handle *h;
	if (fid_all_ff(fid_in)) {
		struct bs_conn *c = conn_of(smb2);
		if (c == NULL || !c->have_last) {
			return NULL;
		}
		memcpy(fid, c->last_fid, SMB2_FD_SIZE);
	} else {
		memcpy(fid, fid_in, SMB2_FD_SIZE);
	}
	p = get64(fid);
	v = get64(fid + 8);
	slot = (unsigned) (p & 0xFFFF);
	if (slot == 0 || slot > MAX_HANDLES || v != (p ^ FID_KEY)) {
		return NULL;
	}
	h = &g_handles[slot - 1];
	if (!h->used || h->owner != smb2 || h->gen != (uint16_t) (p >> 16)) {
		return NULL;
	}
	return h;
}








static uint32_t stato_senza_handle(struct smb2_context *smb2, const uint8_t *fid)
{
	struct bs_conn *c;
	if (fid != NULL && fid_all_ff(fid)) {
		c = conn_of(smb2);
		if (c != NULL && !c->have_last && c->last_status != 0) {
			return c->last_status;
		}
	}
	return BS_FILE_CLOSED;
}

static int reply_status(struct smb2_context *smb2, uint8_t cmd, uint32_t status)
{
	struct smb2_error_reply err;
	struct smb2_pdu *pdu;
	if (status != BS_NO_MORE_FILES) {
		bs_log("reply cmd %u status %08x mid %llu", cmd, status, (unsigned long long) smb2->message_id);
	}
	memset(&err, 0, sizeof(err));
	pdu = smb2_cmd_error_reply_async(smb2, &err, cmd, (int) status, NULL, NULL);
	if (pdu == NULL) {
		return -1;
	}
	smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
	smb2_queue_pdu(smb2, pdu);
	return 1;
}

static int reply_status_mid(struct smb2_context *smb2, uint8_t cmd, uint32_t status, uint64_t mid)
{
	struct smb2_error_reply err;
	struct smb2_pdu *pdu;
	memset(&err, 0, sizeof(err));
	pdu = smb2_cmd_error_reply_async(smb2, &err, cmd, (int) status, NULL, NULL);
	if (pdu == NULL) {
		return -1;
	}
	smb2_set_pdu_message_id(smb2, pdu, mid);
	smb2_queue_pdu(smb2, pdu);
	return 1;
}

static int in_use(const char *path, int dir)
{
	if (g_cfg.in_use == NULL) {
		return 0;
	}
	return g_cfg.in_use(path, dir);
}

static FRESULT zero_fill(FIL *fil, FSIZE_t from, FSIZE_t to)
{
	static uint8_t zeros[4096];
	FRESULT fr = f_lseek(fil, from);
	while (fr == FR_OK && from < to) {
		UINT chunk = (UINT) ((to - from) > sizeof(zeros) ? sizeof(zeros) : (to - from));
		UINT bw = 0;
		fr = f_write(fil, zeros, chunk, &bw);
		if (fr == FR_OK && bw != chunk) {
			fr = FR_DENIED;
		}
		from += bw;
	}
	return fr;
}

static FRESULT file_materialize(struct bs_file *f)
{
	FRESULT fr;
	if (!f->writable || f->eof <= f->phys) {
		return FR_OK;
	}
	fr = zero_fill(&f->fil, f->phys, f->eof);
	if (fr == FR_OK) {
		f->phys = f->eof;
	}
	return fr;
}

static uint32_t file_release(struct bs_file *f)
{
	FRESULT fr = FR_OK;
	FRESULT fr2;
	uint32_t status = BS_SUCCESS;
	if (--f->refs > 0) {
		return BS_SUCCESS;
	}
	if (!f->delete_pending) {
		fr = file_materialize(f);
	}
	fr2 = f_close(&f->fil);
	if (fr == FR_OK) {
		fr = fr2;
	}
	if (fr != FR_OK && !f->delete_pending) {
		status = fr_to_status(fr);
	}
	if (f->delete_pending) {
		fr = f_unlink(f->path);
		if (fr != FR_OK && fr != FR_NO_FILE) {
			status = fr_to_status(fr);
		}
		bs_log("delete %s -> %d", f->path, (int) fr);
	} else {
		if (f->time_pending) {
			g_fno.fdate = f->fdate;
			g_fno.ftime = f->ftime;
			f_utime(f->path, &g_fno);
		}
		if (f->attr_pending) {
			f_chmod(f->path, f->attr, AM_RDO | AM_HID | AM_SYS | AM_ARC);
		}
	}
	f->used = 0;
	return status;
}

static uint32_t file_acquire(const char *path, BYTE mode, int writable, int *out)
{
	struct bs_file *f = open_file_of(path);
	FRESULT fr;
	int i;
	if (f != NULL) {
		if (mode & (FA_CREATE_ALWAYS | FA_CREATE_NEW)) {
			return BS_SHARING_VIOLATION;
		}
		if (writable && !f->writable) {
			FSIZE_t keep_eof = f->eof;
			fr = f_close(&f->fil);
			if (fr == FR_OK) {
				fr = f_open(&f->fil, f->path, FA_READ | FA_WRITE | FA_OPEN_EXISTING);
			}
			if (fr != FR_OK) {
				f_open(&f->fil, f->path, FA_READ | FA_OPEN_EXISTING);
				return fr_to_status(fr);
			}
			f->writable = 1;
			f->phys = f_size(&f->fil);
			f->eof = keep_eof > f->phys ? keep_eof : f->phys;
		}
		f->refs++;
		*out = (int) (f - g_files);
		return BS_SUCCESS;
	}
	for (i = 0; i < MAX_FILES; i++) {
		if (!g_files[i].used) {
			break;
		}
	}
	if (i == MAX_FILES) {
		return BS_INSUFFICIENT_RESOURCES;
	}
	f = &g_files[i];
	memset(f, 0, sizeof(*f));
	snprintf(f->path, sizeof(f->path), "%s", path);
	fr = f_open(&f->fil, path, (BYTE) (mode | FA_READ | (writable ? FA_WRITE : 0)));
	if (fr != FR_OK) {
		return fr_to_status(fr);
	}
	f->used = 1;
	f->refs = 1;
	f->writable = writable;
	f->phys = f_size(&f->fil);
	f->eof = f->phys;
	*out = i;
	return BS_SUCCESS;
}

static int need_write(uint32_t access)
{
	return (access & (AC_WRITE_DATA | AC_APPEND_DATA | AC_GENERIC_WRITE | AC_GENERIC_ALL | AC_MAXIMUM_ALLOWED)) != 0;
}

static int need_read(uint32_t access)
{
	return (access & (AC_READ_DATA | AC_EXECUTE | AC_GENERIC_READ | AC_GENERIC_EXECUTE | AC_GENERIC_ALL
	                  | AC_MAXIMUM_ALLOWED)) != 0;
}

static int explicit_write(uint32_t access)
{
	return (access & (AC_WRITE_DATA | AC_APPEND_DATA | AC_GENERIC_WRITE | AC_GENERIC_ALL)) != 0;
}

static void handle_free(struct bs_handle *h)
{
	if (h->dj_open) {
		f_closedir(&h->dj);
		h->dj_open = 0;
	}
	if (h->rpc_buf != NULL) {
		free(h->rpc_buf);
		h->rpc_buf = NULL;
	}
	h->used = 0;
}

static void notify_complete(struct bs_handle *h, uint32_t status)
{
	if (h->notify_pending) {
		h->notify_pending = 0;
		reply_status_mid(h->owner, SMB2_CHANGE_NOTIFY, status, h->notify_mid);
	}
}

static uint32_t handle_close(struct bs_handle *h, int send_cleanup)
{
	uint32_t status = BS_SUCCESS;
	char path[MAX_PATH_BYTES];
	int dir = h->dir;
	int del = h->delete_on_close;
	snprintf(path, sizeof(path), "%s", h->path);
	if (send_cleanup) {
		notify_complete(h, BS_NOTIFY_CLEANUP);
	} else {
		h->notify_pending = 0;
	}
	if (h->file >= 0) {
		struct bs_file *f = &g_files[h->file];
		if (del) {
			f->delete_pending = 1;
		}
		status = file_release(f);
		h->file = -1;
		handle_free(h);
		return status;
	}
	handle_free(h);
	if (del && !is_root(path)) {
		if (handles_on_path(path, NULL) == 0) {
			FRESULT fr = f_unlink(path);
			bs_log("delete %s -> %d", path, (int) fr);
			if (fr != FR_OK && fr != FR_NO_FILE) {
				status = fr_to_status(fr);
			}
		} else {
			int i;
			for (i = 0; i < MAX_HANDLES; i++) {
				if (g_handles[i].used && path_equal(g_handles[i].path, path)) {
					g_handles[i].delete_on_close = 1;
					break;
				}
			}
		}
	}
	(void) dir;
	return status;
}

static void close_owner(struct smb2_context *smb2, int tree_only, uint32_t tree_id)
{
	int i;
	for (i = 0; i < MAX_HANDLES; i++) {
		struct bs_handle *h = &g_handles[i];
		if (h->used && h->owner == smb2 && (!tree_only || h->tree_id == tree_id)) {
			handle_close(h, 0);
		}
	}
}

static int destruction_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
	int i;
	struct bs_conn *c = conn_of(smb2);
	(void) srvr;
	close_owner(smb2, 0, 0);
	for (i = 0; i < MAX_TREES; i++) {
		if (g_trees[i].used && g_trees[i].owner == smb2) {
			g_trees[i].used = 0;
		}
	}
	if (c != NULL) {
		c->used = 0;
		smb2_set_opaque(smb2, NULL);
	}
	bs_log("connection closed");
	return 0;
}

static int authorize_handler(struct smb2_server *srvr, struct smb2_context *smb2, const char *user,
                             const char *domain, const char *workstation)
{
	const char *u = user;
	const char *p;
	(void) srvr;
	if (u == NULL || u[0] == 0) {
		bs_log("auth: empty user refused");
		return -1;
	}
	p = strrchr(u, '\\');
	if (p != NULL) {
		u = p + 1;
	}
	if (strcasecmp(u, g_user) != 0) {
		bs_log("auth: user '%s' refused", user);
		return -1;
	}
	smb2_set_password(smb2, g_password);
	bs_log("auth: user '%s' domain '%s' ws '%s'", user, domain ? domain : "", workstation ? workstation : "");
	return 0;
}

static int session_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
	(void) srvr;
	bs_log("session: dialect %04x signing %d", smb2_get_dialect(smb2), smb2->sign);
	return 0;
}

static int logoff_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
	(void) srvr;
	close_owner(smb2, 0, 0);
	return 0;
}

static int share_name_of(const struct smb2_tree_connect_request *req, char *out, size_t room)
{
	size_t n = 0;
	int i;
	int chars = req->path_length / 2;
	for (i = 0; i < chars; i++) {
		uint16_t c = req->path[i];
		if (c == 0) {
			continue;
		}
		if (c == '\\' || c == '/') {
			n = 0;
			continue;
		}
		if (c > 0x7E || n + 1 >= room) {
			return -1;
		}
		out[n++] = (char) c;
	}
	out[n] = 0;
	return n > 0 ? 0 : -1;
}

static int tree_connect_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                                struct smb2_tree_connect_request *req, struct smb2_tree_connect_reply *rep)
{
	char share[40];
	int ipc;
	int i;
	struct smb2_pdu *pdu;
	(void) srvr;
	if (req == NULL || req->path == NULL || share_name_of(req, share, sizeof(share)) != 0) {
		return reply_status(smb2, SMB2_TREE_CONNECT, BS_BAD_NETWORK_NAME);
	}
	ipc = strcasecmp(share, "IPC$") == 0;
	if (!ipc && strcasecmp(share, g_share) != 0) {
		bs_log("tree: share '%s' refused", share);
		return reply_status(smb2, SMB2_TREE_CONNECT, BS_BAD_NETWORK_NAME);
	}
	for (i = 0; i < MAX_TREES; i++) {
		if (!g_trees[i].used) {
			break;
		}
	}
	if (i == MAX_TREES) {
		return reply_status(smb2, SMB2_TREE_CONNECT, BS_INSUFFICIENT_RESOURCES);
	}
	memset(rep, 0, sizeof(*rep));
	rep->share_type = ipc ? SMB2_SHARE_TYPE_PIPE : SMB2_SHARE_TYPE_DISK;
	rep->share_flags = SMB2_SHAREFLAG_MANUAL_CACHING;
	rep->capabilities = 0;
	rep->maximal_access = ipc ? 0x001F00A9u : AC_FULL_FILE;
	g_trees[i].used = 1;
	g_trees[i].owner = smb2;
	g_trees[i].ipc = ipc;
	g_trees[i].id = g_next_tree++;
	if (g_next_tree == 0) {
		g_next_tree = 1;
	}
	pdu = smb2_cmd_tree_connect_reply_async(smb2, rep, g_trees[i].id, NULL, NULL);
	if (pdu == NULL) {
		g_trees[i].used = 0;
		return -1;
	}
	smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
	smb2_queue_pdu(smb2, pdu);
	bs_log("tree: '%s' id %u", share, g_trees[i].id);
	return 1;
}

static int tree_disconnect_handler(struct smb2_server *srvr, struct smb2_context *smb2, const uint32_t tree_id)
{
	struct bs_tree *t = find_tree(smb2, tree_id);
	(void) srvr;
	close_owner(smb2, 1, tree_id);
	if (t != NULL) {
		t->used = 0;
	}
	return 0;
}

struct bs_ctx_req {
	int mxac;
	int qfid;
	int lease;
	int lease_v2;
	uint8_t lease_key[16];
	uint8_t parent_key[16];
	uint32_t lease_flags;
	uint16_t lease_epoch;
};

static int parse_contexts(const struct smb2_create_request *req, struct bs_ctx_req *out)
{
	uint32_t off = 0;
	memset(out, 0, sizeof(*out));
	if (req->create_context_length == 0) {
		return 0;
	}
	if (req->create_context == NULL) {
		return -1;
	}
	while (off + 16 <= req->create_context_length) {
		const uint8_t *c = req->create_context + off;
		uint32_t next = get32(c);
		uint16_t name_off = get16(c + 4);
		uint16_t name_len = get16(c + 6);
		uint16_t data_off = get16(c + 10);
		uint32_t data_len = get32(c + 12);
		uint32_t clen = next ? next : req->create_context_length - off;
		if (clen > req->create_context_length - off || (uint32_t) name_off + name_len > clen
		    || (data_len && (uint32_t) data_off + data_len > clen)) {
			return -1;
		}
		if (name_len == 4) {
			const uint8_t *n = c + name_off;
			if (memcmp(n, "MxAc", 4) == 0) {
				out->mxac = 1;
			} else if (memcmp(n, "QFid", 4) == 0) {
				out->qfid = 1;
			} else if (memcmp(n, "RqLs", 4) == 0 && (data_len == 32 || data_len == 52)) {
				const uint8_t *d = c + data_off;
				out->lease = 1;
				out->lease_v2 = data_len == 52;
				memcpy(out->lease_key, d, 16);
				out->lease_flags = get32(d + 20);
				if (out->lease_v2) {
					memcpy(out->parent_key, d + 32, 16);
					out->lease_epoch = get16(d + 48);
				}
			}
		}
		if (next == 0) {
			break;
		}
		off += next;
	}
	return 0;
}

static size_t add_ctx(size_t used, size_t *prev, int *have_prev, const char *name, const uint8_t *data,
                      size_t data_len, int last)
{
	size_t len = 24 + data_len;
	size_t padded = (len + 7) & ~(size_t) 7;
	uint8_t *c = g_ctx_reply + used;
	if (used + padded > CTX_REPLY_CAP) {
		return used;
	}
	memset(c, 0, padded);
	put16(c + 4, 16);
	put16(c + 6, 4);
	put16(c + 10, 24);
	put32(c + 12, (uint32_t) data_len);
	memcpy(c + 16, name, 4);
	memcpy(c + 24, data, data_len);
	if (*have_prev) {
		put32(g_ctx_reply + *prev, (uint32_t) (used - *prev));
	}
	*prev = used;
	*have_prev = 1;
	return used + (last ? len : padded);
}

static void fill_contexts(const struct bs_ctx_req *cr, const char *path, int dir, int lease_reply,
                          struct smb2_create_reply *rep)
{
	size_t used = 0;
	size_t prev = 0;
	int have_prev = 0;
	uint8_t data[64];
	int want_qfid = cr->qfid;
	int want_lease = cr->lease && lease_reply;
	if (cr->mxac) {
		memset(data, 0, 8);
		put32(data, BS_SUCCESS);
		put32(data + 4, dir ? AC_FULL_FILE : AC_FULL_FILE);
		used = add_ctx(used, &prev, &have_prev, "MxAc", data, 8, !want_qfid && !want_lease);
	}
	if (want_qfid) {
		memset(data, 0, 32);
		put64(data, path_id(path));
		put64(data + 8, g_serial);
		used = add_ctx(used, &prev, &have_prev, "QFid", data, 32, !want_lease);
	}
	if (want_lease) {
		size_t dl = cr->lease_v2 ? 52 : 32;
		memset(data, 0, sizeof(data));
		memcpy(data, cr->lease_key, 16);
		if (cr->lease_v2) {
			if (cr->lease_flags & 0x4) {
				put32(data + 20, 0x4);
				memcpy(data + 32, cr->parent_key, 16);
			}
			put16(data + 48, cr->lease_epoch);
		}
		used = add_ctx(used, &prev, &have_prev, "RqLs", data, dl, 1);
	}
	rep->create_context = used ? g_ctx_reply : NULL;
	rep->create_context_length = (uint32_t) used;
}

static int create_fail(struct smb2_context *smb2, struct smb2_create_request *req, uint32_t status)
{
	struct bs_conn *c = conn_of(smb2);
	bs_log("create '%s' disp %u -> %08x", req && req->name ? req->name : "", req ? req->create_disposition : 0,
	     status);
	if (c != NULL) {
		c->have_last = 0;
		c->last_status = status;
	}
	if (req != NULL && req->name != NULL && req->name[0] != 0) {
		smb2_free_data(smb2, discard_const(req->name));
		req->name = NULL;
	}
	return reply_status(smb2, SMB2_CREATE, status);
}

static struct bs_handle *handle_alloc(struct smb2_context *smb2, uint32_t tree_id, const char *path, int dir,
                                      uint32_t access)
{
	int i;
	struct bs_handle *h;
	for (i = 0; i < MAX_HANDLES; i++) {
		if (!g_handles[i].used) {
			break;
		}
	}
	if (i == MAX_HANDLES) {
		return NULL;
	}
	h = &g_handles[i];
	memset(h, 0, sizeof(*h));
	h->used = 1;
	h->gen = g_next_gen++;
	if (g_next_gen == 0) {
		g_next_gen = 1;
	}
	h->owner = smb2;
	h->tree_id = tree_id;
	h->dir = dir;
	h->file = -1;
	h->access = access;
	snprintf(h->path, sizeof(h->path), "%s", path);
	return h;
}
/* The srvsvc replies follow ZiFi-ESP32-S3-Zero: Copyright (c) 2026 Andrew Lazarev, MIT License (LICENSE-ZiFi-ESP32-S3-Zero.txt). */











#define BS_RPC_MAX          2048
#define BS_PIPE_EMPTY       0xC00000D9u
#define BS_PIPE_BUSY        0xC00000AEu
#define RPC_FAULT_NDR       0x1C000006u
#define RPC_FAULT_CONTEXT   0x1C00001Cu
#define RPC_FAULT_OP_RANGE  0x1C010002u
#define RPC_FAULT_PROTO     0x1C01000Bu

static const uint8_t g_rpc_srvsvc[16] = {
	0xc8, 0x4f, 0x32, 0x4b, 0x70, 0x16, 0xd3, 0x01,
	0x12, 0x78, 0x5a, 0x47, 0xbf, 0x6e, 0xe1, 0x88
};
static const uint8_t g_rpc_ndr32[16] = {
	0x04, 0x5d, 0x88, 0x8a, 0xeb, 0x1c, 0xc9, 0x11,
	0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60
};

struct rpc_share {
	const char *name;
	uint32_t type;
	const char *remark;
};



struct rpc_w {
	uint8_t *b;
	size_t cap;
	size_t n;
	int err;
	uint32_t ref;
};

static void rw_u32(struct rpc_w *w, uint32_t v)
{
	while ((w->n & 3) != 0) {
		if (w->n >= w->cap) {
			w->err = 1;
			return;
		}
		w->b[w->n++] = 0;
	}
	if (w->n + 4 > w->cap) {
		w->err = 1;
		return;
	}
	w->b[w->n++] = (uint8_t) v;
	w->b[w->n++] = (uint8_t) (v >> 8);
	w->b[w->n++] = (uint8_t) (v >> 16);
	w->b[w->n++] = (uint8_t) (v >> 24);
}

static void rw_ptr(struct rpc_w *w, int present)
{
	if (present) {
		w->ref += 4;
		rw_u32(w, w->ref);
	} else {
		rw_u32(w, 0);
	}
}



static void rw_str(struct rpc_w *w, const char *t)
{
	uint32_t i;
	uint32_t len = (uint32_t) strlen(t) + 1;
	rw_u32(w, len);
	rw_u32(w, 0);
	rw_u32(w, len);
	if (w->err || w->n + 2 * (size_t) len > w->cap) {
		w->err = 1;
		return;
	}
	for (i = 0; i < len; i++) {
		w->b[w->n++] = (uint8_t) t[i];
		w->b[w->n++] = 0;
	}
}


struct rpc_r {
	const uint8_t *b;
	size_t n;
	size_t pos;
	int err;
};

static uint32_t rr_u32(struct rpc_r *r)
{
	uint32_t v;
	r->pos = (r->pos + 3) & ~(size_t) 3;
	if (r->pos + 4 > r->n) {
		r->err = 1;
		return 0;
	}
	v = (uint32_t) r->b[r->pos] | ((uint32_t) r->b[r->pos + 1] << 8) |
	    ((uint32_t) r->b[r->pos + 2] << 16) | ((uint32_t) r->b[r->pos + 3] << 24);
	r->pos += 4;
	return v;
}

static void rr_str(struct rpc_r *r, char *out, size_t cap)
{
	uint32_t i;
	size_t k = 0;
	uint32_t max = rr_u32(r);
	uint32_t off = rr_u32(r);
	uint32_t act = rr_u32(r);
	out[0] = 0;
	if (r->err || off != 0 || act > max || act > 1024 || r->pos + 2 * (size_t) act > r->n) {
		r->err = 1;
		return;
	}
	for (i = 0; i < act; i++) {
		uint16_t c = (uint16_t) (r->b[r->pos] | (r->b[r->pos + 1] << 8));
		r->pos += 2;
		if (c != 0 && k + 1 < cap) {
			out[k++] = c < 128 ? (char) c : '?';
		}
	}
	out[k] = 0;
}

static void rr_unique_str(struct rpc_r *r, char *out, size_t cap)
{
	if (rr_u32(r) != 0) {
		rr_str(r, out, cap);
	} else {
		out[0] = 0;
	}
}

static uint16_t rpc_le16(const uint8_t *p)
{
	return (uint16_t) (p[0] | (p[1] << 8));
}

static uint32_t rpc_le32(const uint8_t *p)
{
	return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static void rpc_put16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t) v;
	p[1] = (uint8_t) (v >> 8);
}

static void rpc_put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t) v;
	p[1] = (uint8_t) (v >> 8);
	p[2] = (uint8_t) (v >> 16);
	p[3] = (uint8_t) (v >> 24);
}


static void rpc_head(uint8_t *o, uint8_t type, uint8_t flags, uint16_t len, uint32_t call_id)
{
	memset(o, 0, 16);
	o[0] = 5;
	o[2] = type;
	o[3] = flags;
	o[4] = 0x10;
	rpc_put16(o + 8, len);
	rpc_put32(o + 12, call_id);
}



static uint32_t rpc_fault(uint8_t *o, uint32_t call_id, uint16_t ctx, uint32_t status)
{
	rpc_head(o, 3, 0x23, 32, call_id);
	memset(o + 16, 0, 16);
	rpc_put16(o + 20, ctx);
	rpc_put32(o + 24, status);
	bs_log("srvsvc: fault %08x", status);
	return 32;
}

static uint32_t rpc_bind_ack(const uint8_t *in, uint16_t flen, uint32_t call_id, uint16_t *ctx_ok, uint8_t *o)
{
	static const char sec[] = "\\PIPE\\srvsvc";
	uint16_t res[4];
	uint16_t why[4];
	size_t ip = 28;
	size_t op;
	uint8_t n;
	uint8_t i;
	uint8_t t;
	int preso = 0;
	if (flen < 28) {
		return 0;
	}
	n = in[24];
	if (n == 0 || n > 4) {
		return 0;
	}
	*ctx_ok = 0xFFFF;
	for (i = 0; i < n; i++) {
		uint16_t id;
		uint8_t nt;
		int astratto;
		int ndr32 = 0;
		if (ip + 24 > flen) {
			return 0;
		}
		id = rpc_le16(in + ip);
		nt = in[ip + 2];
		astratto = memcmp(in + ip + 4, g_rpc_srvsvc, 16) == 0 && rpc_le16(in + ip + 20) == 3 &&
		           rpc_le16(in + ip + 22) == 0;
		ip += 24;
		if (nt == 0 || ip + (size_t) nt * 20 > flen) {
			return 0;
		}
		for (t = 0; t < nt; t++) {
			const uint8_t *sx = in + ip + (size_t) t * 20;
			if (memcmp(sx, g_rpc_ndr32, 16) == 0 && rpc_le32(sx + 16) == 2) {
				ndr32 = 1;
			}
		}
		ip += (size_t) nt * 20;
		if (!astratto) {
			res[i] = 2;
			why[i] = 1;
		} else if (!ndr32 || preso) {
			res[i] = 2;
			why[i] = 2;
		} else {
			res[i] = 0;
			why[i] = 0;
			*ctx_ok = id;
			preso = 1;
		}
	}



	op = (26 + sizeof(sec) + 3) & ~(size_t) 3;
	op += 4 + (size_t) n * 24;
	if (op > BS_RPC_MAX) {
		return 0;
	}
	memset(o, 0, op);
	rpc_head(o, 12, 3, (uint16_t) op, call_id);
	rpc_put16(o + 16, rpc_le16(in + 16));
	rpc_put16(o + 18, rpc_le16(in + 18));
	rpc_put32(o + 20, 1);
	rpc_put16(o + 24, (uint16_t) sizeof(sec));
	memcpy(o + 26, sec, sizeof(sec));
	op = (26 + sizeof(sec) + 3) & ~(size_t) 3;
	o[op] = n;
	op += 4;
	for (i = 0; i < n; i++) {
		rpc_put16(o + op, res[i]);
		rpc_put16(o + op + 2, why[i]);
		if (res[i] == 0) {
			memcpy(o + op + 4, g_rpc_ndr32, 16);
			rpc_put32(o + op + 20, 2);
		}
		op += 24;
	}
	return (uint32_t) op;
}


static void rpc_share_struct(struct rpc_w *w, uint32_t level, const struct rpc_share *sh)
{
	rw_ptr(w, 1);
	if (level >= 1) {
		rw_u32(w, sh->type);
		rw_ptr(w, 1);
	}
	if (level == 2) {
		rw_u32(w, 0);
		rw_u32(w, 0xFFFFFFFFu);
		rw_u32(w, 0);
		rw_ptr(w, 1);
		rw_ptr(w, 1);
	}
}

static void rpc_share_strings(struct rpc_w *w, uint32_t level, const struct rpc_share *sh)
{
	rw_str(w, sh->name);
	if (level >= 1) {
		rw_str(w, sh->remark);
	}
	if (level == 2) {
		rw_str(w, sh->name);
		rw_str(w, "");
	}
}



static uint32_t rpc_srvsvc(struct bs_handle *h, const uint8_t *in, size_t len)
{
	struct rpc_share shares[2];
	uint8_t *o = h->rpc_buf;
	uint16_t flen;
	uint16_t ctx;
	uint16_t opnum;
	uint32_t call_id;
	uint32_t level = 0;
	uint32_t i;
	struct rpc_r r;
	struct rpc_w w;
	char a[64];
	char b[64];
	const struct rpc_share *trovata = NULL;

	if (o == NULL || in == NULL || len < 16 || in[0] != 5 || in[1] != 0 || in[4] != 0x10 || in[5] != 0 ||
	    in[6] != 0 || in[7] != 0) {
		return 0;
	}
	flen = rpc_le16(in + 8);
	if (flen < 16 || flen > len || rpc_le16(in + 10) != 0) {
		return 0;
	}
	call_id = rpc_le32(in + 12);
	if (in[2] == 11) {
		uint32_t n = rpc_bind_ack(in, flen, call_id, &h->rpc_ctx, o);
		bs_log("srvsvc: bind -> contesto %u", (unsigned) h->rpc_ctx);
		return n;
	}
	if (in[2] != 0 || flen < 24) {
		return rpc_fault(o, call_id, h->rpc_ctx, RPC_FAULT_PROTO);
	}
	ctx = rpc_le16(in + 20);
	opnum = rpc_le16(in + 22);
	if (ctx != h->rpc_ctx) {
		return rpc_fault(o, call_id, ctx, RPC_FAULT_CONTEXT);
	}
	if ((in[3] & 0x80) != 0 || (in[3] & 3) != 3) {
		return rpc_fault(o, call_id, ctx, RPC_FAULT_PROTO);
	}
	shares[0].name = g_share;
	shares[0].type = 0x00000000u;
	shares[0].remark = "BMC64-NG SD card";
	shares[1].name = "IPC$";
	shares[1].type = 0x80000003u;
	shares[1].remark = "Remote IPC";
	memset(&r, 0, sizeof(r));
	r.b = in + 24;
	r.n = (size_t) flen - 24;
	memset(&w, 0, sizeof(w));
	w.b = o + 24;
	w.cap = BS_RPC_MAX - 24;
	w.ref = 0x00020000u - 4;
	switch (opnum) {
	case 15: {
		uint32_t sw;
		int resume;
		rr_unique_str(&r, a, sizeof(a));
		level = rr_u32(&r);
		sw = rr_u32(&r);
		if (rr_u32(&r) != 0) {
			rr_u32(&r);
			if (rr_u32(&r) != 0) {
				r.err = 1;
			}
		}
		rr_u32(&r);
		resume = rr_u32(&r) != 0;
		if (resume) {
			rr_u32(&r);
		}
		if (r.err || sw != level || level > 2) {
			return rpc_fault(o, call_id, ctx, RPC_FAULT_NDR);
		}
		rw_u32(&w, level);
		rw_u32(&w, level);
		rw_ptr(&w, 1);
		rw_u32(&w, 2);
		rw_ptr(&w, 1);
		rw_u32(&w, 2);
		for (i = 0; i < 2; i++) {
			rpc_share_struct(&w, level, &shares[i]);
		}
		for (i = 0; i < 2; i++) {
			rpc_share_strings(&w, level, &shares[i]);
		}
		rw_u32(&w, 2);
		if (resume) {
			rw_ptr(&w, 1);
			rw_u32(&w, 0);
		} else {
			rw_u32(&w, 0);
		}
		rw_u32(&w, 0);
		bs_log("srvsvc: NetrShareEnum livello %u", level);
		break;
	}
	case 16: {
		rr_unique_str(&r, a, sizeof(a));
		rr_str(&r, b, sizeof(b));
		level = rr_u32(&r);
		if (r.err || level > 2) {
			return rpc_fault(o, call_id, ctx, RPC_FAULT_NDR);
		}
		for (i = 0; i < 2; i++) {
			if (strcasecmp(b, shares[i].name) == 0) {
				trovata = &shares[i];
			}
		}
		rw_u32(&w, level);
		if (trovata != NULL) {
			rw_ptr(&w, 1);
			rpc_share_struct(&w, level, trovata);
			rpc_share_strings(&w, level, trovata);
			rw_u32(&w, 0);
		} else {
			rw_ptr(&w, 0);
			rw_u32(&w, 2310);
		}
		bs_log("srvsvc: NetrShareGetInfo '%s' livello %u", b, level);
		break;
	}
	case 20: {
		rr_unique_str(&r, a, sizeof(a));
		rr_str(&r, b, sizeof(b));
		if (r.err) {
			return rpc_fault(o, call_id, ctx, RPC_FAULT_NDR);
		}
		for (i = 0; i < 2; i++) {
			if (strcasecmp(b, shares[i].name) == 0) {
				trovata = &shares[i];
			}
		}
		rw_u32(&w, trovata != NULL ? trovata->type : 0);
		rw_u32(&w, trovata != NULL ? 0 : 2311);
		bs_log("srvsvc: NetrShareCheck '%s'", b);
		break;
	}
	case 21: {
		rr_unique_str(&r, a, sizeof(a));
		level = rr_u32(&r);
		if (r.err || level != 101) {
			return rpc_fault(o, call_id, ctx, RPC_FAULT_NDR);
		}
		rw_u32(&w, 101);
		rw_ptr(&w, 1);
		rw_u32(&w, 500);
		rw_ptr(&w, 1);
		rw_u32(&w, 10);
		rw_u32(&w, 0);
		rw_u32(&w, 0x00008002u);
		rw_ptr(&w, 1);
		rw_str(&w, g_hostname);
		rw_str(&w, "BMC64-NG");
		rw_u32(&w, 0);
		bs_log("srvsvc: NetrServerGetInfo livello 101");
		break;
	}
	default:
		bs_log("srvsvc: opnum %u", (unsigned) opnum);
		return rpc_fault(o, call_id, ctx, RPC_FAULT_OP_RANGE);
	}
	if (w.err || 24 + w.n > 0xFFFF) {
		return rpc_fault(o, call_id, ctx, RPC_FAULT_NDR);
	}
	rpc_head(o, 2, 3, (uint16_t) (24 + w.n), call_id);
	rpc_put32(o + 16, (uint32_t) w.n);
	rpc_put16(o + 20, ctx);
	o[22] = 0;
	o[23] = 0;
	return (uint32_t) (24 + w.n);
}

static int create_handler(struct smb2_server *srvr, struct smb2_context *smb2, struct smb2_create_request *req,
                          struct smb2_create_reply *rep)
{
	char path[MAX_PATH_BYTES];
	struct bs_ctx_req cr;
	struct bs_tree *tree;
	struct bs_info info;
	struct bs_handle *h;
	struct bs_file *of;
	uint32_t status;
	uint32_t action = ACT_OPENED;
	uint32_t access;
	uint32_t disp;
	int dir;
	int want_dir;
	int want_file;
	int writable;
	int fidx = -1;
	FRESULT fr;
	struct bs_conn *conn = conn_of(smb2);
	(void) srvr;

	if (req == NULL || rep == NULL) {
		return create_fail(smb2, req, BS_INVALID_PARAMETER);
	}
	tree = find_tree(smb2, current_tree(smb2));
	if (tree == NULL) {
		return create_fail(smb2, req, BS_NETWORK_NAME_DELETED);
	}
	if (tree->ipc) {

		const char *nome = req->name != NULL ? req->name : "";
		while (*nome == '\\' || *nome == '/') {
			nome++;
		}
		if (strcasecmp(nome, "srvsvc") != 0) {
			bs_log("create '%s' su IPC$ -> non c'e'", nome);
			return create_fail(smb2, req, BS_OBJECT_NAME_NOT_FOUND);
		}
		h = handle_alloc(smb2, current_tree(smb2), "srvsvc", 0, req->desired_access);
		if (h == NULL) {
			return create_fail(smb2, req, BS_INSUFFICIENT_RESOURCES);
		}
		h->rpc_buf = malloc(BS_RPC_MAX);
		if (h->rpc_buf == NULL) {
			handle_free(h);
			return create_fail(smb2, req, BS_INSUFFICIENT_RESOURCES);
		}
		h->pipe = 1;
		h->rpc_ctx = 0xFFFF;
		memset(rep, 0, sizeof(*rep));
		make_fid(h, rep->file_id);
		rep->oplock_level = SMB2_OPLOCK_LEVEL_NONE;
		rep->create_action = ACT_OPENED;
		rep->file_attributes = FA_ATTR_NORMAL;
		if (conn != NULL) {
			memcpy(conn->last_fid, rep->file_id, SMB2_FD_SIZE);
			conn->have_last = 1;
			conn->last_status = 0;
		}
		bs_log("create 'srvsvc' su IPC$ -> pipe, slot %d", (int) (h - g_handles));
		return 0;
	}
	if (parse_contexts(req, &cr) != 0) {
		return create_fail(smb2, req, BS_INVALID_PARAMETER);
	}
	if (req->create_options & OPT_OPEN_BY_FILE_ID) {
		return create_fail(smb2, req, BS_NOT_SUPPORTED);
	}
	disp = req->create_disposition;
	if (disp > DISP_OVERWRITE_IF) {
		return create_fail(smb2, req, BS_INVALID_PARAMETER);
	}
	status = smb_path_to_fat(req->name, path, sizeof(path));
	if (status != BS_SUCCESS) {
		return create_fail(smb2, req, status);
	}
	access = req->desired_access;
	want_dir = (req->create_options & OPT_DIRECTORY_FILE) != 0;
	want_file = (req->create_options & OPT_NON_DIRECTORY_FILE) != 0;
	if (want_dir && want_file) {
		return create_fail(smb2, req, BS_INVALID_PARAMETER);
	}
	stat_path(path, &info);
	of = open_file_of(path);
	if (of != NULL && of->delete_pending) {
		return create_fail(smb2, req, BS_DELETE_PENDING);
	}
	if (info.exists) {
		dir = info.dir;
		if (want_dir && !dir) {
			return create_fail(smb2, req, BS_NOT_A_DIRECTORY);
		}
		if (want_file && dir) {
			return create_fail(smb2, req, BS_FILE_IS_A_DIRECTORY);
		}
		if (disp == DISP_CREATE) {
			return create_fail(smb2, req, BS_OBJECT_NAME_COLLISION);
		}
		if (dir && (disp == DISP_OVERWRITE || disp == DISP_OVERWRITE_IF || disp == DISP_SUPERSEDE)) {
			return create_fail(smb2, req, BS_FILE_IS_A_DIRECTORY);
		}
		{
			int overwrite = disp == DISP_OVERWRITE || disp == DISP_OVERWRITE_IF || disp == DISP_SUPERSEDE;
			int busy = in_use(path, dir);
			if (busy && (explicit_write(access) || (access & (AC_DELETE | AC_GENERIC_ALL)) || overwrite
			             || (req->create_options & OPT_DELETE_ON_CLOSE))) {
				return create_fail(smb2, req, BS_SHARING_VIOLATION);
			}
			if (!dir && (info.attr & AM_RDO)) {
				if (explicit_write(access) || overwrite) {
					return create_fail(smb2, req, BS_ACCESS_DENIED);
				}
				if (req->create_options & OPT_DELETE_ON_CLOSE) {
					return create_fail(smb2, req, BS_CANNOT_DELETE);
				}
			}
			if ((busy || (!dir && (info.attr & AM_RDO))) && (access & AC_MAXIMUM_ALLOWED)) {
				access = (access & ~AC_MAXIMUM_ALLOWED) | AC_READ_DATA | AC_READ_EA | AC_READ_ATTRIBUTES
				       | AC_EXECUTE | AC_READ_CONTROL | AC_SYNCHRONIZE;
			}
		}
	} else {
		if (disp == DISP_OPEN || disp == DISP_OVERWRITE) {
			char parent[MAX_PATH_BYTES];
			char *slash;
			snprintf(parent, sizeof(parent), "%s", path);
			slash = strrchr(parent, '/');
			if (slash != NULL && slash > parent + strlen(g_volume)) {
				struct bs_info pi;
				*slash = 0;
				stat_path(parent, &pi);
				if (!pi.exists || !pi.dir) {
					return create_fail(smb2, req, BS_OBJECT_PATH_NOT_FOUND);
				}
			}
			return create_fail(smb2, req, BS_OBJECT_NAME_NOT_FOUND);
		}
		if (is_root(path)) {
			return create_fail(smb2, req, BS_OBJECT_NAME_NOT_FOUND);
		}
		dir = want_dir;
		if (in_use(path, dir)) {
			return create_fail(smb2, req, BS_SHARING_VIOLATION);
		}
	}

	h = handle_alloc(smb2, tree->id, path, dir, access);
	if (h == NULL) {
		return create_fail(smb2, req, BS_INSUFFICIENT_RESOURCES);
	}

	if (dir) {
		if (!info.exists) {
			fr = f_mkdir(path);
			if (fr != FR_OK) {
				handle_free(h);
				return create_fail(smb2, req, fr_to_status(fr));
			}
			action = ACT_CREATED;
			stat_path(path, &info);
		}
	} else {
		BYTE mode = FA_OPEN_EXISTING;
		writable = need_write(access) || disp == DISP_OVERWRITE || disp == DISP_OVERWRITE_IF
		        || disp == DISP_SUPERSEDE || !info.exists;
		if (info.attr & AM_RDO) {
			writable = 0;
		}
		if (!info.exists) {
			mode = FA_CREATE_NEW;
			action = ACT_CREATED;
		} else if (disp == DISP_OVERWRITE || disp == DISP_OVERWRITE_IF || disp == DISP_SUPERSEDE) {
			if (handles_on_path(path, h) > 0) {
				handle_free(h);
				return create_fail(smb2, req, BS_SHARING_VIOLATION);
			}
			mode = FA_CREATE_ALWAYS;
			action = disp == DISP_SUPERSEDE ? ACT_SUPERSEDED : ACT_OVERWRITTEN;
		}
		if (need_read(access) || need_write(access) || mode != FA_OPEN_EXISTING) {
			status = file_acquire(path, mode, writable, &fidx);
			if (status != BS_SUCCESS) {
				handle_free(h);
				return create_fail(smb2, req, status);
			}
			h->file = fidx;
			if (mode == FA_CREATE_ALWAYS || mode == FA_CREATE_NEW) {
				uint32_t a = req->file_attributes & (FA_ATTR_READONLY | FA_ATTR_HIDDEN | FA_ATTR_SYSTEM | FA_ATTR_ARCHIVE);
				if (a & (FA_ATTR_HIDDEN | FA_ATTR_SYSTEM)) {
					g_files[fidx].attr_pending = 1;
					g_files[fidx].attr = (BYTE) (a | AM_ARC);
				}
			}
		}
		if (action != ACT_OPENED) {
			stat_path(path, &info);
		}
	}
	if (req->create_options & OPT_DELETE_ON_CLOSE) {
		if (!(access & (AC_DELETE | AC_GENERIC_ALL | AC_MAXIMUM_ALLOWED))) {
			handle_close(h, 0);
			return create_fail(smb2, req, BS_ACCESS_DENIED);
		}
		h->delete_on_close = 1;
	}

	memset(rep, 0, sizeof(*rep));
	make_fid(h, rep->file_id);
	rep->oplock_level = (req->requested_oplock_level == SMB2_OPLOCK_LEVEL_LEASE && cr.lease)
	                  ? SMB2_OPLOCK_LEVEL_LEASE : SMB2_OPLOCK_LEVEL_NONE;
	rep->create_action = action;
	rep->creation_time = info_time(&info);
	rep->last_access_time = rep->creation_time;
	rep->last_write_time = rep->creation_time;
	rep->change_time = rep->creation_time;
	rep->end_of_file = dir ? 0 : info.size;
	rep->allocation_size = alloc_size(info.size, dir);
	rep->file_attributes = fat_attr_to_smb(info.attr, dir);
	fill_contexts(&cr, path, dir, rep->oplock_level == SMB2_OPLOCK_LEVEL_LEASE, rep);
	if (conn != NULL) {
		memcpy(conn->last_fid, rep->file_id, SMB2_FD_SIZE);
		conn->have_last = 1;
		conn->last_status = 0;
	}
	bs_log("create '%s' disp %u acc %08x opt %08x -> %s act %u slot %d", req->name ? req->name : "", disp, access,
	     req->create_options, path, action, (int) (h - g_handles));
	return 0;
}

static void fill_close_attrs(const char *path, int dir, struct smb2_close_reply *rep)
{
	struct bs_info info;
	stat_path(path, &info);
	if (!info.exists) {
		return;
	}
	rep->flags = SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB;
	rep->creation_time = info_time(&info);
	rep->last_access_time = rep->creation_time;
	rep->last_write_time = rep->creation_time;
	rep->change_time = rep->creation_time;
	rep->end_of_file = dir ? 0 : info.size;
	rep->allocation_size = alloc_size(info.size, dir);
	rep->file_attributes = fat_attr_to_smb(info.attr, dir);
}

static int close_handler(struct smb2_server *srvr, struct smb2_context *smb2, struct smb2_close_request *req,
                         struct smb2_close_reply *rep)
{
	struct bs_handle *h;
	uint32_t status;
	char path[MAX_PATH_BYTES];
	int dir;
	(void) srvr;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_CLOSE, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	snprintf(path, sizeof(path), "%s", h->path);
	dir = h->dir;
	status = handle_close(h, 1);
	if (status != BS_SUCCESS) {
		bs_log("close %s -> %08x", path, status);
		return reply_status(smb2, SMB2_CLOSE, status);
	}
	memset(rep, 0, sizeof(*rep));
	if (req->flags & SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB) {
		fill_close_attrs(path, dir, rep);
	}
	return 0;
}

static int flush_handler(struct smb2_server *srvr, struct smb2_context *smb2, struct smb2_flush_request *req)
{
	struct bs_handle *h;
	FRESULT fr = FR_OK;
	(void) srvr;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_FLUSH, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	if (h->file >= 0) {
		struct bs_file *f = &g_files[h->file];
		fr = file_materialize(f);
		if (fr == FR_OK && f->writable) {
			fr = f_sync(&f->fil);
		}
	}
	if (fr != FR_OK) {
		return reply_status(smb2, SMB2_FLUSH, fr_to_status(fr));
	}
	return 0;
}

static int read_handler(struct smb2_server *srvr, struct smb2_context *smb2, struct smb2_read_request *req,
                        struct smb2_read_reply *rep)
{
	struct bs_handle *h;
	struct bs_file *f;
	uint64_t off;
	uint64_t n;
	uint64_t from_disk;
	uint8_t *buf;
	UINT br = 0;
	FRESULT fr;
	(void) srvr;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_READ, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	if (h->pipe) {

		uint32_t resto;
		uint32_t quanti;
		if (h->rpc_pos >= h->rpc_len) {
			return reply_status(smb2, SMB2_READ, BS_PIPE_EMPTY);
		}
		resto = h->rpc_len - h->rpc_pos;
		quanti = req->length < resto ? req->length : resto;
		buf = malloc(quanti ? quanti : 1);
		if (buf == NULL) {
			return reply_status(smb2, SMB2_READ, BS_INSUFFICIENT_RESOURCES);
		}
		memcpy(buf, h->rpc_buf + h->rpc_pos, quanti);
		h->rpc_pos += quanti;
		memset(rep, 0, sizeof(*rep));
		rep->data = buf;
		rep->data_length = quanti;
		rep->data_remaining = h->rpc_len - h->rpc_pos;
		if (h->rpc_pos >= h->rpc_len) {
			h->rpc_len = 0;
			h->rpc_pos = 0;
		}
		return 0;
	}
	if (h->dir) {
		return reply_status(smb2, SMB2_READ, BS_INVALID_DEVICE_REQUEST);
	}
	if (h->file < 0 || !need_read(h->access)) {
		return reply_status(smb2, SMB2_READ, BS_ACCESS_DENIED);
	}
	f = &g_files[h->file];
	off = req->offset;
	if (off >= f->eof) {
		return reply_status(smb2, SMB2_READ, BS_END_OF_FILE);
	}
	n = f->eof - off;
	if (n > req->length) {
		n = req->length;
	}
	if (n < req->minimum_count) {
		return reply_status(smb2, SMB2_READ, BS_END_OF_FILE);
	}
	buf = malloc(n ? (size_t) n : 1);
	if (buf == NULL) {
		return reply_status(smb2, SMB2_READ, BS_INSUFFICIENT_RESOURCES);
	}
	from_disk = off < f->phys ? f->phys - off : 0;
	if (from_disk > n) {
		from_disk = n;
	}
	if (from_disk > 0) {
		fr = f_lseek(&f->fil, (FSIZE_t) off);
		if (fr == FR_OK) {
			fr = f_read(&f->fil, buf, (UINT) from_disk, &br);
		}
		if (fr != FR_OK || br != from_disk) {
			free(buf);
			bs_log("read %s off %llu -> fr %d br %u", f->path, (unsigned long long) off, (int) fr, br);
			return reply_status(smb2, SMB2_READ, BS_IO_DEVICE_ERROR);
		}
	}
	if (n > from_disk) {
		memset(buf + from_disk, 0, (size_t) (n - from_disk));
	}
	h->position = off + n;
	memset(rep, 0, sizeof(*rep));
	rep->data = buf;
	rep->data_length = (uint32_t) n;
	rep->data_remaining = 0;
	return 0;
}

static int write_handler(struct smb2_server *srvr, struct smb2_context *smb2, struct smb2_write_request *req,
                         struct smb2_write_reply *rep)
{
	struct bs_handle *h;
	struct bs_file *f;
	uint64_t off;
	UINT bw = 0;
	FRESULT fr;
	(void) srvr;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_WRITE, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	if (h->pipe) {

		if (h->rpc_pos < h->rpc_len) {
			return reply_status(smb2, SMB2_WRITE, BS_PIPE_BUSY);
		}
		h->rpc_len = rpc_srvsvc(h, req->buf, req->length);
		h->rpc_pos = 0;
		if (h->rpc_len == 0) {
			return reply_status(smb2, SMB2_WRITE, BS_INVALID_PARAMETER);
		}
		memset(rep, 0, sizeof(*rep));
		rep->count = req->length;
		rep->remaining = 0;
		return 0;
	}
	if (h->dir) {
		return reply_status(smb2, SMB2_WRITE, BS_INVALID_DEVICE_REQUEST);
	}
	if (h->file < 0 || !need_write(h->access) || !g_files[h->file].writable) {
		return reply_status(smb2, SMB2_WRITE, BS_ACCESS_DENIED);
	}
	f = &g_files[h->file];
	off = req->offset;
	if (off == 0xFFFFFFFFFFFFFFFFULL) {
		off = f->eof;
	}
	if (off + req->length > 0xFFFFFFFFULL && f->fil.obj.fs->fs_type != FS_EXFAT) {
		return reply_status(smb2, SMB2_WRITE, BS_DISK_FULL);
	}
	fr = FR_OK;
	if (off > f->phys) {
		fr = zero_fill(&f->fil, f->phys, (FSIZE_t) off);
		if (fr == FR_OK) {
			f->phys = (FSIZE_t) off;
		}
	}
	if (fr == FR_OK) {
		fr = f_lseek(&f->fil, (FSIZE_t) off);
	}
	if (fr == FR_OK && req->length > 0) {
		fr = f_write(&f->fil, req->buf, req->length, &bw);
	}
	if (fr != FR_OK || bw != req->length) {
		f->failed = 1;
		bs_log("write %s off %llu len %u -> fr %d bw %u", f->path, (unsigned long long) off, req->length, (int) fr, bw);
		return reply_status(smb2, SMB2_WRITE, fr == FR_OK ? BS_DISK_FULL : fr_to_status(fr));
	}
	if (off + bw > f->phys) {
		f->phys = (FSIZE_t) (off + bw);
	}
	if (f->phys > f->eof) {
		f->eof = f->phys;
	}
	if (req->flags & SMB2_WRITEFLAG_WRITE_THROUGH) {
		f_sync(&f->fil);
	}
	h->position = off + bw;
	memset(rep, 0, sizeof(*rep));
	rep->count = bw;
	rep->remaining = 0;
	return 0;
}

static int oplock_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                          struct smb2_oplock_break_acknowledgement *req)
{
	(void) srvr;
	(void) req;
	return reply_status(smb2, SMB2_OPLOCK_BREAK, BS_INVALID_PARAMETER);
}

static int lease_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                         struct smb2_lease_break_acknowledgement *req)
{
	(void) srvr;
	(void) req;
	return reply_status(smb2, SMB2_OPLOCK_BREAK, BS_INVALID_PARAMETER);
}

static int lock_handler(struct smb2_server *srvr, struct smb2_context *smb2, struct smb2_lock_request *req)
{
	struct bs_handle *h;
	(void) srvr;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_LOCK, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	if (h->dir || req->lock_count == 0) {
		return reply_status(smb2, SMB2_LOCK, BS_INVALID_PARAMETER);
	}
	return 0;
}



static void ioctl_input_free(struct smb2_context *smb2, struct smb2_ioctl_request *req)
{
	if (req->input != NULL) {
		smb2_free_data(smb2, req->input);
		req->input = NULL;
	}
}

static int ioctl_handler(struct smb2_server *srvr, struct smb2_context *smb2, struct smb2_ioctl_request *req,
                         struct smb2_ioctl_reply *rep)
{
	uint32_t status = BS_NOT_SUPPORTED;
	(void) srvr;
	(void) rep;
	if (req == NULL) {
		return reply_status(smb2, SMB2_IOCTL, BS_INVALID_PARAMETER);
	}


	if (req->flags == SMB2_0_IOCTL_IS_FSCTL && req->ctl_code == FSCTL_PIPE_WAIT_CODE) {
		bs_log("ioctl %08x -> attesa della pipe finita", req->ctl_code);
		ioctl_input_free(smb2, req);
		return 0;
	}
	if (req->flags == SMB2_0_IOCTL_IS_FSCTL && req->ctl_code == FSCTL_PIPE_TRANSCEIVE_CODE) {
		struct bs_handle *h = find_handle(smb2, req->file_id);
		struct smb2_pdu *pdu;
		uint32_t n;
		if (h == NULL || !h->pipe) {
			ioctl_input_free(smb2, req);
			return reply_status(smb2, SMB2_IOCTL, h == NULL ? stato_senza_handle(smb2, req->file_id) : BS_INVALID_DEVICE_REQUEST);
		}
		if (req->input == NULL || req->input_count == 0) {
			return reply_status(smb2, SMB2_IOCTL, BS_INVALID_PARAMETER);
		}
		if (h->rpc_pos < h->rpc_len) {
			ioctl_input_free(smb2, req);
			return reply_status(smb2, SMB2_IOCTL, BS_PIPE_BUSY);
		}
		n = rpc_srvsvc(h, (const uint8_t *) req->input, req->input_count);
		ioctl_input_free(smb2, req);
		if (n == 0) {
			return reply_status(smb2, SMB2_IOCTL, BS_INVALID_PARAMETER);
		}
		if (n > req->max_output_response) {
			bs_log("srvsvc: risposta di %u byte, il client ne vuole %u", n, req->max_output_response);
			return reply_status(smb2, SMB2_IOCTL, BS_BUFFER_TOO_SMALL);
		}
		rep->input_count = 0;
		rep->output_count = n;
		rep->output = h->rpc_buf;
		rep->flags = 0;

		smb2_set_passthrough(smb2, 1);
		pdu = smb2_cmd_ioctl_reply_async(smb2, rep, NULL, NULL);
		smb2_set_passthrough(smb2, 0);
		if (pdu == NULL) {
			return -1;
		}
		smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
		smb2_queue_pdu(smb2, pdu);
		return 1;
	}
	if (req->flags == SMB2_0_IOCTL_IS_FSCTL) {
		switch (req->ctl_code) {
		case FSCTL_DFS_GET_REFERRALS:
		case FSCTL_DFS_GET_REFERRALS_EX:
			status = BS_FS_DRIVER_REQUIRED;
			break;
		case FSCTL_GET_REPARSE_POINT:
			status = BS_NOT_A_REPARSE_POINT;
			break;
		case FSCTL_SRV_REQUEST_RESUME_KEY:
		case FSCTL_SRV_COPYCHUNK:
		case FSCTL_SRV_COPYCHUNK_WRITE:
		case FSCTL_QUERY_NETWORK_INTERFACE:
		case FSCTL_PIPE_TRANSCEIVE_CODE:
		case FSCTL_PIPE_WAIT_CODE:
			status = BS_NOT_SUPPORTED;
			break;
		default:
			status = BS_INVALID_DEVICE_REQUEST;
			break;
		}
	}
	bs_log("ioctl %08x -> %08x", req->ctl_code, status);
	return reply_status(smb2, SMB2_IOCTL, status);
}

static int cancel_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
	int i;
	uint64_t async_id = smb2->hdr.async.async_id;
	uint64_t mid = smb2->hdr.message_id;
	int is_async = (smb2->hdr.flags & SMB2_FLAGS_ASYNC_COMMAND) != 0;
	(void) srvr;
	for (i = 0; i < MAX_HANDLES; i++) {
		struct bs_handle *h = &g_handles[i];
		if (!h->used || h->owner != smb2 || !h->notify_pending) {
			continue;
		}
		if (is_async) {
			struct smb2_pdu *p = smb2_find_pdu(smb2, h->notify_mid);
			if (p == NULL || p->header.async.async_id != async_id) {
				continue;
			}
		} else if (h->notify_mid != mid) {
			continue;
		}
		notify_complete(h, BS_CANCELLED);
		break;
	}
	return 0;
}

static int echo_handler(struct smb2_server *srvr, struct smb2_context *smb2)
{
	(void) srvr;
	(void) smb2;
	return 0;
}

static int dir_fixed_size(uint8_t cls)
{
	switch (cls) {
	case SMB2_FILE_DIRECTORY_INFORMATION:
		return 64;
	case SMB2_FILE_FULL_DIRECTORY_INFORMATION:
		return 68;
	case SMB2_FILE_BOTH_DIRECTORY_INFORMATION:
		return 94;
	case 0x0C:
		return 12;
	case SMB2_FILE_ID_BOTH_DIRECTORY_INFORMATION:
		return 104;
	case SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION:
		return 80;
	default:
		return 0;
	}
}

static int encode_dir_entry(uint8_t cls, uint8_t *out, size_t room, const char *fat_name, int dir, BYTE attr,
                            WORD fdate, WORD ftime, FSIZE_t size, uint64_t file_id)
{
	int fixed = dir_fixed_size(cls);
	uint8_t name16[FF_LFN_BUF * 2 + 4];
	int nlen = fat_to_utf16(fat_name, name16, sizeof(name16));
	uint64_t t;
	uint64_t eof = dir ? 0 : (uint64_t) size;
	uint64_t al = alloc_size(size, dir);
	uint32_t fa = fat_attr_to_smb(attr, dir);
	if (nlen < 0 || (size_t) (fixed + nlen) > room) {
		return -1;
	}
	memset(out, 0, (size_t) fixed);
	t = fdate ? fat_to_filetime(fdate, ftime) : g_root_time;
	if (cls == 0x0C) {
		put32(out + 8, (uint32_t) nlen);
		memcpy(out + 12, name16, (size_t) nlen);
		return fixed + nlen;
	}
	put64(out + 8, t);
	put64(out + 16, t);
	put64(out + 24, t);
	put64(out + 32, t);
	put64(out + 40, eof);
	put64(out + 48, al);
	put32(out + 56, fa);
	put32(out + 60, (uint32_t) nlen);
	if (cls == SMB2_FILE_ID_BOTH_DIRECTORY_INFORMATION) {
		put64(out + 96, file_id);
	} else if (cls == SMB2_FILE_ID_FULL_DIRECTORY_INFORMATION) {
		put64(out + 72, file_id);
	}
	memcpy(out + fixed, name16, (size_t) nlen);
	return fixed + nlen;
}

static void set_pattern(struct bs_handle *h, const char *name)
{
	const unsigned char *s = (const unsigned char *) name;
	uint32_t cp;
	int n = 0;
	int wild = 0;
	if (name == NULL || name[0] == 0) {
		s = (const unsigned char *) "*";
	}
	while (n < MAX_PATTERN && utf8_next(&s, &cp) > 0) {
		if (cp == '*' || cp == '?' || cp == '<' || cp == '>' || cp == '"') {
			wild = 1;
		}
		h->pattern[n++] = (uint16_t) ff_wtoupper(cp > 0xFFFF ? '?' : cp);
	}
	h->pattern_len = n;
	h->pattern_set = 1;
	h->pattern_wild = wild;
}

static int wild_match(const uint16_t *p, int pl, const uint16_t *n, int nl)
{
	int k;
	if (pl == 0) {
		return nl == 0;
	}
	switch (p[0]) {
	case '*':
	case '<':
		for (k = 0; k <= nl; k++) {
			if (wild_match(p + 1, pl - 1, n + k, nl - k)) {
				return 1;
			}
		}
		return 0;
	case '?':
		return nl > 0 && wild_match(p + 1, pl - 1, n + 1, nl - 1);
	case '>':
		if (nl == 0 || n[0] == '.') {
			return wild_match(p + 1, pl - 1, n, nl);
		}
		return wild_match(p + 1, pl - 1, n + 1, nl - 1);
	case '"':
		if (nl == 0) {
			return wild_match(p + 1, pl - 1, n, nl);
		}
		return n[0] == '.' && wild_match(p + 1, pl - 1, n + 1, nl - 1);
	default:
		return nl > 0 && p[0] == n[0] && wild_match(p + 1, pl - 1, n + 1, nl - 1);
	}
}

static int name_matches(const struct bs_handle *h, const char *fat_name)
{
	uint16_t n[FF_LFN_BUF + 1];
	int nl = 0;
	const unsigned char *s = (const unsigned char *) fat_name;
	while (*s && nl < FF_LFN_BUF) {
		uint32_t cp = fat_cp(&s);
		n[nl++] = (uint16_t) (cp > 0xFFFF ? 0xFFFD : fat_upper(cp));
	}
	return wild_match(h->pattern, h->pattern_len, n, nl);
}

static int dir_next(struct bs_handle *h, int *kind, FILINFO *fno)
{
	FRESULT fr;
	if (h->held) {
		*kind = h->held_kind;
		*fno = h->held_fno;
		h->held = 0;
		return 1;
	}
	if (h->dir_stage == 0) {
		h->dir_stage = 1;
		memset(fno, 0, sizeof(*fno));
		strcpy(fno->fname, ".");
		*kind = 1;
		return 1;
	}
	if (h->dir_stage == 1) {
		h->dir_stage = 2;
		memset(fno, 0, sizeof(*fno));
		strcpy(fno->fname, "..");
		*kind = 2;
		return 1;
	}
	if (!h->dj_open) {
		fr = f_opendir(&h->dj, h->path);
		if (fr != FR_OK) {
			return -1;
		}
		h->dj_open = 1;
	}
	for (;;) {
		fr = f_readdir(&h->dj, fno);
		if (fr != FR_OK) {
			return -1;
		}
		if (fno->fname[0] == 0) {
			return 0;
		}
		if (fat_name_ok(fno->fname)) {
			*kind = 3;
			return 1;
		}
	}
}

static int query_dir_reply(struct smb2_context *smb2, struct smb2_query_directory_request *req, uint8_t *buf,
                           uint32_t len)
{
	struct smb2_query_directory_reply rep;
	struct smb2_pdu *pdu;
	memset(&rep, 0, sizeof(rep));
	rep.output_buffer = buf;
	rep.output_buffer_length = len;
	smb2_set_passthrough(smb2, 1);
	pdu = smb2_cmd_query_directory_reply_async(smb2, req, &rep, NULL, NULL);
	smb2_set_passthrough(smb2, 0);
	if (pdu == NULL) {
		return -1;
	}
	smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
	smb2_queue_pdu(smb2, pdu);
	return 1;
}

static int query_directory_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                                   struct smb2_query_directory_request *req, struct smb2_query_directory_reply *rep)
{
	struct bs_handle *h;
	uint8_t cls;
	uint32_t room;
	uint8_t *buf;
	uint32_t used = 0;
	uint32_t last = 0;
	int count = 0;
	int kind = 0;
	int r;
	(void) srvr;
	(void) rep;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_QUERY_DIRECTORY, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	if (!h->dir) {
		return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_INVALID_PARAMETER);
	}
	cls = req->file_information_class;
	if (dir_fixed_size(cls) == 0) {
		bs_log("dir class %u not supported", cls);
		if (cls == 0x3C || cls == 0x3F || (cls >= 0x4E && cls <= 0x51)) {
			return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_NOT_SUPPORTED);
		}
		return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_INVALID_INFO_CLASS);
	}
	if (req->flags & (SMB2_RESTART_SCANS | SMB2_REOPEN)) {
		if (h->dj_open) {
			f_closedir(&h->dj);
			h->dj_open = 0;
		}
		h->dir_stage = 0;
		h->dir_ended = 0;
		h->dir_any = 0;
		h->held = 0;
		h->pattern_set = 0;
	}
	if (!h->pattern_set) {
		set_pattern(h, req->name);
		if (is_root(h->path) || !h->pattern_wild) {
			h->dir_stage = 2;
		}
	}
	if (h->dir_ended) {
		return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_NO_MORE_FILES);
	}
	room = req->output_buffer_length;
	if (room > 65536) {
		room = 65536;
	}
	buf = malloc(room ? room : 1);
	if (buf == NULL) {
		return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_INSUFFICIENT_RESOURCES);
	}
	if (!h->pattern_wild && h->dir_stage == 2 && !h->held && !h->dj_open) {
		char full[MAX_PATH_BYTES];
		char nome[FF_LFN_BUF + 1];
		const unsigned char *s = (const unsigned char *) (req->name ? req->name : "");
		const unsigned char *s0 = s;
		size_t n = 0;
		uint32_t cp;
		int ok = 1;
		int r;
		while ((r = utf8_next(&s, &cp)) > 0) {
			size_t k = (size_t) (s - s0);
			if ((cp >= 0xD800 && cp <= 0xDFFF) || n + k >= sizeof(nome)) {
				ok = 0;
				break;
			}
			memcpy(nome + n, s0, k);
			n += k;
			s0 = s;
		}
		if (r < 0) {
			ok = 0;
		}
		nome[n] = 0;
		h->dir_stage = 3;
		h->dir_ended = 1;
		if (ok && n > 0 && snprintf(full, sizeof(full), "%s%s%s", h->path, is_root(h->path) ? "" : "/", nome)
		                   < (int) sizeof(full)) {
			if (f_stat(full, &g_fno) == FR_OK && fat_name_ok(g_fno.fname)) {
				struct bs_file *of;
				char child[MAX_PATH_BYTES];
				if (snprintf(child, sizeof(child), "%s%s%s", h->path, is_root(h->path) ? "" : "/", g_fno.fname)
				    >= (int) sizeof(child)) {
					free(buf);
					return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_NO_SUCH_FILE);
				}
				of = open_file_of(child);
				r = encode_dir_entry(cls, buf, room, g_fno.fname, (g_fno.fattrib & AM_DIR) != 0, g_fno.fattrib,
				                     g_fno.fdate, g_fno.ftime, of ? of->eof : g_fno.fsize, path_id(child));
				if (r < 0) {
					free(buf);
					return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_BUFFER_TOO_SMALL);
				}
				h->dir_any = 1;
				r = query_dir_reply(smb2, req, buf, (uint32_t) r);
				free(buf);
				return r;
			}
		}
		free(buf);
		return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_NO_SUCH_FILE);
	}
	for (;;) {
		FILINFO *fno = &g_iter_fno;
		struct bs_info self;
		char child[MAX_PATH_BYTES];
		uint64_t fid;
		int isdir;
		BYTE attr;
		WORD fd;
		WORD ft;
		FSIZE_t sz;
		uint32_t pos;
		r = dir_next(h, &kind, fno);
		if (r < 0) {
			free(buf);
			return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_IO_DEVICE_ERROR);
		}
		if (r == 0) {
			h->dir_ended = 1;
			break;
		}
		if (!name_matches(h, fno->fname)) {
			continue;
		}
		if (kind == 3) {
			struct bs_file *of;
			if (snprintf(child, sizeof(child), "%s%s%s", h->path, is_root(h->path) ? "" : "/", fno->fname)
			    >= (int) sizeof(child)) {
				continue;
			}
			of = open_file_of(child);
			isdir = (fno->fattrib & AM_DIR) != 0;
			attr = fno->fattrib;
			fd = fno->fdate;
			ft = fno->ftime;
			sz = of ? of->eof : fno->fsize;
			if (of && of->time_pending) {
				fd = of->fdate;
				ft = of->ftime;
			}
			fid = path_id(child);
		} else {
			stat_path(h->path, &self);
			isdir = 1;
			attr = AM_DIR;
			fd = kind == 1 ? self.fdate : 0;
			ft = kind == 1 ? self.ftime : 0;
			sz = 0;
			fid = kind == 1 ? path_id(h->path) : 0;
		}
		pos = (used + 7u) & ~7u;
		if (count > 0 && pos > room) {
			h->held = 1;
			h->held_kind = kind;
			h->held_fno = *fno;
			break;
		}
		r = count > 0 ? encode_dir_entry(cls, buf + pos, room - pos, fno->fname, isdir, attr, fd, ft, sz, fid)
		              : encode_dir_entry(cls, buf, room, fno->fname, isdir, attr, fd, ft, sz, fid);
		if (r < 0) {
			if (count == 0) {
				free(buf);
				return reply_status(smb2, SMB2_QUERY_DIRECTORY, BS_BUFFER_TOO_SMALL);
			}
			h->held = 1;
			h->held_kind = kind;
			h->held_fno = *fno;
			break;
		}
		if (count > 0) {
			memset(buf + used, 0, pos - used);
			put32(buf + last, pos - last);
		} else {
			pos = 0;
		}
		last = pos;
		used = pos + (uint32_t) r;
		count++;
		if (req->flags & SMB2_RETURN_SINGLE_ENTRY) {
			break;
		}
	}
	if (count == 0) {
		free(buf);
		return reply_status(smb2, SMB2_QUERY_DIRECTORY, h->dir_any ? BS_NO_MORE_FILES : BS_NO_SUCH_FILE);
	}
	h->dir_any = 1;
	bs_log("dir %s class %u entries %d bytes %u", h->path, cls, count, used);
	r = query_dir_reply(smb2, req, buf, used);
	free(buf);
	return r;
}

static int change_notify_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                                 struct smb2_change_notify_request *req, struct smb2_change_notify_reply *rep)
{
	struct bs_handle *h;
	(void) srvr;
	(void) rep;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_CHANGE_NOTIFY, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	if (!h->dir) {
		return reply_status(smb2, SMB2_CHANGE_NOTIFY, BS_INVALID_PARAMETER);
	}
	if (h->notify_pending) {
		notify_complete(h, BS_CANCELLED);
	}
	h->notify_pending = 1;
	h->notify_mid = smb2->message_id;
	return reply_status_mid(smb2, SMB2_CHANGE_NOTIFY, BS_PENDING, h->notify_mid);
}

static int info_reply(struct smb2_context *smb2, struct smb2_query_info_request *req, const uint8_t *data,
                      uint32_t len, uint32_t fixed)
{
	struct smb2_pdu *pdu;
	struct smb2_iovec *iov;
	uint8_t *head;
	uint8_t *body;
	uint32_t status = BS_SUCCESS;
	if (req->output_buffer_length < fixed) {
		return reply_status(smb2, SMB2_QUERY_INFO, BS_INFO_LENGTH_MISMATCH);
	}
	if (len > req->output_buffer_length) {
		len = req->output_buffer_length;
		status = BS_BUFFER_OVERFLOW;
	}
	pdu = smb2_allocate_pdu(smb2, SMB2_QUERY_INFO, NULL, NULL);
	if (pdu == NULL) {
		return -1;
	}
	head = calloc(1, 8);
	if (head == NULL) {
		smb2_free_pdu(smb2, pdu);
		return -1;
	}
	iov = smb2_add_iovector(smb2, &pdu->out, head, 8, free);
	if (iov == NULL) {
		free(head);
		smb2_free_pdu(smb2, pdu);
		return -1;
	}
	smb2_set_uint16(iov, 0, SMB2_QUERY_INFO_REPLY_SIZE);
	smb2_set_uint16(iov, 2, (uint16_t) (len ? SMB2_HEADER_SIZE + 8 : 0));
	smb2_set_uint32(iov, 4, len);
	if (len > 0) {
		size_t plen = PAD_TO_64BIT(len);
		body = calloc(1, plen);
		if (body == NULL) {
			smb2_free_pdu(smb2, pdu);
			return -1;
		}
		memcpy(body, data, len);
		if (smb2_add_iovector(smb2, &pdu->out, body, plen, free) == NULL) {
			free(body);
			smb2_free_pdu(smb2, pdu);
			return -1;
		}
	}
	if (smb2_pad_to_64bit(smb2, &pdu->out) != 0) {
		smb2_free_pdu(smb2, pdu);
		return -1;
	}
	if (status != BS_SUCCESS) {
		smb2_set_pdu_status(smb2, pdu, (int) status);
	}
	smb2_set_pdu_message_id(smb2, pdu, smb2->message_id);
	smb2_queue_pdu(smb2, pdu);
	return 1;
}

static uint32_t name_info(const struct bs_handle *h, uint8_t *out, size_t room, int leading)
{
	const char *rel = rel_path(h->path);
	uint32_t n = 0;
	const unsigned char *p = (const unsigned char *) rel;
	if (leading) {
		if (room < 2) {
			return 0;
		}
		put16(out, '\\');
		n = 2;
	}
	while (*p) {
		uint32_t cp;
		int k;
		if (*p == '/') {
			cp = '\\';
			p++;
		} else {
			cp = fat_cp(&p);
		}
		k = put_utf16(out, n, room, cp);
		if (k < 0) {
			break;
		}
		n += (uint32_t) k;
	}
	return n;
}

static int query_info_handler(struct smb2_server *srvr, struct smb2_context *smb2,
                              struct smb2_query_info_request *req, struct smb2_query_info_reply *rep)
{
	struct bs_handle *h;
	struct bs_info info;
	uint8_t out[1400];
	uint64_t t;
	uint64_t eof;
	uint64_t al;
	uint32_t fa;
	uint32_t n;
	int deleting;
	(void) srvr;
	(void) rep;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_QUERY_INFO, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	memset(out, 0, sizeof(out));
	if (req->info_type == SMB2_0_INFO_FILE) {
		if (h->pipe) {


			memset(&info, 0, sizeof(info));
			info.exists = 1;
			snprintf(info.name, sizeof(info.name), "srvsvc");
			bs_log("query_info sulla pipe, classe %u", (unsigned) req->file_info_class);
		} else {
			stat_path(h->path, &info);
		}
		if (!info.exists) {
			return reply_status(smb2, SMB2_QUERY_INFO, BS_FILE_CLOSED);
		}
		t = info_time(&info);
		eof = h->dir ? 0 : (uint64_t) info.size;
		al = alloc_size(info.size, h->dir);
		fa = fat_attr_to_smb(info.attr, h->dir);
		deleting = h->delete_on_close || (h->file >= 0 && g_files[h->file].delete_pending);
		switch (req->file_info_class) {
		case SMB2_FILE_BASIC_INFORMATION:
			put64(out, t);
			put64(out + 8, t);
			put64(out + 16, t);
			put64(out + 24, t);
			put32(out + 32, fa);
			return info_reply(smb2, req, out, 40, 40);
		case SMB2_FILE_STANDARD_INFORMATION:
			put64(out, al);
			put64(out + 8, eof);
			put32(out + 16, 1);
			out[20] = (uint8_t) deleting;
			out[21] = (uint8_t) h->dir;
			return info_reply(smb2, req, out, 24, 24);
		case SMB2_FILE_INTERNAL_INFORMATION:
			put64(out, path_id(h->path));
			return info_reply(smb2, req, out, 8, 8);
		case SMB2_FILE_EA_INFORMATION:
			return info_reply(smb2, req, out, 4, 4);
		case SMB2_FILE_ACCESS_INFORMATION:
			put32(out, h->access & (AC_MAXIMUM_ALLOWED | AC_GENERIC_ALL) ? AC_FULL_FILE : h->access);
			return info_reply(smb2, req, out, 4, 4);
		case SMB2_FILE_POSITION_INFORMATION:
			put64(out, h->position);
			return info_reply(smb2, req, out, 8, 8);
		case SMB2_FILE_MODE_INFORMATION:
		case SMB2_FILE_ALIGNMENT_INFORMATION:
			return info_reply(smb2, req, out, 4, 4);
		case SMB2_FILE_ALL_INFORMATION:
			put64(out, t);
			put64(out + 8, t);
			put64(out + 16, t);
			put64(out + 24, t);
			put32(out + 32, fa);
			put64(out + 40, al);
			put64(out + 48, eof);
			put32(out + 56, 1);
			out[60] = (uint8_t) deleting;
			out[61] = (uint8_t) h->dir;
			put64(out + 64, path_id(h->path));
			put32(out + 72, 0);
			put32(out + 76, h->access & (AC_MAXIMUM_ALLOWED | AC_GENERIC_ALL) ? AC_FULL_FILE : h->access);
			put64(out + 80, h->position);
			put32(out + 88, 0);
			put32(out + 92, 0);
			n = name_info(h, out + 100, sizeof(out) - 100, 1);
			put32(out + 96, n);
			return info_reply(smb2, req, out, 100 + n, 100);
		case SMB2_FILE_NAME_INFORMATION:
			n = name_info(h, out + 4, sizeof(out) - 4, 1);
			put32(out, n);
			return info_reply(smb2, req, out, 4 + n, 4);
		case SMB2_FILE_NORMALIZED_NAME_INFORMATION:
			n = name_info(h, out + 4, sizeof(out) - 4, 0);
			put32(out, n);
			return info_reply(smb2, req, out, 4 + n, 4);
		case SMB2_FILE_NETWORK_OPEN_INFORMATION:
			put64(out, t);
			put64(out + 8, t);
			put64(out + 16, t);
			put64(out + 24, t);
			put64(out + 32, al);
			put64(out + 40, eof);
			put32(out + 48, fa);
			return info_reply(smb2, req, out, 56, 56);
		case SMB2_FILE_ATTRIBUTE_TAG_INFORMATION:
			put32(out, fa);
			return info_reply(smb2, req, out, 8, 8);
		case SMB2_FILE_STREAM_INFORMATION:
			if (h->dir) {
				return info_reply(smb2, req, out, 0, 0);
			}
			put32(out + 4, 14);
			put64(out + 8, eof);
			put64(out + 16, al);
			{
				const char *s = "::$DATA";
				int i;
				for (i = 0; s[i]; i++) {
					put16(out + 24 + i * 2, (uint16_t) s[i]);
				}
			}
			return info_reply(smb2, req, out, 38, 24);
		case SMB2_FILE_COMPRESSION_INFORMATION:
			put64(out, eof);
			return info_reply(smb2, req, out, 16, 16);
		case 59:
			put64(out, g_serial);
			put64(out + 8, path_id(h->path));
			return info_reply(smb2, req, out, 24, 24);
		default:
			bs_log("query info file class %u unsupported", req->file_info_class);
			return reply_status(smb2, SMB2_QUERY_INFO, BS_NOT_SUPPORTED);
		}
	}
	if (req->info_type == SMB2_0_INFO_FILESYSTEM) {
		DWORD nclst = 0;
		FATFS *fs = NULL;
		char vol[20];
		uint64_t total;
		uint32_t spc;
		snprintf(vol, sizeof(vol), "%s/", g_volume);
		if (f_getfree(vol, &nclst, &fs) != FR_OK || fs == NULL) {
			return reply_status(smb2, SMB2_QUERY_INFO, BS_IO_DEVICE_ERROR);
		}
		total = (uint64_t) (fs->n_fatent - 2);
		spc = fs->csize;
		switch (req->file_info_class) {
		case SMB2_FILE_FS_VOLUME_INFORMATION: {
			const char *label = "BMC64-NG";
			int i;
			put64(out, g_root_time);
			put32(out + 8, g_serial);
			put32(out + 12, (uint32_t) strlen(label) * 2);
			for (i = 0; label[i]; i++) {
				put16(out + 18 + i * 2, (uint16_t) label[i]);
			}
			return info_reply(smb2, req, out, 18 + (uint32_t) strlen(label) * 2, 18);
		}
		case SMB2_FILE_FS_SIZE_INFORMATION:
			put64(out, total);
			put64(out + 8, nclst);
			put32(out + 16, spc);
			put32(out + 20, 512);
			return info_reply(smb2, req, out, 24, 24);
		case SMB2_FILE_FS_FULL_SIZE_INFORMATION:
			put64(out, total);
			put64(out + 8, nclst);
			put64(out + 16, nclst);
			put32(out + 24, spc);
			put32(out + 28, 512);
			return info_reply(smb2, req, out, 32, 32);
		case SMB2_FILE_FS_DEVICE_INFORMATION:
			put32(out, FILE_DEVICE_DISK);
			put32(out + 4, FILE_DEVICE_IS_MOUNTED);
			return info_reply(smb2, req, out, 8, 8);
		case SMB2_FILE_FS_ATTRIBUTE_INFORMATION: {
			const char *name = fs->fs_type == FS_EXFAT ? "exFAT" : "FAT32";
			int i;
			put32(out, 0x00000006u);
			put32(out + 4, 255);
			put32(out + 8, (uint32_t) strlen(name) * 2);
			for (i = 0; name[i]; i++) {
				put16(out + 12 + i * 2, (uint16_t) name[i]);
			}
			return info_reply(smb2, req, out, 12 + (uint32_t) strlen(name) * 2, 12);
		}
		case SMB2_FILE_FS_SECTOR_SIZE_INFORMATION:
			put32(out, 512);
			put32(out + 4, 512);
			put32(out + 8, 512);
			put32(out + 12, 512);
			put32(out + 16, 0x3);
			return info_reply(smb2, req, out, 28, 28);
		default:
			bs_log("query info fs class %u unsupported", req->file_info_class);
			return reply_status(smb2, SMB2_QUERY_INFO, BS_NOT_SUPPORTED);
		}
	}
	bs_log("query info type %u class %u unsupported", req->info_type, req->file_info_class);
	return reply_status(smb2, SMB2_QUERY_INFO, BS_NOT_SUPPORTED);
}

static void rebase_paths(const char *oldp, const char *newp)
{
	int i;
	size_t ol = strlen(oldp);
	char tmp[MAX_PATH_BYTES];
	for (i = 0; i < MAX_HANDLES; i++) {
		struct bs_handle *h = &g_handles[i];
		if (!h->used) {
			continue;
		}
		if (path_equal(h->path, oldp)) {
			snprintf(h->path, sizeof(h->path), "%s", newp);
		} else if (path_under(h->path, oldp)) {
			snprintf(tmp, sizeof(tmp), "%s%s", newp, h->path + ol);
			snprintf(h->path, sizeof(h->path), "%s", tmp);
		}
	}
	for (i = 0; i < MAX_FILES; i++) {
		struct bs_file *f = &g_files[i];
		if (!f->used) {
			continue;
		}
		if (path_equal(f->path, oldp)) {
			snprintf(f->path, sizeof(f->path), "%s", newp);
		} else if (path_under(f->path, oldp)) {
			snprintf(tmp, sizeof(tmp), "%s%s", newp, f->path + ol);
			snprintf(f->path, sizeof(f->path), "%s", tmp);
		}
	}
}

static uint32_t do_rename(struct bs_handle *h, const uint8_t *name16, uint32_t name_len, int replace)
{
	char *utf8;
	char target[MAX_PATH_BYTES];
	uint32_t status;
	struct bs_info ti;
	struct bs_file *f = open_file_of(h->path);
	FRESULT fr;
	uint16_t *tmp16;
	uint32_t i;
	if ((name_len & 1) || name_len == 0) {
		return BS_INVALID_PARAMETER;
	}
	tmp16 = malloc(name_len);
	if (tmp16 == NULL) {
		return BS_INSUFFICIENT_RESOURCES;
	}
	for (i = 0; i < name_len / 2; i++) {
		tmp16[i] = get16(name16 + i * 2);
	}
	utf8 = discard_const(smb2_utf16_to_utf8(tmp16, name_len / 2));
	free(tmp16);
	if (utf8 == NULL) {
		return BS_OBJECT_NAME_INVALID;
	}
	status = smb_path_to_fat(utf8, target, sizeof(target));
	free(utf8);
	if (status != BS_SUCCESS) {
		return status;
	}
	if (is_root(h->path) || is_root(target)) {
		return BS_ACCESS_DENIED;
	}
	if (in_use(h->path, h->dir)) {
		return BS_SHARING_VIOLATION;
	}
	if (strcmp(h->path, target) == 0) {
		return BS_SUCCESS;
	}
	stat_path(target, &ti);
	if (ti.exists && !path_equal(h->path, target)) {
		if (!replace) {
			return BS_OBJECT_NAME_COLLISION;
		}
		if (ti.dir || handles_on_path(target, NULL) > 0 || in_use(target, 0)) {
			return BS_ACCESS_DENIED;
		}
		fr = f_unlink(target);
		if (fr != FR_OK) {
			return fr_to_status(fr);
		}
	}
	if (f != NULL) {
		fr = f_close(&f->fil);
		if (fr != FR_OK) {
			return fr_to_status(fr);
		}
	}
	fr = f_rename(h->path, target);
	bs_log("rename %s -> %s : %d", h->path, target, (int) fr);
	if (fr == FR_OK) {
		rebase_paths(h->path, target);
	}
	if (f != NULL) {
		FRESULT fr2 = f_open(&f->fil, f->path, (BYTE) (FA_READ | (f->writable ? FA_WRITE : 0) | FA_OPEN_EXISTING));
		if (fr2 != FR_OK) {
			int i;
			int idx = (int) (f - g_files);
			for (i = 0; i < MAX_HANDLES; i++) {
				if (g_handles[i].used && g_handles[i].file == idx) {
					g_handles[i].file = -1;
				}
			}
			f->used = 0;
			return fr_to_status(fr2);
		}
		f->phys = f_size(&f->fil);
	}
	return fr_to_status(fr);
}

static uint32_t set_delete(struct bs_handle *h, int del)
{
	if (!del) {
		h->delete_on_close = 0;
		if (h->file >= 0) {
			g_files[h->file].delete_pending = 0;
		}
		return BS_SUCCESS;
	}
	if (is_root(h->path)) {
		return BS_CANNOT_DELETE;
	}
	if (!(h->access & (AC_DELETE | AC_GENERIC_ALL | AC_MAXIMUM_ALLOWED))) {
		return BS_ACCESS_DENIED;
	}
	if (in_use(h->path, h->dir)) {
		return BS_SHARING_VIOLATION;
	}
	if (h->dir) {
		DIR dj;
		FRESULT fr = f_opendir(&dj, h->path);
		if (fr != FR_OK) {
			return fr_to_status(fr);
		}
		fr = f_readdir(&dj, &g_fno);
		f_closedir(&dj);
		if (fr != FR_OK) {
			return fr_to_status(fr);
		}
		if (g_fno.fname[0] != 0) {
			return BS_DIRECTORY_NOT_EMPTY;
		}
	} else {
		struct bs_info info;
		stat_path(h->path, &info);
		if (info.attr & AM_RDO) {
			return BS_CANNOT_DELETE;
		}
	}
	h->delete_on_close = 1;
	return BS_SUCCESS;
}

static uint32_t set_eof(struct bs_handle *h, uint64_t size)
{
	struct bs_file *f;
	FRESULT fr;
	if (h->dir || h->file < 0) {
		return BS_INVALID_PARAMETER;
	}
	f = &g_files[h->file];
	if (!f->writable) {
		return BS_ACCESS_DENIED;
	}
	if (size > 0xFFFFFFFFULL && f->fil.obj.fs->fs_type != FS_EXFAT) {
		return BS_DISK_FULL;
	}
	if (size < f->phys) {
		fr = f_lseek(&f->fil, (FSIZE_t) size);
		if (fr == FR_OK) {
			fr = f_truncate(&f->fil);
		}
		if (fr != FR_OK) {
			return fr_to_status(fr);
		}
		f->phys = (FSIZE_t) size;
	}
	f->eof = (FSIZE_t) size;
	return BS_SUCCESS;
}

static uint32_t set_basic(struct bs_handle *h, const uint8_t *d)
{
	uint64_t wt = get64(d + 16);
	uint32_t attr = get32(d + 32);
	WORD fdate;
	WORD ftime;
	struct bs_file *f = open_file_of(h->path);
	if (filetime_to_fat(wt, &fdate, &ftime)) {
		if (f != NULL) {
			f->time_pending = 1;
			f->fdate = fdate;
			f->ftime = ftime;
		} else if (!is_root(h->path)) {
			g_fno.fdate = fdate;
			g_fno.ftime = ftime;
			f_utime(h->path, &g_fno);
		}
	}
	if (attr != 0 && !is_root(h->path)) {
		BYTE a = (BYTE) (attr & (AM_RDO | AM_HID | AM_SYS | AM_ARC));
		if (f != NULL) {
			f->attr_pending = 1;
			f->attr = a;
		} else {
			f_chmod(h->path, a, AM_RDO | AM_HID | AM_SYS | AM_ARC);
		}
	}
	return BS_SUCCESS;
}

static int set_info_handler(struct smb2_server *srvr, struct smb2_context *smb2, struct smb2_set_info_request *req)
{
	struct bs_handle *h;
	const uint8_t *d;
	uint32_t len;
	uint32_t status = BS_NOT_SUPPORTED;
	(void) srvr;
	h = req ? find_handle(smb2, req->file_id) : NULL;
	if (h == NULL) {
		return reply_status(smb2, SMB2_SET_INFO, req ? stato_senza_handle(smb2, req->file_id) : BS_FILE_CLOSED);
	}
	if (req->info_type != SMB2_0_INFO_FILE) {
		bs_log("set info type %u class %u unsupported", req->info_type, req->file_info_class);
		return reply_status(smb2, SMB2_SET_INFO, BS_NOT_SUPPORTED);
	}
	d = (const uint8_t *) req->input_data;
	len = req->buffer_length;
	if (d == NULL && len > 0) {
		return reply_status(smb2, SMB2_SET_INFO, BS_INVALID_PARAMETER);
	}
	switch (req->file_info_class) {
	case SMB2_FILE_BASIC_INFORMATION:
		status = len < 36 ? BS_INFO_LENGTH_MISMATCH : set_basic(h, d);
		break;
	case SMB2_FILE_RENAME_INFORMATION:
		if (len < 20 || get32(d + 16) > len - 20) {
			status = BS_INFO_LENGTH_MISMATCH;
		} else if (!(h->access & (AC_DELETE | AC_GENERIC_ALL | AC_MAXIMUM_ALLOWED))) {
			status = BS_ACCESS_DENIED;
		} else {
			status = do_rename(h, d + 20, get32(d + 16), d[0] != 0);
		}
		break;
	case 65:
		if (len < 20 || get32(d + 16) > len - 20) {
			status = BS_INFO_LENGTH_MISMATCH;
		} else if (!(h->access & (AC_DELETE | AC_GENERIC_ALL | AC_MAXIMUM_ALLOWED))) {
			status = BS_ACCESS_DENIED;
		} else {
			status = do_rename(h, d + 20, get32(d + 16), (get32(d) & 1) != 0);
		}
		break;
	case SMB2_FILE_DISPOSITION_INFORMATION:
		status = len < 1 ? BS_INFO_LENGTH_MISMATCH : set_delete(h, d[0] != 0);
		break;
	case 64:
		if (len < 4) {
			status = BS_INFO_LENGTH_MISMATCH;
		} else {
			uint32_t fl = get32(d);
			if ((fl & 1) && (fl & 0x10) && !h->dir) {
				f_chmod(h->path, 0, AM_RDO);
			}
			status = set_delete(h, (fl & 1) != 0);
		}
		break;
	case SMB2_FILE_POSITION_INFORMATION:
		if (len < 8) {
			status = BS_INFO_LENGTH_MISMATCH;
		} else {
			h->position = get64(d);
			status = BS_SUCCESS;
		}
		break;
	case SMB2_FILE_MODE_INFORMATION:
	case 39:
		status = BS_SUCCESS;
		break;
	case SMB2_FILE_ALLOCATION_INFORMATION:
		if (len < 8) {
			status = BS_INFO_LENGTH_MISMATCH;
		} else if (h->file >= 0 && get64(d) < g_files[h->file].eof) {
			status = set_eof(h, get64(d));
		} else {
			status = BS_SUCCESS;
		}
		break;
	case SMB2_FILE_END_OF_FILE_INFORMATION:
		status = len < 8 ? BS_INFO_LENGTH_MISMATCH : set_eof(h, get64(d));
		break;
	default:
		status = BS_NOT_SUPPORTED;
		break;
	}
	if (status != BS_SUCCESS) {
		bs_log("set info class %u on %s -> %08x", req->file_info_class, h->path, status);
		return reply_status(smb2, SMB2_SET_INFO, status);
	}
	return 0;
}

void bmc_smb_setup(struct smb2_server *server, const struct bmc_smb_config *config)
{
	g_cfg = *config;
	snprintf(g_volume, sizeof(g_volume), "%s", config->volume ? config->volume : "SD:");
	if (g_volume[0] && g_volume[strlen(g_volume) - 1] == '/') {
		g_volume[strlen(g_volume) - 1] = 0;
	}
	snprintf(g_share, sizeof(g_share), "%s", config->share ? config->share : "sdcard");
	snprintf(g_hostname, sizeof(g_hostname), "%s", config->hostname ? config->hostname : "BMC64-NG");
	snprintf(g_user, sizeof(g_user), "%s", config->user ? config->user : "bmc64");
	snprintf(g_password, sizeof(g_password), "%s", config->password ? config->password : "bmc64");
	g_cfg.volume = g_volume;
	g_cfg.share = g_share;
	g_cfg.hostname = g_hostname;
	g_cfg.user = g_user;
	g_cfg.password = g_password;
	memset(g_files, 0, sizeof(g_files));
	memset(g_handles, 0, sizeof(g_handles));
	memset(g_trees, 0, sizeof(g_trees));
	memset(g_conns, 0, sizeof(g_conns));
	g_cluster_bytes = 0;
	{
		DWORD t = get_fattime();
		g_root_time = fat_to_filetime((WORD) (t >> 16), (WORD) t);
		g_serial = (uint32_t) (t ^ 0xB64C0DE5u);
	}
	memset(&g_handlers, 0, sizeof(g_handlers));
	g_handlers.destruction_event = destruction_handler;
	g_handlers.authorize_user = authorize_handler;
	g_handlers.session_established = session_handler;
	g_handlers.logoff_cmd = logoff_handler;
	g_handlers.tree_connect_cmd = tree_connect_handler;
	g_handlers.tree_disconnect_cmd = tree_disconnect_handler;
	g_handlers.create_cmd = create_handler;
	g_handlers.close_cmd = close_handler;
	g_handlers.flush_cmd = flush_handler;
	g_handlers.read_cmd = read_handler;
	g_handlers.write_cmd = write_handler;
	g_handlers.oplock_break_cmd = oplock_handler;
	g_handlers.lease_break_cmd = lease_handler;
	g_handlers.lock_cmd = lock_handler;
	g_handlers.ioctl_cmd = ioctl_handler;
	g_handlers.cancel_cmd = cancel_handler;
	g_handlers.echo_cmd = echo_handler;
	g_handlers.query_directory_cmd = query_directory_handler;
	g_handlers.change_notify_cmd = change_notify_handler;
	g_handlers.query_info_cmd = query_info_handler;
	g_handlers.set_info_cmd = set_info_handler;
	server->handlers = &g_handlers;
	snprintf(server->hostname, sizeof(server->hostname), "%s", g_hostname);
	snprintf(server->domain, sizeof(server->domain), "%s", "WORKGROUP");
	memcpy(server->guid, "BMC64-NG-SMB-SRV", 16);
	server->signing_enabled = 1;
	server->allow_anonymous = 0;
	server->max_transact_size = 65536;
	server->max_read_size = 65536;
	server->max_write_size = 65536;
}

static void library_error(struct smb2_context *smb2, const char *text)
{
	(void) smb2;
	bs_log("libsmb2: %s", text ? text : "");
}

void bmc_smb_new_client(struct smb2_context *smb2, void *cb_data)
{
	int i;
	(void) cb_data;
	for (i = 0; i < MAX_CONNS; i++) {
		if (!g_conns[i].used) {
			break;
		}
	}
	if (i < MAX_CONNS) {
		memset(&g_conns[i], 0, sizeof(g_conns[i]));
		g_conns[i].used = 1;
		g_conns[i].ctx = smb2;
		smb2_set_opaque(smb2, &g_conns[i]);
	}
	smb2_set_version(smb2, SMB2_VERSION_ANY);
	smb2_set_sign(smb2, 1);
	smb2_set_authentication(smb2, SMB2_SEC_NTLMSSP);
	smb2_set_timeout(smb2, 0);
	smb2_register_error_callback(smb2, library_error);
	bs_log("new connection (slot %d)", i);
}

void bmc_smb_close_all(void)
{
	int i;
	for (i = 0; i < MAX_HANDLES; i++) {
		if (g_handles[i].used) {
			handle_close(&g_handles[i], 0);
		}
	}
	for (i = 0; i < MAX_FILES; i++) {
		if (g_files[i].used) {
			g_files[i].refs = 1;
			file_release(&g_files[i]);
		}
	}
}

int bmc_smb_open_count(void)
{
	int i;
	int n = 0;
	for (i = 0; i < MAX_FILES; i++) {
		if (g_files[i].used) {
			n++;
		}
	}
	return n;
}
