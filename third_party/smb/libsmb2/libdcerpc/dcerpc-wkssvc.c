
/*
   Copyright (C) 2026 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef STDC_HEADERS
#include <stddef.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_UNISTD_H
#include <sys/unistd.h>
#endif

#include <errno.h>
#include <stdio.h>

#include "compat.h"

#include "smb2.h"
#include "libsmb2.h"
#include <dcerpc/dcerpc.h>
#include <dcerpc/dcerpc-srvsvc.h>
#include <dcerpc/dcerpc-wkssvc.h>
#include "libsmb2-raw.h"
#include "libsmb2-private.h"


#define WKSSVC_UUID    0x6bffd098, 0xa112, 0x3610, {0x98, 0x33, 0x46, 0xc3, 0xf8, 0x7e, 0x34, 0x5a}

p_syntax_id_t wkssvc_interface = {
        {WKSSVC_UUID}, 1, 0
};


static struct dcerpc_uint32_pretty_printer platform_id_pp = {
        .fmt = "%u",
        .bitfields = {
                { "PLATFORM_ID_DOS", 0xffffffff, SRVSVC_PLATFORM_ID_DOS },
                { "PLATFORM_ID_OS2", 0xffffffff, SRVSVC_PLATFORM_ID_OS2 },
                { "PLATFORM_ID_NT",  0xffffffff, SRVSVC_PLATFORM_ID_NT },
                { "PLATFORM_ID_OSF", 0xffffffff, SRVSVC_PLATFORM_ID_OSF },
                { "PLATFORM_ID_VMS", 0xffffffff, SRVSVC_PLATFORM_ID_VMS },
                { NULL, 0, 0},
        },
};


static struct dcerpc_uint32_pretty_printer use_status_pp = {
        .fmt = "%u",
        .bitfields = {
                { "USE_OK",       0xffffffff, WKSSVC_USE_OK },
                { "USE_PAUSED",   0xffffffff, WKSSVC_USE_PAUSED },
                { "USE_SESSLOST", 0xffffffff, WKSSVC_USE_SESSLOST },
                { "USE_NETERR",   0xffffffff, WKSSVC_USE_NETERR },
                { "USE_CONN",     0xffffffff, WKSSVC_USE_CONN },
                { "USE_RECONN",   0xffffffff, WKSSVC_USE_RECONN },
                { NULL, 0, 0},
        },
};


static struct dcerpc_uint32_pretty_printer use_asg_type_pp = {
        .fmt = "0x%08x",
        .bitfields = {
                { "USE_DISKDEV",  0xffffffff, WKSSVC_USE_DISKDEV },
                { "USE_SPOOLDEV", 0xffffffff, WKSSVC_USE_SPOOLDEV },
                { "USE_CHARDEV",  0xffffffff, WKSSVC_USE_CHARDEV },
                { "USE_IPC",      0xffffffff, WKSSVC_USE_IPC },
                { "USE_WILDCARD", 0xffffffff, WKSSVC_USE_WILDCARD },
                { NULL, 0, 0},
        },
};














int
wkssvc_WKSTA_INFO_100_coder(char *name, struct dcerpc_context *dce,
                            struct dcerpc_pdu *pdu,
                            struct dcerpc_iovec *iov, int *offset,
                            void *ptr)
{
        struct wkssvc_WKSTA_INFO_100 *wi = ptr;

        if (dcerpc_uint32_coder_pp("Platform_Id", dce, pdu, iov, offset,
                                   &wi->platform_id, &platform_id_pp)) {
                return -1;
        }
        if (dcerpc_ptr_coder("ComputerName", dce, pdu, iov, offset, &wi->computername,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("LanGroup", dce, pdu, iov, offset, &wi->langroup,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Version_Major", dce, pdu, iov, offset, &wi->ver_major)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Version_Minor", dce, pdu, iov, offset, &wi->ver_minor)) {
                return -1;
        }
        return 0;
}

int
wkssvc_WKSTA_INFO_100_STRUCT_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_WKSTA_INFO_100_coder);
}











int
wkssvc_WKSTA_INFO_101_coder(char *name, struct dcerpc_context *dce,
                            struct dcerpc_pdu *pdu,
                            struct dcerpc_iovec *iov, int *offset,
                            void *ptr)
{
        struct wkssvc_WKSTA_INFO_101 *wi = ptr;

        if (dcerpc_uint32_coder_pp("Platform_Id", dce, pdu, iov, offset,
                                   &wi->platform_id, &platform_id_pp)) {
                return -1;
        }
        if (dcerpc_ptr_coder("ComputerName", dce, pdu, iov, offset, &wi->computername,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("LanGroup", dce, pdu, iov, offset, &wi->langroup,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Version_Major", dce, pdu, iov, offset, &wi->ver_major)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Version_Minor", dce, pdu, iov, offset, &wi->ver_minor)) {
                return -1;
        }
        if (dcerpc_ptr_coder("LanRoot", dce, pdu, iov, offset, &wi->lanroot,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        return 0;
}

int
wkssvc_WKSTA_INFO_101_STRUCT_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_WKSTA_INFO_101_coder);
}












int
wkssvc_WKSTA_INFO_102_coder(char *name, struct dcerpc_context *dce,
                            struct dcerpc_pdu *pdu,
                            struct dcerpc_iovec *iov, int *offset,
                            void *ptr)
{
        struct wkssvc_WKSTA_INFO_102 *wi = ptr;

        if (dcerpc_uint32_coder_pp("Platform_Id", dce, pdu, iov, offset,
                                   &wi->platform_id, &platform_id_pp)) {
                return -1;
        }
        if (dcerpc_ptr_coder("ComputerName", dce, pdu, iov, offset, &wi->computername,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("LanGroup", dce, pdu, iov, offset, &wi->langroup,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Version_Major", dce, pdu, iov, offset, &wi->ver_major)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Version_Minor", dce, pdu, iov, offset, &wi->ver_minor)) {
                return -1;
        }
        if (dcerpc_ptr_coder("LanRoot", dce, pdu, iov, offset, &wi->lanroot,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("LoggedOnUsers", dce, pdu, iov, offset, &wi->logged_on_users)) {
                return -1;
        }
        return 0;
}

int
wkssvc_WKSTA_INFO_102_STRUCT_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_WKSTA_INFO_102_coder);
}








































int
wkssvc_WKSTA_INFO_502_coder(char *name, struct dcerpc_context *dce,
                            struct dcerpc_pdu *pdu,
                            struct dcerpc_iovec *iov, int *offset,
                            void *ptr)
{
        struct wkssvc_WKSTA_INFO_502 *wi = ptr;

        if (dcerpc_uint32_coder("CharWait", dce, pdu, iov, offset, &wi->char_wait)) {
                return -1;
        }
        if (dcerpc_uint32_coder("CollectionTime", dce, pdu, iov, offset, &wi->collection_time)) {
                return -1;
        }
        if (dcerpc_uint32_coder("MaximumCollectionCount", dce, pdu, iov, offset, &wi->maximum_collection_count)) {
                return -1;
        }
        if (dcerpc_uint32_coder("KeepConn", dce, pdu, iov, offset, &wi->keep_conn)) {
                return -1;
        }
        if (dcerpc_uint32_coder("MaxCmds", dce, pdu, iov, offset, &wi->max_cmds)) {
                return -1;
        }
        if (dcerpc_uint32_coder("SessTimeout", dce, pdu, iov, offset, &wi->sess_timeout)) {
                return -1;
        }
        if (dcerpc_uint32_coder("SizCharBuf", dce, pdu, iov, offset, &wi->siz_char_buf)) {
                return -1;
        }
        if (dcerpc_uint32_coder("MaxThreads", dce, pdu, iov, offset, &wi->max_threads)) {
                return -1;
        }
        if (dcerpc_uint32_coder("LockQuota", dce, pdu, iov, offset, &wi->lock_quota)) {
                return -1;
        }
        if (dcerpc_uint32_coder("LockIncrement", dce, pdu, iov, offset, &wi->lock_increment)) {
                return -1;
        }
        if (dcerpc_uint32_coder("LockMaximum", dce, pdu, iov, offset, &wi->lock_maximum)) {
                return -1;
        }
        if (dcerpc_uint32_coder("PipeIncrement", dce, pdu, iov, offset, &wi->pipe_increment)) {
                return -1;
        }
        if (dcerpc_uint32_coder("PipeMaximum", dce, pdu, iov, offset, &wi->pipe_maximum)) {
                return -1;
        }
        if (dcerpc_uint32_coder("CacheFileTimeout", dce, pdu, iov, offset, &wi->cache_file_timeout)) {
                return -1;
        }
        if (dcerpc_uint32_coder("DormantFileLimit", dce, pdu, iov, offset, &wi->dormant_file_limit)) {
                return -1;
        }
        if (dcerpc_uint32_coder("ReadAheadThroughput", dce, pdu, iov, offset, &wi->read_ahead_throughput)) {
                return -1;
        }
        if (dcerpc_uint32_coder("NumMailslotBuffers", dce, pdu, iov, offset, &wi->num_mailslot_buffers)) {
                return -1;
        }
        if (dcerpc_uint32_coder("NumSrvAnnounceBuffers", dce, pdu, iov, offset, &wi->num_srv_announce_buffers)) {
                return -1;
        }
        if (dcerpc_uint32_coder("MaxIllegalDatagramEvents", dce, pdu, iov, offset, &wi->max_illegal_datagram_events)) {
                return -1;
        }
        if (dcerpc_uint32_coder("IllegalDatagramEventResetFrequency", dce, pdu, iov, offset, &wi->illegal_datagram_event_reset_frequency)) {
                return -1;
        }
        if (dcerpc_uint32_coder("LogElectionPackets", dce, pdu, iov, offset, &wi->log_election_packets)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseOpportunisticLocking", dce, pdu, iov, offset, &wi->use_opportunistic_locking)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseUnlockBehind", dce, pdu, iov, offset, &wi->use_unlock_behind)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseCloseBehind", dce, pdu, iov, offset, &wi->use_close_behind)) {
                return -1;
        }
        if (dcerpc_uint32_coder("BufNamedPipes", dce, pdu, iov, offset, &wi->buf_named_pipes)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseLockReadUnlock", dce, pdu, iov, offset, &wi->use_lock_read_unlock)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UtilizeNtCaching", dce, pdu, iov, offset, &wi->utilize_nt_caching)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseRawRead", dce, pdu, iov, offset, &wi->use_raw_read)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseRawWrite", dce, pdu, iov, offset, &wi->use_raw_write)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseWriteRawData", dce, pdu, iov, offset, &wi->use_write_raw_data)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseEncryption", dce, pdu, iov, offset, &wi->use_encryption)) {
                return -1;
        }
        if (dcerpc_uint32_coder("BufFilesDenyWrite", dce, pdu, iov, offset, &wi->buf_files_deny_write)) {
                return -1;
        }
        if (dcerpc_uint32_coder("BufReadOnlyFiles", dce, pdu, iov, offset, &wi->buf_read_only_files)) {
                return -1;
        }
        if (dcerpc_uint32_coder("ForceCoreCreateMode", dce, pdu, iov, offset, &wi->force_core_create_mode)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Use512ByteMaxTransfer", dce, pdu, iov, offset, &wi->use_512_byte_max_transfer)) {
                return -1;
        }
        return 0;
}

int
wkssvc_WKSTA_INFO_502_STRUCT_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_WKSTA_INFO_502_coder);
}









static int
wkssvc_WKSTA_INFO_coder(char *name, struct dcerpc_context *dce,
                        struct dcerpc_pdu *pdu,
                        struct dcerpc_iovec *iov, int *offset,
                        void *ptr)
{
        union wkssvc_WKSTA_INFO *info = ptr;

        switch (dcerpc_get_switch_is(pdu)) {
        case 100:
                if (dcerpc_ptr_coder("WkstaInfo100", dce, pdu, iov, offset, &info->WkstaInfo100,
                                     PTR_UNIQUE, wkssvc_WKSTA_INFO_100_STRUCT_coder)) {
                        return -1;
                }
                break;
        case 101:
                if (dcerpc_ptr_coder("WkstaInfo101", dce, pdu, iov, offset, &info->WkstaInfo101,
                                     PTR_UNIQUE, wkssvc_WKSTA_INFO_101_STRUCT_coder)) {
                        return -1;
                }
                break;
        case 102:
                if (dcerpc_ptr_coder("WkstaInfo102", dce, pdu, iov, offset, &info->WkstaInfo102,
                                     PTR_UNIQUE, wkssvc_WKSTA_INFO_102_STRUCT_coder)) {
                        return -1;
                }
                break;
        case 502:
                if (dcerpc_ptr_coder("WkstaInfo502", dce, pdu, iov, offset, &info->WkstaInfo502,
                                     PTR_UNIQUE, wkssvc_WKSTA_INFO_502_STRUCT_coder)) {
                        return -1;
                }
                break;
        default:
                if (dcerpc_get_cr(pdu)) {
                        return 0;
                }
                return -1;
        };

        return 0;
}

static int
wkssvc_WKSTA_INFO_STRUCT_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        uint32_t Level = dcerpc_get_switch_is(pdu);

        if (dcerpc_union_coder("WkstaInfo", dce, pdu, iov, offset,
                               &Level, ptr,
                               wkssvc_WKSTA_INFO_coder)) {
                return -1;
        }
        return 0;
}









int
wkssvc_NetrWkstaGetInfo_req_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct wkssvc_NetrWkstaGetInfo_req *req = ptr;

        if (dcerpc_ptr_coder("ServerName", dce, pdu, iov, offset, &req->ServerName,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Level", dce, pdu, iov, offset, &req->Level)) {
                return -1;
        }
        dcerpc_set_switch_is(pdu, req->Level);

        return 0;
}

int
wkssvc_NetrWkstaGetInfo_rep_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct wkssvc_NetrWkstaGetInfo_rep *rep = ptr;

        struct wkssvc_NetrWkstaGetInfo_req *req = dcerpc_get_request(pdu);

        dcerpc_set_switch_is(pdu, req->Level);

        if (dcerpc_ptr_coder("WkstaInfo", dce, pdu, iov, offset, &rep->WkstaInfo,
                             PTR_REF, wkssvc_WKSTA_INFO_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}










int
wkssvc_NetrWkstaSetInfo_req_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct wkssvc_NetrWkstaSetInfo_req *req = ptr;

        if (dcerpc_ptr_coder("ServerName", dce, pdu, iov, offset, &req->ServerName,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Level", dce, pdu, iov, offset, &req->Level)) {
                return -1;
        }
        dcerpc_set_switch_is(pdu, req->Level);

        if (dcerpc_ptr_coder("WkstaInfo", dce, pdu, iov, offset, &req->WkstaInfo,
                             PTR_REF, wkssvc_WKSTA_INFO_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("ErrorParameter", dce, pdu, iov, offset, &req->ErrorParameter,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }

        return 0;
}

int
wkssvc_NetrWkstaSetInfo_rep_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct wkssvc_NetrWkstaSetInfo_rep *rep = ptr;

        if (dcerpc_ptr_coder("ErrorParameter", dce, pdu, iov, offset, &rep->ErrorParameter,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}






int
wkssvc_WKSTA_USER_INFO_0_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        struct wkssvc_WKSTA_USER_INFO_0 *ui = ptr;

        if (dcerpc_ptr_coder("UserName", dce, pdu, iov, offset, &ui->username,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        return 0;
}

int
wkssvc_WKSTA_USER_INFO_0_STRUCT_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_WKSTA_USER_INFO_0_coder);
}

static int
wkssvc_WKSTA_USER_INFO_0_carray_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr)
{
        return dcerpc_carray_coder("UserInfo0", dce, pdu, iov, offset,
                                   dcerpc_get_size_is(pdu), ptr,
                                   sizeof(struct wkssvc_WKSTA_USER_INFO_0),
                                   wkssvc_WKSTA_USER_INFO_0_STRUCT_coder);
}







int
wkssvc_WKSTA_USER_INFO_0_CONTAINER_coder(char *name, struct dcerpc_context *dce,
                                         struct dcerpc_pdu *pdu,
                                         struct dcerpc_iovec *iov, int *offset,
                                         void *ptr)
{
        struct wkssvc_WKSTA_USER_INFO_0_CONTAINER *ctr = ptr;

        if (dcerpc_uint32_coder("EntriesRead", dce, pdu, iov, offset, &ctr->EntriesRead)) {
                return -1;
        }
        dcerpc_set_size_is(pdu, ctr->EntriesRead);
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE && ctr->EntriesRead) {
                if (ctr->Buffer == NULL) {
                        size_t esize = sizeof(struct wkssvc_WKSTA_USER_INFO_0);

                        if (ctr->EntriesRead > SIZE_MAX / esize) {
                                return -1;
                        }
                        ctr->Buffer = dcerpc_alloc_data(pdu,
                                (size_t)ctr->EntriesRead * esize);
                        if (ctr->Buffer == NULL) {
                                return -1;
                        }
                }
        }
        if (dcerpc_ptr_coder("UserInfo0", dce, pdu, iov, offset, ctr->Buffer,
                             PTR_UNIQUE, wkssvc_WKSTA_USER_INFO_0_carray_coder)) {
                return -1;
        }

        return 0;
}









int
wkssvc_WKSTA_USER_INFO_1_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        struct wkssvc_WKSTA_USER_INFO_1 *ui = ptr;

        if (dcerpc_ptr_coder("UserName", dce, pdu, iov, offset, &ui->username,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("LogonDomain", dce, pdu, iov, offset, &ui->logon_domain,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("OthDomains", dce, pdu, iov, offset, &ui->oth_domains,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("LogonServer", dce, pdu, iov, offset, &ui->logon_server,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        return 0;
}

int
wkssvc_WKSTA_USER_INFO_1_STRUCT_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_WKSTA_USER_INFO_1_coder);
}

static int
wkssvc_WKSTA_USER_INFO_1_carray_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr)
{
        return dcerpc_carray_coder("UserInfo1", dce, pdu, iov, offset,
                                   dcerpc_get_size_is(pdu), ptr,
                                   sizeof(struct wkssvc_WKSTA_USER_INFO_1),
                                   wkssvc_WKSTA_USER_INFO_1_STRUCT_coder);
}







int
wkssvc_WKSTA_USER_INFO_1_CONTAINER_coder(char *name, struct dcerpc_context *dce,
                                         struct dcerpc_pdu *pdu,
                                         struct dcerpc_iovec *iov, int *offset,
                                         void *ptr)
{
        struct wkssvc_WKSTA_USER_INFO_1_CONTAINER *ctr = ptr;

        if (dcerpc_uint32_coder("EntriesRead", dce, pdu, iov, offset, &ctr->EntriesRead)) {
                return -1;
        }
        dcerpc_set_size_is(pdu, ctr->EntriesRead);
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE && ctr->EntriesRead) {
                if (ctr->Buffer == NULL) {
                        size_t esize = sizeof(struct wkssvc_WKSTA_USER_INFO_1);

                        if (ctr->EntriesRead > SIZE_MAX / esize) {
                                return -1;
                        }
                        ctr->Buffer = dcerpc_alloc_data(pdu,
                                (size_t)ctr->EntriesRead * esize);
                        if (ctr->Buffer == NULL) {
                                return -1;
                        }
                }
        }
        if (dcerpc_ptr_coder("UserInfo1", dce, pdu, iov, offset, ctr->Buffer,
                             PTR_UNIQUE, wkssvc_WKSTA_USER_INFO_1_carray_coder)) {
                return -1;
        }

        return 0;
}







static int
wkssvc_WKSTA_USER_ENUM_UNION_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        union wkssvc_WKSTA_USER_ENUM_UNION *info = ptr;

        switch (dcerpc_get_switch_is(pdu)) {
        case 0:
                if (dcerpc_ptr_coder("UserInfo0Container", dce, pdu, iov, offset, &info->Level0,
                                     PTR_UNIQUE, wkssvc_WKSTA_USER_INFO_0_CONTAINER_coder)) {
                        return -1;
                }
                break;
        case 1:
                if (dcerpc_ptr_coder("UserInfo1Container", dce, pdu, iov, offset, &info->Level1,
                                     PTR_UNIQUE, wkssvc_WKSTA_USER_INFO_1_CONTAINER_coder)) {
                        return -1;
                }
                break;
        default:
                if (dcerpc_get_cr(pdu)) {
                        return 0;
                }
                return -1;
        };

        return 0;
}







int
wkssvc_WKSTA_USER_ENUM_STRUCT_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr)
{
        struct wkssvc_WKSTA_USER_ENUM_STRUCT *ues = ptr;

        if (dcerpc_uint32_coder("Level", dce, pdu, iov, offset, &ues->Level)) {
                return -1;
        }

        if (dcerpc_union_coder("WkstaUserInfo", dce, pdu, iov, offset,
                               &ues->Level, &ues->WkstaUserInfo,
                               wkssvc_WKSTA_USER_ENUM_UNION_coder)) {
                return -1;
        }

        return 0;
}

int
wkssvc_WKSTA_USER_ENUM_STRUCT_struct_coder(char *name, struct dcerpc_context *dce,
                                           struct dcerpc_pdu *pdu,
                                           struct dcerpc_iovec *iov, int *offset,
                                           void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_WKSTA_USER_ENUM_STRUCT_coder);
}











int
wkssvc_NetrWkstaUserEnum_req_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        struct wkssvc_NetrWkstaUserEnum_req *req = ptr;

        if (dcerpc_ptr_coder("ServerName", dce, pdu, iov, offset, &req->ServerName,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("UserInfo", dce, pdu, iov, offset, &req->UserInfo,
                             PTR_REF, wkssvc_WKSTA_USER_ENUM_STRUCT_struct_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("PreferredMaximumLength", dce, pdu, iov, offset,
                             &req->PreferredMaximumLength,
                             PTR_REF, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("ResumeHandle", dce, pdu, iov, offset, &req->ResumeHandle,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }

        return 0;
}

int
wkssvc_NetrWkstaUserEnum_rep_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        struct wkssvc_NetrWkstaUserEnum_rep *rep = ptr;

        if (dcerpc_ptr_coder("UserInfo", dce, pdu, iov, offset, &rep->UserInfo,
                             PTR_REF, wkssvc_WKSTA_USER_ENUM_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("TotalEntries", dce, pdu, iov, offset, &rep->total_entries,
                             PTR_REF, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("ResumeHandle", dce, pdu, iov, offset, &rep->resume_handle,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}







int
wkssvc_USE_INFO_0_coder(char *name, struct dcerpc_context *dce,
                        struct dcerpc_pdu *pdu,
                        struct dcerpc_iovec *iov, int *offset,
                        void *ptr)
{
        struct wkssvc_USE_INFO_0 *ui = ptr;

        if (dcerpc_ptr_coder("Local", dce, pdu, iov, offset, &ui->local,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("Remote", dce, pdu, iov, offset, &ui->remote,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        return 0;
}

int
wkssvc_USE_INFO_0_STRUCT_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_USE_INFO_0_coder);
}

static int
wkssvc_USE_INFO_0_carray_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        return dcerpc_carray_coder("UseInfo0", dce, pdu, iov, offset,
                                   dcerpc_get_size_is(pdu), ptr,
                                   sizeof(struct wkssvc_USE_INFO_0),
                                   wkssvc_USE_INFO_0_STRUCT_coder);
}







int
wkssvc_USE_INFO_0_CONTAINER_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct wkssvc_USE_INFO_0_CONTAINER *ctr = ptr;

        if (dcerpc_uint32_coder("EntriesRead", dce, pdu, iov, offset, &ctr->EntriesRead)) {
                return -1;
        }
        dcerpc_set_size_is(pdu, ctr->EntriesRead);
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE && ctr->EntriesRead) {
                if (ctr->Buffer == NULL) {
                        size_t esize = sizeof(struct wkssvc_USE_INFO_0);

                        if (ctr->EntriesRead > SIZE_MAX / esize) {
                                return -1;
                        }
                        ctr->Buffer = dcerpc_alloc_data(pdu,
                                (size_t)ctr->EntriesRead * esize);
                        if (ctr->Buffer == NULL) {
                                return -1;
                        }
                }
        }
        if (dcerpc_ptr_coder("UseInfo0", dce, pdu, iov, offset, ctr->Buffer,
                             PTR_UNIQUE, wkssvc_USE_INFO_0_carray_coder)) {
                return -1;
        }

        return 0;
}












int
wkssvc_USE_INFO_1_coder(char *name, struct dcerpc_context *dce,
                        struct dcerpc_pdu *pdu,
                        struct dcerpc_iovec *iov, int *offset,
                        void *ptr)
{
        struct wkssvc_USE_INFO_1 *ui = ptr;

        if (dcerpc_ptr_coder("Local", dce, pdu, iov, offset, &ui->local,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("Remote", dce, pdu, iov, offset, &ui->remote,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("Password", dce, pdu, iov, offset, &ui->password,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("Status", dce, pdu, iov, offset, &ui->status,
                                   &use_status_pp)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("AsgType", dce, pdu, iov, offset, &ui->asg_type,
                                   &use_asg_type_pp)) {
                return -1;
        }
        if (dcerpc_uint32_coder("RefCount", dce, pdu, iov, offset, &ui->refcount)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseCount", dce, pdu, iov, offset, &ui->usecount)) {
                return -1;
        }
        return 0;
}

int
wkssvc_USE_INFO_1_STRUCT_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_USE_INFO_1_coder);
}

static int
wkssvc_USE_INFO_1_carray_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        return dcerpc_carray_coder("UseInfo1", dce, pdu, iov, offset,
                                   dcerpc_get_size_is(pdu), ptr,
                                   sizeof(struct wkssvc_USE_INFO_1),
                                   wkssvc_USE_INFO_1_STRUCT_coder);
}







int
wkssvc_USE_INFO_1_CONTAINER_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct wkssvc_USE_INFO_1_CONTAINER *ctr = ptr;

        if (dcerpc_uint32_coder("EntriesRead", dce, pdu, iov, offset, &ctr->EntriesRead)) {
                return -1;
        }
        dcerpc_set_size_is(pdu, ctr->EntriesRead);
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE && ctr->EntriesRead) {
                if (ctr->Buffer == NULL) {
                        size_t esize = sizeof(struct wkssvc_USE_INFO_1);

                        if (ctr->EntriesRead > SIZE_MAX / esize) {
                                return -1;
                        }
                        ctr->Buffer = dcerpc_alloc_data(pdu,
                                (size_t)ctr->EntriesRead * esize);
                        if (ctr->Buffer == NULL) {
                                return -1;
                        }
                }
        }
        if (dcerpc_ptr_coder("UseInfo1", dce, pdu, iov, offset, ctr->Buffer,
                             PTR_UNIQUE, wkssvc_USE_INFO_1_carray_coder)) {
                return -1;
        }

        return 0;
}














int
wkssvc_USE_INFO_2_coder(char *name, struct dcerpc_context *dce,
                        struct dcerpc_pdu *pdu,
                        struct dcerpc_iovec *iov, int *offset,
                        void *ptr)
{
        struct wkssvc_USE_INFO_2 *ui = ptr;

        if (dcerpc_ptr_coder("Local", dce, pdu, iov, offset, &ui->local,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("Remote", dce, pdu, iov, offset, &ui->remote,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("Password", dce, pdu, iov, offset, &ui->password,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("Status", dce, pdu, iov, offset, &ui->status,
                                   &use_status_pp)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("AsgType", dce, pdu, iov, offset, &ui->asg_type,
                                   &use_asg_type_pp)) {
                return -1;
        }
        if (dcerpc_uint32_coder("RefCount", dce, pdu, iov, offset, &ui->refcount)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseCount", dce, pdu, iov, offset, &ui->usecount)) {
                return -1;
        }
        if (dcerpc_ptr_coder("UserName", dce, pdu, iov, offset, &ui->username,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("DomainName", dce, pdu, iov, offset, &ui->domainname,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        return 0;
}

int
wkssvc_USE_INFO_2_STRUCT_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_USE_INFO_2_coder);
}

static int
wkssvc_USE_INFO_2_carray_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr)
{
        return dcerpc_carray_coder("UseInfo2", dce, pdu, iov, offset,
                                   dcerpc_get_size_is(pdu), ptr,
                                   sizeof(struct wkssvc_USE_INFO_2),
                                   wkssvc_USE_INFO_2_STRUCT_coder);
}







int
wkssvc_USE_INFO_2_CONTAINER_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct wkssvc_USE_INFO_2_CONTAINER *ctr = ptr;

        if (dcerpc_uint32_coder("EntriesRead", dce, pdu, iov, offset, &ctr->EntriesRead)) {
                return -1;
        }
        dcerpc_set_size_is(pdu, ctr->EntriesRead);
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE && ctr->EntriesRead) {
                if (ctr->Buffer == NULL) {
                        size_t esize = sizeof(struct wkssvc_USE_INFO_2);

                        if (ctr->EntriesRead > SIZE_MAX / esize) {
                                return -1;
                        }
                        ctr->Buffer = dcerpc_alloc_data(pdu,
                                (size_t)ctr->EntriesRead * esize);
                        if (ctr->Buffer == NULL) {
                                return -1;
                        }
                }
        }
        if (dcerpc_ptr_coder("UseInfo2", dce, pdu, iov, offset, ctr->Buffer,
                             PTR_UNIQUE, wkssvc_USE_INFO_2_carray_coder)) {
                return -1;
        }

        return 0;
}








static int
wkssvc_USE_ENUM_UNION_coder(char *name, struct dcerpc_context *dce,
                            struct dcerpc_pdu *pdu,
                            struct dcerpc_iovec *iov, int *offset,
                            void *ptr)
{
        union wkssvc_USE_ENUM_UNION *info = ptr;

        switch (dcerpc_get_switch_is(pdu)) {
        case 0:
                if (dcerpc_ptr_coder("UseInfo0Container", dce, pdu, iov, offset, &info->Level0,
                                     PTR_UNIQUE, wkssvc_USE_INFO_0_CONTAINER_coder)) {
                        return -1;
                }
                break;
        case 1:
                if (dcerpc_ptr_coder("UseInfo1Container", dce, pdu, iov, offset, &info->Level1,
                                     PTR_UNIQUE, wkssvc_USE_INFO_1_CONTAINER_coder)) {
                        return -1;
                }
                break;
        case 2:
                if (dcerpc_ptr_coder("UseInfo2Container", dce, pdu, iov, offset, &info->Level2,
                                     PTR_UNIQUE, wkssvc_USE_INFO_2_CONTAINER_coder)) {
                        return -1;
                }
                break;
        default:
                if (dcerpc_get_cr(pdu)) {
                        return 0;
                }
                return -1;
        };

        return 0;
}







int
wkssvc_USE_ENUM_STRUCT_coder(char *name, struct dcerpc_context *dce,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr)
{
        struct wkssvc_USE_ENUM_STRUCT *ues = ptr;

        if (dcerpc_uint32_coder("Level", dce, pdu, iov, offset, &ues->Level)) {
                return -1;
        }

        if (dcerpc_union_coder("UseInfo", dce, pdu, iov, offset,
                               &ues->Level, &ues->UseInfo,
                               wkssvc_USE_ENUM_UNION_coder)) {
                return -1;
        }

        return 0;
}

int
wkssvc_USE_ENUM_STRUCT_struct_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_USE_ENUM_STRUCT_coder);
}











int
wkssvc_NetrUseEnum_req_coder(char *name, struct dcerpc_context *dce,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr)
{
        struct wkssvc_NetrUseEnum_req *req = ptr;

        if (dcerpc_ptr_coder("ServerName", dce, pdu, iov, offset, &req->ServerName,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("InfoStruct", dce, pdu, iov, offset, &req->InfoStruct,
                             PTR_REF, wkssvc_USE_ENUM_STRUCT_struct_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("PreferedMaximumLength", dce, pdu, iov, offset,
                             &req->PreferedMaximumLength,
                             PTR_REF, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("ResumeHandle", dce, pdu, iov, offset, &req->ResumeHandle,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }

        return 0;
}

int
wkssvc_NetrUseEnum_rep_coder(char *name, struct dcerpc_context *dce,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr)
{
        struct wkssvc_NetrUseEnum_rep *rep = ptr;

        if (dcerpc_ptr_coder("InfoStruct", dce, pdu, iov, offset, &rep->InfoStruct,
                             PTR_REF, wkssvc_USE_ENUM_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("TotalEntries", dce, pdu, iov, offset, &rep->total_entries,
                             PTR_REF, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("ResumeHandle", dce, pdu, iov, offset, &rep->resume_handle,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}




















int
wkssvc_STAT_WORKSTATION_0_coder(char *name, struct dcerpc_context *dce,
                                struct dcerpc_pdu *pdu,
                                struct dcerpc_iovec *iov, int *offset,
                                void *ptr)
{
        struct wkssvc_STAT_WORKSTATION_0 *st = ptr;

        if (dcerpc_uint64_coder("StatisticsStartTime", dce, pdu, iov, offset,
                                &st->StatisticsStartTime)) {
                return -1;
        }
        if (dcerpc_uint64_coder("BytesReceived", dce, pdu, iov, offset,
                                &st->BytesReceived)) {
                return -1;
        }
        if (dcerpc_uint64_coder("SmbsReceived", dce, pdu, iov, offset,
                                &st->SmbsReceived)) {
                return -1;
        }
        if (dcerpc_uint64_coder("PagingReadBytesRequested", dce, pdu, iov, offset,
                                &st->PagingReadBytesRequested)) {
                return -1;
        }
        if (dcerpc_uint64_coder("NonPagingReadBytesRequested", dce, pdu, iov, offset,
                                &st->NonPagingReadBytesRequested)) {
                return -1;
        }
        if (dcerpc_uint64_coder("CacheReadBytesRequested", dce, pdu, iov, offset,
                                &st->CacheReadBytesRequested)) {
                return -1;
        }
        if (dcerpc_uint64_coder("NetworkReadBytesRequested", dce, pdu, iov, offset,
                                &st->NetworkReadBytesRequested)) {
                return -1;
        }
        if (dcerpc_uint64_coder("BytesTransmitted", dce, pdu, iov, offset,
                                &st->BytesTransmitted)) {
                return -1;
        }
        if (dcerpc_uint64_coder("SmbsTransmitted", dce, pdu, iov, offset,
                                &st->SmbsTransmitted)) {
                return -1;
        }
        if (dcerpc_uint64_coder("PagingWriteBytesRequested", dce, pdu, iov, offset,
                                &st->PagingWriteBytesRequested)) {
                return -1;
        }
        if (dcerpc_uint64_coder("NonPagingWriteBytesRequested", dce, pdu, iov, offset,
                                &st->NonPagingWriteBytesRequested)) {
                return -1;
        }
        if (dcerpc_uint64_coder("CacheWriteBytesRequested", dce, pdu, iov, offset,
                                &st->CacheWriteBytesRequested)) {
                return -1;
        }
        if (dcerpc_uint64_coder("NetworkWriteBytesRequested", dce, pdu, iov, offset,
                                &st->NetworkWriteBytesRequested)) {
                return -1;
        }
        if (dcerpc_uint32_coder("InitiallyFailedOperations", dce, pdu, iov, offset,
                                &st->InitiallyFailedOperations)) {
                return -1;
        }
        if (dcerpc_uint32_coder("FailedCompletionOperations", dce, pdu, iov, offset,
                                &st->FailedCompletionOperations)) {
                return -1;
        }
        if (dcerpc_uint32_coder("ReadOperations", dce, pdu, iov, offset,
                                &st->ReadOperations)) {
                return -1;
        }
        if (dcerpc_uint32_coder("RandomReadOperations", dce, pdu, iov, offset,
                                &st->RandomReadOperations)) {
                return -1;
        }
        if (dcerpc_uint32_coder("ReadSmbs", dce, pdu, iov, offset, &st->ReadSmbs)) {
                return -1;
        }
        if (dcerpc_uint32_coder("LargeReadSmbs", dce, pdu, iov, offset,
                                &st->LargeReadSmbs)) {
                return -1;
        }
        if (dcerpc_uint32_coder("SmallReadSmbs", dce, pdu, iov, offset,
                                &st->SmallReadSmbs)) {
                return -1;
        }
        if (dcerpc_uint32_coder("WriteOperations", dce, pdu, iov, offset,
                                &st->WriteOperations)) {
                return -1;
        }
        if (dcerpc_uint32_coder("RandomWriteOperations", dce, pdu, iov, offset,
                                &st->RandomWriteOperations)) {
                return -1;
        }
        if (dcerpc_uint32_coder("WriteSmbs", dce, pdu, iov, offset, &st->WriteSmbs)) {
                return -1;
        }
        if (dcerpc_uint32_coder("LargeWriteSmbs", dce, pdu, iov, offset,
                                &st->LargeWriteSmbs)) {
                return -1;
        }
        if (dcerpc_uint32_coder("SmallWriteSmbs", dce, pdu, iov, offset,
                                &st->SmallWriteSmbs)) {
                return -1;
        }
        if (dcerpc_uint32_coder("RawReadsDenied", dce, pdu, iov, offset,
                                &st->RawReadsDenied)) {
                return -1;
        }
        if (dcerpc_uint32_coder("RawWritesDenied", dce, pdu, iov, offset,
                                &st->RawWritesDenied)) {
                return -1;
        }
        if (dcerpc_uint32_coder("NetworkErrors", dce, pdu, iov, offset,
                                &st->NetworkErrors)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Sessions", dce, pdu, iov, offset, &st->Sessions)) {
                return -1;
        }
        if (dcerpc_uint32_coder("FailedSessions", dce, pdu, iov, offset,
                                &st->FailedSessions)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Reconnects", dce, pdu, iov, offset, &st->Reconnects)) {
                return -1;
        }
        if (dcerpc_uint32_coder("CoreConnects", dce, pdu, iov, offset,
                                &st->CoreConnects)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Lanman20Connects", dce, pdu, iov, offset,
                                &st->Lanman20Connects)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Lanman21Connects", dce, pdu, iov, offset,
                                &st->Lanman21Connects)) {
                return -1;
        }
        if (dcerpc_uint32_coder("LanmanNtConnects", dce, pdu, iov, offset,
                                &st->LanmanNtConnects)) {
                return -1;
        }
        if (dcerpc_uint32_coder("ServerDisconnects", dce, pdu, iov, offset,
                                &st->ServerDisconnects)) {
                return -1;
        }
        if (dcerpc_uint32_coder("HungSessions", dce, pdu, iov, offset,
                                &st->HungSessions)) {
                return -1;
        }
        if (dcerpc_uint32_coder("UseCount", dce, pdu, iov, offset, &st->UseCount)) {
                return -1;
        }
        if (dcerpc_uint32_coder("FailedUseCount", dce, pdu, iov, offset,
                                &st->FailedUseCount)) {
                return -1;
        }
        if (dcerpc_uint32_coder("CurrentCommands", dce, pdu, iov, offset,
                                &st->CurrentCommands)) {
                return -1;
        }
        return 0;
}

int
wkssvc_STAT_WORKSTATION_0_STRUCT_coder(char *name, struct dcerpc_context *dce,
                                       struct dcerpc_pdu *pdu,
                                       struct dcerpc_iovec *iov, int *offset,
                                       void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   wkssvc_STAT_WORKSTATION_0_coder);
}











int
wkssvc_NetrWorkstationStatisticsGet_req_coder(char *name, struct dcerpc_context *dce,
                                              struct dcerpc_pdu *pdu,
                                              struct dcerpc_iovec *iov, int *offset,
                                              void *ptr)
{
        struct wkssvc_NetrWorkstationStatisticsGet_req *req = ptr;
        void *service_ptr = &req->ServiceName;

        if (dcerpc_ptr_coder("ServerName", dce, pdu, iov, offset, &req->ServerName,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }




        if (dcerpc_pdu_direction(pdu) == DCERPC_ENCODE) {
                if (req->ServiceName == NULL) {
                        service_ptr = NULL;
                }
        }
        if (dcerpc_ptr_coder("ServiceName", dce, pdu, iov, offset, service_ptr,
                             PTR_UNIQUE, dcerpc_utf16z_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Level", dce, pdu, iov, offset, &req->Level)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Options", dce, pdu, iov, offset, &req->Options)) {
                return -1;
        }

        return 0;
}

int
wkssvc_NetrWorkstationStatisticsGet_rep_coder(char *name, struct dcerpc_context *dce,
                                              struct dcerpc_pdu *pdu,
                                              struct dcerpc_iovec *iov, int *offset,
                                              void *ptr)
{
        struct wkssvc_NetrWorkstationStatisticsGet_rep *rep = ptr;

        if (dcerpc_ptr_coder("Buffer", dce, pdu, iov, offset, &rep->Buffer,
                             PTR_UNIQUE, wkssvc_STAT_WORKSTATION_0_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}

struct dcerpc_procedure wkssvc_procs[] = {
        {WKSSVC_NETRWKSTAGETINFO, "NetrWkstaGetInfo",
         wkssvc_NetrWkstaGetInfo_req_coder, sizeof(struct wkssvc_NetrWkstaGetInfo_req),
         wkssvc_NetrWkstaGetInfo_rep_coder, sizeof(struct wkssvc_NetrWkstaGetInfo_rep),
        },
        {WKSSVC_NETRWKSTASETINFO, "NetrWkstaSetInfo",
         wkssvc_NetrWkstaSetInfo_req_coder, sizeof(struct wkssvc_NetrWkstaSetInfo_req),
         wkssvc_NetrWkstaSetInfo_rep_coder, sizeof(struct wkssvc_NetrWkstaSetInfo_rep),
        },
        {WKSSVC_NETRWKSTAUSERENUM, "NetrWkstaUserEnum",
         wkssvc_NetrWkstaUserEnum_req_coder, sizeof(struct wkssvc_NetrWkstaUserEnum_req),
         wkssvc_NetrWkstaUserEnum_rep_coder, sizeof(struct wkssvc_NetrWkstaUserEnum_rep),
        },
        {WKSSVC_NETRUSEENUM, "NetrUseEnum",
         wkssvc_NetrUseEnum_req_coder, sizeof(struct wkssvc_NetrUseEnum_req),
         wkssvc_NetrUseEnum_rep_coder, sizeof(struct wkssvc_NetrUseEnum_rep),
        },
        {WKSSVC_NETRWORKSTATIONSTATISTICSGET, "NetrWorkstationStatisticsGet",
         wkssvc_NetrWorkstationStatisticsGet_req_coder,
         sizeof(struct wkssvc_NetrWorkstationStatisticsGet_req),
         wkssvc_NetrWorkstationStatisticsGet_rep_coder,
         sizeof(struct wkssvc_NetrWorkstationStatisticsGet_rep),
        },
        {-1, NULL, NULL, 0, NULL, 0}
};
