/*
 * Broadcom bcm4330 wifi (sdio interface)
 */

#ifndef __circle__
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"
#include "../port/netif.h"
#include "../port/sd.h"
#else
#include "p9compat.h"
#endif
#include "bmxwlanstatus.h"

extern int sdiocardintr(int);

#ifndef __circle__
#include "etherif.h"
#endif
#define CACHELINESZ 64	/* temp */

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE_LOG
#define BMX_WLAN_LOW_LOG(...) print(__VA_ARGS__)
#else
#define BMX_WLAN_LOW_LOG(...)
#endif

enum{
	SDIODEBUG = 0,
	SBDEBUG = 0,
	EVENTDEBUG = 0,
	VARDEBUG = 0,
	FWDEBUG  = 0,

	Corescansz = 512,
	Uploadsz = 2048,

	Wifichan = 0,		/* default channel */
	Firmwarecmp	= 1,
	WlPi5PktYieldBatch = 2,

	ARMcm3		= 0x82A,
	ARM7tdmi	= 0x825,
	ARMcr4		= 0x83E,

	Fn0	= 0,
	Fn1 	= 1,
	Fn2	= 2,
	Fbr1	= 0x100,
	Fbr2	= 0x200,

	/* CCCR */
	Ioenable	= 0x02,
	Ioready		= 0x03,
	Intenable	= 0x04,
	Intpend		= 0x05,
	Ioabort		= 0x06,
	Busifc		= 0x07,
	Capability	= 0x08,
	Blksize		= 0x10,
	Highspeed	= 0x13,

	/* SDIOCommands */
	GO_IDLE_STATE		= 0,
	SEND_RELATIVE_ADDR	= 3,
	IO_SEND_OP_COND		= 5,
	SELECT_CARD		= 7,
	VOLTAGE_SWITCH 		= 11,
	IO_RW_DIRECT 		= 52,
	IO_RW_EXTENDED 		= 53,

	/* SELECT_CARD args */
	Rcashift	= 16,

	/* SEND_OP_COND args */
	Hcs	= 1<<30,	/* host supports SDHC & SDXC */
	V3_3	= 3<<20,	/* 3.2-3.4 volts */
	V2_8	= 3<<15,	/* 2.7-2.9 volts */
	V2_0	= 1<<8,		/* 2.0-2.1 volts */
	S18R	= 1<<24,	/* switch to 1.8V request */

	/* Sonics Silicon Backplane (access to cores on chip) */
	Sbwsize	= 0x8000,
	Sb32bit	= 0x8000,
	Sbaddr	= 0x1000a,
		Enumbase	= 	0x18000000,
	Framectl= 0x1000d,
		Rfhalt		=	0x01,
		Wfhalt		=	0x02,
	Clkcsr	= 0x1000e,
		ForceALP	=	0x01,	/* active low-power clock */
		ForceHT		= 	0x02,	/* high throughput clock */
		ForceILP	=	0x04,	/* idle low-power clock */
		ReqALP		=	0x08,
		ReqHT		=	0x10,
		Nohwreq		=	0x20,
		ALPavail	=	0x40,
		HTavail		=	0x80,
	Pullups	= 0x1000f,
	Wfrmcnt	= 0x10019,
	Watermark	= 0x10008,
	Devicectl	= 0x10009,
		DevctlF2Watermark = 0x10,
	Mesbusyctrl	= 0x1001d,
		MesbusyctrlEnable = 0x80,
		Bcm43455F2Watermark = 0x60,
		Bcm43455MesWatermark = 0x50,
	Rfrmcnt	= 0x1001b,

	/* core control regs */
	Ioctrl		= 0x408,
	Resetctrl	= 0x800,

	/* socram regs */
	Coreinfo	= 0x00,
	Bankidx		= 0x10,
	Bankinfo	= 0x40,
	Bankpda		= 0x44,

	/* armcr4 regs */
	Cr4Cap		= 0x04,
	Cr4Bankidx	= 0x40,
	Cr4Bankinfo	= 0x44,
	Cr4Cpuhalt	= 0x20,

	/* chipcommon regs */
	Gpiopullup	= 0x58,
	Gpiopulldown	= 0x5c,
	Chipctladdr	= 0x650,
	Chipctldata	= 0x654,

	/* sdio core regs */
	Intstatus	= 0x20,
		Fcstate		= 1<<4,
		Fcchange	= 1<<5,
		FrameInt	= 1<<6,
		MailboxInt	= 1<<7,
	Intmask		= 0x24,
	Sbmbox		= 0x40,
	Sbmboxdata	= 0x48,
	Hostmboxdata= 0x4c,
		Fwready		= 0x80,

	/* wifi control commands */
	GetVar	= 262,
	SetVar	= 263,

	/* status */
	Disconnected=	0,
	Connecting,
	Connected,
};

typedef struct Ctlr Ctlr;

enum{
	Wpa		= 1,
	Wep		= 2,
	Wpa2		= 3,
	WNameLen	= 32,
	WNKeys		= 4,
	WNFirmwareKeys	= 6,
	WKeyLen		= 32,
	WMinKeyLen	= 5,
	WMaxKeyLen	= 13,
	WlcmdTimeoutMs	= 5000,
	WljoinTimeoutMs	= 15000,
	WlcmdRetryDelayMs	= 50,
	WlcmdWsecKeyRetries	= 1,
	FwloadAlpTimeoutUs	= 1000000,
	FwloadUploadTimeoutMs	= 10000,
	FwloadUploadSlowMs	= 500,
	FwloadUploadProgressBytes	= 65536,
};

typedef struct WKey WKey;
struct WKey
{
	ushort	len;
	char	dat[WKeyLen];
};

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum {
	BMXWLDBGCmdRing = 64,
	BMXWLDBGRxRing = 128,
	BMXWLDBGDataRing = 64,
	BMXWLDBGEapolRing = 32,
};

typedef struct BMXWLDBGCmd BMXWLDBGCmd;
struct BMXWLDBGCmd {
	ulong	startticks;
	ulong	doneticks;
	int	id;
	int	op;
	int	write;
	int	result;
	int	status;
	int	txseq0;
	int	txwin0;
	int	fcmask0;
	int	txseq1;
	int	txwin1;
	int	fcmask1;
	int	rspid;
	int	rspop;
	int	rspstatus;
	int	pending;
	int	pendingop;
	char	name[24];
};

typedef struct BMXWLDBGRx BMXWLDBGRx;
struct BMXWLDBGRx {
	ulong	ticks;
	int	chan;
	int	len;
	int	doffset;
	int	bdc;
	int	sdseq;
	int	nextlen;
	int	window;
	int	fcmask;
	int	rxseq;
	int	txseq;
	int	txwin;
	int	ctlfcmask;
	int	cmdid;
	int	cmdop;
	int	cmdstatus;
	int	pending;
	int	pendingop;
	int	event;
	int	eventstatus;
	int	eventreason;
	int	eventflags;
	int	eapolmsg;
	int	eapolkey;
	int	eapoldata;
	int	ethertype;
	int	drop;
	unsigned authgen;
	long	authrelms;
};
#endif

struct Ctlr {
	Ether*	edev;
	QLock	cmdlock;
	QLock	pktlock;
	QLock	tlock;
	QLock	alock;
	Lock	txwinlock;
	Rendez	cmdr;
	Rendez	joinr;
	Rendez	scanr;
	int	joinstatus;
	int	joinssidok;
	int	joinlinkup;
	int	cryptotype;
	int	keyinstalled[WNFirmwareKeys];
	int	chanid;
	uchar	bssid[Eaddrlen];
	char	essid[WNameLen + 1];
	WKey	keys[WNKeys];
	Block	*rsp;
	int	cmdpending;
	int	cmdop;
	Block	*scanb;
	int	scansecs;
	int	scanactive;
	int	status;
	volatile int resetting;
	const char *alockowner;
	ulong	alockticks;
	int	chipid;
	int	chiprev;
	int	armcore;
	char	country[3];
	char	*regufile;
	union {
		u32int i;
		uchar c[4];
	} resetvec;
	ulong	chipcommon;
	ulong	armctl;
	ulong	armregs;
	ulong	d11ctl;
	ulong	socramregs;
	ulong	socramctl;
	ulong	sdregs;
	int	sdiorev;
	int	socramrev;
	ulong	socramsize;
	ulong	rambase;
	short	reqid;
	uchar	fcmask;
	uchar	txwindow;
	uchar	txseq;
	uchar	rxseq;
	ether_event_handler_t *evhndlr;
	void	*evcontext;
	Lock	statlock;
	uvlong	stat_txframes;
	uvlong	stat_rxdataframes;
	uvlong	stat_txwindowupdates;
	uvlong	stat_txflowupdates;
	uvlong	stat_txwindowstalls;
	uvlong	stat_txwindowstallticks;
	uvlong	stat_txwindowstallmaxticks;
	ulong	stat_txwindowstallstart;
	int	stat_txwindowblocked;
	uvlong	stat_txflowstalls;
	uvlong	stat_txflowstallticks;
	uvlong	stat_txflowstallmaxticks;
	ulong	stat_txflowstallstart;
	int	stat_txflowblocked;
	uvlong	stat_txtimingsamples;
	uvlong	stat_txqueueus;
	uvlong	stat_txqueuemaxus;
	uvlong	stat_txpktlockwaitus;
	uvlong	stat_txpktlockwaitmaxus;
	uvlong	stat_txsdious;
	uvlong	stat_txsdiomaxus;
	uvlong	stat_txpktlockyieldcalls;
	uvlong	stat_txpktlockyieldus;
	uvlong	stat_txpktlockyieldmaxus;
#if RASPPI >= 5
	int	txyieldbatch;
#endif
	uvlong	stat_rxtimingsamples;
	uvlong	stat_rxpktlockwaitus;
	uvlong	stat_rxpktlockwaitmaxus;
	uvlong	stat_rxsdious;
	uvlong	stat_rxsdiomaxus;
	uvlong	stat_rxpktlockyieldcalls;
	uvlong	stat_rxpktlockyieldus;
	uvlong	stat_rxpktlockyieldmaxus;
	uvlong	stat_rxtonetdevsamples;
	uvlong	stat_rxtonetdevus;
	uvlong	stat_rxtonetdevmaxus;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	ulong	dbg_iw_calls;
	ulong	dbg_iw_intpend0;
	ulong	dbg_iw_frameint;
	ulong	dbg_iw_mailboxint;
	ulong	dbg_iw_fcchange;
	ulong	dbg_rp_calls;
	ulong	dbg_rp_zero;
	ulong	dbg_rp_bad;
	ulong	dbg_rp_chan0;
	ulong	dbg_rp_chan1;
	ulong	dbg_rp_chan2;
	ulong	dbg_rp_chanx;
	ulong	dbg_rsp_short;
	ulong	dbg_rsp_stale;
	ulong	dbg_rsp_match;
	ulong	dbg_rsp_replace;
	ulong	dbg_event_count;
	ulong	dbg_event_setssid;
	ulong	dbg_event_assoc;
	ulong	dbg_event_linkup;
	ulong	dbg_event_linkdown;
	ulong	dbg_event_deauth;
	ulong	dbg_event_disassoc;
	ulong	dbg_event_escan;
	uchar	dbg_last_intpend;
	ulong	dbg_last_intstatus;
	ulong	dbg_last_hostmbox;
	int	dbg_last_len;
	int	dbg_last_lenck;
	int	dbg_last_chanflg;
	int	dbg_last_nextlen;
	int	dbg_last_doffset;
	int	dbg_last_window;
	int	dbg_last_fcmask;
	int	dbg_last_rsp_id;
	int	dbg_last_rsp_op;
	int	dbg_last_rsp_pending;
	int	dbg_last_rsp_pendingop;
	ulong	dbg_last_rsp_status;
	int	dbg_last_rsp_len;
	int	dbg_last_rsp_doffset;
	int	dbg_pending_id;
	int	dbg_pending_op;
	int	dbg_pending_write;
	int	dbg_pending_txseq;
	int	dbg_pending_txwindow;
	int	dbg_pending_fcmask;
	ulong	dbg_pending_start;
	char	dbg_pending_var[32];
	ulong	dbg_last_event;
	ulong	dbg_last_event_status;
	ulong	dbg_last_event_reason;
	int	dbg_last_event_flags;
	ulong	dbg_join_event;
	ulong	dbg_join_event_status;
	ulong	dbg_join_event_reason;
	int	dbg_join_event_flags;
	BMXWLDBGCmd dbg_cmdring[BMXWLDBGCmdRing];
	unsigned dbg_cmdwrite;
	unsigned dbg_cmdcount;
	BMXWLDBGRx dbg_rxring[BMXWLDBGRxRing];
	unsigned dbg_rxwrite;
	unsigned dbg_rxcount;
	BMXWLDBGRx dbg_dataring[BMXWLDBGDataRing];
	unsigned dbg_datawrite;
	unsigned dbg_datacount;
	BMXWLDBGRx dbg_eapolring[BMXWLDBGEapolRing];
	unsigned dbg_eapolwrite;
	unsigned dbg_eapolcount;
	ulong	dbg_data_seen;
	ulong	dbg_data_ok;
	ulong	dbg_data_eapol;
	ulong	dbg_data_short_bdc;
	ulong	dbg_data_short_eth;
	ulong	dbg_event_short_bdc;
	ulong	dbg_event_short_body;
	unsigned dbg_authgen;
	ulong	dbg_authstartticks;
#endif
};

#ifdef BMC64_WLAN_TRACE
enum {
	WlTraceETHHeaderLen = 14,
	WlTraceIPHeaderMinLen = 20,
	WlTraceUDPHeaderLen = 8,
	WlTraceETHProtIP = 0x0800,
	WlTraceETHProtEAPOL = 0x888E,
	WlTraceIPProtoUDP = 17,
	WlTraceDHCPPortServer = 67,
	WlTraceDHCPPortClient = 68,
};

static ushort
wldhcptracebe16(const uchar *p)
{
	return (ushort)p[0]<<8 | p[1];
}

static ulong
wldhcptracebe32(const uchar *p)
{
	return   (ulong)p[0]<<24
	       | (ulong)p[1]<<16
	       | (ulong)p[2]<<8
	       | (ulong)p[3];
}

static char*
wldhcptracetypename(int type)
{
	switch(type){
	case 1:
		return "DISCOVER";
	case 2:
		return "OFFER";
	case 3:
		return "REQUEST";
	case 5:
		return "ACK";
	case 6:
		return "NAK";
	default:
		return "UNKNOWN";
	}
}

static int
wldhcptracetype(const uchar *payload, int len)
{
	int i, opt, optlen;

	if(len < 240)
		return 0;
	if(payload[236] != 99 || payload[237] != 130 ||
	   payload[238] != 83 || payload[239] != 99)
		return 0;

	for(i = 240; i < len; ){
		opt = payload[i];
		if(opt == 255)
			break;
		if(opt == 0){
			i++;
			continue;
		}
		if(i+2 > len)
			break;
		optlen = payload[i+1];
		if(i+2+optlen > len)
			break;
		if(opt == 53 && optlen >= 1)
			return payload[i+2];
		i += 2+optlen;
	}

	return 0;
}

static int
wldhcptraceisdhcpport(ushort sport, ushort dport)
{
	return sport == WlTraceDHCPPortServer ||
	       sport == WlTraceDHCPPortClient ||
	       dport == WlTraceDHCPPortServer ||
	       dport == WlTraceDHCPPortClient;
}

static void
wldhcptrace(const char *stage, const uchar *frame, int len, Ctlr *ctl)
{
	const uchar *ip, *udp, *dhcp;
	int iphdrlen, udplen, dhcplen, type;
	ushort sport, dport, flags;
	ulong xid;

	if(frame == nil || len < WlTraceETHHeaderLen + WlTraceIPHeaderMinLen + WlTraceUDPHeaderLen)
		return;
	if(wldhcptracebe16(frame+12) != WlTraceETHProtIP)
		return;

	ip = frame + WlTraceETHHeaderLen;
	iphdrlen = (ip[0] & 0x0F) * 4;
	if((ip[0] >> 4) != 4 ||
	   iphdrlen < WlTraceIPHeaderMinLen ||
	   len < WlTraceETHHeaderLen + iphdrlen + WlTraceUDPHeaderLen ||
	   ip[9] != WlTraceIPProtoUDP)
		return;

	udp = ip + iphdrlen;
	sport = wldhcptracebe16(udp);
	dport = wldhcptracebe16(udp+2);
	if(!wldhcptraceisdhcpport(sport, dport))
		return;

	udplen = wldhcptracebe16(udp+4);
	if(udplen < WlTraceUDPHeaderLen ||
	   len < WlTraceETHHeaderLen + iphdrlen + udplen){
		print("bmx-wl-dhcp: %s truncated len %d udplen %d ports %d>%d txseq %d txwin %d fcmask %#x qlen %u\n",
			stage, len, udplen, sport, dport,
			ctl != nil ? ctl->txseq : -1,
			ctl != nil ? ctl->txwindow : -1,
			ctl != nil ? ctl->fcmask : 0,
			ctl != nil && ctl->edev != nil && ctl->edev->oq != nil ? qlen(ctl->edev->oq) : 0);
		return;
	}

	dhcp = udp + WlTraceUDPHeaderLen;
	dhcplen = udplen - WlTraceUDPHeaderLen;
	type = wldhcptracetype(dhcp, dhcplen);
	xid = dhcplen >= 8 ? wldhcptracebe32(dhcp+4) : 0;
	flags = dhcplen >= 12 ? wldhcptracebe16(dhcp+10) : 0;

	print("bmx-wl-dhcp: %s len %d eth %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x>%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x ip %d.%d.%d.%d>%d.%d.%d.%d udp %d>%d dhcp %s(%d) xid %8.8lx flags %4.4x txseq %d txwin %d fcmask %#x qlen %u\n",
		stage, len,
		frame[6], frame[7], frame[8], frame[9], frame[10], frame[11],
		frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
		ip[12], ip[13], ip[14], ip[15],
		ip[16], ip[17], ip[18], ip[19],
		sport, dport, wldhcptracetypename(type), type, xid, flags,
		ctl != nil ? ctl->txseq : -1,
		ctl != nil ? ctl->txwindow : -1,
		ctl != nil ? ctl->fcmask : 0,
		ctl != nil && ctl->edev != nil && ctl->edev->oq != nil ? qlen(ctl->edev->oq) : 0);
}

static char*
wleapoltracetypename(int type)
{
	switch(type){
	case 0:
		return "packet";
	case 1:
		return "start";
	case 2:
		return "logoff";
	case 3:
		return "key";
	case 4:
		return "asf-alert";
	default:
		return "unknown";
	}
}

static char*
wleapoltracekeydesc(int desc)
{
	switch(desc){
	case 2:
		return "rsn";
	case 254:
		return "wpa";
	default:
		return "unknown";
	}
}

static char*
wleapoltracemsg(ushort keyinfo)
{
	int pairwise, install, ack, mic, secure, encr;

	pairwise = (keyinfo & 0x0008) != 0;
	install = (keyinfo & 0x0040) != 0;
	ack = (keyinfo & 0x0080) != 0;
	mic = (keyinfo & 0x0100) != 0;
	secure = (keyinfo & 0x0200) != 0;
	encr = (keyinfo & 0x1000) != 0;

	if(pairwise){
		if(ack && !mic)
			return "4way-1/4";
		if(!ack && mic && !secure)
			return "4way-2/4";
		if(ack && mic && install && secure && encr)
			return "4way-3/4";
		if(!ack && mic && secure)
			return "4way-4/4";
		return "4way-key";
	}

	if(ack && mic && secure)
		return "group-1/2";
	if(!ack && mic && secure)
		return "group-2/2";
	return "group-key";
}

static void
wleapoltrace(const char *stage, const uchar *frame, int len, Ctlr *ctl)
{
	const uchar *eapol;
	int payloadlen, version, type, eapollen, desc, keylen, keydatalen;
	ushort keyinfo;

	if(frame == nil || len < WlTraceETHHeaderLen + 4)
		return;
	if(wldhcptracebe16(frame+12) != WlTraceETHProtEAPOL)
		return;

	eapol = frame + WlTraceETHHeaderLen;
	payloadlen = len - WlTraceETHHeaderLen;
	version = eapol[0];
	type = eapol[1];
	eapollen = wldhcptracebe16(eapol+2);
	if(type != 3 || payloadlen < 99){
		print("bmx-wl-eapol: %s len %d eth %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x>%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x version %d type %s(%d) eapol_len %d txseq %d txwin %d fcmask %#x qlen %u\n",
			stage, len,
			frame[6], frame[7], frame[8], frame[9], frame[10], frame[11],
			frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
			version, wleapoltracetypename(type), type, eapollen,
			ctl != nil ? ctl->txseq : -1,
			ctl != nil ? ctl->txwindow : -1,
			ctl != nil ? ctl->fcmask : 0,
			ctl != nil && ctl->edev != nil && ctl->edev->oq != nil ? qlen(ctl->edev->oq) : 0);
		return;
	}

	desc = eapol[4];
	keyinfo = wldhcptracebe16(eapol+5);
	keylen = wldhcptracebe16(eapol+7);
	keydatalen = wldhcptracebe16(eapol+97);
	print("bmx-wl-eapol: %s len %d eth %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x>%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x version %d eapol_len %d desc %s(%d) msg %s key_info %#4.4x pairwise %d install %d ack %d mic %d secure %d encr %d request %d error %d key_len %d replay %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x key_data_len %d txseq %d txwin %d fcmask %#x qlen %u\n",
		stage, len,
		frame[6], frame[7], frame[8], frame[9], frame[10], frame[11],
		frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
		version, eapollen, wleapoltracekeydesc(desc), desc,
		wleapoltracemsg(keyinfo), keyinfo,
		(keyinfo & 0x0008) != 0, (keyinfo & 0x0040) != 0,
		(keyinfo & 0x0080) != 0, (keyinfo & 0x0100) != 0,
		(keyinfo & 0x0200) != 0, (keyinfo & 0x1000) != 0,
		(keyinfo & 0x0800) != 0, (keyinfo & 0x0400) != 0,
		keylen,
		eapol[9], eapol[10], eapol[11], eapol[12],
		eapol[13], eapol[14], eapol[15], eapol[16],
		keydatalen,
		ctl != nil ? ctl->txseq : -1,
		ctl != nil ? ctl->txwindow : -1,
		ctl != nil ? ctl->fcmask : 0,
		ctl != nil && ctl->edev != nil && ctl->edev->oq != nil ? qlen(ctl->edev->oq) : 0);
}
#else
#define wldhcptrace(_stage, _frame, _len, _ctl)
#define wleapoltrace(_stage, _frame, _len, _ctl)
#endif

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum {
	BMX_EAPOL_STAGE_WL_TX_QGET = 10,
	BMX_EAPOL_STAGE_WL_TX_SDIO = 11,
	BMX_EAPOL_STAGE_WL_TX_SDIO_DONE = 12,
	BMX_EAPOL_STAGE_WL_TX_SDIO_ERROR = 13,
	BMX_EAPOL_STAGE_WL_RX_SDIO = 14,
};

extern void bmx_eapol_low_frame(unsigned stage, const uchar *frame, uint len,
	int txseq, int txwin, uint fcmask, uint qlen);
extern void bmx_eapol_low_key_install(uint keyid, int pairwise, uint ms);
extern void bmx_dhcp_low_frame(unsigned stage, const uchar *frame, uint len,
	int txseq, int txwin, uint fcmask, uint qlen);

enum {
	BMX_DHCP_STAGE_WL_TX_QGET = 38,
	BMX_DHCP_STAGE_WL_TX_SDIO = 39,
	BMX_DHCP_STAGE_WL_TX_SDIO_DONE = 40,
	BMX_DHCP_STAGE_WL_TX_SDIO_ERROR = 41,
	BMX_DHCP_STAGE_WL_RX_SDIO = 42,
};

#define wleapollowtrace(_stage, _frame, _len, _ctl) \
	bmx_eapol_low_frame((_stage), (_frame), (_len), \
		(_ctl) != nil ? (_ctl)->txseq : -1, \
		(_ctl) != nil ? (_ctl)->txwindow : -1, \
		(_ctl) != nil ? (_ctl)->fcmask : 0, \
		(_ctl) != nil && (_ctl)->edev != nil && (_ctl)->edev->oq != nil ? qlen((_ctl)->edev->oq) : 0)
#define wldhcplowtrace(_stage, _frame, _len, _ctl) \
	bmx_dhcp_low_frame((_stage), (_frame), (_len), \
		(_ctl) != nil ? (_ctl)->txseq : -1, \
		(_ctl) != nil ? (_ctl)->txwindow : -1, \
		(_ctl) != nil ? (_ctl)->fcmask : 0, \
		(_ctl) != nil && (_ctl)->edev != nil && (_ctl)->edev->oq != nil ? qlen((_ctl)->edev->oq) : 0)
#else
#define wleapollowtrace(_stage, _frame, _len, _ctl)
#define wldhcplowtrace(_stage, _frame, _len, _ctl)
#endif

#ifdef __circle__
#define BMX_WAIT_PENDING_8021X_TX_MS 950
extern void bmx_l2_note_done_8021x_tx(void);
extern int bmx_l2_wait_pending_8021x_tx(uint timeout_ms);
#else
#define BMX_WAIT_PENDING_8021X_TX_MS 0
static void bmx_l2_note_done_8021x_tx(void) {}
static int bmx_l2_wait_pending_8021x_tx(uint timeout_ms) { USED(timeout_ms); return 1; }
#endif

typedef struct Sdpcm Sdpcm;
typedef struct Cmd Cmd;
struct Sdpcm {
	uchar	len[2];
	uchar	lenck[2];
	uchar	seq;
	uchar	chanflg;
	uchar	nextlen;
	uchar	doffset;
	uchar	fcmask;
	uchar	window;
	uchar	version;
	uchar	pad;
};

struct Cmd {
	uchar	cmd[4];
	uchar	len[4];
	uchar	flags[2];
	uchar	id[2];
	uchar	status[4];
};

static ulong get4(uchar*);
static char* evstring(uint);

static int
wleis8021x(const uchar *frame, int len)
{
	return len >= ETHERHDRSIZE && frame[12] == 0x88 && frame[13] == 0x8e;
}

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
enum {
	BMXWLDBGEapolNone = 0,
	BMXWLDBGEapolM1,
	BMXWLDBGEapolM2,
	BMXWLDBGEapolM3,
	BMXWLDBGEapolM4,
	BMXWLDBGEapolOther,
	BMXWLDBGEapolTrunc,
};

enum {
	BMXWLDBGDropNone = 0,
	BMXWLDBGDropDataShortBdc,
	BMXWLDBGDropDataShortEth,
	BMXWLDBGDropEventShortBdc,
	BMXWLDBGDropEventShortBody,
};

static int
bmxwldbg_eapolmsg(const uchar *frame, int len, int *keyinfo, int *keydata)
{
	const uchar *p;
	int ki, kdlen;

	if(keyinfo != nil)
		*keyinfo = 0;
	if(keydata != nil)
		*keydata = 0;
	if(!wleis8021x(frame, len))
		return BMXWLDBGEapolNone;

	p = frame + ETHERHDRSIZE;
	len -= ETHERHDRSIZE;
	if(len < 4)
		return BMXWLDBGEapolTrunc;
	if(p[1] != 3)
		return BMXWLDBGEapolOther;
	if(len < 99)
		return BMXWLDBGEapolTrunc;

	ki = (p[5]<<8) | p[6];
	kdlen = (p[97]<<8) | p[98];
	if(keyinfo != nil)
		*keyinfo = ki;
	if(keydata != nil)
		*keydata = kdlen;

	if((ki & 0x0100) == 0 && (ki & 0x0080) != 0)
		return BMXWLDBGEapolM1;
	if((ki & 0x0100) != 0 && (ki & 0x0080) != 0)
		return BMXWLDBGEapolM3;
	if((ki & 0x0100) != 0 && kdlen != 0)
		return BMXWLDBGEapolM2;
	if((ki & 0x0100) != 0)
		return BMXWLDBGEapolM4;

	return BMXWLDBGEapolOther;
}

static char*
bmxwldbg_eapolname(int msg)
{
	switch(msg){
	case BMXWLDBGEapolM1:
		return "m1";
	case BMXWLDBGEapolM2:
		return "m2";
	case BMXWLDBGEapolM3:
		return "m3";
	case BMXWLDBGEapolM4:
		return "m4";
	case BMXWLDBGEapolOther:
		return "other";
	case BMXWLDBGEapolTrunc:
		return "trunc";
	default:
		return "-";
	}
}

static char*
bmxwldbg_dropname(int drop)
{
	switch(drop){
	case BMXWLDBGDropDataShortBdc:
		return "data-short-bdc";
	case BMXWLDBGDropDataShortEth:
		return "data-short-eth";
	case BMXWLDBGDropEventShortBdc:
		return "event-short-bdc";
	case BMXWLDBGDropEventShortBody:
		return "event-short-body";
	default:
		return "-";
	}
}

static void
bmxwldbg_rxcopy(BMXWLDBGRx *dst, const BMXWLDBGRx *src)
{
	memmove(dst, src, sizeof *dst);
}

static void
bmxwldbg_rxpush(Ctlr *ctl, const BMXWLDBGRx *src)
{
	BMXWLDBGRx *e;

	e = &ctl->dbg_rxring[ctl->dbg_rxwrite % BMXWLDBGRxRing];
	bmxwldbg_rxcopy(e, src);
	ctl->dbg_rxwrite++;
	if(ctl->dbg_rxcount < BMXWLDBGRxRing)
		ctl->dbg_rxcount++;
}

static void
bmxwldbg_datapush(Ctlr *ctl, const BMXWLDBGRx *src)
{
	BMXWLDBGRx *e;

	e = &ctl->dbg_dataring[ctl->dbg_datawrite % BMXWLDBGDataRing];
	bmxwldbg_rxcopy(e, src);
	ctl->dbg_datawrite++;
	if(ctl->dbg_datacount < BMXWLDBGDataRing)
		ctl->dbg_datacount++;
}

static void
bmxwldbg_eapolpush(Ctlr *ctl, const BMXWLDBGRx *src)
{
	BMXWLDBGRx *e;

	e = &ctl->dbg_eapolring[ctl->dbg_eapolwrite % BMXWLDBGEapolRing];
	bmxwldbg_rxcopy(e, src);
	ctl->dbg_eapolwrite++;
	if(ctl->dbg_eapolcount < BMXWLDBGEapolRing)
		ctl->dbg_eapolcount++;
}

static void
bmxwldbg_cmdstart(Ctlr *ctl, int id, int op, const char *name, int write)
{
	BMXWLDBGCmd *e;

	if(ctl == nil)
		return;
	e = &ctl->dbg_cmdring[ctl->dbg_cmdwrite % BMXWLDBGCmdRing];
	memset(e, 0, sizeof *e);
	e->startticks = m->ticks;
	e->id = id;
	e->op = op;
	e->write = write;
	e->result = 0;
	e->txseq0 = ctl->txseq;
	e->txwin0 = ctl->txwindow;
	e->fcmask0 = ctl->fcmask;
	e->pending = ctl->cmdpending;
	e->pendingop = ctl->cmdop;
	snprint(e->name, sizeof e->name, "%s", name != nil ? name : "-");
	ctl->dbg_cmdwrite++;
	if(ctl->dbg_cmdcount < BMXWLDBGCmdRing)
		ctl->dbg_cmdcount++;
}

static BMXWLDBGCmd*
bmxwldbg_cmdfind(Ctlr *ctl, int id, int op)
{
	BMXWLDBGCmd *e;
	unsigned i, idx;

	if(ctl == nil)
		return nil;
	for(i = 0; i < ctl->dbg_cmdcount; i++){
		idx = (ctl->dbg_cmdwrite - 1 - i) % BMXWLDBGCmdRing;
		e = &ctl->dbg_cmdring[idx];
		if(e->id == id && e->op == op)
			return e;
	}
	return nil;
}

static void
bmxwldbg_cmdaftertx(Ctlr *ctl, int id, int op)
{
	BMXWLDBGCmd *e;

	e = bmxwldbg_cmdfind(ctl, id, op);
	if(e == nil)
		return;
	e->txseq1 = ctl->txseq;
	e->txwin1 = ctl->txwindow;
	e->fcmask1 = ctl->fcmask;
}

static void
bmxwldbg_cmdfinish(Ctlr *ctl, int id, int op, int result, int rspid, int rspop,
	int status)
{
	BMXWLDBGCmd *e;

	e = bmxwldbg_cmdfind(ctl, id, op);
	if(e == nil)
		return;
	e->doneticks = m->ticks;
	e->result = result;
	e->rspid = rspid;
	e->rspop = rspop;
	e->rspstatus = status;
	e->status = status;
	e->txseq1 = ctl->txseq;
	e->txwin1 = ctl->txwindow;
	e->fcmask1 = ctl->fcmask;
	e->pending = ctl->cmdpending;
	e->pendingop = ctl->cmdop;
}

static void
bmxwldbg_rxrecord(Ctlr *ctl, Sdpcm *sd, Block *b)
{
	BMXWLDBGRx entry;
	uchar *p;
	int len, chan, bdc, keyinfo, keydata;

	if(ctl == nil || sd == nil || b == nil)
		return;

	len = BLEN(b);
	chan = sd->chanflg & 0xF;
	memset(&entry, 0, sizeof entry);
	entry.ticks = m->ticks;
	entry.chan = chan;
	entry.len = len;
	entry.doffset = sd->doffset;
	entry.sdseq = sd->seq;
	entry.nextlen = sd->nextlen;
	entry.window = sd->window;
	entry.fcmask = sd->fcmask;
	entry.rxseq = ctl->rxseq;
	entry.txseq = ctl->txseq;
	entry.txwin = ctl->txwindow;
	entry.ctlfcmask = ctl->fcmask;
	entry.pending = ctl->cmdpending;
	entry.pendingop = ctl->cmdop;
	entry.eapolmsg = BMXWLDBGEapolNone;
	entry.authgen = ctl->dbg_authgen;
	entry.authrelms = ctl->dbg_authstartticks != 0 && m->ticks >= ctl->dbg_authstartticks
		? (long)((m->ticks - ctl->dbg_authstartticks) * 1000 / HZ) : -1;

	if(chan == 0 && sd->doffset >= sizeof(Sdpcm) && len >= sd->doffset + (int)sizeof(Cmd)){
		Cmd *q = (Cmd*)(b->rp + sd->doffset);
		entry.cmdid = q->id[0] | q->id[1]<<8;
		entry.cmdop = get4(q->cmd);
		entry.cmdstatus = get4(q->status);
	}else if(chan == 1){
		if(len <= sd->doffset + 4){
			entry.drop = BMXWLDBGDropEventShortBdc;
			ctl->dbg_event_short_bdc++;
		}else{
			bdc = 4 + (b->rp[sd->doffset + 3] << 2);
			entry.bdc = bdc;
			if(len <= sd->doffset + bdc){
				entry.drop = BMXWLDBGDropEventShortBody;
				ctl->dbg_event_short_body++;
			}else{
				p = b->rp + sd->doffset + bdc;
				if(len > sd->doffset + bdc + ETHERHDRSIZE + 10 + 46){
					uchar *ev = p + ETHERHDRSIZE + 10;
					entry.eventflags = nhgets(ev + 2);
					entry.event = nhgets(ev + 6);
					entry.eventstatus = nhgetl(ev + 8);
					entry.eventreason = nhgetl(ev + 12);
				}
			}
		}
	}else if(chan == 2){
		ctl->dbg_data_seen++;
		if(len <= sd->doffset + 4){
			entry.drop = BMXWLDBGDropDataShortBdc;
			ctl->dbg_data_short_bdc++;
		}else{
			bdc = 4 + (b->rp[sd->doffset + 3] << 2);
			entry.bdc = bdc;
			if(len < sd->doffset + bdc + ETHERHDRSIZE){
				entry.drop = BMXWLDBGDropDataShortEth;
				ctl->dbg_data_short_eth++;
			}else{
				p = b->rp + sd->doffset + bdc;
				entry.ethertype = (p[12] << 8) | p[13];
				entry.eapolmsg = bmxwldbg_eapolmsg(p, len - sd->doffset - bdc,
					&keyinfo, &keydata);
				entry.eapolkey = keyinfo;
				entry.eapoldata = keydata;
				ctl->dbg_data_ok++;
				if(entry.eapolmsg != BMXWLDBGEapolNone){
					ctl->dbg_data_eapol++;
					bmxwldbg_eapolpush(ctl, &entry);
				}
			}
		}
		bmxwldbg_datapush(ctl, &entry);
	}

	bmxwldbg_rxpush(ctl, &entry);
}

static void
bmxwldbg_printrx(const char *name, unsigned i, const BMXWLDBGRx *re)
{
	print("ether4330: trace %s idx %u t %lu gen %u rel %ldms ch %d len %d off %d bdc %d sdseq %d next %d win %d fm %#x ctlrx %d tx %d/%d/%#x cmd %d/%d st %d pend %d/%d event %s(%d) st %d reason %d flags %#x eth %#x drop %s eapol %s key %#x data %d\n",
		name, i, re->ticks, re->authgen, re->authrelms,
		re->chan, re->len, re->doffset, re->bdc,
		re->sdseq, re->nextlen, re->window, re->fcmask,
		re->rxseq, re->txseq, re->txwin, re->ctlfcmask,
		re->cmdid, re->cmdop, re->cmdstatus, re->pending,
		re->pendingop, evstring(re->event), re->event,
		re->eventstatus, re->eventreason, re->eventflags,
		re->ethertype, bmxwldbg_dropname(re->drop),
		bmxwldbg_eapolname(re->eapolmsg), re->eapolkey,
		re->eapoldata);
}

static void
bmxwldbg_dump(Ctlr *ctl, const char *reason)
{
	BMXWLDBGCmd *ce;
	BMXWLDBGRx *re;
	unsigned i, n, idx;
	long authrelms;

	if(ctl == nil)
		return;

	authrelms = ctl->dbg_authstartticks != 0 && m->ticks >= ctl->dbg_authstartticks
		? (long)((m->ticks - ctl->dbg_authstartticks) * 1000 / HZ) : -1;
	print("ether4330: trace dump reason %s authgen %u authrel %ldms status %d join %d/%d/%d scan %d/%d txseq %d txwindow %d fcmask %#x rxseq %d pending %d/%d reqid %d rsp %lu/%lu/%lu/%lu data %lu/%lu eapol %lu short %lu/%lu evshort %lu/%lu\n",
		reason != nil ? reason : "-", ctl->dbg_authgen, authrelms,
		ctl->status, ctl->joinstatus,
		ctl->joinssidok, ctl->joinlinkup, ctl->scanactive, ctl->scansecs,
		ctl->txseq, ctl->txwindow, ctl->fcmask, ctl->rxseq,
		ctl->cmdpending, ctl->cmdop, ctl->reqid,
		ctl->dbg_rsp_match, ctl->dbg_rsp_stale,
		ctl->dbg_rsp_short, ctl->dbg_rsp_replace,
		ctl->dbg_data_seen, ctl->dbg_data_ok, ctl->dbg_data_eapol,
		ctl->dbg_data_short_bdc, ctl->dbg_data_short_eth,
		ctl->dbg_event_short_bdc, ctl->dbg_event_short_body);

	n = ctl->dbg_cmdcount;
	if(n > 24)
		n = 24;
	print("ether4330: trace cmd last %u/%u\n", n, ctl->dbg_cmdcount);
	for(i = 0; i < n; i++){
		idx = (ctl->dbg_cmdwrite - n + i) % BMXWLDBGCmdRing;
		ce = &ctl->dbg_cmdring[idx];
		print("ether4330: trace cmd idx %u t %lu id %d op %d var %s write %d result %d rsp %d/%d status %d tx %d/%d/%#x -> %d/%d/%#x pending %d/%d ms %lu\n",
			i, ce->startticks, ce->id, ce->op, ce->name,
			ce->write, ce->result, ce->rspid, ce->rspop,
			ce->rspstatus, ce->txseq0, ce->txwin0, ce->fcmask0,
			ce->txseq1, ce->txwin1, ce->fcmask1, ce->pending,
			ce->pendingop,
			ce->doneticks >= ce->startticks ? (ce->doneticks - ce->startticks) * 1000 / HZ : 0);
	}

	n = ctl->dbg_rxcount;
	if(n > 32)
		n = 32;
	print("ether4330: trace rx last %u/%u\n", n, ctl->dbg_rxcount);
	for(i = 0; i < n; i++){
		idx = (ctl->dbg_rxwrite - n + i) % BMXWLDBGRxRing;
		re = &ctl->dbg_rxring[idx];
		bmxwldbg_printrx("rx", i, re);
	}

	n = ctl->dbg_datacount;
	if(n > 32)
		n = 32;
	print("ether4330: trace data last %u/%u\n", n, ctl->dbg_datacount);
	for(i = 0; i < n; i++){
		idx = (ctl->dbg_datawrite - n + i) % BMXWLDBGDataRing;
		re = &ctl->dbg_dataring[idx];
		bmxwldbg_printrx("data", i, re);
	}

	n = ctl->dbg_eapolcount;
	if(n > 16)
		n = 16;
	print("ether4330: trace eapol last %u/%u\n", n, ctl->dbg_eapolcount);
	for(i = 0; i < n; i++){
		idx = (ctl->dbg_eapolwrite - n + i) % BMXWLDBGEapolRing;
		re = &ctl->dbg_eapolring[idx];
		bmxwldbg_printrx("eapol", i, re);
	}
}
#endif

enum{
	CMauth,
	CMchannel,
	CMcrypt,
	CMessid,
	CMkey1,
	CMkey2,
	CMkey3,
	CMkey4,
	CMrxkey,
	CMrxkey0,
	CMrxkey1,
	CMrxkey2,
	CMrxkey3,
	CMtxkey,
	CMclearkey,
	CMdebug,
	CMjoin,
	CMdisassoc,
	CMescan,
	CMcountry,
	CMcreate,
	CMdown,
};

static Cmdtab cmds[] = {
	{CMauth,	"auth", 2},
	{CMchannel,	"channel", 2},
	{CMcrypt,	"crypt", 2},
	{CMessid,	"essid", 2},
	{CMkey1,	"key1",	2},
	{CMkey2,	"key2",	2},
	{CMkey3,	"key3",	2},
	{CMkey4,	"key4",	2},
	{CMrxkey,	"rxkey", 3},
	{CMrxkey0,	"rxkey0", 3},
	{CMrxkey1,	"rxkey1", 3},
	{CMrxkey2,	"rxkey2", 3},
	{CMrxkey3,	"rxkey3", 3},
	{CMtxkey,	"txkey", 3},
	{CMclearkey,	"clearkey", 2},
	{CMdebug,	"debug", 2},
	{CMjoin,	"join", 5},
	{CMdisassoc,	"disassoc", 2},
	{CMescan,	"escan", 2},
	{CMcountry,	"country", 2},
	{CMcreate,	"create", 4},
	{CMdown,	"down", 1},
};

static char config40181[] = "bcmdhd.cal.40181";
static char config40183[] = "bcmdhd.cal.40183.26MHz";

static struct {
	int chipid;
	int chiprev;
	char *fwfile;
	char *cfgfile;
	char *regufile;
} firmware[] = {
	{ 0x4330, 3,	"fw_bcm40183b1.bin", config40183, 0 },
	{ 0x4330, 4,	"fw_bcm40183b2.bin", config40183, 0 },
	{ 43362, 0,	"fw_bcm40181a0.bin", config40181, 0 },
	{ 43362, 1,	"fw_bcm40181a2.bin", config40181, 0 },
	{ 43430, 1,	"brcmfmac43430-sdio.bin", "brcmfmac43430-sdio.txt", "brcmfmac43430-sdio.clm_blob" },
	{ 43430, 2,	"brcmfmac43436-sdio.bin", "brcmfmac43436-sdio.txt", "brcmfmac43436-sdio.clm_blob" },
	// This may be necessary for newer Raspberry Pi Zero 2 W:
	// { ???, ???,	"brcmfmac43436s-sdio.bin", "brcmfmac43436s-sdio.txt", 0 },
#if RASPPI <= 4
	{ 0x4345, 6, "brcmfmac43455-sdio.bin", "brcmfmac43455-sdio.txt", "brcmfmac43455-sdio.clm_blob" },
#else
	{ 0x4345, 6, "brcmfmac43455-sdio.raspberrypi,5-model-b.bin",
		     "brcmfmac43455-sdio.raspberrypi,5-model-b.txt",
		     "brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob" },
#endif
	{ 0x4345, 9, "brcmfmac43456-sdio.bin", "brcmfmac43456-sdio.txt", "brcmfmac43456-sdio.clm_blob" },
};

static QLock sdiolock;
static int iodebug;
#if defined(BMC64_WLAN_TRACE) || defined(BMC64_WLAN_LOW_IMPACT_TRACE)
static volatile int eventcallbackdepth;
#endif

#ifndef __circle__
static void etherbcmintr(void *);
#endif
static void bcmevent(Ctlr*, uchar*, int);
static void wlscanresult(Ether*, uchar*, int);
static void joinfinish(Ctlr*, int);
static void joinlinkup(Ctlr*);
static void wlsetvar(Ctlr*, char*, void*, int);
static void wlsetcountry(Ctlr*, const char*);
static void etherbcmscan(void *a, uint secs);
static void callevhndlr(Ctlr*, ether_event_type_t, const ether_event_params_t *);
static int queueflush(Queue*);

static uchar*
put2(uchar *p, short v)
{
	p[0] = v;
	p[1] = v >> 8;
	return p + 2;
}

static uchar*
put4(uchar *p, long v)
{
	p[0] = v;
	p[1] = v >> 8;
	p[2] = v >> 16;
	p[3] = v >> 24;
	return p + 4;
}

#ifndef __circle__

static ushort
get2(uchar *p)
{
	return p[0] | p[1]<<8;
}

#endif

static ulong
get4(uchar *p)
{
	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
}

static void
dump(char *s, void *a, int n)
{
#ifndef __circle__
	int i;
	uchar *p;

	p = a;
	print("%s:", s);
	for(i = 0; i < n; i++)
		print("%c%2.2x", i&15? ' ' : '\n', *p++);
	print("\n");
#else
	hexdump (a, n, s);
#endif
}

/*
 * SDIO communication with dongle
 */
static ulong
sdiocmd_locked(int cmd, ulong arg)
{
	u32int resp[4];

	sdio.cmd(cmd, arg, resp);
	return resp[0];
}

static ulong
sdiocmd(int cmd, ulong arg)
{
	ulong r;

	qlock(&sdiolock);
	if(waserror()){
		if(SDIODEBUG) print("sdiocmd error: cmd %d arg %lx\n", cmd, arg);
		qunlock(&sdiolock);
		nexterror();
	}
	r = sdiocmd_locked(cmd, arg);
	qunlock(&sdiolock);
	poperror();
	return r;

}

static ulong
trysdiocmd(int cmd, ulong arg)
{
	ulong r;

	if(waserror())
		return 0;
	r = sdiocmd(cmd, arg);
	poperror();
	return r;
}

static int
sdiord(int fn, int addr)
{
	int r;

	r = sdiocmd(IO_RW_DIRECT, (0<<31)|((fn&7)<<28)|((addr&0x1FFFF)<<9));
	if(r & 0xCF00){
		print("ether4330: sdiord(%x, %x) fail: %2.2x %2.2x\n", fn, addr, (r>>8)&0xFF, r&0xFF);
		error(Eio);
	}
	return r & 0xFF;
}

static void
sdiowr(int fn, int addr, int data)
{
	int r;
	int retry;

	r = 0;
	for(retry = 0; retry < 10; retry++){
		r = sdiocmd(IO_RW_DIRECT, (1<<31)|((fn&7)<<28)|((addr&0x1FFFF)<<9)|(data&0xFF));
		if((r & 0xCF00) == 0)
			return;
	}
	print("ether4330: sdiowr(%x, %x, %x) fail: %2.2x %2.2x\n", fn, addr, data, (r>>8)&0xFF, r&0xFF);
	error(Eio);
}

static void
sdiorwext(int fn, int write, void *a, int len, int addr, int incr, int deferyield)
{
	int bsize, blk, bcount, m;

	bsize = fn == Fn2? 512 : 64;
	while(len > 0){
		if(len >= 511*bsize){
			blk = 1;
			bcount = 511;
			m = bcount*bsize;
		}else if(len > bsize){
			blk = 1;
			bcount = len/bsize;
			m = bcount*bsize;
		}else{
			blk = 0;
			bcount = len;
			m = bcount;
		}
		qlock(&sdiolock);
		if(waserror()){
			print("ether4330: sdiorwext fail: %s\n", up->errstr);
			if(deferyield)
				qunlock_no_yield(&sdiolock);
			else
				qunlock(&sdiolock);
			nexterror();
		}
		if(blk)
			sdio.iosetup(write, a, bsize, bcount);
		else
			sdio.iosetup(write, a, bcount, 1);
		sdiocmd_locked(IO_RW_EXTENDED,
			write<<31 | (fn&7)<<28 | blk<<27 | incr<<26 | (addr&0x1FFFF)<<9 | (bcount&0x1FF));
		sdio.io(write, a, m);
		if(deferyield)
			qunlock_no_yield(&sdiolock);
		else
			qunlock(&sdiolock);
		poperror();
		len -= m;
		a = (char*)a + m;
		if(incr)
			addr += m;
	}
}

static void
sdioset(int fn, int addr, int bits)
{
	sdiowr(fn, addr, sdiord(fn, addr) | bits);
}

static void
sdioinit(void)
{
	ulong ocr, rca;
	int i;

#if RASPPI <= 4
	/* disconnect emmc from SD card (connect sdhost instead) */
	for(i = 48; i <= 53; i++)
		gpiosel(i, Alt0);
	/* connect emmc to wifi */
	for(i = 34; i <= 39; i++){
		gpiosel(i, Alt3);
		if(i == 34)
			gpiopulloff(i);
		else
			gpiopullup(i);
	}
#else
	/* See: https://forums.raspberrypi.com/viewtopic.php?t=362326#p2173647 */
	gpiosel(28, Output);
	gpioset(28, 1);		/* switch WLAN on */
	microdelay(150000);

	int d0 = get_soc_stepping() >= SOC_STEPPING_D0;		/* BCM2712D0 has different mapping */

	gpiosel(30, d0 ? Func1 : Func4); gpiopulloff(30);	/* sdio_clk */
	gpiosel(31, d0 ? Func1 : Func4); gpiopullup(31);	/* sdio_cmd */
	gpiosel(32, d0 ? Func1 : Func4); gpiopullup(32);	/* sdio_d0 */
	gpiosel(33, d0 ? Func1 : Func3); gpiopullup(33);	/* sdio_d1 */
	gpiosel(34, d0 ? Func1 : Func4); gpiopullup(34);	/* sdio_d2 */
	gpiosel(35, d0 ? Func1 : Func3); gpiopullup(35);	/* sdio_d3 */
#endif
	sdio.init();
	sdio.enable();
	sdiocmd(GO_IDLE_STATE, 0);
	ocr = trysdiocmd(IO_SEND_OP_COND, 0);
	i = 0;
	while((ocr & (1<<31)) == 0){
		if(++i > 5){
			print("ether4330: no response to sdio access: ocr = %lx\n", ocr);
			error(Eio);
		}
		ocr = trysdiocmd(IO_SEND_OP_COND, V3_3);
		tsleep(&up->sleep, return0, nil, 100);
	}
	rca = sdiocmd(SEND_RELATIVE_ADDR, 0) >> Rcashift;
	sdiocmd(SELECT_CARD, rca << Rcashift);
	sdioset(Fn0, Highspeed, 2);
	sdioset(Fn0, Busifc, 2);	/* bus width 4 */
	sdiowr(Fn0, Fbr1+Blksize, 64);
	sdiowr(Fn0, Fbr1+Blksize+1, 64>>8);
	sdiowr(Fn0, Fbr2+Blksize, 512);
	sdiowr(Fn0, Fbr2+Blksize+1, 512>>8);
	sdioset(Fn0, Ioenable, 1<<Fn1);
	sdiowr(Fn0, Intenable, 0);
	for(i = 0; !(sdiord(Fn0, Ioready) & 1<<Fn1); i++){
		if(i == 10){
			print("ether4330: can't enable SDIO function\n");
			error(Eio);
		}
		tsleep(&up->sleep, return0, nil, 100);
	}
}

static void
sdioreset(void)
{
	sdiowr(Fn0, Ioabort, 1<<3);	/* reset */
}

static void
trysdioreset(void)
{
	if(waserror())
		return;
	sdioreset();
	poperror();
}

static void
wlanpowercycle(void)
{
#if RASPPI >= 5
	gpiosel(28, Output);
	gpioset(28, 0);
	microdelay(100000);
	gpioset(28, 1);
	microdelay(150000);
#else
	microdelay(50000);
#endif
}

static void
sdioabort(int fn)
{
	sdiowr(Fn0, Ioabort, fn);
}

/*
 * Chip register and memory access via SDIO
 */

static void
cfgw(ulong off, int val)
{
	sdiowr(Fn1, off, val);
}

static int
cfgr(ulong off)
{
	return sdiord(Fn1, off);
}

static ulong
cfgreadl(int fn, ulong off)
{
	uchar cbuf[2*CACHELINESZ];
	uchar *p;

	p = (uchar*)ROUND((uintptr)cbuf, CACHELINESZ);
	memset(p, 0, 4);
	sdiorwext(fn, 0, p, 4, off|Sb32bit, 1, 0);
	if(SDIODEBUG) print("cfgreadl %lx: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);
	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
}

static void
cfgwritel(int fn, ulong off, u32int data)
{
	uchar cbuf[2*CACHELINESZ];
	uchar *p;
	int retry;

	p = (uchar*)ROUND((uintptr)cbuf, CACHELINESZ);
	put4(p, data);
	if(SDIODEBUG) print("cfgwritel %lx: %2.2x %2.2x %2.2x %2.2x\n", off, p[0], p[1], p[2], p[3]);
	retry = 0;
	while(waserror()){
		print("ether4330: cfgwritel retry %lx %x\n", off, data);
		sdioabort(fn);
		if(++retry == 3)
			nexterror();
	}
	sdiorwext(fn, 1, p, 4, off|Sb32bit, 1, 0);
	poperror();
}

static void
sbwindow(ulong addr)
{
	addr &= ~(Sbwsize-1);
	cfgw(Sbaddr, addr>>8);
	cfgw(Sbaddr+1, addr>>16);
	cfgw(Sbaddr+2, addr>>24);
}

static void
sbrw(int fn, int write, uchar *buf, int len, ulong off)
{
	int n;
	USED(fn);

	if(waserror()){
		print("ether4330: sbrw err off %lx len %d\n", off, len);
		nexterror();
	}
	if(write){
		if(len >= 4){
			n = len;
			n &= ~3;
			sdiorwext(Fn1, write, buf, n, off|Sb32bit, 1, 0);
			off += n;
			buf += n;
			len -= n;
		}
		while(len > 0){
			sdiowr(Fn1, off|Sb32bit, *buf);
			off++;
			buf++;
			len--;
		}
	}else{
		if(len >= 4){
			n = len;
			n &= ~3;
			sdiorwext(Fn1, write, buf, n, off|Sb32bit, 1, 0);
			off += n;
			buf += n;
			len -= n;
		}
		while(len > 0){
			*buf = sdiord(Fn1, off|Sb32bit);
			off++;
			buf++;
			len--;
		}
	}
	poperror();
}

static void
sbmem(int write, uchar *buf, int len, ulong off)
{
	ulong n;

	n = ROUNDUP(off, Sbwsize) - off;
	if(n == 0)
		n = Sbwsize;
	while(len > 0){
		if(n > len)
			n = len;
		sbwindow(off);
		sbrw(Fn1, write, buf, n, off & (Sbwsize-1));
		off += n;
		buf += n;
		len -= n;
		n = Sbwsize;
	}
}

static void
packetrw(int write, uchar *buf, int len)
{
	uchar b[2048];
	int n, m;
	int retry;

	n = 2048;
	while(len > 0){
		m = n;
		if(n > len)
			n = ROUND(len, 4);
		retry = 0;
		while(waserror()){
			sdioabort(Fn2);
			if(++retry == 3)
				nexterror();
		}
		if (m != n){
			if (write)
				memcpy(b, buf, len);

			sdiorwext(Fn2, write, b, n, Enumbase, 0, 1);
			if (!write)
				memcpy(buf, b, len);
		}
		else
			sdiorwext(Fn2, write, buf, n, Enumbase, 0, 1);
		poperror();
		buf += n;
		len -= n;
	}
}

/*
 * Configuration and control of chip cores via Silicon Backplane
 */

static void
sbdisable(ulong regs, int pre, int ioctl)
{
	sbwindow(regs);
	if((cfgreadl(Fn1, regs + Resetctrl) & 1) != 0){
		cfgwritel(Fn1, regs + Ioctrl, 3|ioctl);
		cfgreadl(Fn1, regs + Ioctrl);
		return;
	}
	cfgwritel(Fn1, regs + Ioctrl, 3|pre);
	cfgreadl(Fn1, regs + Ioctrl);
	cfgwritel(Fn1, regs + Resetctrl, 1);
	microdelay(10);
	while((cfgreadl(Fn1, regs + Resetctrl) & 1) == 0)
		;
	cfgwritel(Fn1, regs + Ioctrl, 3|ioctl);
	cfgreadl(Fn1, regs + Ioctrl);
}

static void
sbreset(ulong regs, int pre, int ioctl)
{
	sbdisable(regs, pre, ioctl);
	sbwindow(regs);
	if(SBDEBUG) print("sbreset %#p %#lx %#lx ->", regs,
		cfgreadl(Fn1, regs+Ioctrl), cfgreadl(Fn1, regs+Resetctrl));
	while((cfgreadl(Fn1, regs + Resetctrl) & 1) != 0){
		cfgwritel(Fn1, regs + Resetctrl, 0);
		microdelay(40);
	}
	cfgwritel(Fn1, regs + Ioctrl, 1|ioctl);
	cfgreadl(Fn1, regs + Ioctrl);
	if(SBDEBUG) print("%#lx %#lx\n",
		cfgreadl(Fn1, regs+Ioctrl), cfgreadl(Fn1, regs+Resetctrl));
}

static void
corescan(Ctlr *ctl, ulong r)
{
	uchar *buf;
	int i, coreid, corerev;
	ulong addr;

	buf = sdmalloc(Corescansz);
	if(buf == nil)
		error(Enomem);
	sbmem(0, buf, Corescansz, r);
	coreid = 0;
	corerev = 0;
	for(i = 0; i < Corescansz; i += 4){
		switch(buf[i]&0xF){
		case 0xF:	/* end */
			sdfree(buf);
			return;
		case 0x1:	/* core info */
			if((buf[i+4]&0xF) != 0x1)
				break;
			coreid = (buf[i+1] | buf[i+2]<<8) & 0xFFF;
			i += 4;
			corerev = buf[i+3];
			break;
		case 0x05:	/* address */
			addr = buf[i+1]<<8 | buf[i+2]<<16 | buf[i+3]<<24;
			addr &= ~0xFFF;
			if(SBDEBUG) print("core %x %s %#p\n", coreid, buf[i]&0xC0? "ctl" : "mem", addr);
			switch(coreid){
			case 0x800:
				if((buf[i] & 0xC0) == 0)
					ctl->chipcommon = addr;
				break;
			case ARMcm3:
			case ARM7tdmi:
			case ARMcr4:
				ctl->armcore = coreid;
				if(buf[i] & 0xC0){
					if(ctl->armctl == 0)
						ctl->armctl = addr;
				}else{
					if(ctl->armregs == 0)
						ctl->armregs = addr;
				}
				break;
			case 0x80E:
				if(buf[i] & 0xC0)
					ctl->socramctl = addr;
				else if(ctl->socramregs == 0)
					ctl->socramregs = addr;
				ctl->socramrev = corerev;
				break;
			case 0x829:
				if((buf[i] & 0xC0) == 0)
					ctl->sdregs = addr;
				ctl->sdiorev = corerev;
				break;
			case 0x812:
				if(buf[i] & 0xC0)
					ctl->d11ctl = addr;
				break;
			}
		}
	}
	sdfree(buf);
}

static void
ramscan(Ctlr *ctl)
{
	ulong r, n, size;
	int banks, i;

	if(ctl->armcore == ARMcr4){
		r = ctl->armregs;
		sbwindow(r);
		n = cfgreadl(Fn1, r + Cr4Cap);
		if(SBDEBUG) print("cr4 banks %lx\n", n);
		banks = ((n>>4) & 0xF) + (n & 0xF);
		size = 0;
		for(i = 0; i < banks; i++){
			cfgwritel(Fn1, r + Cr4Bankidx, i);
			n = cfgreadl(Fn1, r + Cr4Bankinfo);
			if(SBDEBUG) print("bank %d reg %lx size %ld\n", i, n, 8192 * ((n & 0x3F) + 1));
			size += 8192 * ((n & 0x3F) + 1);
		}
		ctl->socramsize = size;
		ctl->rambase = 0x198000;
		return;
	}
	if(ctl->socramrev <= 7 || ctl->socramrev == 12){
		print("ether4330: SOCRAM rev %d not supported\n", ctl->socramrev);
		error(Eio);
	}
	sbreset(ctl->socramctl, 0, 0);
	r = ctl->socramregs;
	sbwindow(r);
	n = cfgreadl(Fn1, r + Coreinfo);
	if(SBDEBUG) print("socramrev %d coreinfo %lx\n", ctl->socramrev, n);
	banks = (n>>4) & 0xF;
	size = 0;
	for(i = 0; i < banks; i++){
		cfgwritel(Fn1, r + Bankidx, i);
		n = cfgreadl(Fn1, r + Bankinfo);
		if(SBDEBUG) print("bank %d reg %lx size %ld\n", i, n, 8192 * ((n & 0x3F) + 1));
		size += 8192 * ((n & 0x3F) + 1);
	}
	ctl->socramsize = size;
	ctl->rambase = 0;
	if(ctl->chipid == 43430){
		cfgwritel(Fn1, r + Bankidx, 3);
		cfgwritel(Fn1, r + Bankpda, 0);
	}
}

static void
sbinit(Ctlr *ctl)
{
	ulong r;
	int chipid;
	char buf[16];

	sbwindow(Enumbase);
	r = cfgreadl(Fn1, Enumbase);
	chipid = r & 0xFFFF;
	sprint(buf, chipid > 43000 ? "%d" : "%#x", chipid);
	print("ether4330: chip %s rev %ld type %ld\n", buf, (r>>16)&0xF, (r>>28)&0xF);
	switch(chipid){
		case 0x4330:
		case 43362:
		case 43430:
		case 0x4345:
			ctl->chipid = chipid;
			ctl->chiprev = (r>>16)&0xF;
			break;
		default:
			print("ether4330: chipid %#x (%d) not supported\n", chipid, chipid);
			error(Eio);
	}
	r = cfgreadl(Fn1, Enumbase + 63*4);
	corescan(ctl, r);
	if(ctl->armctl == 0 || ctl->d11ctl == 0 ||
	   (ctl->armcore == ARMcm3 && (ctl->socramctl == 0 || ctl->socramregs == 0)))
		error("corescan didn't find essential cores\n");
	if(ctl->armcore == ARMcr4)
		sbreset(ctl->armctl, Cr4Cpuhalt, Cr4Cpuhalt);
	else
		sbdisable(ctl->armctl, 0, 0);
	sbreset(ctl->d11ctl, 8|4, 4);
	ramscan(ctl);
	if(SBDEBUG) print("ARM %#p D11 %#p SOCRAM %#p,%#p %ld bytes @ %#p\n",
		ctl->armctl, ctl->d11ctl, ctl->socramctl, ctl->socramregs, ctl->socramsize, ctl->rambase);
	cfgw(Clkcsr, 0);
	microdelay(10);
	if(SBDEBUG) print("chipclk: %x\n", cfgr(Clkcsr));
	cfgw(Clkcsr, Nohwreq | ReqALP);
	while((cfgr(Clkcsr) & (HTavail|ALPavail)) == 0)
		microdelay(10);
	cfgw(Clkcsr, Nohwreq | ForceALP);
	microdelay(65);
	if(SBDEBUG) print("chipclk: %x\n", cfgr(Clkcsr));
	cfgw(Pullups, 0);
	sbwindow(ctl->chipcommon);
	cfgwritel(Fn1, ctl->chipcommon + Gpiopullup, 0);
	cfgwritel(Fn1, ctl->chipcommon + Gpiopulldown, 0);
	if(ctl->chipid != 0x4330 && ctl->chipid != 43362)
		return;
	cfgwritel(Fn1, ctl->chipcommon + Chipctladdr, 1);
	if(cfgreadl(Fn1, ctl->chipcommon + Chipctladdr) != 1)
		print("ether4330: can't set Chipctladdr\n");
	else{
		r = cfgreadl(Fn1, ctl->chipcommon + Chipctldata);
		if(SBDEBUG) print("chipcommon PMU (%lx) %lx", cfgreadl(Fn1, ctl->chipcommon + Chipctladdr), r);
		/* set SDIO drive strength >= 6mA */
		r &= ~0x3800;
		if(ctl->chipid == 0x4330)
			r |= 3<<11;
		else
			r |= 7<<11;
		cfgwritel(Fn1, ctl->chipcommon + Chipctldata, r);
		if(SBDEBUG) print("-> %lx (= %lx)\n", r, cfgreadl(Fn1, ctl->chipcommon + Chipctldata));
	}
}

static void
wlinitf2watermark(Ctlr *ctl)
{
	int devctl;

	if(ctl->chipid != 0x4345)
		return;





	sdiowr(Fn1, Watermark, Bcm43455F2Watermark);
	devctl = sdiord(Fn1, Devicectl);
	sdiowr(Fn1, Devicectl, devctl | DevctlF2Watermark);
	sdiowr(Fn1, Mesbusyctrl, Bcm43455MesWatermark | MesbusyctrlEnable);
	print("ether4330: bcm43455 f2 watermark 0x%x mesbusy 0x%x\n",
		Bcm43455F2Watermark, Bcm43455MesWatermark | MesbusyctrlEnable);
}

static void
sbenable(Ctlr *ctl)
{
	int i;

	if(SBDEBUG) print("enabling HT clock...");
	cfgw(Clkcsr, 0);
	delay(1);
	cfgw(Clkcsr, ReqHT);
	for(i = 0; (cfgr(Clkcsr) & HTavail) == 0; i++){
		if(i == 50){
			print("ether4330: can't enable HT clock: csr %x\n", cfgr(Clkcsr));
			error(Eio);
		}
		tsleep(&up->sleep, return0, nil, 100);
	}
	cfgw(Clkcsr, cfgr(Clkcsr) | ForceHT);
	delay(10);
	if(SBDEBUG) print("chipclk: %x\n", cfgr(Clkcsr));
	sbwindow(ctl->sdregs);
	cfgwritel(Fn1, ctl->sdregs + Sbmboxdata, 4 << 16);	/* protocol version */
	cfgwritel(Fn1, ctl->sdregs + Intmask, FrameInt | MailboxInt | Fcchange);
	sdioset(Fn0, Ioenable, 1<<Fn2);
	for(i = 0; !(sdiord(Fn0, Ioready) & 1<<Fn2); i++){
		if(i == 10){
			print("ether4330: can't enable SDIO function 2 - ioready %x\n", sdiord(Fn0, Ioready));
			error(Eio);
		}
		tsleep(&up->sleep, return0, nil, 100);
	}
	wlinitf2watermark(ctl);
	sdiowr(Fn0, Intenable, (1<<Fn1) | (1<<Fn2) | 1);
}


/*
 * Firmware and config file uploading
 */

/*
 * Condense config file contents (in buffer buf with length n)
 * to 'var=value\0' list for firmware:
 *	- remove comments (starting with '#') and blank lines
 *	- remove carriage returns
 *	- convert newlines to nulls
 *	- mark end with two nulls
 *	- pad with nulls to multiple of 4 bytes total length
 */
static int
condense(uchar *buf, int n)
{
	uchar *p, *ep, *lp, *op;
	int c, skipping;

	skipping = 0;	/* true if in a comment */
	ep = buf + n;	/* end of input */
	op = buf;	/* end of output */
	lp = buf;	/* start of current output line */
	for(p = buf; p < ep; p++){
		switch(c = *p){
		case '#':
			skipping = 1;
			break;
		case '\0':
		case '\n':
			skipping = 0;
			if(op != lp){
				*op++ = '\0';
				lp = op;
			}
			break;
		case '\r':
			break;
		default:
			if(!skipping)
				*op++ = c;
			break;
		}
	}
	if(!skipping && op != lp)
		*op++ = '\0';
	*op++ = '\0';
	for(n = op - buf; n & 03; n++)
		*op++ = '\0';
	return n;
}

/*
 * Try to find firmware file in /boot or in /sys/lib/firmware.
 * Throw an error if not found.
 */
static Chan*
findfirmware(char *file)
{
#ifndef __circle__
	char nbuf[64];
#endif
	Chan *c;

	if(!waserror()){
#ifndef __circle__
		snprint(nbuf, sizeof nbuf, "/boot/%s", file);
		c = namec(nbuf, Aopen, OREAD, 0);
		poperror();
	}else if(!waserror()){
		snprint(nbuf, sizeof nbuf, "/sys/lib/firmware/%s", file);
		c = namec(nbuf, Aopen, OREAD, 0);
		poperror();
#else
		c = namec(file, Aopen, OREAD, 0);
		poperror();
#endif
	}else{
		c = nil;
#ifndef __circle__
		snprint(up->genbuf, sizeof up->genbuf, "can't find %s in /boot or /sys/lib/firmware", file);
#else
		snprint(up->genbuf, sizeof up->genbuf, "can't find %s", file);
#endif
		error(up->genbuf);
	}
	return c;
}

#if defined(BMC64_WLAN_TRACE) || defined(BMC64_WLAN_LOW_IMPACT_TRACE)
static ulong wltrace_ms(ulong start);
#endif

static int
upload(Ctlr *ctl, char *file, int isconfig)
{
	Chan *c;
	uchar *buf;
	uchar *cbuf;
	int off, n;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	int chunk, lastlog;
	ulong start, stepstart;
#endif

	off = 0;
	n = 0;
	buf = cbuf = nil;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	start = m->ticks;
	stepstart = start;
	chunk = 0;
	lastlog = 0;
	BMX_WLAN_LOW_LOG("ether4330: upload start file %s config %d\n", file, isconfig);
#endif
	c = findfirmware(file);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: upload opened file %s ms %lu\n", file, wltrace_ms(start));
#endif
	if(waserror()){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		print("ether4330: upload error file %s off %d chunk %d ms %lu err %s\n",
			file, off, chunk, wltrace_ms(start),
			up->errstr != nil ? up->errstr : "-");
#endif
		cclose(c);
		sdfree(buf);
		sdfree(cbuf);
		nexterror();
	}
	buf = sdmalloc(Uploadsz);
	if(buf == nil)
		error(Enomem);
	if(Firmwarecmp){
		cbuf = sdmalloc(Uploadsz);
		if(cbuf == nil)
			error(Enomem);
	}
	off = 0;
	for(;;){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		stepstart = m->ticks;
		if(chunk == 0 || off - lastlog >= FwloadUploadProgressBytes){
			BMX_WLAN_LOW_LOG("ether4330: upload read-start file %s config %d off %d chunk %d ms %lu\n",
				file, isconfig, off, chunk, wltrace_ms(start));
			lastlog = off;
		}
#endif
		n = devtab[c->type]->read(c, buf, Uploadsz, off);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		if(wltrace_ms(stepstart) >= FwloadUploadSlowMs)
			BMX_WLAN_LOW_LOG("ether4330: upload read-slow file %s off %d chunk %d n %d step_ms %lu total_ms %lu\n",
				file, off, chunk, n, wltrace_ms(stepstart),
				wltrace_ms(start));
#endif
		if(n <= 0)
			break;
		if(isconfig){
			n = condense(buf, n);
			off = ctl->socramsize - n - 4;
		}else if(off == 0)
			memmove(ctl->resetvec.c, buf, sizeof(ctl->resetvec.c));
		while(n&3)
			buf[n++] = 0;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		stepstart = m->ticks;
		if(chunk == 0 || off == lastlog)
			BMX_WLAN_LOW_LOG("ether4330: upload sbmem-start file %s config %d off %d n %d addr %#lx chunk %d ms %lu\n",
				file, isconfig, off, n, ctl->rambase + off,
				chunk, wltrace_ms(start));
#endif
		sbmem(1, buf, n, ctl->rambase + off);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		if(wltrace_ms(stepstart) >= FwloadUploadSlowMs)
			BMX_WLAN_LOW_LOG("ether4330: upload sbmem-slow file %s off %d n %d chunk %d step_ms %lu total_ms %lu\n",
				file, off, n, chunk, wltrace_ms(stepstart),
				wltrace_ms(start));
		chunk++;
		if(wltrace_ms(start) >= FwloadUploadTimeoutMs){
			print("ether4330: upload timeout file %s config %d off %d n %d chunk %d ms %lu\n",
				file, isconfig, off, n, chunk, wltrace_ms(start));
			error("fw upload timeout");
		}
#endif
		if(isconfig)
			break;
		off += n;
	}
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: upload data-done file %s config %d off %d chunks %d ms %lu\n",
		file, isconfig, off, chunk, wltrace_ms(start));
#endif
	if(Firmwarecmp){
		if(FWDEBUG) print("compare...");
		if(!isconfig)
			off = 0;
		for(;;){
			if(!isconfig){
				n = devtab[c->type]->read(c, buf, Uploadsz, off);
				if(n <= 0)
					break;
			while(n&3)
				buf[n++] = 0;
			}
			sbmem(0, cbuf, n, ctl->rambase + off);
			if(memcmp(buf, cbuf, n) != 0){
				print("ether4330: firmware load failed offset %d\n", off);
				error(Eio);
			}
			if(isconfig)
				break;
			off += n;
		}
	}
	if(FWDEBUG) print("\n");
	poperror();
	cclose(c);
	sdfree(buf);
	sdfree(cbuf);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: upload done file %s config %d result %d chunks %d ms %lu\n",
		file, isconfig, n, chunk, wltrace_ms(start));
#endif
	return n;
}

/*
 * Upload regulatory file (.clm) to firmware.
 * Packet format is
 *	[2]flag [2]type [4]len [4]crc [len]data
 */
static void
reguload(Ctlr *ctl, char *file)
{
	Chan *c;
	uchar *buf;
	int off, n, flag;
	enum {
		Reguhdr = 2+2+4+4,
		Regusz	= 1400,
		Regutyp	= 2,
		Flagclm	= 1<<12,
		Firstpkt= 1<<1,
		Lastpkt	= 1<<2,
	};

	buf = nil;
	c = findfirmware(file);
	if(waserror()){
		cclose(c);
		free(buf);
		nexterror();
	}
	buf = malloc(Reguhdr+Regusz+1);
	if(buf == nil)
		error(Enomem);
	put2(buf+2, Regutyp);
	put2(buf+8, 0);
	off = 0;
	flag = Flagclm | Firstpkt;
	while((flag&Lastpkt) == 0){
		n = devtab[c->type]->read(c, buf+Reguhdr, Regusz+1, off);
		if(n <= 0)
			break;
		if(n == Regusz+1)
			--n;
		else{
			while(n&7)
				buf[Reguhdr+n++] = 0;
			flag |= Lastpkt;
		}
		put2(buf+0, flag);
		put4(buf+4, n);
		wlsetvar(ctl, "clmload", buf, Reguhdr + n);
		off += n;
		flag &= ~Firstpkt;
	}
	poperror();
	cclose(c);
	free(buf);
}

static void
fwload(Ctlr *ctl)
{
	uchar buf[4];
	uint i, n, alpwait;

	i = 0;
	while(firmware[i].chipid != ctl->chipid ||
		   firmware[i].chiprev != ctl->chiprev){
		if(++i == nelem(firmware)){
			print("ether4330: no firmware for chipid %x (%d) chiprev %d\n",
				ctl->chipid, ctl->chipid, ctl->chiprev);
			error("no firmware");
		}
	}
	ctl->regufile = firmware[i].regufile;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: firmware files fw %s cfg %s clm %s\n",
		firmware[i].fwfile, firmware[i].cfgfile,
		firmware[i].regufile != nil ? firmware[i].regufile : "-");
	BMX_WLAN_LOW_LOG("ether4330: fwload step alp-request\n");
#endif
	cfgw(Clkcsr, ReqALP);
	for(alpwait = 0; (cfgr(Clkcsr) & ALPavail) == 0; alpwait += 10){
		if(alpwait >= FwloadAlpTimeoutUs){
			print("ether4330: fwload alp timeout csr %x after %u us\n",
				cfgr(Clkcsr), alpwait);
			error("fwload alp timeout");
		}
		microdelay(10);
	}
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: fwload step alp-ready csr %x us %u\n", cfgr(Clkcsr), alpwait);
#endif
	memset(buf, 0, 4);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: fwload step clear-tail off %#lx\n",
		ctl->rambase + ctl->socramsize - 4);
#endif
	sbmem(1, buf, 4, ctl->rambase + ctl->socramsize - 4);
	if(FWDEBUG) print("firmware load...");
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: fwload step upload-fw file %s\n", firmware[i].fwfile);
#endif
	upload(ctl, firmware[i].fwfile, 0);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: fwload step upload-fw-done\n");
#endif
	if(FWDEBUG) print("config load...");
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: fwload step upload-cfg file %s\n", firmware[i].cfgfile);
#endif
	n = upload(ctl, firmware[i].cfgfile, 1);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: fwload step upload-cfg-done bytes %u\n", n);
#endif
	n /= 4;
	n = (n & 0xFFFF) | (~n << 16);
	put4(buf, n);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: fwload step cfg-tail value %#x\n", n);
#endif
	sbmem(1, buf, 4, ctl->rambase + ctl->socramsize - 4);
	if(ctl->armcore == ARMcr4){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		BMX_WLAN_LOW_LOG("ether4330: fwload step release-cr4 resetvec %u\n",
			ctl->resetvec.i != 0);
#endif
		sbwindow(ctl->sdregs);
		cfgwritel(Fn1, ctl->sdregs + Intstatus, ~0);
		if(ctl->resetvec.i != 0){
			if(SBDEBUG) print("%x\n", ctl->resetvec.i);
			sbmem(1, ctl->resetvec.c, sizeof(ctl->resetvec.c), 0);
		}
		sbreset(ctl->armctl, Cr4Cpuhalt, 0);
	}else{
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		BMX_WLAN_LOW_LOG("ether4330: fwload step release-arm\n");
#endif
		sbreset(ctl->armctl, 0, 0);
	}
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: fwload step done\n");
#endif
}

/*
 * Communication of data and control packets
 */

static void
intwait(Ctlr *ctlr, int wait)
{
	ulong ints, mbox;
	int i;

	if(waserror())
		return;
	for(;;){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		ctlr->dbg_iw_calls++;
#endif
		if(ctlr->resetting)
			break;
		sdiocardintr(wait);
		if(ctlr->resetting)
			break;
		sbwindow(ctlr->sdregs);
		i = sdiord(Fn0, Intpend);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		ctlr->dbg_last_intpend = i;
#endif
		if(i == 0){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctlr->dbg_iw_intpend0++;
#endif
			continue;
		}
		ints = cfgreadl(Fn1, ctlr->sdregs + Intstatus);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		ctlr->dbg_last_intstatus = ints;
		if(ints & FrameInt)
			ctlr->dbg_iw_frameint++;
		if(ints & MailboxInt)
			ctlr->dbg_iw_mailboxint++;
		if(ints & Fcchange)
			ctlr->dbg_iw_fcchange++;
#endif
		cfgwritel(Fn1, ctlr->sdregs + Intstatus, ints);
		if(0) print("INTS: (%x) %lx -> %lx\n", i, ints, cfgreadl(Fn1, ctlr->sdregs + Intstatus));
		if(ints & MailboxInt){
			mbox = cfgreadl(Fn1, ctlr->sdregs + Hostmboxdata);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctlr->dbg_last_hostmbox = mbox;
#endif
			cfgwritel(Fn1, ctlr->sdregs + Sbmbox, 2);	/* ack */
			if(mbox & 0x8)
				print("ether4330: firmware ready\n");
		}
		if(ints & FrameInt)
			break;
	}
	poperror();
}

static Block*
wlreadpkt(Ctlr *ctl)
{
	Block *b;
	Sdpcm *p;
	int len, lenck;
	uvlong pktlockstart, pktlockwaitus, sdious, sdiostart;
	uvlong unlockus;
#if RASPPI <= 4
	uvlong unlockstart;
#endif

	b = allocb(2048);
	b->timingus = p9microseconds();
	p = (Sdpcm*)b->wp;
	pktlockstart = p9microseconds();
	qlock(&ctl->pktlock);
	pktlockwaitus = p9microseconds() - pktlockstart;
	sdious = 0;
	if(ctl->resetting){
		freeb(b);
		qunlock(&ctl->pktlock);
		return nil;
	}
	for(;;){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		ctl->dbg_rp_calls++;
#endif
		if(ctl->resetting){
			freeb(b);
			b = nil;
			break;
		}
		sdiostart = p9microseconds();
		packetrw(0, b->wp, sizeof(*p));
		sdious += p9microseconds() - sdiostart;
		len = p->len[0] | p->len[1]<<8;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		ctl->dbg_last_len = len;
		ctl->dbg_last_lenck = p->lenck[0] | p->lenck[1]<<8;
		ctl->dbg_last_chanflg = p->chanflg;
		ctl->dbg_last_nextlen = p->nextlen;
		ctl->dbg_last_doffset = p->doffset;
		ctl->dbg_last_window = p->window;
		ctl->dbg_last_fcmask = p->fcmask;
#endif
		if(len == 0){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctl->dbg_rp_zero++;
#endif
			freeb(b);
			b = nil;
			break;
		}
		lenck = p->lenck[0] | p->lenck[1]<<8;
		if(lenck != (len ^ 0xFFFF) ||
		   len < sizeof(*p) || len > 2048){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctl->dbg_rp_bad++;
#endif
			print("ether4330: wlreadpkt error len %.4x lenck %.4x\n", len, lenck);
			cfgw(Framectl, Rfhalt);
			while(cfgr(Rfrmcnt+1))
				;
			while(cfgr(Rfrmcnt))
				;
			continue;
		}
		if(len > sizeof(*p)){
			sdiostart = p9microseconds();
			packetrw(0, b->wp + sizeof(*p), len - sizeof(*p));
			sdious += p9microseconds() - sdiostart;
		}
		b->wp += len;
		break;
	}
#if RASPPI >= 5
	qunlock_no_yield(&ctl->pktlock);
	unlockus = 0;
#else
	unlockstart = p9microseconds();
	qunlock(&ctl->pktlock);
	unlockus = p9microseconds() - unlockstart;
#endif
	lock(&ctl->statlock);
	ctl->stat_rxtimingsamples++;
	ctl->stat_rxpktlockwaitus += pktlockwaitus;
	if(pktlockwaitus > ctl->stat_rxpktlockwaitmaxus)
		ctl->stat_rxpktlockwaitmaxus = pktlockwaitus;
	ctl->stat_rxsdious += sdious;
	if(sdious > ctl->stat_rxsdiomaxus)
		ctl->stat_rxsdiomaxus = sdious;
#if RASPPI <= 4
	ctl->stat_rxpktlockyieldcalls++;
#endif
	ctl->stat_rxpktlockyieldus += unlockus;
	if(unlockus > ctl->stat_rxpktlockyieldmaxus)
		ctl->stat_rxpktlockyieldmaxus = unlockus;
	unlock(&ctl->statlock);
	return b;
}

static void
wlstatblock(Ctlr *ctl, int window)
{
	lock(&ctl->statlock);
	if(window){
		if(!ctl->stat_txwindowblocked){
			ctl->stat_txwindowblocked = 1;
			ctl->stat_txwindowstallstart = m->ticks;
			ctl->stat_txwindowstalls++;
		}
	}else if(!ctl->stat_txflowblocked){
		ctl->stat_txflowblocked = 1;
		ctl->stat_txflowstallstart = m->ticks;
		ctl->stat_txflowstalls++;
	}
	unlock(&ctl->statlock);
}

static void
wlstatunblock(Ctlr *ctl, int window)
{
	uvlong elapsed;

	lock(&ctl->statlock);
	if(window){
		if(ctl->stat_txwindowblocked){
			elapsed = m->ticks - ctl->stat_txwindowstallstart;
			ctl->stat_txwindowstallticks += elapsed;
			if(elapsed > ctl->stat_txwindowstallmaxticks)
				ctl->stat_txwindowstallmaxticks = elapsed;
			ctl->stat_txwindowblocked = 0;
		}
	}else if(ctl->stat_txflowblocked){
		elapsed = m->ticks - ctl->stat_txflowstallstart;
		ctl->stat_txflowstallticks += elapsed;
		if(elapsed > ctl->stat_txflowstallmaxticks)
			ctl->stat_txflowstallmaxticks = elapsed;
		ctl->stat_txflowblocked = 0;
	}
	unlock(&ctl->statlock);
}

static uvlong
wlstattoms(uvlong ticks)
{
	return ticks * 1000 / HZ;
}

int
ether4330flowstatus(Ether *edev, bmx_wlan_flow_status_t *status)
{
	Ctlr *ctl;
	uvlong current;

	if(edev == nil || status == nil || edev->ctlr == nil)
		return 0;
	ctl = edev->ctlr;
	memset(status, 0, sizeof *status);

	lock(&ctl->txwinlock);
	status->tx_sequence = ctl->txseq;
	status->tx_window = ctl->txwindow;
	status->flow_control_mask = ctl->fcmask;
	unlock(&ctl->txwinlock);
	status->tx_queue_frames = edev->oq != nil ? qlen(edev->oq) : 0;

	lock(&ctl->statlock);
	status->tx_frames = ctl->stat_txframes;
	status->rx_data_frames = ctl->stat_rxdataframes;
	status->tx_window_updates = ctl->stat_txwindowupdates;
	status->tx_flow_updates = ctl->stat_txflowupdates;
	status->tx_window_stalls = ctl->stat_txwindowstalls;
	status->tx_window_stall_ms = wlstattoms(ctl->stat_txwindowstallticks);
	status->tx_window_stall_max_ms = wlstattoms(ctl->stat_txwindowstallmaxticks);
	if(ctl->stat_txwindowblocked){
		current = m->ticks - ctl->stat_txwindowstallstart;
		status->tx_window_stall_current_ms = wlstattoms(current);
	}
	status->tx_flow_stalls = ctl->stat_txflowstalls;
	status->tx_flow_stall_ms = wlstattoms(ctl->stat_txflowstallticks);
	status->tx_flow_stall_max_ms = wlstattoms(ctl->stat_txflowstallmaxticks);
	if(ctl->stat_txflowblocked){
		current = m->ticks - ctl->stat_txflowstallstart;
		status->tx_flow_stall_current_ms = wlstattoms(current);
	}
	status->tx_timing_samples = ctl->stat_txtimingsamples;
	status->tx_queue_us = ctl->stat_txqueueus;
	status->tx_queue_max_us = ctl->stat_txqueuemaxus;
	status->tx_pktlock_wait_us = ctl->stat_txpktlockwaitus;
	status->tx_pktlock_wait_max_us = ctl->stat_txpktlockwaitmaxus;
	status->tx_sdio_us = ctl->stat_txsdious;
	status->tx_sdio_max_us = ctl->stat_txsdiomaxus;
	status->tx_pktlock_yield_calls = ctl->stat_txpktlockyieldcalls;
	status->tx_pktlock_yield_us = ctl->stat_txpktlockyieldus;
	status->tx_pktlock_yield_max_us = ctl->stat_txpktlockyieldmaxus;
	status->rx_timing_samples = ctl->stat_rxtimingsamples;
	status->rx_pktlock_wait_us = ctl->stat_rxpktlockwaitus;
	status->rx_pktlock_wait_max_us = ctl->stat_rxpktlockwaitmaxus;
	status->rx_sdio_us = ctl->stat_rxsdious;
	status->rx_sdio_max_us = ctl->stat_rxsdiomaxus;
	status->rx_pktlock_yield_calls = ctl->stat_rxpktlockyieldcalls;
	status->rx_pktlock_yield_us = ctl->stat_rxpktlockyieldus;
	status->rx_pktlock_yield_max_us = ctl->stat_rxpktlockyieldmaxus;
	status->rx_to_netdev_samples = ctl->stat_rxtonetdevsamples;
	status->rx_to_netdev_us = ctl->stat_rxtonetdevus;
	status->rx_to_netdev_max_us = ctl->stat_rxtonetdevmaxus;
	unlock(&ctl->statlock);
	bmx_emmc_wait_status(status);

	return 1;
}

void
ether4330rxhandoff(Ether *edev, uvlong elapsedus)
{
	Ctlr *ctl;

	if(edev == nil || edev->ctlr == nil)
		return;
	ctl = edev->ctlr;
	lock(&ctl->statlock);
	ctl->stat_rxtonetdevsamples++;
	ctl->stat_rxtonetdevus += elapsedus;
	if(elapsedus > ctl->stat_rxtonetdevmaxus)
		ctl->stat_rxtonetdevmaxus = elapsedus;
	unlock(&ctl->statlock);
}

static void
txstart(Ether *edev)
{
	Ctlr *ctl;
	Sdpcm *p;
	Block *b;
	int len, off;
	uvlong queueus, pktlockstart, pktlockwaitus;
	uvlong sdiostart, sdious, unlockstart, unlockus;
	int yieldcall;

	ctl = edev->ctlr;
	if(ctl->resetting)
		return;
	if(!canqlock(&ctl->tlock))
		return;
	if(waserror()){
		qunlock(&ctl->tlock);
		return;
	}
	for(;;){
		lock(&ctl->txwinlock);
		if(ctl->txseq == ctl->txwindow){
#ifdef BMC64_WLAN_TRACE
			int txseq = ctl->txseq;
			int txwindow = ctl->txwindow;
			int fcmask = ctl->fcmask;
#endif
			//print("f");
			unlock(&ctl->txwinlock);
			if(qlen(edev->oq) != 0)
				wlstatblock(ctl, 1);
#ifdef BMC64_WLAN_TRACE
			if(qlen(edev->oq) != 0)
				print("bmx-wl-dhcp: tx blocked window txseq %d txwin %d fcmask %#x qlen %u\n",
					txseq, txwindow, fcmask, qlen(edev->oq));
#endif
			break;
		}
		if(ctl->fcmask & 1<<2){
#ifdef BMC64_WLAN_TRACE
			int txseq = ctl->txseq;
			int txwindow = ctl->txwindow;
			int fcmask = ctl->fcmask;
#endif
			//print("x");
			unlock(&ctl->txwinlock);
			if(qlen(edev->oq) != 0)
				wlstatblock(ctl, 0);
#ifdef BMC64_WLAN_TRACE
			if(qlen(edev->oq) != 0)
				print("bmx-wl-dhcp: tx blocked flow txseq %d txwin %d fcmask %#x qlen %u\n",
					txseq, txwindow, fcmask, qlen(edev->oq));
#endif
			break;
		}
		unlock(&ctl->txwinlock);
		b = qget(edev->oq);
		if(b == nil)
			break;
		queueus = p9microseconds() - b->timingus;
		off = ((uintptr)b->rp & 3) + sizeof(Sdpcm);
		wldhcptrace("wl tx-qget", b->rp, BLEN(b), ctl);
		wleapoltrace("wl tx-qget", b->rp, BLEN(b), ctl);
		wleapollowtrace(BMX_EAPOL_STAGE_WL_TX_QGET, b->rp, BLEN(b), ctl);
		wldhcplowtrace(BMX_DHCP_STAGE_WL_TX_QGET, b->rp, BLEN(b), ctl);
		b = padblock(b, off + 4);
		len = BLEN(b);
		p = (Sdpcm*)b->rp;
		memset(p, 0, off);	/* TODO: refactor dup code */
		put2(p->len, len);
		put2(p->lenck, ~len);
		p->chanflg = 2;
		p->seq = ctl->txseq;
		p->doffset = off;
		put4(b->rp + off, 0x20);	/* BDC header */
		if(iodebug) dump("send", b->rp, len);
		pktlockstart = p9microseconds();
		qlock(&ctl->pktlock);
		pktlockwaitus = p9microseconds() - pktlockstart;
		wldhcptrace("wl tx-sdio", b->rp + off + 4, len - off - 4, ctl);
		wleapoltrace("wl tx-sdio", b->rp + off + 4, len - off - 4, ctl);
		wleapollowtrace(BMX_EAPOL_STAGE_WL_TX_SDIO, b->rp + off + 4, len - off - 4, ctl);
		wldhcplowtrace(BMX_DHCP_STAGE_WL_TX_SDIO, b->rp + off + 4, len - off - 4, ctl);
		if(waserror()){
			if(iodebug) print("halt frame %x %x\n", cfgr(Wfrmcnt+1), cfgr(Wfrmcnt+1));
			wldhcptrace("wl tx-sdio-error", b->rp + off + 4, len - off - 4, ctl);
			wleapoltrace("wl tx-sdio-error", b->rp + off + 4, len - off - 4, ctl);
			wleapollowtrace(BMX_EAPOL_STAGE_WL_TX_SDIO_ERROR, b->rp + off + 4, len - off - 4, ctl);
			wldhcplowtrace(BMX_DHCP_STAGE_WL_TX_SDIO_ERROR, b->rp + off + 4, len - off - 4, ctl);
			if(wleis8021x(b->rp + off + 4, len - off - 4))
				bmx_l2_note_done_8021x_tx();
			cfgw(Framectl, Wfhalt);
			while(cfgr(Wfrmcnt+1))
				;
			while(cfgr(Wfrmcnt))
				;
			qunlock(&ctl->pktlock);
			nexterror();
		}
		sdiostart = p9microseconds();
		packetrw(1, b->rp, len);
		sdious = p9microseconds() - sdiostart;
		wldhcptrace("wl tx-sdio-done", b->rp + off + 4, len - off - 4, ctl);
		wleapoltrace("wl tx-sdio-done", b->rp + off + 4, len - off - 4, ctl);
		wleapollowtrace(BMX_EAPOL_STAGE_WL_TX_SDIO_DONE, b->rp + off + 4, len - off - 4, ctl);
		wldhcplowtrace(BMX_DHCP_STAGE_WL_TX_SDIO_DONE, b->rp + off + 4, len - off - 4, ctl);
		if(wleis8021x(b->rp + off + 4, len - off - 4))
			bmx_l2_note_done_8021x_tx();
		ctl->txseq++;
		poperror();
		unlockstart = p9microseconds();
		yieldcall = 0;
#if RASPPI >= 5
		qunlock_no_yield(&ctl->pktlock);
		if(++ctl->txyieldbatch >= WlPi5PktYieldBatch){
			ctl->txyieldbatch = 0;
			p9yield();
			yieldcall = 1;
		}
#else
		qunlock(&ctl->pktlock);
		yieldcall = 1;
#endif
		unlockus = p9microseconds() - unlockstart;
		lock(&ctl->statlock);
		ctl->stat_txframes++;
		ctl->stat_txtimingsamples++;
		ctl->stat_txqueueus += queueus;
		if(queueus > ctl->stat_txqueuemaxus)
			ctl->stat_txqueuemaxus = queueus;
		ctl->stat_txpktlockwaitus += pktlockwaitus;
		if(pktlockwaitus > ctl->stat_txpktlockwaitmaxus)
			ctl->stat_txpktlockwaitmaxus = pktlockwaitus;
		ctl->stat_txsdious += sdious;
		if(sdious > ctl->stat_txsdiomaxus)
			ctl->stat_txsdiomaxus = sdious;
		if(yieldcall){
			ctl->stat_txpktlockyieldcalls++;
			ctl->stat_txpktlockyieldus += unlockus;
			if(unlockus > ctl->stat_txpktlockyieldmaxus)
				ctl->stat_txpktlockyieldmaxus = unlockus;
		}
		unlock(&ctl->statlock);
		freeb(b);
	}
	poperror();

	qunlock_no_yield(&ctl->tlock);
}

#if RASPPI >= 5
static void
wlrxyield(Ctlr *ctl)
{
	uvlong start, elapsed;

	start = p9microseconds();
	p9yield();
	elapsed = p9microseconds() - start;
	lock(&ctl->statlock);
	ctl->stat_rxpktlockyieldcalls++;
	ctl->stat_rxpktlockyieldus += elapsed;
	if(elapsed > ctl->stat_rxpktlockyieldmaxus)
		ctl->stat_rxpktlockyieldmaxus = elapsed;
	unlock(&ctl->statlock);
}
#endif

static void
rproc(void *a)
{
	Ether *edev;
	Ctlr *ctl;
	Block *b;
	Sdpcm *p;
	Cmd *q;
	int flowstart;
#if RASPPI >= 5
	int rxyieldbatch;
#endif
	int bdc;
	int cmdid, cmdop;
	ulong cmdstatus;

	edev = a;
	ctl = edev->ctlr;
	flowstart = 0;
#if RASPPI >= 5
	rxyieldbatch = 0;
#endif
	for(;;){
		if(ctl->resetting){
#if RASPPI >= 5
			rxyieldbatch = 0;
#endif
			tsleep(&up->sleep, return0, 0, 10);
			continue;
		}
#if RASPPI >= 5
		if(rxyieldbatch >= WlPi5PktYieldBatch){
			rxyieldbatch = 0;
			wlrxyield(ctl);
		}
#endif
		if(flowstart){
			//print("F");
			flowstart = 0;
			txstart(edev);
		}
			b = wlreadpkt(ctl);
			if(ctl->resetting){
				if(b != nil)
					freeb(b);
				continue;
			}
			if(b == nil){
#if RASPPI >= 5
				rxyieldbatch = 0;
#endif
				intwait(ctl, 1);
				continue;
			}
#if RASPPI >= 5
		rxyieldbatch++;
#endif
		p = (Sdpcm*)b->rp;
		if(p->window != ctl->txwindow || p->fcmask != ctl->fcmask){
			int windowchanged = 0;
			int flowchanged = 0;
			int windowopen;
			int flowopen;
			lock(&ctl->txwinlock);
			if(p->window != ctl->txwindow){
				windowchanged = 1;
				if(ctl->txseq == ctl->txwindow)
					flowstart = 1;
				ctl->txwindow = p->window;
			}
			if(p->fcmask != ctl->fcmask){
				flowchanged = 1;
				if((p->fcmask & 1<<2) == 0)
					flowstart = 1;
				ctl->fcmask = p->fcmask;
			}
			windowopen = ctl->txseq != ctl->txwindow;
			flowopen = (ctl->fcmask & 1<<2) == 0;
			unlock(&ctl->txwinlock);
			lock(&ctl->statlock);
			ctl->stat_txwindowupdates += windowchanged;
			ctl->stat_txflowupdates += flowchanged;
			unlock(&ctl->statlock);
			if(windowopen)
				wlstatunblock(ctl, 1);
			if(flowopen)
				wlstatunblock(ctl, 0);
		}
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		bmxwldbg_rxrecord(ctl, p, b);
#endif
		switch(p->chanflg & 0xF){
		case 0:
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctl->dbg_rp_chan0++;
#endif
			if(iodebug) dump("rsp", b->rp, BLEN(b));
			if(p->doffset < sizeof(Sdpcm) || BLEN(b) < p->doffset + sizeof(Cmd)){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
				ctl->dbg_rsp_short++;
				ctl->dbg_last_rsp_len = BLEN(b);
				ctl->dbg_last_rsp_doffset = p->doffset;
#endif
#ifdef BMC64_WLAN_TRACE
				print("bmx-wl: short rsp doffset %d len %ld\n",
					p->doffset, BLEN(b));
#endif
				break;
			}
			q = (Cmd*)(b->rp + p->doffset);
			cmdid = q->id[0] | q->id[1]<<8;
			cmdop = get4(q->cmd);
			cmdstatus = get4(q->status);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctl->dbg_last_rsp_id = cmdid;
			ctl->dbg_last_rsp_op = cmdop;
			ctl->dbg_last_rsp_pending = ctl->cmdpending;
			ctl->dbg_last_rsp_pendingop = ctl->cmdop;
			ctl->dbg_last_rsp_status = cmdstatus;
			ctl->dbg_last_rsp_len = BLEN(b);
			ctl->dbg_last_rsp_doffset = p->doffset;
#endif
			if(cmdid != ctl->cmdpending || cmdop != ctl->cmdop){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
				ctl->dbg_rsp_stale++;
				if(ctl->cmdpending != 0)
					print("ether4330: wlcmd stale rsp id %d op %d status %lu pending %d/%d var %s reqid %d len %ld off %d txseq %d txwindow %d fcmask %#x\n",
						cmdid, cmdop, cmdstatus,
						ctl->cmdpending, ctl->cmdop,
						ctl->dbg_pending_var, ctl->reqid,
						BLEN(b), p->doffset, ctl->txseq,
						ctl->txwindow, ctl->fcmask);
#endif
#ifdef BMC64_WLAN_TRACE
				print("bmx-wl: stale rsp id %d pending %d reqid %d cmd %d pendingop %d status %lu len %ld\n",
					cmdid, ctl->cmdpending, ctl->reqid,
					cmdop, ctl->cmdop, cmdstatus, BLEN(b));
#endif
				break;
			}
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctl->dbg_rsp_match++;
#endif
			if(ctl->rsp != nil){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
				ctl->dbg_rsp_replace++;
#endif
#ifdef BMC64_WLAN_TRACE
				print("bmx-wl: replacing unread rsp id %d pending %d reqid %d\n",
					cmdid, ctl->cmdpending, ctl->reqid);
#endif
				freeb(ctl->rsp);
				ctl->rsp = nil;
			}
			ctl->rsp = b;
			wakeup(&ctl->cmdr);
			continue;
		case 1:
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctl->dbg_rp_chan1++;
#endif
			if(iodebug) dump("event", b->rp, BLEN(b));
			if(BLEN(b) > p->doffset + 4){
				bdc = 4 + (b->rp[p->doffset + 3] << 2);
				if(BLEN(b) > p->doffset + bdc){
					b->rp += p->doffset + bdc;	/* skip BDC header */
					bcmevent(ctl, b->rp, BLEN(b));
					break;
				}
			}
			if(iodebug && BLEN(b) != p->doffset)
				print("short event %ld %d\n", BLEN(b), p->doffset);
			break;
		case 2:
			lock(&ctl->statlock);
			ctl->stat_rxdataframes++;
			unlock(&ctl->statlock);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctl->dbg_rp_chan2++;
#endif
			if(iodebug) dump("packet", b->rp, BLEN(b));
			if(BLEN(b) > p->doffset + 4){
				bdc = 4 + (b->rp[p->doffset + 3] << 2);
				if(BLEN(b) >= p->doffset + bdc + ETHERHDRSIZE){
					b->rp += p->doffset + bdc;	/* skip BDC header */
					wldhcptrace("wl rx-sdio", b->rp, BLEN(b), ctl);
					wleapoltrace("wl rx-sdio", b->rp, BLEN(b), ctl);
					wleapollowtrace(BMX_EAPOL_STAGE_WL_RX_SDIO, b->rp, BLEN(b), ctl);
					wldhcplowtrace(BMX_DHCP_STAGE_WL_RX_SDIO, b->rp, BLEN(b), ctl);
					etheriq(edev, b, 1);
					continue;
				}
			}
			break;
		default:
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			ctl->dbg_rp_chanx++;
#endif
			dump("ether4330: bad packet", b->rp, BLEN(b));
			break;
		}
		freeb(b);
	}
}

static void
joinfinish(Ctlr *ctl, int status)
{
	if(ctl->status != Connecting)
		return;
	if(ctl->joinstatus != 0)
		return;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	ctl->dbg_join_event = ctl->dbg_last_event;
	ctl->dbg_join_event_status = ctl->dbg_last_event_status;
	ctl->dbg_join_event_reason = ctl->dbg_last_event_reason;
	ctl->dbg_join_event_flags = ctl->dbg_last_event_flags;
	BMX_WLAN_LOW_LOG("ether4330: join finish result %d event %s(%lu) event_status %lu reason %lu flags %#x ssidok %d linkup %d status %d\n",
		status, evstring(ctl->dbg_join_event), ctl->dbg_join_event,
		ctl->dbg_join_event_status, ctl->dbg_join_event_reason,
		ctl->dbg_join_event_flags, ctl->joinssidok, ctl->joinlinkup,
		ctl->status);
#endif
	ctl->joinstatus = 1 + status;
	wakeup(&ctl->joinr);
}

static void
joinlinkup(Ctlr *ctl)
{
	if(ctl->status != Connecting){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		BMX_WLAN_LOW_LOG("ether4330: ignore link up outside join status %d\n",
			ctl->status);
#endif
		return;
	}
	ctl->joinlinkup = 1;
	if(ctl->joinssidok)
		joinfinish(ctl, 0);
}

static void
scanfinish(Ctlr *ctl)
{
	ctl->scanactive = 0;
	wakeup(&ctl->scanr);
}

static void
linkdown(Ctlr *ctl)
{
	Ether *edev;
#ifndef __circle__
	Netfile *f;
	int i;
#endif

	edev = ctl->edev;
	if(edev == nil || ctl->status == Disconnected)
		return;
	if(ctl->status == Connecting)
		joinfinish(ctl, 1);
	ctl->status = Disconnected;
	ctl->joinssidok = 0;
	ctl->joinlinkup = 0;
	memset(ctl->bssid, 0, Eaddrlen);
#ifndef __circle__
	/* send eof to aux/wpa */
	for(i = 0; i < edev->nfile; i++){
		f = edev->f[i];
		if(f == nil || f->in == nil || f->inuse == 0 || f->type != 0x888e)
			continue;
		qwrite(f->in, 0, 0);
	}
#endif
}

static void
linkdowncleanup(Ctlr *ctl, const char *reason)
{
	Block *scanb;
	Ether *edev;
	int oldstatus, oldscanactive, oldscansecs, flushed;
	uchar oldfcmask;

	if(ctl == nil)
		return;

	edev = ctl->edev;
	oldstatus = ctl->status;
	oldscanactive = ctl->scanactive;
	oldscansecs = ctl->scansecs;
	oldfcmask = ctl->fcmask;
	flushed = 0;

	ctl->status = Disconnected;
	ctl->joinstatus = 0;
	ctl->joinssidok = 0;
	ctl->joinlinkup = 0;
	memset(ctl->bssid, 0, Eaddrlen);

	scanb = ctl->scanb;
	ctl->scanb = nil;
	ctl->scansecs = 0;
	ctl->scanactive = 0;
	wakeup(&ctl->scanr);
	wakeup(&ctl->joinr);
	if(scanb != nil)
		freeb(scanb);

	lock(&ctl->txwinlock);
	ctl->fcmask = 0;
	unlock(&ctl->txwinlock);

	if(edev != nil)
		flushed = queueflush(edev->oq);

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	BMX_WLAN_LOW_LOG("ether4330: linkdown cleanup reason %s status %d scan %d/%d fcmask %#x flushed %d txseq %d txwindow %d\n",
		reason != nil ? reason : "-", oldstatus, oldscanactive,
		oldscansecs, oldfcmask, flushed, ctl->txseq, ctl->txwindow);
#endif
}

void
ether4330linkdowncleanup(Ether *edev, const char *reason)
{
	if(edev == nil || edev->ctlr == nil)
		return;

	linkdowncleanup(edev->ctlr, reason);
}

void
ether4330dumptrace(Ether *edev, const char *reason)
{
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	if(edev == nil || edev->ctlr == nil)
		return;

	bmxwldbg_dump(edev->ctlr, reason);
#else
	USED(edev);
	USED(reason);
#endif
}

void
ether4330debugauthstart(Ether *edev, unsigned gen)
{
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	Ctlr *ctl;

	if(edev == nil || edev->ctlr == nil)
		return;

	ctl = edev->ctlr;
	ctl->dbg_authgen = gen;
	ctl->dbg_authstartticks = m->ticks;
#else
	USED(edev);
	USED(gen);
#endif
}

/*
 * Command interface between host and firmware
 */

static char *eventnames[] = {
	[0] = "set ssid",
	[1] = "join",
	[2] = "start",
	[3] = "auth",
	[4] = "auth ind",
	[5] = "deauth",
	[6] = "deauth ind",
	[7] = "assoc",
	[8] = "assoc ind",
	[9] = "reassoc",
	[10] = "reassoc ind",
	[11] = "disassoc",
	[12] = "disassoc ind",
	[13] = "quiet start",
	[14] = "quiet end",
	[15] = "beacon rx",
	[16] = "link",
	[17] = "mic error",
	[18] = "ndis link",
	[19] = "roam",
	[20] = "txfail",
	[21] = "pmkid cache",
	[22] = "retrograde tsf",
	[23] = "prune",
	[24] = "autoauth",
	[25] = "eapol msg",
	[26] = "scan complete",
	[27] = "addts ind",
	[28] = "delts ind",
	[29] = "bcnsent ind",
	[30] = "bcnrx msg",
	[31] = "bcnlost msg",
	[32] = "roam prep",
	[33] = "pfn net found",
	[34] = "pfn net lost",
	[35] = "reset complete",
	[36] = "join start",
	[37] = "roam start",
	[38] = "assoc start",
	[39] = "ibss assoc",
	[40] = "radio",
	[41] = "psm watchdog",
	[44] = "probreq msg",
	[45] = "scan confirm ind",
	[46] = "psk sup",
	[47] = "country code changed",
	[48] = "exceeded medium time",
	[49] = "icv error",
	[50] = "unicast decode error",
	[51] = "multicast decode error",
	[52] = "trace",
	[53] = "bta hci event",
	[54] = "if",
	[55] = "p2p disc listen complete",
	[56] = "rssi",
	[57] = "pfn scan complete",
	[58] = "extlog msg",
	[59] = "action frame",
	[60] = "action frame complete",
	[61] = "pre assoc ind",
	[62] = "pre reassoc ind",
	[63] = "channel adopted",
	[64] = "ap started",
	[65] = "dfs ap stop",
	[66] = "dfs ap resume",
	[67] = "wai sta event",
	[68] = "wai msg",
	[69] = "escan result",
	[70] = "action frame off chan complete",
	[71] = "probresp msg",
	[72] = "p2p probreq msg",
	[73] = "dcs request",
	[74] = "fifo credit map",
	[75] = "action frame rx",
	[76] = "wake event",
	[77] = "rm complete",
	[78] = "htsfsync",
	[79] = "overlay req",
	[80] = "csa complete ind",
	[81] = "excess pm wake event",
	[82] = "pfn scan none",
	[83] = "pfn scan allgone",
	[84] = "gtk plumbed",
	[85] = "assoc ind ndis",
	[86] = "reassoc ind ndis",
	[87] = "assoc req ie",
	[88] = "assoc resp ie",
	[89] = "assoc recreated",
	[90] = "action frame rx ndis",
	[91] = "auth req",
	[92] = "tdls peer event",
	[127] = "bcmc credit support"
};

static char*
evstring(uint event)
{
	static char buf[12];

	if(event >= nelem(eventnames) || eventnames[event] == 0){
		/* not reentrant but only called from one kproc */
		snprint(buf, sizeof buf, "%d", event);
		return buf;
	}
	return eventnames[event];
}

static void
bcmevent(Ctlr *ctl, uchar *p, int len)
{
	int flags;
	long event, status, reason;
	ether_event_params_t params;

	memset (&params, 0, sizeof params);

	if(len < ETHERHDRSIZE + 10 + 46)
		return;
	p += ETHERHDRSIZE + 10;			/* skip bcm_ether header */
	len -= ETHERHDRSIZE + 10;
	flags = nhgets(p + 2);
	event = nhgets(p + 6);
	status = nhgetl(p + 8);
	reason = nhgetl(p + 12);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	ctl->dbg_event_count++;
	ctl->dbg_last_event = event;
	ctl->dbg_last_event_status = status;
	ctl->dbg_last_event_reason = reason;
	ctl->dbg_last_event_flags = flags;
	switch(event){
	case 0:
		ctl->dbg_event_setssid++;
		break;
	case 7:
	case 9:
		ctl->dbg_event_assoc++;
		break;
	case 16:
		if(flags&1)
			ctl->dbg_event_linkup++;
		else
			ctl->dbg_event_linkdown++;
		break;
	case 5:
	case 6:
		ctl->dbg_event_deauth++;
		break;
	case 12:
		ctl->dbg_event_disassoc++;
		break;
	case 69:
		ctl->dbg_event_escan++;
		break;
	}
#endif
	if(EVENTDEBUG)
		print("ether4330: [%s] status %ld flags %#x reason %ld\n",
			evstring(event), status, flags, reason);
	switch(event){
	case 19:	/* E_ROAM */
		if(status == 0)
			break;
	/* fall through */
	case 0:		/* E_SET_SSID */
		if(status == 0){
			memcpy(ctl->bssid, p + 24, Eaddrlen);
			ctl->joinssidok = 1;
			if(ctl->joinlinkup)
				joinfinish(ctl, 0);
		}else{
			ctl->joinssidok = 0;
			ctl->joinlinkup = 0;
			joinfinish(ctl, status);
		}
		break;
	case 7:
	case 9:
		if(status == 0)
			joinlinkup(ctl);
		else if(ctl->status == Connecting)
			joinfinish(ctl, status);
		break;
	case 5:		/* E_DEAUTH */
	case 6:		/* E_DEAUTH_IND */
		linkdown(ctl);
		callevhndlr(ctl, ether_event_deauth, 0);
		break;
	case 16:	/* E_LINK */
		if(flags&1){	/* link up */
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
			if(ctl->status != Connecting)
				print("ether4330: ignore E_LINK up outside join status %d\n",
					ctl->status);
#endif
			if(ctl->status == Connecting){
				joinlinkup(ctl);
				callevhndlr(ctl, ether_event_link, 0);
			}
			break;
		}
	/* fall through */
	case 12:	/* E_DISASSOC_IND */
		linkdown(ctl);
		callevhndlr(ctl, ether_event_disassoc, 0);
		break;
	case 17:	/* E_MIC_ERROR */
		params.mic_error.group = !!(flags & 4);
		memcpy(params.mic_error.addr, p + 24, Eaddrlen);
		callevhndlr(ctl, ether_event_mic_error, &params);
		break;
	case 3:		/* E_AUTH */
	case 26:	/* E_SCAN_COMPLETE */
		break;
	case 69:	/* E_ESCAN_RESULT */
		if(status == 8)
			wlscanresult(ctl->edev, p + 48, len - 48);
		else{
			scanfinish(ctl);
			callevhndlr(ctl, ether_event_scan_complete, 0);
		}
		break;
	default:
		if(status){
			if(!EVENTDEBUG)
				print("ether4330: [%s] error status %ld flags %#x reason %ld\n",
					evstring(event), status, flags, reason);
			dump("event", p, len);
		}
	}
}

static int
joindone(void *a)
{
	return ((Ctlr*)a)->joinstatus;
}

static int
waitjoin(Ctlr *ctl)
{
	int n;

	tsleep(&ctl->joinr, joindone, ctl, WljoinTimeoutMs);
	n = ctl->joinstatus;
	ctl->joinstatus = 0;
	if(n == 0){
		print("ether4330: join timeout after %d ms ssidok %d linkup %d\n",
			WljoinTimeoutMs, ctl->joinssidok, ctl->joinlinkup);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		print("ether4330: join timeout detail last_event %s(%lu) event_status %lu reason %lu flags %#x status %d\n",
			evstring(ctl->dbg_last_event), ctl->dbg_last_event,
			ctl->dbg_last_event_status, ctl->dbg_last_event_reason,
			ctl->dbg_last_event_flags, ctl->status);
#endif
		ctl->status = Disconnected;
		ctl->joinssidok = 0;
		ctl->joinlinkup = 0;
		error("wifi join timeout");
	}
	return n - 1;
}

static int
scanidle(void *a)
{
	return ((Ctlr*)a)->scanactive == 0;
}

static void
waitscanidle(Ctlr *ctl)
{
	if(!ctl->scanactive)
		return;

	BMX_WLAN_LOW_LOG("ether4330: join waits for active scan\n");
	tsleep(&ctl->scanr, scanidle, ctl, WlcmdTimeoutMs * 2);
	if(ctl->scanactive)
		BMX_WLAN_LOW_LOG("ether4330: join proceeds after scan wait timeout\n");
}

static int
cmddone(void *a)
{
	return ((Ctlr*)a)->rsp != nil;
}

#if defined(BMC64_WLAN_TRACE) || defined(BMC64_WLAN_LOW_IMPACT_TRACE)
static ulong
wltrace_ms(ulong start)
{
	return (m->ticks - start) * 1000 / HZ;
}
#endif

#ifdef BMC64_WLAN_TRACE
static int
wltrace_var(const char *name)
{
	if(name == nil)
		return 0;

	return strcmp(name, "join") == 0
	    || strcmp(name, "wsec_key") == 0
	    || strcmp(name, "country") == 0
	    || strcmp(name, "escan") == 0;
}

static const char*
wltrace_keyname(int id)
{
	switch(id){
	case CMrxkey:
		return "rxkey";
	case CMrxkey0:
		return "rxkey0";
	case CMrxkey1:
		return "rxkey1";
	case CMrxkey2:
		return "rxkey2";
	case CMrxkey3:
		return "rxkey3";
	case CMtxkey:
		return "txkey";
	default:
		return "key";
	}
}
#endif

static void
wlcmd(Ctlr *ctl, int write, int op, void *data, int dlen, void *res, int rlen)
{
	Block *b;
	Block *volatile cleanupb;
	Sdpcm *p;
	Cmd *q;
	int len, tlen, cmdid;
	const char *cmdname;
	int timeoutbusfatal;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	const char *diag_name = "-";
	int diag_critical = 0;
	int diag_pending_id = 0;
	int diag_pending_op = 0;
	int diag_pending_write = 0;
	int diag_pending_txseq = 0;
	int diag_pending_txwindow = 0;
	int diag_pending_fcmask = 0;
	ulong diag_start = m->ticks;

	if(op == 52)
		diag_name = "disassoc";
	else if(op == 26)
		diag_name = "setssid";
	else if((op == SetVar || op == GetVar) && data != nil && dlen > 0)
		diag_name = (const char*)data;
	diag_critical = op == 52 || op == 26 ||
		strcmp(diag_name, "escan") == 0 ||
		strcmp(diag_name, "wsec_key") == 0;
#endif
#ifdef BMC64_WLAN_TRACE
	const char *trace_name = nil;
	int trace_enabled = 0;
	int trace_reqid = 0;
	ulong trace_start = m->ticks;

	if((op == SetVar || op == GetVar) && data != nil && dlen > 0){
		trace_name = (const char*)data;
		trace_enabled = wltrace_var(trace_name);
	}
#endif

	cmdname = "-";
	if(op == 52)
		cmdname = "disassoc";
	else if(op == 26)
		cmdname = "setssid";
	else if((op == SetVar || op == GetVar) && data != nil && dlen > 0)
		cmdname = (const char*)data;
	timeoutbusfatal = !(op == 52 || strcmp(cmdname, "wsec_key") == 0 ||
		strcmp(cmdname, "escan") == 0);

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	if(eventcallbackdepth != 0)
		print("ether4330: BUG wlcmd from rx event context depth %d op %d var %s\n",
			eventcallbackdepth, op, diag_name);
#endif

	if(write)
		tlen = dlen + rlen;
	else
		tlen = MAX(dlen, rlen);
	len = sizeof(Sdpcm) + sizeof(Cmd) + tlen;
	b = allocb(len);
	cleanupb = b;
	qlock(&ctl->cmdlock);
	if(waserror()){
		ctl->cmdpending = 0;
		ctl->cmdop = 0;
		if(cleanupb != nil)
			freeb(cleanupb);
		qunlock(&ctl->cmdlock);
		nexterror();
	}
	if(ctl->resetting)
		error("wlan resetting");
	if(ctl->rsp != nil){
#ifdef BMC64_WLAN_TRACE
		if(trace_enabled)
			print("bmx-wl: drop stale rsp before cmd op %d var %s reqid %d\n",
				op, trace_name, ctl->reqid);
#endif
		freeb(ctl->rsp);
		ctl->rsp = nil;
	}
	memset(b->wp, 0, len);
	qlock(&ctl->pktlock);
	p = (Sdpcm*)b->wp;
	put2(p->len, len);
	put2(p->lenck, ~len);
	p->seq = ctl->txseq;
	p->doffset = sizeof(Sdpcm);
	b->wp += sizeof(*p);

	q = (Cmd*)b->wp;
	put4(q->cmd, op);
	put4(q->len, tlen);
	put2(q->flags, write? 2 : 0);
	put2(q->id, ++ctl->reqid);
	ctl->cmdpending = ctl->reqid;
	ctl->cmdop = op;
	cmdid = ctl->reqid;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	ctl->dbg_pending_id = cmdid;
	ctl->dbg_pending_op = op;
	ctl->dbg_pending_write = write;
	ctl->dbg_pending_txseq = ctl->txseq;
	ctl->dbg_pending_txwindow = ctl->txwindow;
	ctl->dbg_pending_fcmask = ctl->fcmask;
	ctl->dbg_pending_start = m->ticks;
	snprint(ctl->dbg_pending_var, sizeof ctl->dbg_pending_var, "%s", diag_name);
	bmxwldbg_cmdstart(ctl, cmdid, op, diag_name, write);
#endif
	put4(q->status, 0);
	b->wp += sizeof(*q);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	diag_start = m->ticks;
	if(diag_critical)
		BMX_WLAN_LOW_LOG("ether4330: wlcmd start id %d op %d var %s write %d dlen %d rlen %d txseq %d txwindow %d fcmask %#x status %d joinssidok %d joinlinkup %d pending %d/%d\n",
			cmdid, op, diag_name, write, dlen, rlen, ctl->txseq,
			ctl->txwindow, ctl->fcmask, ctl->status,
			ctl->joinssidok, ctl->joinlinkup, ctl->cmdpending,
			ctl->cmdop);
#endif
#ifdef BMC64_WLAN_TRACE
	trace_reqid = cmdid;
	trace_start = m->ticks;
	if(trace_enabled)
		print("bmx-wl: wlcmd start id %d op %d var %s write %d dlen %d rlen %d txseq %d txwin %d fcmask %#x\n",
			trace_reqid, op, trace_name, write, dlen, rlen,
			ctl->txseq, ctl->txwindow, ctl->fcmask);
#endif

	if(dlen > 0)
		memmove(b->wp, data, dlen);
	if(write)
		memmove(b->wp + dlen, res, rlen);
	b->wp += tlen;

	if(iodebug) dump("cmd", b->rp, len);
	packetrw(1, b->rp, len);
	ctl->txseq++;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	bmxwldbg_cmdaftertx(ctl, cmdid, op);
#endif
	qunlock(&ctl->pktlock);
	freeb(b);
	cleanupb = nil;
	b = nil;
	USED(b);
	tsleep(&ctl->cmdr, cmddone, ctl, WlcmdTimeoutMs);
	b = ctl->rsp;
	ctl->rsp = nil;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	diag_pending_id = ctl->cmdpending;
	diag_pending_op = ctl->cmdop;
	diag_pending_write = ctl->dbg_pending_write;
	diag_pending_txseq = ctl->dbg_pending_txseq;
	diag_pending_txwindow = ctl->dbg_pending_txwindow;
	diag_pending_fcmask = ctl->dbg_pending_fcmask;
#endif
	ctl->cmdpending = 0;
	ctl->cmdop = 0;
	if(b == nil){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		uchar intpend, framectl, rfrmcntlo, rfrmnthi, wfrmcntlo, wfrmnthi;
		ulong intstatus, hostmbox;

		sbwindow(ctl->sdregs);
		intpend = sdiord(Fn0, Intpend);
		intstatus = cfgreadl(Fn1, ctl->sdregs + Intstatus);
		hostmbox = cfgreadl(Fn1, ctl->sdregs + Hostmboxdata);
		framectl = cfgr(Framectl);
		rfrmcntlo = cfgr(Rfrmcnt);
		rfrmnthi = cfgr(Rfrmcnt+1);
		wfrmcntlo = cfgr(Wfrmcnt);
		wfrmnthi = cfgr(Wfrmcnt+1);
#endif
		print("ether4330: wlcmd timeout id %d op %d txseq %d txwindow %d fcmask %#x after %d ms\n",
			cmdid, op, ctl->txseq, ctl->txwindow, ctl->fcmask, WlcmdTimeoutMs);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		print("ether4330: wlcmd pendingdiag var %s cmdid %d op %d write %d pending_before_clear %d/%d pending_dbg %d/%d reqid %d start_txseq %d start_txwin %d start_fm %#x now_txseq %d now_txwin %d now_fm %#x\n",
			diag_name, cmdid, op, diag_pending_write,
			diag_pending_id, diag_pending_op,
			ctl->dbg_pending_id, ctl->dbg_pending_op, ctl->reqid,
			diag_pending_txseq, diag_pending_txwindow,
			diag_pending_fcmask, ctl->txseq, ctl->txwindow,
			ctl->fcmask);
		print("ether4330: wlcmd timeout state intpend %u intstatus %lu hostmbox %lu framectl %u rfrmcnt %u/%u wfrmcnt %u/%u status %d joinssidok %d joinlinkup %d resetting %d\n",
			intpend, intstatus, hostmbox, framectl, rfrmnthi, rfrmcntlo,
			wfrmnthi, wfrmcntlo, ctl->status, ctl->joinssidok,
			ctl->joinlinkup, ctl->resetting);
		print("ether4330: wlcmd rxdiag var %s iw %lu ip0 %lu frame %lu mbox %lu fcchg %lu last_ip %u last_is %lu last_hmb %lu rp %lu zero %lu bad %lu ch %lu/%lu/%lu/%lu last_len %d/%d chan %d next %d off %d win %d fm %#x\n",
			diag_name, ctl->dbg_iw_calls, ctl->dbg_iw_intpend0,
			ctl->dbg_iw_frameint, ctl->dbg_iw_mailboxint,
			ctl->dbg_iw_fcchange, ctl->dbg_last_intpend,
			ctl->dbg_last_intstatus, ctl->dbg_last_hostmbox,
			ctl->dbg_rp_calls, ctl->dbg_rp_zero, ctl->dbg_rp_bad,
			ctl->dbg_rp_chan0, ctl->dbg_rp_chan1, ctl->dbg_rp_chan2,
			ctl->dbg_rp_chanx, ctl->dbg_last_len,
			ctl->dbg_last_lenck, ctl->dbg_last_chanflg,
			ctl->dbg_last_nextlen, ctl->dbg_last_doffset,
			ctl->dbg_last_window, ctl->dbg_last_fcmask);
		print("ether4330: wlcmd rspdiag var %s short %lu stale %lu match %lu replace %lu last_rsp id %d op %d status %lu pending %d/%d len %d off %d\n",
			diag_name, ctl->dbg_rsp_short, ctl->dbg_rsp_stale,
			ctl->dbg_rsp_match, ctl->dbg_rsp_replace,
			ctl->dbg_last_rsp_id, ctl->dbg_last_rsp_op,
			ctl->dbg_last_rsp_status, ctl->dbg_last_rsp_pending,
			ctl->dbg_last_rsp_pendingop, ctl->dbg_last_rsp_len,
			ctl->dbg_last_rsp_doffset);
		print("ether4330: wlcmd eventdiag var %s events %lu setssid %lu assoc %lu link %lu/%lu deauth %lu disassoc %lu escan %lu last event %lu status %lu reason %lu flags %#x\n",
			diag_name, ctl->dbg_event_count, ctl->dbg_event_setssid,
			ctl->dbg_event_assoc, ctl->dbg_event_linkup,
			ctl->dbg_event_linkdown, ctl->dbg_event_deauth,
			ctl->dbg_event_disassoc, ctl->dbg_event_escan,
			ctl->dbg_last_event, ctl->dbg_last_event_status,
			ctl->dbg_last_event_reason, ctl->dbg_last_event_flags);
		bmxwldbg_cmdfinish(ctl, cmdid, op, -1, 0, 0, 0);
		bmxwldbg_dump(ctl, diag_name);
#endif
#ifdef BMC64_WLAN_TRACE
		if(trace_enabled)
			print("bmx-wl: wlcmd timeout id %d op %d var %s ms %lu\n",
				trace_reqid, op, trace_name, wltrace_ms(trace_start));
#endif
		if(timeoutbusfatal)
			ctl->resetting = 1;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		else
			print("ether4330: wlcmd timeout contained var %s op %d; bus remains active\n",
				cmdname, op);
#endif
		error("wlcmd timeout");
	}
	p = (Sdpcm*)b->rp;
	if(p->doffset < sizeof(Sdpcm) || BLEN(b) < p->doffset + sizeof(Cmd)){
		print("ether4330: short wlcmd response id %d op %d doffset %d len %ld\n",
			cmdid, op, p->doffset, BLEN(b));
		dump("ether4330: short wlcmd response", b->rp, BLEN(b));
		error("wlcmd short response");
	}
	q = (Cmd*)(b->rp + p->doffset);
	if((q->id[0] | q->id[1]<<8) != cmdid || get4(q->cmd) != op){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		bmxwldbg_cmdfinish(ctl, cmdid, op, -2,
			q->id[0] | q->id[1]<<8, get4(q->cmd), get4(q->status));
#endif
		print("ether4330: mismatched wlcmd response id %d expected %d cmd %lu expected %d\n",
			q->id[0] | q->id[1]<<8, cmdid, get4(q->cmd), op);
		dump("ether4330: mismatched wlcmd response", b->rp, BLEN(b));
		error("wlcmd mismatched response");
	}
	if(!write && BLEN(b) < p->doffset + sizeof(Cmd) + rlen){
		print("ether4330: short wlcmd payload id %d op %d len %ld need %d\n",
			cmdid, op, BLEN(b), p->doffset + (int)sizeof(Cmd) + rlen);
		error("wlcmd short response");
	}
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	bmxwldbg_cmdfinish(ctl, cmdid, op, 1,
		q->id[0] | q->id[1]<<8, get4(q->cmd), get4(q->status));
	if(diag_critical)
		BMX_WLAN_LOW_LOG("ether4330: wlcmd done id %d op %d var %s status %lu ms %lu txseq %d txwindow %d fcmask %#x status_state %d joinssidok %d joinlinkup %d\n",
			cmdid, op, diag_name, get4(q->status),
			wltrace_ms(diag_start), ctl->txseq, ctl->txwindow,
			ctl->fcmask, ctl->status, ctl->joinssidok,
			ctl->joinlinkup);
#endif
#ifdef BMC64_WLAN_TRACE
	if(trace_enabled)
		print("bmx-wl: wlcmd done id %d op %d var %s ms %lu status %lu\n",
			trace_reqid, op, trace_name, wltrace_ms(trace_start), get4(q->status));
#endif
	if(q->status[0] | q->status[1] | q->status[2] | q->status[3]){
		print("ether4330: cmd %d error status %ld\n", op, get4(q->status));
		dump("ether4330: cmd error", b->rp, BLEN(b));
		error("wlcmd error");
	}
	if(!write)
		memmove(res, q + 1, rlen);
	freeb(b);
	cleanupb = nil;
	qunlock(&ctl->cmdlock);
	poperror();
}

static void
wlcmdint(Ctlr *ctl, int op, int val)
{
	uchar buf[4];

	put4(buf, val);
	wlcmd(ctl, 1, op, buf, 4, nil, 0);
}

static void
wlgetvar(Ctlr *ctl, char *name, void *val, int len)
{
	wlcmd(ctl, 0, GetVar, name, strlen(name) + 1, val, len);
}

static void
wlcmdretry(Ctlr *ctl, int write, int op, void *data, int dlen, void *res, int rlen, int retries)
{
	int attempt;
	const char *name;

	name = nil;
	if((op == SetVar || op == GetVar) && data != nil && dlen > 0)
		name = (const char*)data;
	for(attempt = 0;; attempt++){
		if(waserror()){
			if(strcmp(up->errstr, "wlcmd timeout") != 0 || attempt >= retries)
				nexterror();
			print("ether4330: retry wlcmd op %d var %s attempt %d after timeout\n",
				op, name != nil ? name : "-", attempt + 1);
			tsleep(&up->sleep, return0, nil, WlcmdRetryDelayMs);
			continue;
		}
		wlcmd(ctl, write, op, data, dlen, res, rlen);
		poperror();
		break;
	}
}

static void
wlsetvar(Ctlr *ctl, char *name, void *val, int len)
{
	if(VARDEBUG){
		char buf[32];
		snprint(buf, sizeof buf, "wlsetvar %s:", name);
		dump(buf, val, len);
	}
	wlcmdretry(ctl, 1, SetVar, name, strlen(name) + 1, val, len,
		strcmp(name, "wsec_key") == 0 ? WlcmdWsecKeyRetries : 0);
}

static void
wlsetint(Ctlr *ctl, char *name, int val)
{
	uchar buf[4];

	put4(buf, val);
	wlsetvar(ctl, name, buf, 4);
}

static void
wlwepkey(Ctlr *ctl, int i)
{
	uchar params[164];
	uchar *p;

	memset(params, 0, sizeof params);
	p = params;
	p = put4(p, i);		/* index */
	p = put4(p, ctl->keys[i].len);
	memmove(p, ctl->keys[i].dat, ctl->keys[i].len);
	p += 32 + 18*4;		/* keydata, pad */
	if(ctl->keys[i].len == WMinKeyLen)
		p = put4(p, 1);		/* algo = WEP1 */
	else
		p = put4(p, 3);		/* algo = WEP128 */
	put4(p, 2);		/* flags = Primarykey */

	wlsetvar(ctl, "wsec_key", params, sizeof params);
	if(i >= 0 && i < WNFirmwareKeys)
		ctl->keyinstalled[i] = 1;
}

#ifndef __circle__
static void
memreverse(char *dst, char *src, int len)
{
	src += len;
	while(len-- > 0)
		*dst++ = *--src;
}
#endif

static void
wlwpakey(Ctlr *ctl, int id, uvlong iv, uchar *ea)
{
	uchar params[164], wsec[4];
	uchar *p;
	int pairwise, keyidx;
#if defined(BMC64_WLAN_TRACE) || defined(BMC64_WLAN_LOW_IMPACT_TRACE)
	ulong trace_start = m->ticks;
#endif

	if(id == CMrxkey)
		return;
	pairwise = (id == CMrxkey || id == CMtxkey);
	keyidx = pairwise ? 0 : id - CMrxkey0;
	bmx_l2_wait_pending_8021x_tx(BMX_WAIT_PENDING_8021X_TX_MS);
	memset(params, 0, sizeof params);
	p = params;
	p = put4(p, keyidx);
	p = put4(p, ctl->keys[0].len);
	memmove((char*)p,  ctl->keys[0].dat, ctl->keys[0].len);
	p += 32 + 18*4;		/* keydata, pad */
	if(ctl->cryptotype == Wpa)
		p = put4(p, 2);	/* algo = TKIP */
	else
		p = put4(p, 4);	/* algo = AES_CCM */
	if(pairwise)
		p = put4(p, 0);
	else
		p = put4(p, 2);		/* flags = Primarykey */
	p += 3*4;

	p = put4(p, 1);		/* iv initialised */
	p += 4;
	p = put4(p, iv>>16);	/* iv high */
	p = put2(p, iv&0xFFFF);	/* iv low */
	p += 2 + 2*4;		/* align, pad */
	if(pairwise)
		memmove(p, ea, Eaddrlen);

#ifdef BMC64_WLAN_TRACE
	print("bmx-wl: key install start id %s pairwise %d len %d\n",
		wltrace_keyname(id), pairwise, ctl->keys[0].len);
#endif
	wlsetvar(ctl, "wsec_key", params, sizeof params);
	if(!pairwise){

		wlgetvar(ctl, "wsec", wsec, sizeof wsec);
		wlsetint(ctl, "wsec", get4(wsec) | (ctl->cryptotype == Wpa ? 2 : 4));
	}
	if(keyidx >= 0 && keyidx < WNFirmwareKeys)
		ctl->keyinstalled[keyidx] = 1;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	bmx_eapol_low_key_install(id, pairwise, wltrace_ms(trace_start));
#endif
#ifdef BMC64_WLAN_TRACE
	print("bmx-wl: key install done id %s pairwise %d ms %lu\n",
		wltrace_keyname(id), pairwise, wltrace_ms(trace_start));
#endif
}

static void
wlclearkey(Ctlr *ctl, int index)
{
	uchar params[164];
	uchar *p;
#ifdef BMC64_WLAN_TRACE
	ulong trace_start = m->ticks;
#endif

	if(index >= 0 && index < WNFirmwareKeys && !ctl->keyinstalled[index]){
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
		print("ether4330: firmware clear key skip idx %d not-installed\n", index);
#endif
		return;
	}

	bmx_l2_wait_pending_8021x_tx(BMX_WAIT_PENDING_8021X_TX_MS);
	memset(params, 0, sizeof params);
	p = params;
	p = put4(p, index);
	p = put4(p, 0);
	p += 32 + 18*4;		/* keydata, pad */
	p = put4(p, 0);
	put4(p, 2);

#ifdef BMC64_WLAN_TRACE
	print("bmx-wl: key clear start idx %d\n", index);
#endif
	if(index >= 0 && index < WNFirmwareKeys)
		ctl->keyinstalled[index] = 0;
	wlsetvar(ctl, "wsec_key", params, sizeof params);
#ifdef BMC64_WLAN_TRACE
	print("bmx-wl: key clear done idx %d ms %lu\n",
		index, wltrace_ms(trace_start));
#endif
}

static void
wljoin(Ctlr *ctl, char *ssid, int chanspec, uchar *bssid)
{
	uchar params[72];
	uchar *p;
	int n;

	if(chanspec > 0 && chanspec <= 16)
		chanspec |= 0x2b00;
	p = params;
	n = strlen(ssid);
	n = MIN(n, 32);
	p = put4(p, n);
	memmove(p, ssid, n);
	memset(p + n, 0, 32 - n);
	p += 32;
	p = put4(p, 0xff);	/* scan type */
	if(chanspec != 0){
		p = put4(p, 2);		/* num probes */
		p = put4(p, 120);	/* active time */
		p = put4(p, 390);	/* passive time */
	}else{
		p = put4(p, -1);	/* num probes */
		p = put4(p, -1);	/* active time */
		p = put4(p, -1);	/* passive time */
	}
	p = put4(p, -1);	/* home time */
	if(bssid != 0)
		memcpy(p, bssid, Eaddrlen);	/* bssid */
	else
		memset(p, 0xFF, Eaddrlen);
	p += Eaddrlen;
	p = put2(p, 0);		/* pad */
	if(chanspec != 0){
		p = put4(p, 1);		/* num chans */
		p = put2(p, chanspec);	/* chan spec */
		p = put2(p, 0);		/* pad */
		assert(p == params + sizeof(params));
	}else{
		p = put4(p, 0);		/* num chans */
		assert(p == params + sizeof(params) - 4);
	}

	ctl->status = Connecting;
	ctl->joinstatus = 0;
	ctl->joinssidok = 0;
	ctl->joinlinkup = 0;
	waitscanidle(ctl);
	if(waserror()){
		ctl->status = Disconnected;
		ctl->joinssidok = 0;
		ctl->joinlinkup = 0;
		nexterror();
	}
	wlsetvar(ctl, "join", params, chanspec? sizeof params : sizeof params - 4);
	poperror();
	switch(waitjoin(ctl)){
	case 0:
		ctl->status = Connected;
		break;
	case 3:
		ctl->status = Disconnected;
		ctl->joinssidok = 0;
		ctl->joinlinkup = 0;
		error("wifi join: network not found");
	case 1:
		ctl->status = Disconnected;
		ctl->joinssidok = 0;
		ctl->joinlinkup = 0;
		error("wifi join: failed");
	default:
		ctl->status = Disconnected;
		ctl->joinssidok = 0;
		ctl->joinlinkup = 0;
		error("wifi join: error");
	}
}

static void
wlcreateAP(Ctlr *ctl, char *ssid, int channel, int hidden)	/* by @sebastienNEC */
{
	wlcmdint(ctl, 3, 1);		/* DOWN */
	wlcmdint(ctl, 20, 1);		/* SET_INFRA */
	wlcmdint(ctl, 118, 1);		/* SET_AP */
	wlcmdint(ctl, 30, channel);
	wlcmdint(ctl, 2, 1);		/* UP */

	uchar join_params[4+WNameLen+14];
	uchar *p = join_params;
	int n = strlen(ssid);		/* copy ssid */
	n = MIN(n, WNameLen);
	p = put4(p, n);
	memmove(p, ssid, n);
	memset(p + n, 0, WNameLen - n);
	p += WNameLen;
	memset(p, 0, 14);		/* clear assoc params */
	wlcmd(ctl, 1, 26, &join_params, sizeof(join_params), nil, 0);	/* SET_SSID */

	wlsetint(ctl, "closednet", hidden);

	/* TODO? beacon settings */

	ctl->status = Connected;	/* TODO: check return code as in waitjoin() */
}

static void
wlscanstart(Ctlr *ctl)
{
	/* version[4] action[2] sync_id[2] ssidlen[4] ssid[32] bssid[6] bss_type[1]
		scan_type[1] nprobes[4] active_time[4] passive_time[4] home_time[4]
		nchans[2] nssids[2] chans[nchans][2] ssids[nssids][32] */
	/* hack - this is only correct on a little-endian cpu */
	static uchar params[4+2+2+4+32+6+1+1+4*4+2+2+14*2+32+4] = {
		1,0,0,0,
		1,0,
		0x34,0x12,
		0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0xff,0xff,0xff,0xff,0xff,0xff,
		2,
		0,
		0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,
		14,0,
		1,0,
		0x01,0x2b,0x02,0x2b,0x03,0x2b,0x04,0x2b,0x05,0x2e,0x06,0x2e,0x07,0x2e,
		0x08,0x2b,0x09,0x2b,0x0a,0x2b,0x0b,0x2b,0x0c,0x2b,0x0d,0x2b,0x0e,0x2b,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	};

	wlsetvar(ctl, "escan", params, sizeof params);
}

#ifndef __circle__

static uchar*
gettlv(uchar *p, uchar *ep, int tag)
{
	int len;

	while(p + 1 < ep){
		len = p[1];
		if(p + 2 + len > ep)
			return nil;
		if(p[0] == tag)
			return p;
		p += 2 + len;
	}
	return nil;
}

static void
addscan(Block *bp, uchar *p, int len)
{
	char bssid[25], ssid[20];
	char *auth, *auth2;
	uchar *t, *et;
	int ielen, ssidlen;
	static uchar wpaie1[4] = { 0x00, 0x50, 0xf2, 0x01 };

	snprint(bssid, sizeof bssid, ";bssid=%02X:%02X:%02X:%02X:%02X:%02X",
		p[8+0], p[8+1], p[8+2], p[8+3], p[8+4], p[8+5]);
	if(strstr((char*)bp->rp, bssid) != nil)
		return;
	ssidlen = p[18] < 19 ? p[18] : 19;
	strncpy(ssid, (const char *) p+19, ssidlen);
	ssid[ssidlen] = '\0';
	bp->wp = (uchar*)seprint((char*)bp->wp, (char*)bp->lim,
		"ssid=%s%s;signal=%d;noise=%d;chan=%d",
		ssid, bssid,
		(short)get2(p+78), (signed char)p[80],
		get2(p+72) & 0xF);
	auth = auth2 = "";
	if(get2(p + 16) & 0x10)
		auth = ";wep";
	ielen = get4(p + 0x78);
	if(ielen > 0){
		t = p + get4(p + 0x74);
		et = t + ielen;
		if(et > p + len)
			return;
		if(gettlv(t, et, 0x30) != nil){
			auth = "";
			auth2 = ";wpa2";
		}
		while((t = gettlv(t, et, 0xdd)) != nil){
			if(t[1] > 4 && memcmp(t+2, wpaie1, 4) == 0){
				auth = ";wpa";
				break;
			}
			t += 2 + t[1];
		}
	}
	bp->wp = (uchar*)seprint((char*)bp->wp, (char*)bp->lim,
		"%s%s\n", auth, auth2);
}


static void
wlscanresult(Ether *edev, uchar *p, int len)
{
	Ctlr *ctlr;
	Netfile **ep, *f, **fp;
	Block *bp;
	int nbss, i;

	ctlr = edev->ctlr;
	if(get4(p) > len)
		return;
	/* TODO: more syntax checking */
	bp = ctlr->scanb;
	if(bp == nil)
		ctlr->scanb = bp = allocb(8192);
	nbss = get2(p+10);
	p += 12;
	len -= 12;
	if(0) dump("SCAN", p, len);
	if(nbss){
		addscan(bp, p, len);
		return;
	}
	i = edev->scan;
	ep = &edev->f[Ntypes];
	for(fp = edev->f; fp < ep && i > 0; fp++){
		f = *fp;
		if(f == nil || f->scan == 0)
			continue;
		if(i == 1)
			qpass(f->in, bp);
		else
			qpass(f->in, copyblock(bp, BLEN(bp)));
		i--;
	}
	if(i)
		freeb(bp);
	ctlr->scanb = nil;
}

#else

static void
wlscanresult(Ether *edev, uchar *p, int len)
{
	etherscanresult(edev, p, len);
}

#endif

static void
wlsetcountry(Ctlr *ctlr, const char *ccode)
{
	struct{
		char country_ie[4];
		uint revision;
		char country_code[4];
	}params;

	if (   !('A' <= ccode[0] && ccode[0] <= 'Z')
	    || !('A' <= ccode[1] && ccode[1] <= 'Z')
	    || ccode[2] != '\0'){
		error("Invalid country code");
	}

	strcpy (params.country_ie, ccode);
	strcpy (params.country_code, ccode);
	params.revision = (uint) -1;

	wlsetvar(ctlr, "country", &params, sizeof params);
}

static void
lproc(void *a)
{
	Ether *edev;
	Ctlr *ctlr;
	int secs;

	edev = a;
	ctlr = edev->ctlr;
	secs = 0;
	for(;;){
		tsleep(&up->sleep, return0, 0, 1000);
		if(ctlr->resetting){
			secs = 0;
			continue;
		}
		if(ctlr->scansecs){
			if(secs == 0){
				if(waserror())
					ctlr->scansecs = 0;
				else{
					wlscanstart(ctlr);
					poperror();
				}
				secs = ctlr->scansecs;
			}
			--secs;
		}else
			secs = 0;
	}
}

static void
wlinit(Ether *edev, Ctlr *ctlr)
{
	uchar ea[Eaddrlen];
	uchar eventmask[16];
	uchar joinpref[8];
	char version[128];
	char *p;
	static uchar keepalive[12] = {1, 0, 11, 0, 0xd8, 0xd6, 0, 0, 0, 0, 0, 0};

#if RASPPI >= 5
	memmove(ea, edev->ea, Eaddrlen);
	wlsetvar(ctlr, "cur_etheraddr", ea, Eaddrlen);
#else
	wlgetvar(ctlr, "cur_etheraddr", ea, Eaddrlen);
	memmove(edev->ea, ea, Eaddrlen);
#endif
	memmove(edev->addr, ea, Eaddrlen);
	print("ether4330: addr %02X:%02X:%02X:%02X:%02X:%02X\n",
	      ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);
	wlsetint(ctlr, "assoc_listen", 10);
	if(ctlr->chipid == 43430 || ctlr->chipid == 0x4345)
		wlcmdint(ctlr, 0x56, 0);	/* powersave off */
	else
		wlcmdint(ctlr, 0x56, 2);	/* powersave FAST */
	wlsetint(ctlr, "bus:txglom", 0);
	wlsetint(ctlr, "mpc", 1);
	joinpref[0] = 4;
	joinpref[1] = 2;
	joinpref[2] = 8;
	joinpref[3] = 1;
	joinpref[4] = 1;
	joinpref[5] = 2;
	joinpref[6] = 0;
	joinpref[7] = 0;
	wlsetvar(ctlr, "join_pref", joinpref, sizeof joinpref);
	wlsetint(ctlr, "bcn_timeout", 2);
	wlsetint(ctlr, "assoc_retry_max", 3);
	if(ctlr->chipid == 0x4330){
		wlsetint(ctlr, "btc_wire", 4);
		wlsetint(ctlr, "btc_mode", 1);
		wlsetvar(ctlr, "mkeep_alive", keepalive, 11);
	}
	memset(eventmask, 0, sizeof eventmask);
#define ENABLE(n)	eventmask[n/8] |= 1<<(n%8)
	ENABLE(0);	/* E_SET_SSID */
	ENABLE(3);	/* E_AUTH */
	ENABLE(5);	/* E_DEAUTH */
	ENABLE(6);	/* E_DEAUTH_IND */
	ENABLE(7);
	ENABLE(9);
	ENABLE(12);	/* E_DISASSOC_IND */
	ENABLE(16);	/* E_LINK */
	ENABLE(17);	/* E_MIC_ERROR */
	ENABLE(19);	/* E_ROAM */
	ENABLE(26);	/* E_SCAN_COMPLETE */
	ENABLE(54);	/* E_IF */
	ENABLE(69);	/* E_ESCAN_RESULT */
	wlsetvar(ctlr, "event_msgs", eventmask, sizeof eventmask);
	wlcmdint(ctlr, 0xb9, 0x28);	/* SET_SCAN_CHANNEL_TIME */
	wlcmdint(ctlr, 0xbb, 0x28);	/* SET_SCAN_UNASSOC_TIME */
	wlcmdint(ctlr, 0x102, 0x78);	/* SET_SCAN_PASSIVE_TIME */
	wlcmdint(ctlr, 2, 0);		/* UP */
	memset(version, 0, sizeof version);
	wlgetvar(ctlr, "ver", version, sizeof version - 1);
	if((p = strchr(version, '\n')) != nil)
		*p = '\0';
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: firmware version %s\n", version);
	print("ether4330: init policy host-driven pm off mpc on txglom off bcn_timeout 2 roam off assoc_retry_max 3 scan_time 40 passive_time 120 join_pref rssi\n");
#else
	if(0) print("ether4330: %s\n", version);
#endif
	wlsetint(ctlr, "roam_off", 1);
	wlcmdint(ctlr, 0x14, 1);	/* SET_INFRA 1 */
	wlcmdint(ctlr, 10, 0);		/* SET_PROMISC */
	//wlcmdint(ctlr, 0x8e, 0);	/* SET_BAND 0 */
	//wlsetint(ctlr, "wsec", 1);
	wlcmdint(ctlr, 2, 1);		/* UP */
	ctlr->keys[0].len = WMinKeyLen;
	//wlwepkey(ctlr, 0);
}

static int
queueflush(Queue *q)
{
	Block *b;
	int n;

	if(q == nil)
		return 0;

	n = 0;
	while((b = qget(q)) != nil){
		freeb(b);
		n++;
	}

	return n;
}

static ulong
ms_since(ulong ticks)
{
	return (m->ticks - ticks) * 1000 / HZ;
}

static const char*
alockowner(Ctlr *ctlr)
{
	if(ctlr->alockowner == nil)
		return "-";

	return ctlr->alockowner;
}

static void
adapterlock(Ctlr *ctlr, const char *owner)
{
	qlock(&ctlr->alock);
	ctlr->alockowner = owner != nil ? owner : "-";
	ctlr->alockticks = m->ticks;
}

static int
adaptercanlock(Ctlr *ctlr, const char *owner)
{
	if(!canqlock(&ctlr->alock))
		return 0;

	ctlr->alockowner = owner != nil ? owner : "-";
	ctlr->alockticks = m->ticks;

	return 1;
}

static int
adapterlocktimed(Ctlr *ctlr, const char *owner, const char *reason, uint timeoutms)
{
	ulong start, nextlog;

	start = m->ticks;
	nextlog = start + HZ;
	for(;;){
		if(adaptercanlock(ctlr, owner))
			return 1;
		if(ms_since(start) >= timeoutms)
			return 0;
		if((long)(m->ticks - nextlog) >= 0){
			print("ether4330: alock busy owner %s owner_ms %lu wait_ms %lu reason %s\n",
				alockowner(ctlr),
				ctlr->alockticks != 0 ? ms_since(ctlr->alockticks) : 0,
				ms_since(start), reason != nil ? reason : "-");
			nextlog = m->ticks + HZ;
		}
		tsleep(&up->sleep, return0, nil, 10);
	}
}

static void
adapterunlock(Ctlr *ctlr)
{
	ctlr->alockowner = nil;
	ctlr->alockticks = 0;
	qunlock(&ctlr->alock);
}

static void
resetctlstate(Ctlr *ctlr)
{
	if(ctlr->rsp != nil){
		freeb(ctlr->rsp);
		ctlr->rsp = nil;
	}
	if(ctlr->scanb != nil){
		freeb(ctlr->scanb);
		ctlr->scanb = nil;
	}

	ctlr->cmdpending = 0;
	ctlr->cmdop = 0;
	ctlr->joinstatus = 0;
	ctlr->joinssidok = 0;
	ctlr->joinlinkup = 0;
	ctlr->scansecs = 0;
	ctlr->scanactive = 0;
	ctlr->status = Disconnected;
	memset(ctlr->bssid, 0, Eaddrlen);
	memset(ctlr->keys, 0, sizeof ctlr->keys);
	memset(ctlr->keyinstalled, 0, sizeof ctlr->keyinstalled);
	ctlr->keys[0].len = WMinKeyLen;
	ctlr->reqid = 0;
	ctlr->fcmask = 0;
	ctlr->txwindow = 0;
	ctlr->txseq = 0;
	ctlr->rxseq = 0;
	ctlr->stat_txframes = 0;
	ctlr->stat_rxdataframes = 0;
	ctlr->stat_txwindowupdates = 0;
	ctlr->stat_txflowupdates = 0;
	ctlr->stat_txwindowstalls = 0;
	ctlr->stat_txwindowstallticks = 0;
	ctlr->stat_txwindowstallmaxticks = 0;
	ctlr->stat_txwindowstallstart = 0;
	ctlr->stat_txwindowblocked = 0;
	ctlr->stat_txflowstalls = 0;
	ctlr->stat_txflowstallticks = 0;
	ctlr->stat_txflowstallmaxticks = 0;
	ctlr->stat_txflowstallstart = 0;
	ctlr->stat_txflowblocked = 0;
	ctlr->stat_txtimingsamples = 0;
	ctlr->stat_txqueueus = 0;
	ctlr->stat_txqueuemaxus = 0;
	ctlr->stat_txpktlockwaitus = 0;
	ctlr->stat_txpktlockwaitmaxus = 0;
	ctlr->stat_txsdious = 0;
	ctlr->stat_txsdiomaxus = 0;
	ctlr->stat_txpktlockyieldcalls = 0;
	ctlr->stat_txpktlockyieldus = 0;
	ctlr->stat_txpktlockyieldmaxus = 0;
#if RASPPI >= 5
	ctlr->txyieldbatch = 0;
#endif
	ctlr->stat_rxtimingsamples = 0;
	ctlr->stat_rxpktlockwaitus = 0;
	ctlr->stat_rxpktlockwaitmaxus = 0;
	ctlr->stat_rxsdious = 0;
	ctlr->stat_rxsdiomaxus = 0;
	ctlr->stat_rxpktlockyieldcalls = 0;
	ctlr->stat_rxpktlockyieldus = 0;
	ctlr->stat_rxpktlockyieldmaxus = 0;
	ctlr->stat_rxtonetdevsamples = 0;
	ctlr->stat_rxtonetdevus = 0;
	ctlr->stat_rxtonetdevmaxus = 0;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	ctlr->dbg_iw_calls = 0;
	ctlr->dbg_iw_intpend0 = 0;
	ctlr->dbg_iw_frameint = 0;
	ctlr->dbg_iw_mailboxint = 0;
	ctlr->dbg_iw_fcchange = 0;
	ctlr->dbg_rp_calls = 0;
	ctlr->dbg_rp_zero = 0;
	ctlr->dbg_rp_bad = 0;
	ctlr->dbg_rp_chan0 = 0;
	ctlr->dbg_rp_chan1 = 0;
	ctlr->dbg_rp_chan2 = 0;
	ctlr->dbg_rp_chanx = 0;
	ctlr->dbg_rsp_short = 0;
	ctlr->dbg_rsp_stale = 0;
	ctlr->dbg_rsp_match = 0;
	ctlr->dbg_rsp_replace = 0;
	ctlr->dbg_event_count = 0;
	ctlr->dbg_event_setssid = 0;
	ctlr->dbg_event_assoc = 0;
	ctlr->dbg_event_linkup = 0;
	ctlr->dbg_event_linkdown = 0;
	ctlr->dbg_event_deauth = 0;
	ctlr->dbg_event_disassoc = 0;
	ctlr->dbg_event_escan = 0;
	ctlr->dbg_last_intpend = 0;
	ctlr->dbg_last_intstatus = 0;
	ctlr->dbg_last_hostmbox = 0;
	ctlr->dbg_last_len = 0;
	ctlr->dbg_last_lenck = 0;
	ctlr->dbg_last_chanflg = 0;
	ctlr->dbg_last_nextlen = 0;
	ctlr->dbg_last_doffset = 0;
	ctlr->dbg_last_window = 0;
	ctlr->dbg_last_fcmask = 0;
	ctlr->dbg_last_rsp_id = 0;
	ctlr->dbg_last_rsp_op = 0;
	ctlr->dbg_last_rsp_pending = 0;
	ctlr->dbg_last_rsp_pendingop = 0;
	ctlr->dbg_last_rsp_status = 0;
	ctlr->dbg_last_rsp_len = 0;
	ctlr->dbg_last_rsp_doffset = 0;
	ctlr->dbg_pending_id = 0;
	ctlr->dbg_pending_op = 0;
	ctlr->dbg_pending_write = 0;
	ctlr->dbg_pending_txseq = 0;
	ctlr->dbg_pending_txwindow = 0;
	ctlr->dbg_pending_fcmask = 0;
	ctlr->dbg_pending_start = 0;
	ctlr->dbg_pending_var[0] = 0;
	ctlr->dbg_last_event = 0;
	ctlr->dbg_last_event_status = 0;
	ctlr->dbg_last_event_reason = 0;
	ctlr->dbg_last_event_flags = 0;
	ctlr->dbg_join_event = 0;
	ctlr->dbg_join_event_status = 0;
	ctlr->dbg_join_event_reason = 0;
	ctlr->dbg_join_event_flags = 0;
	memset(ctlr->dbg_cmdring, 0, sizeof ctlr->dbg_cmdring);
	ctlr->dbg_cmdwrite = 0;
	ctlr->dbg_cmdcount = 0;
	memset(ctlr->dbg_rxring, 0, sizeof ctlr->dbg_rxring);
	ctlr->dbg_rxwrite = 0;
	ctlr->dbg_rxcount = 0;
	memset(ctlr->dbg_dataring, 0, sizeof ctlr->dbg_dataring);
	ctlr->dbg_datawrite = 0;
	ctlr->dbg_datacount = 0;
	memset(ctlr->dbg_eapolring, 0, sizeof ctlr->dbg_eapolring);
	ctlr->dbg_eapolwrite = 0;
	ctlr->dbg_eapolcount = 0;
	ctlr->dbg_data_seen = 0;
	ctlr->dbg_data_ok = 0;
	ctlr->dbg_data_eapol = 0;
	ctlr->dbg_data_short_bdc = 0;
	ctlr->dbg_data_short_eth = 0;
	ctlr->dbg_event_short_bdc = 0;
	ctlr->dbg_event_short_body = 0;
	ctlr->dbg_authgen = 0;
	ctlr->dbg_authstartticks = 0;
#endif
}

static void
resethwstate(Ctlr *ctlr)
{
	ctlr->chipid = 0;
	ctlr->chiprev = 0;
	ctlr->armcore = 0;
	ctlr->regufile = nil;
	ctlr->resetvec.i = 0;
	ctlr->chipcommon = 0;
	ctlr->armctl = 0;
	ctlr->armregs = 0;
	ctlr->d11ctl = 0;
	ctlr->socramregs = 0;
	ctlr->socramctl = 0;
	ctlr->sdregs = 0;
	ctlr->sdiorev = 0;
	ctlr->socramrev = 0;
	ctlr->socramsize = 0;
	ctlr->rambase = 0;
}

void
ether4330hardreset(Ether *edev, const char *reason)
{
	Ctlr *ctlr;
	volatile int havetlock, havecmdlock, havepktlock;
	int flushed;

	if(edev == nil || (ctlr = edev->ctlr) == nil)
		error(Enonexist);

	havetlock = havecmdlock = havepktlock = 0;
	flushed = 0;

	print("ether4330: hard reset requested reason %s\n",
		reason != nil ? reason : "-");

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step wait-alock reason %s\n",
		reason != nil ? reason : "-");
#endif
	if(!adapterlocktimed(ctlr, "hardreset", reason, 10000)){
		print("ether4330: hard reset alock timeout reason %s owner %s owner_ms %lu\n",
			reason != nil ? reason : "-", alockowner(ctlr),
			ctlr->alockticks != 0 ? ms_since(ctlr->alockticks) : 0);
		error("wlan alock timeout");
	}
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step got-alock reason %s\n",
		reason != nil ? reason : "-");
#endif
	if(waserror()){
		if(havepktlock)
			qunlock(&ctlr->pktlock);
		if(havecmdlock)
			qunlock(&ctlr->cmdlock);
		if(havetlock)
			qunlock(&ctlr->tlock);
		ctlr->resetting = 0;
		adapterunlock(ctlr);
		print("ether4330: hard reset failed reason %s err %s\n",
			reason != nil ? reason : "-", up->errstr != nil ? up->errstr : "-");
		nexterror();
	}

	print("ether4330: hard reset start reason %s\n", reason != nil ? reason : "-");

	ctlr->resetting = 1;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step drain-rproc reason %s\n",
		reason != nil ? reason : "-");
#endif
	tsleep(&up->sleep, return0, nil, 20);

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step wait-tlock reason %s\n",
		reason != nil ? reason : "-");
#endif
	qlock(&ctlr->tlock);
	havetlock = 1;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step got-tlock reason %s\n",
		reason != nil ? reason : "-");
	print("ether4330: hard reset step wait-cmdlock reason %s\n",
		reason != nil ? reason : "-");
#endif
	qlock(&ctlr->cmdlock);
	havecmdlock = 1;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step got-cmdlock reason %s\n",
		reason != nil ? reason : "-");
	print("ether4330: hard reset step wait-pktlock reason %s\n",
		reason != nil ? reason : "-");
#endif
	qlock(&ctlr->pktlock);
	havepktlock = 1;
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step got-pktlock reason %s\n",
		reason != nil ? reason : "-");
#endif

	resetctlstate(ctlr);
	flushed = queueflush(edev->oq);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step flushed reason %s q %d\n",
		reason != nil ? reason : "-", flushed);
#endif

	qunlock(&ctlr->pktlock);
	havepktlock = 0;
	qunlock(&ctlr->cmdlock);
	havecmdlock = 0;

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step trysdioreset reason %s\n",
		reason != nil ? reason : "-");
#endif
	trysdioreset();
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step wlanpowercycle reason %s\n",
		reason != nil ? reason : "-");
#endif
	wlanpowercycle();
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step resethwstate reason %s\n",
		reason != nil ? reason : "-");
#endif
	resethwstate(ctlr);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step sdioinit reason %s\n",
		reason != nil ? reason : "-");
#endif
	sdioinit();
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step sbinit reason %s\n",
		reason != nil ? reason : "-");
#endif
	sbinit(ctlr);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step fwload reason %s\n",
		reason != nil ? reason : "-");
#endif
	fwload(ctlr);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step sbenable reason %s\n",
		reason != nil ? reason : "-");
#endif
	sbenable(ctlr);

	ctlr->resetting = 0;

#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step reguload reason %s has %d\n",
		reason != nil ? reason : "-", ctlr->regufile != nil);
#endif
	if(ctlr->regufile)
		reguload(ctlr, ctlr->regufile);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step wlinit reason %s\n",
		reason != nil ? reason : "-");
#endif
	wlinit(edev, ctlr);
#ifdef BMC64_WLAN_LOW_IMPACT_TRACE
	print("ether4330: hard reset step country reason %s has %d\n",
		reason != nil ? reason : "-", ctlr->country[0] != '\0');
#endif
	if(ctlr->country[0] != '\0')
		wlsetcountry(ctlr, ctlr->country);
	ctlr->status = Disconnected;

	qunlock(&ctlr->tlock);
	havetlock = 0;
	adapterunlock(ctlr);
	poperror();

	print("ether4330: hard reset done reason %s flushed %d\n",
		reason != nil ? reason : "-", flushed);
}

/*
 * Plan 9 driver interface
 */

static long
etherbcmifstat(Ether* edev, void* a, long n, ulong offset)
{
	Ctlr *ctlr;
	char *p;
	int l;
	static char *cryptoname[4] = {
		[0]	"off",
		[Wep]	"wep",
		[Wpa]	"wpa",
		[Wpa2]	"wpa2",
	};
	/* these strings are known by aux/wpa */
	static char* connectstate[] = {
		[Disconnected]	= "unassociated",
		[Connecting] = "connecting",
		[Connected] = "associated",
	};

	ctlr = edev->ctlr;
	if(ctlr == nil)
		return 0;
	p = malloc(READSTR);
	l = 0;

	l += snprint(p+l, READSTR-l, "channel: %d\n", ctlr->chanid);
	l += snprint(p+l, READSTR-l, "bssid: %02x:%02x:%02x:%02x:%02x:%02x\n",
		     ctlr->bssid[0], ctlr->bssid[1], ctlr->bssid[2],
		     ctlr->bssid[3], ctlr->bssid[4], ctlr->bssid[5]);
	l += snprint(p+l, READSTR-l, "essid: %s\n", ctlr->essid);
	l += snprint(p+l, READSTR-l, "crypt: %s\n", cryptoname[ctlr->cryptotype]);
	l += snprint(p+l, READSTR-l, "oq: %d\n", qlen(edev->oq));
	l += snprint(p+l, READSTR-l, "txwin: %d\n", ctlr->txwindow);
	l += snprint(p+l, READSTR-l, "txseq: %d\n", ctlr->txseq);
	l += snprint(p+l, READSTR-l, "status: %s\n", connectstate[ctlr->status]);
	USED(l);
	n = readstr(offset, a, n, p);
	free(p);
	return n;
}

static void
etherbcmtransmit(Ether *edev)
{
	Ctlr *ctlr;

	ctlr = edev->ctlr;
	if(ctlr == nil)
		return;
	txstart(edev);
}

static int
parsehex(char *buf, int buflen, char *a)
{
	int i, k, n;

	k = 0;
	for(i = 0;k < buflen && *a; i++){
		if(*a >= '0' && *a <= '9')
			n = *a++ - '0';
		else if(*a >= 'a' && *a <= 'f')
			n = *a++ - 'a' + 10;
		else if(*a >= 'A' && *a <= 'F')
			n = *a++ - 'A' + 10;
		else
			break;

		if(i & 1){
			buf[k] |= n;
			k++;
		}
		else
			buf[k] = n<<4;
	}
	if(i & 1)
		return -1;
	return k;
}

static int
wepparsekey(WKey* key, char* a)
{
	int i, k, len, n;
	char buf[WMaxKeyLen];

	len = strlen(a);
	if(len == WMinKeyLen || len == WMaxKeyLen){
		memset(key->dat, 0, sizeof(key->dat));
		memmove(key->dat, a, len);
		key->len = len;

		return 0;
	}
	else if(len == WMinKeyLen*2 || len == WMaxKeyLen*2){
		k = 0;
		for(i = 0; i < len; i++){
			if(*a >= '0' && *a <= '9')
				n = *a++ - '0';
			else if(*a >= 'a' && *a <= 'f')
				n = *a++ - 'a' + 10;
			else if(*a >= 'A' && *a <= 'F')
				n = *a++ - 'A' + 10;
			else
				return -1;

			if(i & 1){
				buf[k] |= n;
				k++;
			}
			else
				buf[k] = n<<4;
		}

		memset(key->dat, 0, sizeof(key->dat));
		memmove(key->dat, buf, k);
		key->len = k;

		return 0;
	}

	return -1;
}

static int
wpaparsekey(WKey *key, uvlong *ivp, char *a)
{
	int len;
	char *e;

	if(cistrncmp(a, "tkip:", 5) == 0 || cistrncmp(a, "ccmp:", 5) == 0)
		a += 5;
	else
		return 1;
	len = parsehex(key->dat, sizeof(key->dat), a);
	if(len <= 0)
		return 1;
	key->len = len;
	a += 2*len;
	if(*a++ != '@')
		return 1;
	*ivp = strtoull(a, &e, 16);
	if(e == a)
		return -1;
	return 0;
}

static void
setauth(Ctlr *ctlr, Cmdbuf *cb, char *a)
{
	uchar wpaie[32];
	int i;








	i = parsehex((char*)wpaie, 2, a);
	if(i != 2 || wpaie[1] > (int)sizeof wpaie - 2)
		cmderror(cb, "bad wpa ie syntax");
	i = parsehex((char*)wpaie, wpaie[1] + 2, a);
	if(i != wpaie[1] + 2)
		cmderror(cb, "bad wpa ie syntax");
	if(wpaie[0] == 0xdd)
		ctlr->cryptotype = Wpa;
	else if(wpaie[0] == 0x30)
		ctlr->cryptotype = Wpa2;
	else
		cmderror(cb, "bad wpa ie");
	wlsetvar(ctlr, "wpaie", wpaie, i);
	if(ctlr->cryptotype == Wpa){
		wlsetint(ctlr, "wpa_auth", 4|2);	/* auth_psk | auth_unspecified */
		wlsetint(ctlr, "auth", 0);
		wlsetint(ctlr, "wsec", 2);		/* tkip */
		wlsetint(ctlr, "wpa_auth", 4);		/* auth_psk */
	}else{
		wlsetint(ctlr, "wpa_auth", 0x80|0x40);	/* auth_psk | auth_unspecified */
		wlsetint(ctlr, "auth", 0);
		wlsetint(ctlr, "wsec", 4);		/* aes */
		wlsetint(ctlr, "wpa_auth", 0x80);	/* auth_psk */
	}
}

static int
setcrypt(Ctlr *ctlr, Cmdbuf*cb, char *a)
{
	if(cistrcmp(a, "wep") == 0 || cistrcmp(a, "on") == 0)
		ctlr->cryptotype = Wep;
	else if(cistrcmp(a, "off") == 0 || cistrcmp(a, "none") == 0)
		ctlr->cryptotype = 0;
	else
		return 0;
	wlsetint(ctlr, "auth", ctlr->cryptotype);
	return 1;
}

static long
etherbcmctl(Ether* edev, const void* buf, long n)
{
	Ctlr *ctlr;
	Cmdbuf *cb;
	Cmdtab *ct;
	uchar ea[Eaddrlen];
	uvlong iv = 0;
	int i;
	volatile int havealock;

	if((ctlr = edev->ctlr) == nil)
		error(Enonexist);
	USED(ctlr);
	havealock = 0;

	cb = parsecmd(buf, n);
	if(waserror()){
		if(havealock)
			adapterunlock(ctlr);
		free(cb);
		nexterror();
	}
	ct = lookupcmd(cb, cmds, nelem(cmds));
	adapterlock(ctlr, ct->cmd);
	havealock = 1;
	switch(ct->index){
	case CMauth:
		setauth(ctlr, cb, cb->f[1]);
		if(ctlr->essid[0])
			wljoin(ctlr, ctlr->essid, ctlr->chanid, 0);
		break;
	case CMchannel:
		if((i = atoi(cb->f[1])) < 0 || i > 16)
			cmderror(cb, "bad channel number");
		//wlcmdint(ctlr, 30, i);	/* SET_CHANNEL */
		ctlr->chanid = i;
		break;
	case CMcrypt:
		if(setcrypt(ctlr, cb, cb->f[1])){
			if(ctlr->essid[0])
				wljoin(ctlr, ctlr->essid, ctlr->chanid, 0);
		}else
			cmderror(cb, "bad crypt type");
		break;
	case CMessid:
		if(cistrcmp(cb->f[1], "default") == 0)
			memset(ctlr->essid, 0, sizeof(ctlr->essid));
		else{
			strncpy(ctlr->essid, cb->f[1], sizeof(ctlr->essid) - 1);
			ctlr->essid[sizeof(ctlr->essid) - 1] = '\0';
		}
		if(!waserror()){
			wljoin(ctlr, ctlr->essid, ctlr->chanid, 0);
			poperror();
		}
		break;
	case CMjoin:
		if(strcmp(cb->f[1], "") != 0){	/* empty string for no change */
			if(cistrcmp(cb->f[1], "default") != 0){
				strncpy(ctlr->essid, cb->f[1], sizeof(ctlr->essid)-1);
				ctlr->essid[sizeof(ctlr->essid)-1] = 0;
			}else
				memset(ctlr->essid, 0, sizeof(ctlr->essid));
		}else if(ctlr->essid[0] == 0)
			cmderror(cb, "essid not set");
		if(parseether(ea, cb->f[2]) < 0)
			cmderror(cb, "bad bssid");
		char *e;
		unsigned long chanspec;
		chanspec = strtoul(cb->f[3], &e, 0);
		if(e != cb->f[3] && *e == 0 && chanspec <= 0xFFFF)
			ctlr->chanid = chanspec;
		else
			cmderror(cb, "bad chanspec");
		if(!setcrypt(ctlr, cb, cb->f[4]))
			setauth(ctlr, cb, cb->f[4]);
		if(ctlr->essid[0])
			wljoin(ctlr, ctlr->essid, ctlr->chanid, ea);
		break;
	case CMkey1:
	case CMkey2:
	case CMkey3:
	case CMkey4:
		i = ct->index - CMkey1;
		if(wepparsekey(&ctlr->keys[i], cb->f[1]))
			cmderror(cb, "bad WEP key syntax");
		wlsetint(ctlr, "wsec", 1);	/* wep enabled */
		wlwepkey(ctlr, i);
		break;
	case CMclearkey:
		i = atoi(cb->f[1]);
		if(i < 0 || i > 5)
			cmderror(cb, "bad key index");
#if defined(BMC64_DEBUG_PROFILE) || defined(BMC64_WLAN_TRACE)
		print("ether4330: firmware clear key idx %d\n", i);
#endif
		wlclearkey(ctlr, i);
#if defined(BMC64_DEBUG_PROFILE) || defined(BMC64_WLAN_TRACE)
		print("ether4330: firmware clear key done idx %d\n", i);
#endif
		break;
	case CMrxkey:
	case CMrxkey0:
	case CMrxkey1:
	case CMrxkey2:
	case CMrxkey3:
	case CMtxkey:
		if(parseether(ea, cb->f[1]) < 0)
			cmderror(cb, "bad ether addr");
		if(wpaparsekey(&ctlr->keys[0], &iv, cb->f[2]))
			cmderror(cb, "bad wpa key");
		wlwpakey(ctlr, ct->index, iv, ea);
		break;
	case CMdisassoc:	/* disassoc reason */
		i = atoi(cb->f[1]);
		if(ctlr->status == Connected){
			if(waserror()){
				print("ether4330: firmware disassoc failed reason %d status %d err %s\n",
					i, ctlr->status, up->errstr);
			}else{
				print("ether4330: firmware disassoc reason %d status %d\n",
					i, ctlr->status);
				wlcmd(ctlr, 1, 52, nil, 0, nil, 0);	/* DISASSOC */
				poperror();
			}
		}else if(ctlr->status == Connecting){
			print("ether4330: firmware disassoc skipped reason %d status %d\n",
				i, ctlr->status);
		}
		ctlr->status = Disconnected;
		ctlr->joinstatus = 0;
		ctlr->joinssidok = 0;
		ctlr->joinlinkup = 0;
		memset(ctlr->bssid, 0, Eaddrlen);
		linkdowncleanup(ctlr, "control-disassoc");
		break;
	case CMescan:		/* escan seconds */
		etherbcmscan(edev, atoi(cb->f[1]));
		break;
	case CMcountry:		/* country alpha2 */
		wlsetcountry(ctlr, cb->f[1]);
		ctlr->country[0] = cb->f[1][0];
		ctlr->country[1] = cb->f[1][1];
		ctlr->country[2] = '\0';
		break;
	case CMdebug:
		iodebug = atoi(cb->f[1]);
		break;
	case CMcreate:		/* create essid channel */ /* by @sebastienNEC */
		if(strcmp(cb->f[1], "") != 0) {	/* empty string for no change */
			if(cistrcmp(cb->f[1], "default") != 0) {
				strncpy(ctlr->essid, cb->f[1], sizeof(ctlr->essid)-1);
				ctlr->essid[sizeof(ctlr->essid)-1] = 0;
			}
			else memset(ctlr->essid, 0, sizeof(ctlr->essid));
		}
		else if(ctlr->essid[0] == 0) cmderror(cb, "essid not set");
		if ((i = atoi(cb->f[2])) >= 0 && i <= 16) ctlr->chanid = i;
		else cmderror(cb, "bad channel number");
		if(ctlr->essid[0]) wlcreateAP(ctlr, ctlr->essid, ctlr->chanid, atoi(cb->f[3]));
		break;
	case CMdown:
		wlcmdint(ctlr, 3, 0);		/* DOWN */
		ctlr->status = Disconnected;
		break;
	}
	adapterunlock(ctlr);
	havealock = 0;
	poperror();
	free(cb);
	return n;
}

static void
etherbcmgetbssid (struct Ether *edev, void *bssid)
{
	Ctlr* ctlr;

	ctlr = edev->ctlr;
	memcpy(bssid, ctlr->bssid, Eaddrlen);
}

static void
etherbcmscan(void *a, uint secs)
{
	Ether* edev;
	Ctlr* ctlr;

	edev = a;
	ctlr = edev->ctlr;
#ifdef __circle__
	ctlr->scansecs = 0;
	if(secs == 0){
		ctlr->scanactive = 0;
		wakeup(&ctlr->scanr);
		return;
	}
	if(ctlr->scanactive)
		return;
	ctlr->scanactive = 1;
	if(waserror()){
		ctlr->scanactive = 0;
		wakeup(&ctlr->scanr);
		nexterror();
	}
	wlscanstart(ctlr);
	poperror();
#else
	ctlr->scansecs = secs;
#endif
}

static void
callevhndlr(Ctlr* ctlr, ether_event_type_t type, const ether_event_params_t *params)
{
	if(ctlr->evhndlr != 0){
#if defined(BMC64_WLAN_TRACE) || defined(BMC64_WLAN_LOW_IMPACT_TRACE)
		eventcallbackdepth++;
#endif
		(*ctlr->evhndlr)(type, params, ctlr->evcontext);
#if defined(BMC64_WLAN_TRACE) || defined(BMC64_WLAN_LOW_IMPACT_TRACE)
		eventcallbackdepth--;
#endif
	}
}

static void
etherbcmsetevhndlr(struct Ether *edev, ether_event_handler_t *hndlr, void *context)
{
	Ctlr* ctlr;

	ctlr = edev->ctlr;
	ctlr->evcontext = context;
	ctlr->evhndlr = hndlr;
}

static void
etherbcmattach(Ether* edev)
{
	Ctlr *ctlr;

	ctlr = edev->ctlr;
	adapterlock(ctlr, "attach");
	if(waserror()){
		//print("ether4330: attach failed: %s\n", up->errstr);
		adapterunlock(ctlr);
		nexterror();
	}
	if(ctlr->edev == nil){
		if(ctlr->chipid == 0){
			sdioinit();
			sbinit(ctlr);
		}
		fwload(ctlr);
		sbenable(ctlr);
		kproc("wifireader", rproc, edev);
		kproc("wifitimer", lproc, edev);
		if(ctlr->regufile)
			reguload(ctlr, ctlr->regufile);
		wlinit(edev, ctlr);
		ctlr->edev = edev;
	}
	adapterunlock(ctlr);
	poperror();
}

static void
ethersetmulticast(Ether *edev, void *buf, long n)
{
	Ctlr *ctlr;

	ctlr = edev->ctlr;
	wlsetvar(ctlr, "mcast_list", buf, n);
	wlsetint(ctlr, "allmulti", 0);
}


static void
etherbcmshutdown(Ether*edev)
{
	Ctlr *ctlr;

	ctlr = edev->ctlr;
	adapterlock(ctlr, "shutdown");
	wlcmdint(ctlr, 3, 0);		/* DOWN */
	adapterunlock(ctlr);

	sdioreset();
}


static int
etherbcmpnp(Ether* edev)
{
	Ctlr *ctlr;

	ctlr = malloc(sizeof(Ctlr));
	memset(ctlr, 0, sizeof(Ctlr));
	ctlr->chanid = Wifichan;
	edev->ctlr = ctlr;
	edev->attach = etherbcmattach;
	edev->transmit = etherbcmtransmit;
	edev->ifstat = etherbcmifstat;
	edev->ctl = etherbcmctl;
	edev->getbssid = etherbcmgetbssid;
	edev->scanbs = etherbcmscan;
	edev->setevhndlr = etherbcmsetevhndlr;
	edev->setmulticast = ethersetmulticast;
	edev->shutdown = etherbcmshutdown;
	edev->arg = edev;

	return 0;
}

void
ether4330link(void)
{
	addethercard("4330", etherbcmpnp);
}
