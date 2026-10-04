#include "bmc_nomi.h"
#include "bmc_sock.h"

#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORTA_NBNS 137
#define PORTA_MDNS 5353
#define PORTA_LLMNR 5355

static int s_nbns = -1;
static int s_mdns = -1;
static int s_llmnr = -1;
static unsigned char g_ip[4];
static char g_nome[16];
static bmc_nomi_log_fn g_log;

static void scrivi(const char *testo)
{
	if (g_log) {
		g_log(testo);
	}
}

static int apri_udp(unsigned short porta, unsigned long gruppo, int broadcast)
{
	struct sockaddr_in a;
	int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0) {
		return -1;
	}
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons(porta);
	a.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s, (struct sockaddr *) &a, sizeof(a)) < 0) {
		bmc_sock_close(s);
		return -1;
	}
	if (broadcast) {
		int uno = 1;
		setsockopt(s, SOL_SOCKET, SO_BROADCAST, &uno, sizeof(uno));
	}
	if (gruppo) {
		struct ip_mreq m;
		memset(&m, 0, sizeof(m));
		m.imr_multiaddr.s_addr = htonl(gruppo);
		m.imr_interface.s_addr = htonl(INADDR_ANY);
		setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof(m));
	}
	return s;
}

static void metti16(unsigned char *p, unsigned v)
{
	p[0] = (unsigned char) (v >> 8);
	p[1] = (unsigned char) v;
}

static unsigned prendi16(const unsigned char *p)
{
	return ((unsigned) p[0] << 8) | p[1];
}

static int leggi_nome(const unsigned char *b, int n, int pos, char et[][64], int max_et, int *quante)
{
	int k = 0;
	while (pos < n) {
		int l = b[pos];
		if (l == 0) {
			*quante = k;
			return pos + 1;
		}
		if ((l & 0xC0) != 0 || l > 63 || pos + 1 + l > n) {
			return -1;
		}
		if (k < max_et) {
			memcpy(et[k], b + pos + 1, (size_t) l);
			et[k][l] = '\0';
		}
		k++;
		pos += 1 + l;
	}
	return -1;
}




int bmc_nomi_risposta_dns(int mdns, const unsigned char *q, int qn,
			  unsigned char *out, int max)
{
	char et[4][64];
	int quante = 0;
	int fine;
	unsigned tipo, classe;
	int o;

	if (qn < 12 || (q[2] & 0x80) != 0 || ((q[2] >> 3) & 0x0F) != 0 ||
	    prendi16(q + 4) < 1) {
		return 0;
	}
	fine = leggi_nome(q, qn, 12, et, 4, &quante);
	if (fine < 0 || fine + 4 > qn || quante < 1) {
		return 0;
	}
	tipo = prendi16(q + fine);
	classe = prendi16(q + fine + 2) & 0x7FFF;
	if (classe != 1 && classe != 255) {
		return 0;
	}
	if (strcasecmp(et[0], g_nome) != 0) {
		return 0;
	}
	if (mdns ? (quante != 2 || strcasecmp(et[1], "local") != 0) : quante != 1) {
		return 0;
	}
	if (tipo != 1 && tipo != 255 && !(tipo == 28 && mdns == 0)) {
		return 0;
	}
	if (max < fine + 40) {
		return 0;
	}
	if (mdns == 1) {
		out[0] = 0;
		out[1] = 0;
		out[2] = 0x84;
		out[3] = 0x00;
		metti16(out + 4, 0);
		metti16(out + 6, 1);
		metti16(out + 8, 0);
		metti16(out + 10, 0);
		memcpy(out + 12, q + 12, (size_t) (fine - 12));
		o = fine;
	} else {
		memcpy(out, q, 2);
		out[2] = mdns ? 0x84 : 0x80;
		out[3] = 0x00;
		metti16(out + 4, 1);
		metti16(out + 6, tipo == 28 ? 0 : 1);
		metti16(out + 8, 0);
		metti16(out + 10, 0);
		memcpy(out + 12, q + 12, (size_t) (fine + 4 - 12));
		o = fine + 4;
		if (tipo == 28) {

			return o;
		}
	}
	out[o++] = 0xC0;
	out[o++] = 12;
	metti16(out + o, 1);
	o += 2;
	metti16(out + o, mdns == 1 ? 0x8001 : 1);
	o += 2;
	out[o++] = 0;
	out[o++] = 0;
	out[o++] = 0;
	out[o++] = mdns == 1 ? 120 : (mdns == 2 ? 10 : 30);
	metti16(out + o, 4);
	o += 2;
	memcpy(out + o, g_ip, 4);
	o += 4;
	return o;
}

int bmc_nomi_risposta_nbns(const unsigned char *q, int qn, unsigned char *out, int max)
{
	char nome[17];
	int i;
	int n;

	if (qn < 12 + 34 + 4 || (q[2] & 0x80) != 0 || ((q[2] >> 3) & 0x0F) != 0 ||
	    prendi16(q + 4) != 1 || q[12] != 0x20 || q[12 + 33] != 0) {
		return 0;
	}
	if (prendi16(q + 12 + 34) != 0x0020 || prendi16(q + 12 + 36) != 0x0001) {
		return 0;
	}
	for (i = 0; i < 16; i++) {
		int a = q[13 + 2 * i] - 'A';
		int b = q[14 + 2 * i] - 'A';
		if (a < 0 || a > 15 || b < 0 || b > 15) {
			return 0;
		}
		nome[i] = (char) ((a << 4) | b);
	}
	if (nome[15] != 0x00 && nome[15] != 0x20) {
		return 0;
	}
	nome[15] = '\0';
	n = 15;
	while (n > 0 && nome[n - 1] == ' ') {
		nome[--n] = '\0';
	}
	if (strcasecmp(nome, g_nome) != 0 || max < 12 + 34 + 16) {
		return 0;
	}
	memcpy(out, q, 2);
	out[2] = 0x85;
	out[3] = 0x00;
	metti16(out + 4, 0);
	metti16(out + 6, 1);
	metti16(out + 8, 0);
	metti16(out + 10, 0);
	memcpy(out + 12, q + 12, 34);
	metti16(out + 46, 0x0020);
	metti16(out + 48, 0x0001);
	out[50] = 0x00;
	out[51] = 0x04;
	out[52] = 0x93;
	out[53] = 0xE0;
	metti16(out + 54, 6);
	metti16(out + 56, 0);
	memcpy(out + 58, g_ip, 4);
	return 62;
}

int bmc_nomi_apri(const unsigned char ip[4], const char *nome, bmc_nomi_log_fn log)
{
	g_log = log;
	memcpy(g_ip, ip, 4);
	strncpy(g_nome, nome, sizeof(g_nome) - 1);
	g_nome[sizeof(g_nome) - 1] = '\0';
	s_nbns = apri_udp(PORTA_NBNS, 0, 1);
	s_llmnr = apri_udp(PORTA_LLMNR, 0xE00000FCUL, 0);
	s_mdns = apri_udp(PORTA_MDNS, 0xE00000FBUL, 0);
	scrivi(s_nbns >= 0 ? "nomi: NBNS pronto" : "nomi: NBNS non si apre");
	scrivi(s_llmnr >= 0 ? "nomi: LLMNR pronto" : "nomi: LLMNR non si apre");
	scrivi(s_mdns >= 0 ? "nomi: mDNS pronto" : "nomi: mDNS non si apre");
	return s_nbns >= 0 || s_llmnr >= 0 || s_mdns >= 0;
}

void bmc_nomi_fdset(fd_set *rfds, int *maxfd)
{
	int s[3];
	int i;
	s[0] = s_nbns;
	s[1] = s_llmnr;
	s[2] = s_mdns;
	for (i = 0; i < 3; i++) {
		if (s[i] >= 0) {
			FD_SET(s[i], rfds);
			if (s[i] > *maxfd) {
				*maxfd = s[i];
			}
		}
	}
}

static void servi_uno(int s, int tipo)
{
	unsigned char q[512];
	unsigned char r[512];
	struct sockaddr_in da;
	socklen_t dl = sizeof(da);
	int n, rn;
	int mdns = 0;

	n = (int) recvfrom(s, q, sizeof(q), MSG_DONTWAIT, (struct sockaddr *) &da, &dl);
	if (n <= 0) {
		return;
	}
	if (tipo == 2) {
		mdns = ntohs(da.sin_port) == PORTA_MDNS ? 1 : 2;
	}
	if (tipo == 0) {
		rn = bmc_nomi_risposta_nbns(q, n, r, (int) sizeof(r));
	} else {
		rn = bmc_nomi_risposta_dns(mdns, q, n, r, (int) sizeof(r));
	}
	if (rn <= 0) {
		return;
	}
	if (mdns == 1) {
		struct sockaddr_in a;
		memset(&a, 0, sizeof(a));
		a.sin_family = AF_INET;
		a.sin_port = htons(PORTA_MDNS);
		a.sin_addr.s_addr = htonl(0xE00000FBUL);
		sendto(s, r, (size_t) rn, 0, (struct sockaddr *) &a, sizeof(a));
	} else {
		sendto(s, r, (size_t) rn, 0, (struct sockaddr *) &da, sizeof(da));
	}
	scrivi(tipo == 0 ? "nomi: risposto NBNS" : (tipo == 1 ? "nomi: risposto LLMNR" : "nomi: risposto mDNS"));
}

void bmc_nomi_servi(fd_set *rfds)
{
	if (s_nbns >= 0 && FD_ISSET(s_nbns, rfds)) {
		servi_uno(s_nbns, 0);
	}
	if (s_llmnr >= 0 && FD_ISSET(s_llmnr, rfds)) {
		servi_uno(s_llmnr, 1);
	}
	if (s_mdns >= 0 && FD_ISSET(s_mdns, rfds)) {
		servi_uno(s_mdns, 2);
	}
}

void bmc_nomi_chiudi(void)
{
	if (s_nbns >= 0) {
		bmc_sock_close(s_nbns);
		s_nbns = -1;
	}
	if (s_llmnr >= 0) {
		bmc_sock_close(s_llmnr);
		s_llmnr = -1;
	}
	if (s_mdns >= 0) {
		bmc_sock_close(s_mdns);
		s_mdns = -1;
	}
}
