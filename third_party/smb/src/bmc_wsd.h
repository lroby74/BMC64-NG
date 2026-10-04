#ifndef BMC_WSD_H
#define BMC_WSD_H

#include <time.h>
#include <sys/select.h>

#include "bmc_nomi.h"

#ifdef __cplusplus
extern "C" {
#endif

int bmc_wsd_apri(const unsigned char ip[4], const unsigned char mac[6], const char *nome, bmc_nomi_log_fn log);
void bmc_wsd_fdset(fd_set *rfds, int *maxfd);
void bmc_wsd_servi(fd_set *rfds);
void bmc_wsd_chiudi(void);


void bmc_wsd_identita(const unsigned char ip[4], const unsigned char mac[6], const char *nome, unsigned long istanza);
const char *bmc_wsd_uuid(void);
int bmc_wsd_risposta_udp(const char *q, int qn, char *out, int max);
int bmc_wsd_risposta_http(const char *req, int n, char *out, int max);
int bmc_wsd_hello(char *out, int max);
int bmc_wsd_bye(char *out, int max);

#ifdef __cplusplus
}
#endif

#endif
