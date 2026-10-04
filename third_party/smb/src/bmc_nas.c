













#include "bmc_nas.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "smb2.h"
#include "libsmb2.h"

#ifdef BMC_SOCK_CIRCLE

int circle_get_network_ip_address(char *address, unsigned int address_size);

extern volatile unsigned long bmc_scheda_settori;
#define AVANZA() (bmc_scheda_settori++)
#else
#define AVANZA() ((void) 0)
#endif

#define PEZZO 65536

static const enum smb2_negotiate_version versioni[6] = {
	SMB2_VERSION_ANY, SMB2_VERSION_0202, SMB2_VERSION_0210,
	SMB2_VERSION_0300, SMB2_VERSION_0302, SMB2_VERSION_0311
};
static const char *const nomi_versioni[6] = {
	"Auto", "SMB 2.0", "SMB 2.1", "SMB 3.0", "SMB 3.0.2", "SMB 3.1.1"
};

static struct smb2_context *g_smb2;




static int g_puo_collegarsi = 1;
static char g_ip[48];
static char g_utente[64];
static char g_password[128];

static char g_condivisione[128] = BMC_NAS_CONDIVISIONE;
static int g_versione;
static char g_errore[96];

static void copia(char *a, size_t max, const char *b)
{
	snprintf(a, max, "%s", b ? b : "");
}

void bmc_nas_imposta(const char *ip, const char *utente, const char *password, int versione)
{
	if (versione < 0 || versione > 5) {
		versione = 0;
	}

	if (strcmp(ip ? ip : "", g_ip) != 0 || strcmp(utente ? utente : "", g_utente) != 0 ||
	    strcmp(password ? password : "", g_password) != 0 || versione != g_versione) {
		bmc_nas_scollega();
	}
	copia(g_ip, sizeof(g_ip), ip);
	copia(g_utente, sizeof(g_utente), utente);
	copia(g_password, sizeof(g_password), password);
	g_versione = versione;
}




void bmc_nas_imposta_condivisione(const char *nome)
{
	if (nome == NULL || nome[0] == '\0') {
		nome = BMC_NAS_CONDIVISIONE;
	}
	if (strcmp(nome, g_condivisione) != 0) {
		bmc_nas_scollega();
	}
	copia(g_condivisione, sizeof(g_condivisione), nome);
}

const char *bmc_nas_condivisione(void)
{
	return g_condivisione;
}

int bmc_nas_configurato(void)
{
	return g_ip[0] != '\0';
}

int bmc_nas_collegato(void)
{
	return g_smb2 != NULL;
}

int bmc_nas_permetti_collegamento(int si)
{
	int prima = g_puo_collegarsi;

	g_puo_collegarsi = si;
	return prima;
}

const char *bmc_nas_errore(void)
{
	return g_errore;
}

const char *bmc_nas_versione_usata(void)
{
	return nomi_versioni[g_versione];
}




static void spiega(const char *msg, int err)
{
	if (msg == NULL) {
		msg = "";
	}
	if (strstr(msg, "LOGON_FAILURE") || strstr(msg, "ACCESS_DENIED") ||
	    strstr(msg, "WRONG_PASSWORD") || strstr(msg, "ACCOUNT")) {
		copia(g_errore, sizeof(g_errore), "Wrong user name or password");
	} else if (strstr(msg, "BAD_NETWORK_NAME")) {
		snprintf(g_errore, sizeof(g_errore), "Share \"%s\" not found on the NAS", g_condivisione);
	} else if (strstr(msg, "NOT_SUPPORTED") || strstr(msg, "egotiat")) {
		copia(g_errore, sizeof(g_errore), "The NAS refuses this SMB version");
	} else if (strstr(msg, "onnect") || strstr(msg, "imeout") || strstr(msg, "imed out") ||
		   err == -ETIMEDOUT || err == -ECONNREFUSED) {
		copia(g_errore, sizeof(g_errore), "NAS not reachable");
	} else if (msg[0] != '\0') {
		copia(g_errore, sizeof(g_errore), msg);
	} else {
		snprintf(g_errore, sizeof(g_errore), "NAS error %d", err);
	}
}

void bmc_nas_scollega(void)
{
	if (g_smb2 != NULL) {
		smb2_disconnect_share(g_smb2);
		smb2_destroy_context(g_smb2);
		g_smb2 = NULL;
	}
}

int bmc_nas_collega(void)
{
	struct smb2_context *s;
	int r;

	if (g_smb2 != NULL) {
		return 0;
	}
	g_errore[0] = '\0';
	if (g_ip[0] == '\0') {
		copia(g_errore, sizeof(g_errore), "NAS IP address not set");
		return -1;
	}
	if (g_condivisione[0] == '\0') {
		copia(g_errore, sizeof(g_errore), "NAS Path not set");
		return -1;
	}
	if (!g_puo_collegarsi) {
		copia(g_errore, sizeof(g_errore), "NAS not connected");
		return -1;
	}
#ifdef BMC_SOCK_CIRCLE
	{
		char nostro[32];
		if (!circle_get_network_ip_address(nostro, sizeof(nostro))) {
			copia(g_errore, sizeof(g_errore), "No network (Network Device is Off?)");
			return -1;
		}
	}
#endif
	s = smb2_init_context();
	if (s == NULL) {
		copia(g_errore, sizeof(g_errore), "Out of memory");
		return -1;
	}
	smb2_set_security_mode(s, SMB2_NEGOTIATE_SIGNING_ENABLED);
	smb2_set_version(s, versioni[g_versione]);
	smb2_set_user(s, g_utente);
	smb2_set_password(s, g_password);
	smb2_set_timeout(s, 5);
	r = smb2_connect_share(s, g_ip, g_condivisione, NULL);
	if (r < 0) {



		switch ((unsigned) smb2_get_nterror(s)) {
		case 0xC000006DU:
		case 0xC000006AU:
		case 0xC0000064U:
		case 0xC000006EU:
		case 0xC0000072U:
		case 0xC0000234U:
			copia(g_errore, sizeof(g_errore), "Wrong user name or password");
			break;
		case 0xC0000022U:
			snprintf(g_errore, sizeof(g_errore), "The NAS denies access to \"%s\"", g_condivisione);
			break;
		case 0xC00000CCU:
			snprintf(g_errore, sizeof(g_errore), "Share \"%s\" not found on the NAS", g_condivisione);
			break;
		default:
			spiega(smb2_get_error(s), r);
			break;
		}
		smb2_destroy_context(s);
		return -1;
	}
	g_smb2 = s;
	return 0;
}


static int percorso_smb(const char *p, char *out, size_t max)
{
	size_t n;
	char *c;

	if (p == NULL) {
		return -1;
	}
	if (strncasecmp(p, "NAS:", 4) == 0) {
		p += 4;
	}
	while (*p == '/' || *p == '\\') {
		p++;
	}
	n = strlen(p);
	if (n >= max) {
		return -1;
	}
	memcpy(out, p, n + 1);
	while (n > 0 && (out[n - 1] == '/' || out[n - 1] == '\\')) {
		out[--n] = '\0';
	}
	for (c = out; *c; c++) {
		if (*c == '/') {
			*c = '\\';
		}
	}
	return 0;
}



static void forse_caduta(int r)
{
	if (r == -ENOTCONN || r == -ECONNRESET || r == -EPIPE || r == -ETIMEDOUT || r == -EIO) {
		bmc_nas_scollega();
	}
}

int bmc_nas_leggi_tutto(const char *percorso, char **dati, unsigned *dimensione)
{
	char p[1024];
	struct smb2_stat_64 st;
	struct smb2fh *fh;
	char *buf;
	uint64_t fatto = 0;
	uint32_t pezzo;
	int r;

	*dati = NULL;
	*dimensione = 0;
	if (percorso_smb(percorso, p, sizeof(p)) < 0) {
		return -ENAMETOOLONG;
	}
	if (bmc_nas_collega() < 0) {
		return -EIO;
	}
	fh = smb2_open(g_smb2, p, O_RDONLY);
	if (fh == NULL) {
		r = -ENOENT;
		spiega(smb2_get_error(g_smb2), r);
		return r;
	}
	r = smb2_fstat(g_smb2, fh, &st);
	if (r < 0 || st.smb2_type == SMB2_TYPE_DIRECTORY) {
		smb2_close(g_smb2, fh);
		return r < 0 ? r : -EISDIR;
	}
	if (st.smb2_size > BMC_NAS_MASSIMO) {
		smb2_close(g_smb2, fh);
		copia(g_errore, sizeof(g_errore), "File too big for the NAS (32 MB)");
		return -EFBIG;
	}
	buf = malloc((size_t) st.smb2_size + 1);
	if (buf == NULL) {
		smb2_close(g_smb2, fh);
		return -ENOMEM;
	}
	pezzo = smb2_get_max_read_size(g_smb2);
	if (pezzo == 0 || pezzo > PEZZO) {
		pezzo = PEZZO;
	}
	while (fatto < st.smb2_size) {
		uint32_t vuoi = (uint32_t) (st.smb2_size - fatto);
		if (vuoi > pezzo) {
			vuoi = pezzo;
		}
		r = smb2_pread(g_smb2, fh, (uint8_t *) buf + fatto, vuoi, fatto);
		if (r < 0) {
			spiega(smb2_get_error(g_smb2), r);
			free(buf);
			smb2_close(g_smb2, fh);
			forse_caduta(r);
			return r;
		}
		if (r == 0) {
			break;
		}
		fatto += (uint64_t) r;
		AVANZA();
	}
	smb2_close(g_smb2, fh);
	*dati = buf;
	*dimensione = (unsigned) fatto;
	return 0;
}

int bmc_nas_scrivi(const char *percorso, const char *dati, unsigned da, unsigned quanti, int crea)
{
	char p[1024];
	struct smb2fh *fh;
	uint32_t pezzo;
	unsigned fatto = 0;
	int r;

	if (percorso_smb(percorso, p, sizeof(p)) < 0) {
		return -ENAMETOOLONG;
	}
	if (bmc_nas_collega() < 0) {
		return -EIO;
	}
	fh = smb2_open(g_smb2, p, crea ? (O_WRONLY | O_CREAT | O_TRUNC) : O_WRONLY);
	if (fh == NULL) {
		spiega(smb2_get_error(g_smb2), -EACCES);
		return -EACCES;
	}
	pezzo = smb2_get_max_write_size(g_smb2);
	if (pezzo == 0 || pezzo > PEZZO) {
		pezzo = PEZZO;
	}
	while (fatto < quanti) {
		uint32_t vuoi = quanti - fatto;
		if (vuoi > pezzo) {
			vuoi = pezzo;
		}
		r = smb2_pwrite(g_smb2, fh, (const uint8_t *) dati + da + fatto, vuoi, (uint64_t) da + fatto);
		if (r <= 0) {
			spiega(smb2_get_error(g_smb2), r);
			smb2_close(g_smb2, fh);
			forse_caduta(r < 0 ? r : -EIO);
			return r < 0 ? r : -EIO;
		}
		fatto += (unsigned) r;
		AVANZA();
	}
	r = smb2_close(g_smb2, fh);
	if (r < 0) {
		spiega(smb2_get_error(g_smb2), r);
		forse_caduta(r);
		return r;
	}
	return 0;
}

int bmc_nas_stat(const char *percorso, unsigned long long *dimensione, int *cartella)
{
	char p[1024];
	struct smb2_stat_64 st;
	int r;

	if (percorso_smb(percorso, p, sizeof(p)) < 0) {
		return -ENAMETOOLONG;
	}
	if (bmc_nas_collega() < 0) {
		return -EIO;
	}
	r = smb2_stat(g_smb2, p, &st);
	if (r < 0) {
		forse_caduta(r);
		return r;
	}
	*dimensione = st.smb2_size;
	*cartella = st.smb2_type == SMB2_TYPE_DIRECTORY;
	return 0;
}

struct cartella_nas {
	struct smb2dir *dir;
};

void *bmc_nas_apri_cartella(const char *percorso)
{
	char p[1024];
	struct cartella_nas *c;
	struct smb2dir *d;

	if (percorso_smb(percorso, p, sizeof(p)) < 0 || bmc_nas_collega() < 0) {
		return NULL;
	}
	d = smb2_opendir(g_smb2, p);
	if (d == NULL) {
		spiega(smb2_get_error(g_smb2), -ENOENT);
		return NULL;
	}
	c = malloc(sizeof(*c));
	if (c == NULL) {
		smb2_closedir(g_smb2, d);
		return NULL;
	}
	c->dir = d;
	return c;
}


int bmc_nas_leggi_cartella(void *cp, char *nome, int max, int *cartella)
{
	struct cartella_nas *c = cp;
	struct smb2dirent *e;

	if (c == NULL || g_smb2 == NULL) {
		return 0;
	}
	while ((e = smb2_readdir(g_smb2, c->dir)) != NULL) {
		if (e->name == NULL || strcmp(e->name, ".") == 0 || strcmp(e->name, "..") == 0) {
			continue;
		}
		snprintf(nome, (size_t) max, "%s", e->name);
		*cartella = e->st.smb2_type == SMB2_TYPE_DIRECTORY;
		return 1;
	}
	return 0;
}

void bmc_nas_riavvolgi_cartella(void *cp)
{
	struct cartella_nas *c = cp;

	if (c != NULL && g_smb2 != NULL) {
		smb2_rewinddir(g_smb2, c->dir);
	}
}

void bmc_nas_chiudi_cartella(void *cp)
{
	struct cartella_nas *c = cp;

	if (c == NULL) {
		return;
	}
	if (g_smb2 != NULL) {
		smb2_closedir(g_smb2, c->dir);
	}
	free(c);
}

int bmc_nas_cancella(const char *percorso)
{
	char p[1024];
	int r;

	if (percorso_smb(percorso, p, sizeof(p)) < 0) {
		return -ENAMETOOLONG;
	}
	if (bmc_nas_collega() < 0) {
		return -EIO;
	}
	r = smb2_unlink(g_smb2, p);
	forse_caduta(r);
	return r;
}

int bmc_nas_rinomina(const char *da, const char *a)
{
	char p[1024], q[1024];
	int r;

	if (percorso_smb(da, p, sizeof(p)) < 0 || percorso_smb(a, q, sizeof(q)) < 0) {
		return -ENAMETOOLONG;
	}
	if (bmc_nas_collega() < 0) {
		return -EIO;
	}
	r = smb2_rename(g_smb2, p, q);
	forse_caduta(r);
	return r;
}


































































































































































































































































