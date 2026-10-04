#ifndef BMC_SMB_H
#define BMC_SMB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct smb2_server;
struct smb2_context;

typedef int (*bmc_smb_in_use_fn)(const char *path, int is_dir);
typedef void (*bmc_smb_log_fn)(const char *text);


typedef int (*bmc_smb_tz_fn)(void);
#define BMC_SMB_TZ_FN 1

struct bmc_smb_config {
	const char *volume;
	const char *share;
	const char *hostname;
	const char *user;
	const char *password;
	int tz_offset_minutes;
	bmc_smb_in_use_fn in_use;
	bmc_smb_log_fn log;
	bmc_smb_tz_fn tz_offset;
};

void bmc_smb_setup(struct smb2_server *server, const struct bmc_smb_config *config);
void bmc_smb_new_client(struct smb2_context *smb2, void *cb_data);
void bmc_smb_close_all(void);
int bmc_smb_open_count(void);

#ifdef __cplusplus
}
#endif

#endif
