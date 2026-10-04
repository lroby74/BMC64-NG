/* WS-Discovery messages based on ZiFi-ESP32-S3-Zero: Copyright (c) 2026 Andrew Lazarev, MIT License (LICENSE-ZiFi-ESP32-S3-Zero.txt). */













#include "bmc_wsd.h"
#include "bmc_sock.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORTA_WSD 3702
#define PORTA_HTTP 5357
#define GRUPPO_WSD 0xEFFFFFFAUL
#define HELLO_OGNI 30
#define HTTP_ATTESA 3
#define MAX_UDP 1460
#define MAX_RICHIESTA 3072
#define MAX_RISPOSTA 4096

static const char AZ_PROBE[] = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";
static const char AZ_RESOLVE[] = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Resolve";
static const char AZ_GET[] = "http://schemas.xmlsoap.org/ws/2004/09/transfer/Get";

static int s_udp = -1;
static int s_ascolto = -1;
static int s_cliente = -1;
static char g_nome[32];
static char g_ip[16];
static char g_uuid[37];
static char g_seq[37];
static unsigned long g_istanza;
static unsigned long g_numero;
static unsigned long g_msgid;
static time_t g_hello;
static time_t g_cliente_da;
static int g_rich_n;
static char g_rich[MAX_RICHIESTA + 1];
static char g_udp[MAX_UDP + 1];
static char g_buf[MAX_RISPOSTA];
static char g_corpo[MAX_RISPOSTA];
static bmc_nomi_log_fn g_log;

static void scrivi(const char *testo)
{
	if (g_log) {
		g_log(testo);
	}
}

static void formatta_uuid(const unsigned char b[16], char out[37])
{
	snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		 b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
		 b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

void bmc_wsd_identita(const unsigned char ip[4], const unsigned char mac[6], const char *nome,
		      unsigned long istanza)
{
	unsigned char b[16];
	int i;

	snprintf(g_nome, sizeof(g_nome), "%s", nome);
	snprintf(g_ip, sizeof(g_ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);


	memcpy(b, "BMC64-NG", 8);
	b[8] = 0;
	b[9] = 0;
	memcpy(b + 10, mac, 6);
	b[6] = (unsigned char) ((b[6] & 0x0F) | 0x50);
	b[8] = (unsigned char) ((b[8] & 0x3F) | 0x80);
	formatta_uuid(b, g_uuid);

	g_istanza = istanza ? istanza : 1;
	for (i = 0; i < 16; i++) {
		b[i] = (unsigned char) ((g_istanza >> ((i % 4) * 8)) ^ (unsigned) (mac[i % 6] * (i + 1)) ^
				       (unsigned) (i * 37));
	}
	b[6] = (unsigned char) ((b[6] & 0x0F) | 0x40);
	b[8] = (unsigned char) ((b[8] & 0x3F) | 0x80);
	formatta_uuid(b, g_seq);
	g_numero = 1;
	g_msgid = 0;
}

const char *bmc_wsd_uuid(void)
{
	return g_uuid;
}

static void nuovo_id(char out[46])
{
	g_msgid++;
	snprintf(out, 46, "urn:uuid:%08lx-%04lx-4%03lx-a%03lx-%s", g_istanza & 0xFFFFFFFFUL,
		 (g_msgid >> 16) & 0xFFFFUL, g_msgid & 0xFFFUL, (g_msgid >> 4) & 0xFFFUL, g_uuid + 24);
}



static int fine_nome(char c)
{
	return c == '>' || c == '/' || isspace((unsigned char) c);
}



static int testo_elemento(const char *x, int n, const char *locale, char *out, int max)
{
	const char *fine = x + n;
	const char *c;
	size_t ll = strlen(locale);

	out[0] = '\0';
	for (c = x; c < fine; c++) {
		const char *nome, *loc, *nf, *ap, *v, *vf;

		if (*c != '<' || c + 1 >= fine || c[1] == '/' || c[1] == '!' || c[1] == '?') {
			continue;
		}
		nome = c + 1;
		loc = nome;
		nf = nome;
		while (nf < fine && !fine_nome(*nf)) {
			if (*nf == ':') {
				loc = nf + 1;
			}
			nf++;
		}
		if ((size_t) (nf - loc) != ll || memcmp(loc, locale, ll) != 0) {
			continue;
		}
		ap = nf;
		while (ap < fine && *ap != '>') {
			ap++;
		}
		if (ap >= fine) {
			return -1;
		}
		if (ap[-1] == '/') {
			return 1;
		}
		v = ap + 1;
		vf = v;
		while (vf < fine && *vf != '<') {
			vf++;
		}
		while (v < vf && isspace((unsigned char) *v)) {
			v++;
		}
		while (vf > v && isspace((unsigned char) vf[-1])) {
			vf--;
		}
		if (vf - v >= max) {
			return -1;
		}
		memcpy(out, v, (size_t) (vf - v));
		out[vf - v] = '\0';
		return 1;
	}
	return 0;
}

static int contiene(const char *x, int n, const char *cosa)
{
	int l = (int) strlen(cosa);
	int i;

	for (i = 0; i + l <= n; i++) {
		if (memcmp(x + i, cosa, (size_t) l) == 0) {
			return 1;
		}
	}
	return 0;
}



static int sonda_per_noi(const char *q, int n)
{
	char t[256];
	char *p;
	int r;

	r = testo_elemento(q, n, "Scopes", t, (int) sizeof(t));
	if (r < 0 || (r == 1 && t[0] != '\0')) {
		return 0;
	}
	r = testo_elemento(q, n, "Types", t, (int) sizeof(t));
	if (r == 0 || (r == 1 && t[0] == '\0')) {
		return 1;
	}
	if (r < 0) {
		return 0;
	}
	for (p = strtok(t, " \t\r\n"); p != NULL; p = strtok(NULL, " \t\r\n")) {
		const char *loc = strchr(p, ':');
		loc = loc ? loc + 1 : p;
		if (strcmp(loc, "Device") == 0) {
			return 1;
		}
	}
	return 0;
}

static int lunghezza(int scritti, int max)
{
	return (scritti < 0 || scritti >= max) ? 0 : scritti;
}

static int metti_hello_bye(char *out, int max, int hello)
{
	char id[46];

	nuovo_id(id);
	if (hello) {
		return lunghezza(snprintf(out, (size_t) max,
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
			"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
			"xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
			"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
			"xmlns:dp=\"http://schemas.xmlsoap.org/ws/2006/02/devprof\" "
			"xmlns:pub=\"http://schemas.microsoft.com/windows/pub/2005/07\">"
			"<s:Header><a:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To>"
			"<a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Hello</a:Action>"
			"<a:MessageID>%s</a:MessageID>"
			"<d:AppSequence InstanceId=\"%lu\" SequenceId=\"urn:uuid:%s\" "
			"MessageNumber=\"%lu\"/></s:Header>"
			"<s:Body><d:Hello><a:EndpointReference><a:Address>urn:uuid:%s"
			"</a:Address></a:EndpointReference><d:Types>dp:Device pub:Computer"
			"</d:Types><d:XAddrs>http://%s:%d/%s</d:XAddrs>"
			"<d:MetadataVersion>2</d:MetadataVersion></d:Hello></s:Body>"
			"</s:Envelope>",
			id, g_istanza, g_seq, g_numero++, g_uuid, g_ip, PORTA_HTTP, g_uuid), max);
	}
	return lunghezza(snprintf(out, (size_t) max,
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">"
		"<s:Header><a:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To>"
		"<a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Bye</a:Action>"
		"<a:MessageID>%s</a:MessageID>"
		"<d:AppSequence InstanceId=\"%lu\" SequenceId=\"urn:uuid:%s\" "
		"MessageNumber=\"%lu\"/></s:Header>"
		"<s:Body><d:Bye><a:EndpointReference><a:Address>urn:uuid:%s"
		"</a:Address></a:EndpointReference></d:Bye></s:Body></s:Envelope>",
		id, g_istanza, g_seq, g_numero++, g_uuid), max);
}

int bmc_wsd_hello(char *out, int max)
{
	return metti_hello_bye(out, max, 1);
}

int bmc_wsd_bye(char *out, int max)
{
	return metti_hello_bye(out, max, 0);
}


int bmc_wsd_risposta_udp(const char *q, int qn, char *out, int max)
{
	char azione[96];
	char relativo[128];
	char id[46];
	const char *tipo;

	if (testo_elemento(q, qn, "Action", azione, (int) sizeof(azione)) != 1) {
		return 0;
	}
	if (strcmp(azione, AZ_PROBE) == 0) {
		if (!sonda_per_noi(q, qn)) {
			return 0;
		}
		tipo = "Probe";
	} else if (strcmp(azione, AZ_RESOLVE) == 0) {
		char nostro[48];
		snprintf(nostro, sizeof(nostro), "urn:uuid:%s", g_uuid);
		if (!contiene(q, qn, nostro)) {
			return 0;
		}
		tipo = "Resolve";
	} else {
		return 0;
	}
	if (testo_elemento(q, qn, "MessageID", relativo, (int) sizeof(relativo)) != 1 ||
	    relativo[0] == '\0') {
		return 0;
	}
	nuovo_id(id);
	return lunghezza(snprintf(out, (size_t) max,
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
		"xmlns:dp=\"http://schemas.xmlsoap.org/ws/2006/02/devprof\" "
		"xmlns:pub=\"http://schemas.microsoft.com/windows/pub/2005/07\">"
		"<s:Header><a:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"
		"</a:To><a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/%sMatches"
		"</a:Action><a:MessageID>%s</a:MessageID><a:RelatesTo>%s</a:RelatesTo>"
		"<d:AppSequence InstanceId=\"%lu\" SequenceId=\"urn:uuid:%s\" "
		"MessageNumber=\"%lu\"/></s:Header>"
		"<s:Body><d:%sMatches><d:%sMatch><a:EndpointReference>"
		"<a:Address>urn:uuid:%s</a:Address></a:EndpointReference>"
		"<d:Types>dp:Device pub:Computer</d:Types>"
		"<d:XAddrs>http://%s:%d/%s</d:XAddrs>"
		"<d:MetadataVersion>2</d:MetadataVersion></d:%sMatch>"
		"</d:%sMatches></s:Body></s:Envelope>",
		tipo, id, relativo, g_istanza, g_seq, g_numero++, tipo, tipo, g_uuid, g_ip,
		PORTA_HTTP, g_uuid, tipo, tipo), max);
}



static int errore_http(char *out, int max, int codice, const char *motivo)
{
	return lunghezza(snprintf(out, (size_t) max,
		"HTTP/1.1 %d %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", codice, motivo), max);
}

static int lunghezza_contenuto(const char *r, int testa, long *quanto)
{
	const char *c = r;
	const char *fine = r + testa;

	while (c < fine) {
		const char *riga = c;
		const char *rf = c;
		while (rf + 1 < fine && !(rf[0] == '\r' && rf[1] == '\n')) {
			rf++;
		}
		if (rf - riga > 15 && strncasecmp(riga, "Content-Length:", 15) == 0) {
			const char *v = riga + 15;
			long n = 0;
			int cifre = 0;
			while (v < rf && (*v == ' ' || *v == '\t')) {
				v++;
			}
			while (v < rf && *v >= '0' && *v <= '9' && n < 1000000) {
				n = n * 10 + (*v - '0');
				v++;
				cifre++;
			}
			while (v < rf && (*v == ' ' || *v == '\t')) {
				v++;
			}
			if (cifre == 0 || v != rf) {
				return 0;
			}
			*quanto = n;
			return 1;
		}
		if (rf + 2 > fine) {
			break;
		}
		c = rf + 2;
	}
	return 0;
}



int bmc_wsd_risposta_http(const char *r, int n, char *out, int max)
{
	char atteso[48];
	char relativo[128];
	char id[46];
	const char *corpo = NULL;
	long quanto = 0;
	int testa, i, k, al;

	for (i = 0; i + 3 < n; i++) {
		if (r[i] == '\r' && r[i + 1] == '\n' && r[i + 2] == '\r' && r[i + 3] == '\n') {
			corpo = r + i + 4;
			break;
		}
	}
	if (corpo == NULL) {
		return n >= MAX_RICHIESTA ? errore_http(out, max, 413, "Payload Too Large") : -1;
	}
	testa = (int) (corpo - r);
	if (!lunghezza_contenuto(r, testa, &quanto)) {
		return errore_http(out, max, 411, "Length Required");
	}
	if (testa + quanto > MAX_RICHIESTA) {
		return errore_http(out, max, 413, "Payload Too Large");
	}
	if (n < testa + quanto) {
		return -1;
	}
	al = snprintf(atteso, sizeof(atteso), "POST /%s ", g_uuid);
	if (testa < al || memcmp(r, atteso, (size_t) al) != 0) {
		return errore_http(out, max, 400, "Bad Request");
	}
	if (testo_elemento(corpo, (int) quanto, "Action", relativo, (int) sizeof(relativo)) != 1 ||
	    strcmp(relativo, AZ_GET) != 0 ||
	    testo_elemento(corpo, (int) quanto, "MessageID", relativo, (int) sizeof(relativo)) != 1 ||
	    relativo[0] == '\0') {
		return errore_http(out, max, 400, "Bad Request");
	}
	nuovo_id(id);
	k = lunghezza(snprintf(g_corpo, sizeof(g_corpo),
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
		"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
		"xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
		"xmlns:wsx=\"http://schemas.xmlsoap.org/ws/2004/09/mex\" "
		"xmlns:dp=\"http://schemas.xmlsoap.org/ws/2006/02/devprof\" "
		"xmlns:pnpx=\"http://schemas.microsoft.com/windows/pnpx/2005/10\" "
		"xmlns:pub=\"http://schemas.microsoft.com/windows/pub/2005/07\">"
		"<s:Header><a:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"
		"</a:To><a:Action>http://schemas.xmlsoap.org/ws/2004/09/transfer/GetResponse"
		"</a:Action><a:MessageID>%s</a:MessageID><a:RelatesTo>%s</a:RelatesTo>"
		"</s:Header><s:Body><wsx:Metadata>"
		"<wsx:MetadataSection Dialect=\"http://schemas.xmlsoap.org/ws/2006/02/devprof/ThisDevice\">"
		"<dp:ThisDevice><dp:FriendlyName>%s</dp:FriendlyName>"
		"<dp:FirmwareVersion>BMC64-NG</dp:FirmwareVersion>"
		"<dp:SerialNumber>%s</dp:SerialNumber></dp:ThisDevice></wsx:MetadataSection>"
		"<wsx:MetadataSection Dialect=\"http://schemas.xmlsoap.org/ws/2006/02/devprof/ThisModel\">"
		"<dp:ThisModel><dp:Manufacturer>BMC64-NG</dp:Manufacturer>"
		"<dp:ModelName>BMC64-NG SD Card Sharing</dp:ModelName>"
		"<pnpx:DeviceCategory>Computers</pnpx:DeviceCategory></dp:ThisModel>"
		"</wsx:MetadataSection>"
		"<wsx:MetadataSection Dialect=\"http://schemas.xmlsoap.org/ws/2006/02/devprof/Relationship\">"
		"<dp:Relationship Type=\"http://schemas.xmlsoap.org/ws/2006/02/devprof/host\">"
		"<dp:Host><a:EndpointReference><a:Address>urn:uuid:%s</a:Address>"
		"</a:EndpointReference><dp:Types>pub:Computer</dp:Types>"
		"<dp:ServiceId>urn:uuid:%s</dp:ServiceId>"
		"<pub:Computer>%s/Workgroup:WORKGROUP</pub:Computer></dp:Host>"
		"</dp:Relationship></wsx:MetadataSection></wsx:Metadata></s:Body>"
		"</s:Envelope>",
		id, relativo, g_nome, g_uuid, g_uuid, g_uuid, g_nome), (int) sizeof(g_corpo));
	if (k == 0) {
		return errore_http(out, max, 500, "Internal Server Error");
	}
	i = lunghezza(snprintf(out, (size_t) max,
		"HTTP/1.1 200 OK\r\nContent-Type: application/soap+xml; charset=utf-8\r\n"
		"Content-Length: %d\r\nConnection: close\r\n\r\n", k), max);
	if (i == 0 || i + k > max) {
		return errore_http(out, max, 500, "Internal Server Error");
	}
	memcpy(out + i, g_corpo, (size_t) k);
	return i + k;
}



static void manda_gruppo(const char *m, int n)
{
	struct sockaddr_in a;

	if (s_udp < 0 || n <= 0) {
		return;
	}
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons(PORTA_WSD);
	a.sin_addr.s_addr = htonl(GRUPPO_WSD);
	sendto(s_udp, m, (size_t) n, 0, (struct sockaddr *) &a, sizeof(a));
}

static void chiudi(int *s)
{
	if (*s >= 0) {
		bmc_sock_close(*s);
		*s = -1;
	}
}

int bmc_wsd_apri(const unsigned char ip[4], const unsigned char mac[6], const char *nome,
		 bmc_nomi_log_fn log)
{
	struct sockaddr_in a;
	struct ip_mreq m;

	g_log = log;
	bmc_wsd_identita(ip, mac, nome, (unsigned long) time(NULL));
	s_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	s_ascolto = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s_udp < 0 || s_ascolto < 0) {
		goto non_va;
	}
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_port = htons(PORTA_WSD);
	a.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(s_udp, (struct sockaddr *) &a, sizeof(a)) < 0) {
		goto non_va;
	}
	memset(&m, 0, sizeof(m));
	m.imr_multiaddr.s_addr = htonl(GRUPPO_WSD);
	m.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(s_udp, IPPROTO_IP, IP_ADD_MEMBERSHIP, &m, sizeof(m)) < 0) {
		goto non_va;
	}
	a.sin_port = htons(PORTA_HTTP);
	if (bind(s_ascolto, (struct sockaddr *) &a, sizeof(a)) < 0 || listen(s_ascolto, 2) < 0) {
		goto non_va;
	}

	g_hello = time(NULL);
	manda_gruppo(g_buf, bmc_wsd_hello(g_buf, (int) sizeof(g_buf)));
	scrivi("WS-Discovery pronto (3702 e 5357)");
	return 1;
non_va:
	chiudi(&s_udp);
	chiudi(&s_ascolto);
	scrivi("WS-Discovery non si apre");
	return 0;
}

void bmc_wsd_fdset(fd_set *rfds, int *maxfd)
{
	int s[3];
	int i;

	s[0] = s_udp;
	s[1] = s_ascolto;
	s[2] = s_cliente;
	for (i = 0; i < 3; i++) {
		if (s[i] >= 0) {
			FD_SET(s[i], rfds);
			if (s[i] > *maxfd) {
				*maxfd = s[i];
			}
		}
	}
}

static void servi_udp(void)
{
	struct sockaddr_in da;
	socklen_t dl = sizeof(da);
	int n, rn;

	n = (int) recvfrom(s_udp, g_udp, MAX_UDP, MSG_DONTWAIT, (struct sockaddr *) &da, &dl);
	if (n <= 0) {
		return;
	}
	g_udp[n] = '\0';
	rn = bmc_wsd_risposta_udp(g_udp, n, g_buf, (int) sizeof(g_buf));
	if (rn <= 0 || rn > MAX_UDP) {
		return;
	}
	sendto(s_udp, g_buf, (size_t) rn, 0, (struct sockaddr *) &da, sizeof(da));
	scrivi("WS-Discovery: risposto a una sonda");
}

static void accogli(void)
{
	int c = accept(s_ascolto, NULL, NULL);

	if (c < 0) {
		return;
	}
	if (s_cliente >= 0) {
		bmc_sock_close(c);
		return;
	}
	s_cliente = c;
	g_rich_n = 0;
	g_cliente_da = time(NULL);
}

static void leggi_cliente(void)
{
	int r, k, mandati;

	r = (int) recv(s_cliente, g_rich + g_rich_n, (size_t) (MAX_RICHIESTA - g_rich_n), MSG_DONTWAIT);
	if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		return;
	}
	if (r <= 0) {
		chiudi(&s_cliente);
		return;
	}
	g_rich_n += r;
	g_rich[g_rich_n] = '\0';
	k = bmc_wsd_risposta_http(g_rich, g_rich_n, g_buf, (int) sizeof(g_buf));
	if (k < 0) {
		return;
	}
	mandati = 0;
	while (mandati < k) {
		int s = (int) send(s_cliente, g_buf + mandati, (size_t) (k - mandati), 0);
		if (s <= 0) {
			break;
		}
		mandati += s;
	}
	chiudi(&s_cliente);
	scrivi("WS-Discovery: data la scheda del computer");
}

void bmc_wsd_servi(fd_set *rfds)
{
	time_t ora = time(NULL);

	if (s_udp >= 0 && ora - g_hello >= HELLO_OGNI) {
		g_hello = ora;
		manda_gruppo(g_buf, bmc_wsd_hello(g_buf, (int) sizeof(g_buf)));
	}
	if (s_udp >= 0 && FD_ISSET(s_udp, rfds)) {
		servi_udp();
	}
	if (s_ascolto >= 0 && FD_ISSET(s_ascolto, rfds)) {
		accogli();
	}
	if (s_cliente >= 0 && FD_ISSET(s_cliente, rfds)) {
		leggi_cliente();
	}
	if (s_cliente >= 0 && ora - g_cliente_da > HTTP_ATTESA) {
		chiudi(&s_cliente);
	}
}

void bmc_wsd_chiudi(void)
{
	if (s_udp >= 0) {
		manda_gruppo(g_buf, bmc_wsd_bye(g_buf, (int) sizeof(g_buf)));
	}
	chiudi(&s_cliente);
	chiudi(&s_ascolto);
	chiudi(&s_udp);
}
