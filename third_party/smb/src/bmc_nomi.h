#ifndef BMC_NOMI_H
#define BMC_NOMI_H

#include <sys/select.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*bmc_nomi_log_fn)(const char *text);

int bmc_nomi_apri(const unsigned char ip[4], const char *nome, bmc_nomi_log_fn log);
void bmc_nomi_fdset(fd_set *rfds, int *maxfd);
void bmc_nomi_servi(fd_set *rfds);
void bmc_nomi_chiudi(void);
int bmc_nomi_risposta_dns(int mdns, const unsigned char *q, int qn, unsigned char *out, int max);
int bmc_nomi_risposta_nbns(const unsigned char *q, int qn, unsigned char *out, int max);

#ifdef __cplusplus
}
#endif

#endif
