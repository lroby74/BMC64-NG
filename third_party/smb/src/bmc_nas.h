#ifndef BMC_NAS_H
#define BMC_NAS_H

#ifdef __cplusplus
extern "C" {
#endif



#define BMC_NAS_CONDIVISIONE ""

#define BMC_NAS_MASSIMO (32u * 1024u * 1024u)



void bmc_nas_imposta(const char *ip, const char *utente, const char *password, int versione);
void bmc_nas_imposta_condivisione(const char *nome);
const char *bmc_nas_condivisione(void);
int bmc_nas_configurato(void);
int bmc_nas_collega(void);
void bmc_nas_scollega(void);
int bmc_nas_collegato(void);
int bmc_nas_permetti_collegamento(int si);
const char *bmc_nas_errore(void);
const char *bmc_nas_versione_usata(void);

int bmc_nas_leggi_tutto(const char *percorso, char **dati, unsigned *dimensione);
int bmc_nas_scrivi(const char *percorso, const char *dati, unsigned da, unsigned quanti, int crea);
int bmc_nas_stat(const char *percorso, unsigned long long *dimensione, int *cartella);
void *bmc_nas_apri_cartella(const char *percorso);
int bmc_nas_leggi_cartella(void *c, char *nome, int max, int *cartella);
void bmc_nas_riavvolgi_cartella(void *c);
void bmc_nas_chiudi_cartella(void *c);
int bmc_nas_cancella(const char *percorso);
int bmc_nas_rinomina(const char *da, const char *a);

#ifdef __cplusplus
}
#endif

#endif
