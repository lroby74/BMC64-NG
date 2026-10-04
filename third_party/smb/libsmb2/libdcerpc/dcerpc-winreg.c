
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
#include <dcerpc/dcerpc-winreg.h>
#include "libsmb2-raw.h"
#include "libsmb2-private.h"









#define WINREG_UUID    0x338cd001, 0x2244, 0x31f1, {0xaa, 0xaa, 0x90, 0x00, 0x38, 0x00, 0x10, 0x03}

p_syntax_id_t winreg_interface = {
        {WINREG_UUID}, 1, 0
};


static struct dcerpc_uint32_pretty_printer regsam_pp = {
        .fmt = "0x%08x",
        .bitfields = {
                { "KEY_QUERY_VALUE", KEY_QUERY_VALUE, KEY_QUERY_VALUE },
                { "KEY_SET_VALUE", KEY_SET_VALUE, KEY_SET_VALUE },
                { "KEY_CREATE_SUB_KEY", KEY_CREATE_SUB_KEY, KEY_CREATE_SUB_KEY },
                { "KEY_ENUMERATE_SUB_KEYS", KEY_ENUMERATE_SUB_KEYS,
                  KEY_ENUMERATE_SUB_KEYS },
                { "KEY_NOTIFY", KEY_NOTIFY, KEY_NOTIFY },
                { "KEY_CREATE_LINK", KEY_CREATE_LINK, KEY_CREATE_LINK },
                { "KEY_WOW64_64KEY", KEY_WOW64_64KEY, KEY_WOW64_64KEY },
                { "KEY_WOW64_32KEY", KEY_WOW64_32KEY, KEY_WOW64_32KEY },
                { "DELETE", WINREG_DELETE, WINREG_DELETE },
                { "READ_CONTROL", 0x00020000, 0x00020000 },
                { "WRITE_DAC", 0x00040000, 0x00040000 },
                { "WRITE_OWNER", 0x00080000, 0x00080000 },
                { "SYNCHRONIZE", 0x00100000, 0x00100000 },
                { "KEY_READ", KEY_READ, KEY_READ },
                { "KEY_WRITE", KEY_WRITE, KEY_WRITE },
                { "KEY_ALL_ACCESS", KEY_ALL_ACCESS, KEY_ALL_ACCESS },
                { NULL, 0, 0},
        },
};


static struct dcerpc_uint32_pretty_printer reg_option_pp = {
        .fmt = "0x%08x",
        .bitfields = {
                { "REG_OPTION_NON_VOLATILE", 0xffffffff, REG_OPTION_NON_VOLATILE },
                { "REG_OPTION_VOLATILE", REG_OPTION_VOLATILE, REG_OPTION_VOLATILE },
                { "REG_OPTION_CREATE_LINK", REG_OPTION_CREATE_LINK,
                  REG_OPTION_CREATE_LINK },
                { "REG_OPTION_BACKUP_RESTORE", REG_OPTION_BACKUP_RESTORE,
                  REG_OPTION_BACKUP_RESTORE },
                { "REG_OPTION_OPEN_LINK", REG_OPTION_OPEN_LINK,
                  REG_OPTION_OPEN_LINK },
                { NULL, 0, 0},
        },
};


static struct dcerpc_uint32_pretty_printer reg_disposition_pp = {
        .fmt = "0x%08x",
        .bitfields = {
                { "REG_CREATED_NEW_KEY", 0xffffffff, REG_CREATED_NEW_KEY },
                { "REG_OPENED_EXISTING_KEY", 0xffffffff,
                  REG_OPENED_EXISTING_KEY },
                { NULL, 0, 0},
        },
};


static struct dcerpc_uint32_pretty_printer reg_type_pp = {
        .fmt = "%u",
        .bitfields = {
                { "REG_NONE", 0xffffffff, REG_NONE },
                { "REG_SZ", 0xffffffff, REG_SZ },
                { "REG_EXPAND_SZ", 0xffffffff, REG_EXPAND_SZ },
                { "REG_BINARY", 0xffffffff, REG_BINARY },
                { "REG_DWORD", 0xffffffff, REG_DWORD },
                { "REG_DWORD_BIG_ENDIAN", 0xffffffff, REG_DWORD_BIG_ENDIAN },
                { "REG_LINK", 0xffffffff, REG_LINK },
                { "REG_MULTI_SZ", 0xffffffff, REG_MULTI_SZ },
                { "REG_RESOURCE_LIST", 0xffffffff, REG_RESOURCE_LIST },
                { "REG_FULL_RESOURCE_DESCRIPTOR", 0xffffffff,
                  REG_FULL_RESOURCE_DESCRIPTOR },
                { "REG_RESOURCE_REQUIREMENTS_LIST", 0xffffffff,
                  REG_RESOURCE_REQUIREMENTS_LIST },
                { "REG_QWORD", 0xffffffff, REG_QWORD },
                { NULL, 0, 0},
        },
};


static int
reg_type_uint32_coder(char *name, struct dcerpc_context *dce,
                      struct dcerpc_pdu *pdu, struct dcerpc_iovec *iov,
                      int *offset, void *ptr)
{
        return dcerpc_uint32_coder_pp(name, dce, pdu, iov, offset, ptr,
                                      &reg_type_pp);
}

static int
reg_disposition_uint32_coder(char *name, struct dcerpc_context *dce,
                             struct dcerpc_pdu *pdu, struct dcerpc_iovec *iov,
                             int *offset, void *ptr)
{
        return dcerpc_uint32_coder_pp(name, dce, pdu, iov, offset, ptr,
                                      &reg_disposition_pp);
}

static int
winreg_RPC_HKEY_STRUCT_coder(char *name, struct dcerpc_context *dce,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   dcerpc_context_handle_coder);
}









static int
winreg_ServerName_coder(char *name, struct dcerpc_context *dce,
                        struct dcerpc_pdu *pdu,
                        struct dcerpc_iovec *iov, int *offset,
                        void *ptr)
{
        smb2_set_error(dcerpc_get_smb2_context(dce),
                       "winreg ServerName must be NULL");
        return -1;
}






static int
winreg_OpenRootKey_req_coder(char *name, struct dcerpc_context *dce,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr)
{
        struct winreg_OpenRootKey_req *req = ptr;

        if (dcerpc_ptr_coder("ServerName", dce, pdu, iov, offset, req->ServerName,
                             PTR_UNIQUE, winreg_ServerName_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("samDesired", dce, pdu, iov, offset,
                                   &req->samDesired, &regsam_pp)) {
                return -1;
        }
        return 0;
}

static int
winreg_OpenRootKey_rep_coder(char *name, struct dcerpc_context *dce,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr)
{
        struct winreg_OpenRootKey_rep *rep = ptr;

        if (dcerpc_ptr_coder("phKey", dce, pdu, iov, offset, &rep->phKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}








int
winreg_OpenClassesRoot_req_coder(char *name, struct dcerpc_context *dce,
                                 struct dcerpc_pdu *pdu,
                                 struct dcerpc_iovec *iov, int *offset,
                                 void *ptr)
{
        return winreg_OpenRootKey_req_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenClassesRoot_rep_coder(char *name, struct dcerpc_context *dce,
                                 struct dcerpc_pdu *pdu,
                                 struct dcerpc_iovec *iov, int *offset,
                                 void *ptr)
{
        return winreg_OpenRootKey_rep_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenCurrentUser_req_coder(char *name, struct dcerpc_context *dce,
                                 struct dcerpc_pdu *pdu,
                                 struct dcerpc_iovec *iov, int *offset,
                                 void *ptr)
{
        return winreg_OpenRootKey_req_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenCurrentUser_rep_coder(char *name, struct dcerpc_context *dce,
                                 struct dcerpc_pdu *pdu,
                                 struct dcerpc_iovec *iov, int *offset,
                                 void *ptr)
{
        return winreg_OpenRootKey_rep_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenLocalMachine_req_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        return winreg_OpenRootKey_req_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenLocalMachine_rep_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        return winreg_OpenRootKey_rep_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenUsers_req_coder(char *name, struct dcerpc_context *dce,
                           struct dcerpc_pdu *pdu,
                           struct dcerpc_iovec *iov, int *offset,
                           void *ptr)
{
        return winreg_OpenRootKey_req_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenUsers_rep_coder(char *name, struct dcerpc_context *dce,
                           struct dcerpc_pdu *pdu,
                           struct dcerpc_iovec *iov, int *offset,
                           void *ptr)
{
        return winreg_OpenRootKey_rep_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenCurrentConfig_req_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        return winreg_OpenRootKey_req_coder(name, dce, pdu, iov, offset, ptr);
}

int
winreg_OpenCurrentConfig_rep_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr)
{
        return winreg_OpenRootKey_rep_coder(name, dce, pdu, iov, offset, ptr);
}







int
winreg_BaseRegCloseKey_req_coder(char *name, struct dcerpc_context *dce,
                                 struct dcerpc_pdu *pdu,
                                 struct dcerpc_iovec *iov, int *offset,
                                 void *ptr)
{
        struct winreg_BaseRegCloseKey_req *req = ptr;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }

        return 0;
}

int
winreg_BaseRegCloseKey_rep_coder(char *name, struct dcerpc_context *dce,
                                 struct dcerpc_pdu *pdu,
                                 struct dcerpc_iovec *iov, int *offset,
                                 void *ptr)
{
        struct winreg_BaseRegCloseKey_rep *rep = ptr;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &rep->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}

static int
winreg_FILETIME_coder(char *name, struct dcerpc_context *dce,
                      struct dcerpc_pdu *pdu,
                      struct dcerpc_iovec *iov, int *offset,
                      void *ptr)
{
        struct winreg_FILETIME *ft = ptr;

        if (dcerpc_uint32_coder("dwLowDateTime", dce, pdu, iov, offset,
                                &ft->dwLowDateTime)) {
                return -1;
        }
        if (dcerpc_uint32_coder("dwHighDateTime", dce, pdu, iov, offset,
                                &ft->dwHighDateTime)) {
                return -1;
        }
        return 0;
}

static int
winreg_FILETIME_STRUCT_coder(char *name, struct dcerpc_context *dce,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr)
{
        return dcerpc_struct_coder(name, dce, pdu, iov, offset, ptr,
                                   winreg_FILETIME_coder);
}

















int
winreg_BaseRegQueryInfoKey_req_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr)
{
        struct winreg_BaseRegQueryInfoKey_req *req = ptr;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }

        if (dcerpc_ptr_coder("lpClass", dce, pdu, iov, offset, &req->lpClass,
                             PTR_REF, dcerpc_RRP_UNICODE_STRING_coder)) {
                return -1;
        }
        return 0;
}

int
winreg_BaseRegQueryInfoKey_rep_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr)
{
        struct winreg_BaseRegQueryInfoKey_rep *rep = ptr;


        if (dcerpc_ptr_coder("lpClass", dce, pdu, iov, offset, &rep->lpClass,
                             PTR_REF, dcerpc_RRP_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("lpcSubKeys", dce, pdu, iov, offset,
                                &rep->lpcSubKeys)) {
                return -1;
        }
        if (dcerpc_uint32_coder("lpcbMaxSubKeyLen", dce, pdu, iov, offset,
                                &rep->lpcbMaxSubKeyLen)) {
                return -1;
        }
        if (dcerpc_uint32_coder("lpcbMaxClassLen", dce, pdu, iov, offset,
                                &rep->lpcbMaxClassLen)) {
                return -1;
        }
        if (dcerpc_uint32_coder("lpcValues", dce, pdu, iov, offset,
                                &rep->lpcValues)) {
                return -1;
        }
        if (dcerpc_uint32_coder("lpcbMaxValueNameLen", dce, pdu, iov, offset,
                                &rep->lpcbMaxValueNameLen)) {
                return -1;
        }
        if (dcerpc_uint32_coder("lpcbMaxValueLen", dce, pdu, iov, offset,
                                &rep->lpcbMaxValueLen)) {
                return -1;
        }
        if (dcerpc_uint32_coder("lpcbSecurityDescriptor", dce, pdu, iov, offset,
                                &rep->lpcbSecurityDescriptor)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpftLastWriteTime", dce, pdu, iov, offset,
                             &rep->lpftLastWriteTime,
                             PTR_REF, winreg_FILETIME_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}




















#define WINREG_ENUMKEY_NAME_MAX_LENGTH  1024
#define WINREG_ENUMKEY_CLASS_MAX_LENGTH 128

int
winreg_BaseRegEnumKey_req_coder(char *name, struct dcerpc_context *dce,
                                struct dcerpc_pdu *pdu,
                                struct dcerpc_iovec *iov, int *offset,
                                void *ptr)
{
        struct winreg_BaseRegEnumKey_req *req = ptr;
        uint16_t name_ml, class_ml;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("dwIndex", dce, pdu, iov, offset, &req->dwIndex)) {
                return -1;
        }




        name_ml = req->lpName_max_length ?
                req->lpName_max_length : WINREG_ENUMKEY_NAME_MAX_LENGTH;
        dcerpc_set_unicode_max_length(pdu, name_ml);
        if (dcerpc_ptr_coder("lpName", dce, pdu, iov, offset, &req->lpName,
                             PTR_REF, dcerpc_RPC_UNICODE_STRING_coder)) {
                return -1;
        }




        class_ml = req->lpClass_max_length ?
                req->lpClass_max_length : WINREG_ENUMKEY_CLASS_MAX_LENGTH;
        dcerpc_set_unicode_max_length(pdu, class_ml);
        if (dcerpc_ptr_coder("lpClass", dce, pdu, iov, offset, &req->lpClass,
                             PTR_UNIQUE, dcerpc_RPC_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpftLastWriteTime", dce, pdu, iov, offset,
                             req->lpftLastWriteTime,
                             PTR_UNIQUE, winreg_FILETIME_STRUCT_coder)) {
                return -1;
        }
        return 0;
}

int
winreg_BaseRegEnumKey_rep_coder(char *name, struct dcerpc_context *dce,
                                struct dcerpc_pdu *pdu,
                                struct dcerpc_iovec *iov, int *offset,
                                void *ptr)
{
        struct winreg_BaseRegEnumKey_rep *rep = ptr;

        if (dcerpc_ptr_coder("lpName", dce, pdu, iov, offset, &rep->lpName,
                             PTR_REF, dcerpc_RPC_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpClass", dce, pdu, iov, offset, &rep->lpClass,
                             PTR_UNIQUE, dcerpc_RPC_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpftLastWriteTime", dce, pdu, iov, offset,
                             &rep->lpftLastWriteTime,
                             PTR_UNIQUE, winreg_FILETIME_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}
















int
winreg_BaseRegCreateKey_req_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct winreg_BaseRegCreateKey_req *req = ptr;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpSubKey", dce, pdu, iov, offset, &req->lpSubKey,
                             PTR_REF, dcerpc_RRP_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpClass", dce, pdu, iov, offset, &req->lpClass,
                             PTR_REF, dcerpc_RRP_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("dwOptions", dce, pdu, iov, offset,
                                   &req->dwOptions, &reg_option_pp)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("samDesired", dce, pdu, iov, offset,
                                   &req->samDesired, &regsam_pp)) {
                return -1;
        }

        if (dcerpc_ptr_coder("lpSecurityAttributes", dce, pdu, iov, offset,
                             NULL, PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }

        if (dcerpc_ptr_coder("lpdwDisposition", dce, pdu, iov, offset,
                             &req->disposition,
                             PTR_UNIQUE, reg_disposition_uint32_coder)) {
                return -1;
        }
        return 0;
}

int
winreg_BaseRegCreateKey_rep_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct winreg_BaseRegCreateKey_rep *rep = ptr;

        if (dcerpc_ptr_coder("phkResult", dce, pdu, iov, offset, &rep->phkResult,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpdwDisposition", dce, pdu, iov, offset,
                             &rep->disposition,
                             PTR_UNIQUE, reg_disposition_uint32_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}








int
winreg_BaseRegDeleteKey_req_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct winreg_BaseRegDeleteKey_req *req = ptr;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpSubKey", dce, pdu, iov, offset, &req->lpSubKey,
                             PTR_REF, dcerpc_RRP_UNICODE_STRING_coder)) {
                return -1;
        }
        return 0;
}

int
winreg_BaseRegDeleteKey_rep_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct winreg_BaseRegDeleteKey_rep *rep = ptr;

        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}








int
winreg_BaseRegDeleteValue_req_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr)
{
        struct winreg_BaseRegDeleteValue_req *req = ptr;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpValueName", dce, pdu, iov, offset,
                             &req->lpValueName,
                             PTR_REF, dcerpc_RRP_UNICODE_STRING_coder)) {
                return -1;
        }
        return 0;
}

int
winreg_BaseRegDeleteValue_rep_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr)
{
        struct winreg_BaseRegDeleteValue_rep *rep = ptr;

        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}




struct winreg_blob {
        uint8_t *data;
        uint32_t max_count;
        uint32_t actual_count;
};





static int
winreg_conformant_bytes_coder(char *name, struct dcerpc_context *dce,
                              struct dcerpc_pdu *pdu,
                              struct dcerpc_iovec *iov, int *offset,
                              void *ptr)
{
        struct winreg_blob *b = ptr;
        uint32_t max_count;
        uint32_t i;

        if (dcerpc_pdu_encoding(pdu) != ENCODING_NDR) {
                return 0;
        }
        if (dcerpc_get_cr(pdu)) {
                return 0;
        }

        if (dcerpc_pdu_direction(pdu) == DCERPC_ENCODE) {
                max_count = b->max_count;
        } else {
                max_count = 0;
        }
        if (dcerpc_uint32_coder("MaxCount", dce, pdu, iov, offset, &max_count)) {
                return -1;
        }
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE) {
                if (max_count > 0x4000000) {
                        return -1;
                }
                b->max_count = max_count;
                b->actual_count = max_count;
                if (max_count == 0) {
                        b->data = NULL;
                        return 0;
                }
                b->data = dcerpc_alloc_data(pdu,
                                          max_count);
                if (b->data == NULL) {
                        return -1;
                }
                for (i = 0; i < max_count; i++) {
                        if (ndr_uint8_coder("Data", dce, pdu, iov, offset,
                                            &b->data[i])) {
                                return -1;
                        }
                }
                return 0;
        }
        for (i = 0; i < max_count; i++) {
                uint8_t byte = b->data ? b->data[i] : 0;

                if (ndr_uint8_coder("Data", dce, pdu, iov, offset, &byte)) {
                        return -1;
                }
        }
        return 0;
}











int
winreg_BaseRegSetValue_req_coder(char *name, struct dcerpc_context *dce,
                                 struct dcerpc_pdu *pdu,
                                 struct dcerpc_iovec *iov, int *offset,
                                 void *ptr)
{
        struct winreg_BaseRegSetValue_req *req = ptr;
        struct winreg_blob blob;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpValueName", dce, pdu, iov, offset,
                             &req->lpValueName,
                             PTR_REF, dcerpc_RRP_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("dwType", dce, pdu, iov, offset, &req->dwType,
                                   &reg_type_pp)) {
                return -1;
        }
        blob.data = req->lpData;
        blob.max_count = req->cbData;
        blob.actual_count = req->cbData;




        if (winreg_conformant_bytes_coder("lpData", dce, pdu, iov, offset,
                                          &blob)) {
                return -1;
        }
        if (dcerpc_uint32_coder("cbData", dce, pdu, iov, offset, &req->cbData)) {
                return -1;
        }
        return 0;
}

int
winreg_BaseRegSetValue_rep_coder(char *name, struct dcerpc_context *dce,
                                 struct dcerpc_pdu *pdu,
                                 struct dcerpc_iovec *iov, int *offset,
                                 void *ptr)
{
        struct winreg_BaseRegSetValue_rep *rep = ptr;

        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}











int
winreg_BaseRegOpenKey_req_coder(char *name, struct dcerpc_context *dce,
                                struct dcerpc_pdu *pdu,
                                struct dcerpc_iovec *iov, int *offset,
                                void *ptr)
{
        struct winreg_BaseRegOpenKey_req *req = ptr;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpSubKey", dce, pdu, iov, offset, &req->lpSubKey,
                             PTR_REF, dcerpc_RRP_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("dwOptions", dce, pdu, iov, offset,
                                   &req->dwOptions, &reg_option_pp)) {
                return -1;
        }
        if (dcerpc_uint32_coder_pp("samDesired", dce, pdu, iov, offset,
                                   &req->samDesired, &regsam_pp)) {
                return -1;
        }
        return 0;
}

int
winreg_BaseRegOpenKey_rep_coder(char *name, struct dcerpc_context *dce,
                                struct dcerpc_pdu *pdu,
                                struct dcerpc_iovec *iov, int *offset,
                                void *ptr)
{
        struct winreg_BaseRegOpenKey_rep *rep = ptr;

        if (dcerpc_ptr_coder("phkResult", dce, pdu, iov, offset, &rep->phkResult,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}







static int
winreg_blob_coder(char *name, struct dcerpc_context *dce,
                  struct dcerpc_pdu *pdu,
                  struct dcerpc_iovec *iov, int *offset,
                  void *ptr)
{
        struct winreg_blob *b = ptr;
        uint32_t max_count;
        uint32_t arr_offset = 0;
        uint32_t actual;
        uint32_t i;

        if (dcerpc_pdu_encoding(pdu) != ENCODING_NDR) {

                return 0;
        }

        if (dcerpc_get_cr(pdu)) {
                return 0;
        }

        if (dcerpc_pdu_direction(pdu) == DCERPC_ENCODE) {
                max_count = b->max_count;
                actual = b->actual_count;
                if (actual > max_count) {
                        return -1;
                }
        } else {
                max_count = 0;
                actual = 0;
        }

        if (dcerpc_uint32_coder("MaxCount", dce, pdu, iov, offset, &max_count)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Offset", dce, pdu, iov, offset, &arr_offset)) {
                return -1;
        }
        if (dcerpc_uint32_coder("ActualCount", dce, pdu, iov, offset, &actual)) {
                return -1;
        }

        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE) {
                if (actual > max_count || actual > 0x4000000) {
                        return -1;
                }
                b->max_count = max_count;
                b->actual_count = actual;
                if (actual == 0) {
                        b->data = NULL;
                        return 0;
                }
                b->data = dcerpc_alloc_data(pdu,
                                          actual);
                if (b->data == NULL) {
                        return -1;
                }
                for (i = 0; i < actual; i++) {
                        if (ndr_uint8_coder("Data", dce, pdu, iov, offset,
                                            &b->data[i])) {
                                return -1;
                        }
                }
                return 0;
        }


        for (i = 0; i < actual; i++) {
                uint8_t byte = b->data ? b->data[i] : 0;

                if (ndr_uint8_coder("Data", dce, pdu, iov, offset, &byte)) {
                        return -1;
                }
        }
        return 0;
}














#define WINREG_ENUMVALUE_NAME_MAX_LENGTH  1024
#define WINREG_ENUMVALUE_DATA_DEFAULT     4096

int
winreg_BaseRegEnumValue_req_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct winreg_BaseRegEnumValue_req *req = ptr;
        struct winreg_blob blob;
        uint16_t name_ml;

        if (dcerpc_ptr_coder("hKey", dce, pdu, iov, offset, &req->hKey,
                             PTR_REF, winreg_RPC_HKEY_STRUCT_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("dwIndex", dce, pdu, iov, offset, &req->dwIndex)) {
                return -1;
        }
        name_ml = req->lpValueName_max_length ?
                req->lpValueName_max_length : WINREG_ENUMVALUE_NAME_MAX_LENGTH;
        dcerpc_set_unicode_max_length(pdu, name_ml);
        if (dcerpc_ptr_coder("lpValueName", dce, pdu, iov, offset,
                             &req->lpValueName,
                             PTR_REF, dcerpc_RPC_UNICODE_STRING_coder)) {
                return -1;
        }

        if (dcerpc_ptr_coder("lpType", dce, pdu, iov, offset, &req->type,
                             PTR_UNIQUE, reg_type_uint32_coder)) {
                return -1;
        }
        blob.data = req->lpData;
        blob.max_count = req->cbData;
        blob.actual_count = req->cbLen;
        if (blob.max_count == 0 && req->lpData == NULL) {

                blob.max_count = 0;
                blob.actual_count = 0;
        }
        if (dcerpc_ptr_coder("lpData", dce, pdu, iov, offset, &blob,
                             PTR_UNIQUE, winreg_blob_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpcbData", dce, pdu, iov, offset, &req->cbData,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpcbLen", dce, pdu, iov, offset, &req->cbLen,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }
        return 0;
}

int
winreg_BaseRegEnumValue_rep_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr)
{
        struct winreg_BaseRegEnumValue_rep *rep = ptr;
        struct winreg_blob blob;

        memset(&blob, 0, sizeof(blob));

        if (dcerpc_ptr_coder("lpValueName", dce, pdu, iov, offset,
                             &rep->lpValueName,
                             PTR_REF, dcerpc_RPC_UNICODE_STRING_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpType", dce, pdu, iov, offset, &rep->type,
                             PTR_UNIQUE, reg_type_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpData", dce, pdu, iov, offset, &blob,
                             PTR_UNIQUE, winreg_blob_coder)) {
                return -1;
        }
        if (dcerpc_pdu_direction(pdu) == DCERPC_DECODE) {
                rep->lpData = blob.data;




                rep->cbLen = blob.actual_count;
                rep->cbData = blob.max_count;
        }
        if (dcerpc_ptr_coder("lpcbData", dce, pdu, iov, offset, &rep->cbData,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_ptr_coder("lpcbLen", dce, pdu, iov, offset, &rep->cbLen,
                             PTR_UNIQUE, dcerpc_uint32_coder)) {
                return -1;
        }
        if (dcerpc_uint32_coder("Status", dce, pdu, iov, offset, &rep->status)) {
                return -1;
        }

        return 0;
}

struct dcerpc_procedure winreg_procs[] = {
        {WINREG_OPENCLASSESROOT, "OpenClassesRoot",
         winreg_OpenClassesRoot_req_coder, sizeof(struct winreg_OpenClassesRoot_req),
         winreg_OpenClassesRoot_rep_coder, sizeof(struct winreg_OpenClassesRoot_rep),
        },
        {WINREG_OPENCURRENTUSER, "OpenCurrentUser",
         winreg_OpenCurrentUser_req_coder, sizeof(struct winreg_OpenCurrentUser_req),
         winreg_OpenCurrentUser_rep_coder, sizeof(struct winreg_OpenCurrentUser_rep),
        },
        {WINREG_OPENLOCALMACHINE, "OpenLocalMachine",
         winreg_OpenLocalMachine_req_coder, sizeof(struct winreg_OpenLocalMachine_req),
         winreg_OpenLocalMachine_rep_coder, sizeof(struct winreg_OpenLocalMachine_rep),
        },
        {WINREG_OPENUSERS, "OpenUsers",
         winreg_OpenUsers_req_coder, sizeof(struct winreg_OpenUsers_req),
         winreg_OpenUsers_rep_coder, sizeof(struct winreg_OpenUsers_rep),
        },
        {WINREG_OPENCURRENTCONFIG, "OpenCurrentConfig",
         winreg_OpenCurrentConfig_req_coder,
         sizeof(struct winreg_OpenCurrentConfig_req),
         winreg_OpenCurrentConfig_rep_coder,
         sizeof(struct winreg_OpenCurrentConfig_rep),
        },
        {WINREG_BASEREGCLOSEKEY, "BaseRegCloseKey",
         winreg_BaseRegCloseKey_req_coder, sizeof(struct winreg_BaseRegCloseKey_req),
         winreg_BaseRegCloseKey_rep_coder, sizeof(struct winreg_BaseRegCloseKey_rep),
        },
        {WINREG_BASEREGCREATEKEY, "BaseRegCreateKey",
         winreg_BaseRegCreateKey_req_coder,
         sizeof(struct winreg_BaseRegCreateKey_req),
         winreg_BaseRegCreateKey_rep_coder,
         sizeof(struct winreg_BaseRegCreateKey_rep),
        },
        {WINREG_BASEREGDELETEKEY, "BaseRegDeleteKey",
         winreg_BaseRegDeleteKey_req_coder,
         sizeof(struct winreg_BaseRegDeleteKey_req),
         winreg_BaseRegDeleteKey_rep_coder,
         sizeof(struct winreg_BaseRegDeleteKey_rep),
        },
        {WINREG_BASEREGDELETEVALUE, "BaseRegDeleteValue",
         winreg_BaseRegDeleteValue_req_coder,
         sizeof(struct winreg_BaseRegDeleteValue_req),
         winreg_BaseRegDeleteValue_rep_coder,
         sizeof(struct winreg_BaseRegDeleteValue_rep),
        },
        {WINREG_BASEREGENUMKEY, "BaseRegEnumKey",
         winreg_BaseRegEnumKey_req_coder,
         sizeof(struct winreg_BaseRegEnumKey_req),
         winreg_BaseRegEnumKey_rep_coder,
         sizeof(struct winreg_BaseRegEnumKey_rep),
        },
        {WINREG_BASEREGENUMVALUE, "BaseRegEnumValue",
         winreg_BaseRegEnumValue_req_coder,
         sizeof(struct winreg_BaseRegEnumValue_req),
         winreg_BaseRegEnumValue_rep_coder,
         sizeof(struct winreg_BaseRegEnumValue_rep),
        },
        {WINREG_BASEREGOPENKEY, "BaseRegOpenKey",
         winreg_BaseRegOpenKey_req_coder,
         sizeof(struct winreg_BaseRegOpenKey_req),
         winreg_BaseRegOpenKey_rep_coder,
         sizeof(struct winreg_BaseRegOpenKey_rep),
        },
        {WINREG_BASEREGSETVALUE, "BaseRegSetValue",
         winreg_BaseRegSetValue_req_coder,
         sizeof(struct winreg_BaseRegSetValue_req),
         winreg_BaseRegSetValue_rep_coder,
         sizeof(struct winreg_BaseRegSetValue_rep),
        },
        {WINREG_BASEREGQUERYINFOKEY, "BaseRegQueryInfoKey",
         winreg_BaseRegQueryInfoKey_req_coder,
         sizeof(struct winreg_BaseRegQueryInfoKey_req),
         winreg_BaseRegQueryInfoKey_rep_coder,
         sizeof(struct winreg_BaseRegQueryInfoKey_rep),
        },
        {-1, NULL, NULL, 0, NULL, 0}
};
