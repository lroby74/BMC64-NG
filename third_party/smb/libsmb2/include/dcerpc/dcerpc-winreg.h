
/*
   Copyright (C) 2026 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _DCERPC_WINREG_H_
#define _DCERPC_WINREG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <dcerpc/dcerpc.h>


#define WINREG_OPENCLASSESROOT       0x00
#define WINREG_OPENCURRENTUSER       0x01
#define WINREG_OPENLOCALMACHINE      0x02
#define WINREG_OPENPERFORMANCEDATA   0x03
#define WINREG_OPENUSERS             0x04
#define WINREG_BASEREGCLOSEKEY       0x05
#define WINREG_BASEREGCREATEKEY      0x06
#define WINREG_BASEREGDELETEKEY      0x07
#define WINREG_BASEREGDELETEVALUE    0x08
#define WINREG_BASEREGENUMKEY        0x09
#define WINREG_BASEREGENUMVALUE      0x0a
#define WINREG_BASEREGOPENKEY        0x0f
#define WINREG_BASEREGQUERYINFOKEY   0x10
#define WINREG_BASEREGSETVALUE       0x16
#define WINREG_OPENCURRENTCONFIG     0x1b


#define REG_OPTION_NON_VOLATILE      0x00000000
#define REG_OPTION_VOLATILE          0x00000001
#define REG_OPTION_CREATE_LINK       0x00000002
#define REG_OPTION_BACKUP_RESTORE    0x00000004
#define REG_OPTION_OPEN_LINK         0x00000008


#define REG_CREATED_NEW_KEY          0x00000001
#define REG_OPENED_EXISTING_KEY      0x00000002


#define REG_NONE                       0
#define REG_SZ                         1
#define REG_EXPAND_SZ                  2
#define REG_BINARY                     3
#define REG_DWORD                      4
#define REG_DWORD_LITTLE_ENDIAN        4
#define REG_DWORD_BIG_ENDIAN           5
#define REG_LINK                       6
#define REG_MULTI_SZ                   7
#define REG_RESOURCE_LIST              8
#define REG_FULL_RESOURCE_DESCRIPTOR   9
#define REG_RESOURCE_REQUIREMENTS_LIST 10
#define REG_QWORD                      11
#define REG_QWORD_LITTLE_ENDIAN        11




#define KEY_QUERY_VALUE              0x00000001
#define KEY_SET_VALUE                0x00000002
#define KEY_CREATE_SUB_KEY           0x00000004
#define KEY_ENUMERATE_SUB_KEYS       0x00000008
#define KEY_NOTIFY                   0x00000010
#define KEY_CREATE_LINK              0x00000020
#define KEY_WOW64_64KEY              0x00000100
#define KEY_WOW64_32KEY              0x00000200

#define KEY_READ                     0x00020019
#define KEY_WRITE                    0x00020006
#define KEY_EXECUTE                  0x00020019
#define KEY_ALL_ACCESS               0x000F003F

#define WINREG_DELETE                0x00010000

struct dcerpc_context;
struct dcerpc_pdu;












struct winreg_OpenRootKey_req {
        char *ServerName;
        uint32_t samDesired;
};

struct winreg_OpenRootKey_rep {
        uint32_t status;

        struct dcerpc_context_handle phKey;
};


struct winreg_OpenClassesRoot_req {
        char *ServerName;
        uint32_t samDesired;
};
struct winreg_OpenClassesRoot_rep {
        uint32_t status;
        struct dcerpc_context_handle phKey;
};

struct winreg_OpenCurrentUser_req {
        char *ServerName;
        uint32_t samDesired;
};
struct winreg_OpenCurrentUser_rep {
        uint32_t status;
        struct dcerpc_context_handle phKey;
};

struct winreg_OpenLocalMachine_req {
        char *ServerName;
        uint32_t samDesired;
};

struct winreg_OpenLocalMachine_rep {
        uint32_t status;

        struct dcerpc_context_handle phKey;
};

struct winreg_OpenUsers_req {
        char *ServerName;
        uint32_t samDesired;
};
struct winreg_OpenUsers_rep {
        uint32_t status;
        struct dcerpc_context_handle phKey;
};

struct winreg_OpenCurrentConfig_req {
        char *ServerName;
        uint32_t samDesired;
};
struct winreg_OpenCurrentConfig_rep {
        uint32_t status;
        struct dcerpc_context_handle phKey;
};






struct winreg_BaseRegCloseKey_req {
        struct dcerpc_context_handle hKey;
};

struct winreg_BaseRegCloseKey_rep {
        uint32_t status;

        struct dcerpc_context_handle hKey;
};







struct winreg_FILETIME {
        uint32_t dwLowDateTime;
        uint32_t dwHighDateTime;
};



















struct winreg_BaseRegQueryInfoKey_req {
        struct dcerpc_context_handle hKey;
        char *lpClass;
};

struct winreg_BaseRegQueryInfoKey_rep {
        uint32_t status;

        char *lpClass;
        uint32_t lpcSubKeys;
        uint32_t lpcbMaxSubKeyLen;
        uint32_t lpcbMaxClassLen;
        uint32_t lpcValues;
        uint32_t lpcbMaxValueNameLen;
        uint32_t lpcbMaxValueLen;
        uint32_t lpcbSecurityDescriptor;
        struct winreg_FILETIME lpftLastWriteTime;
};




















struct winreg_BaseRegEnumKey_req {
        struct dcerpc_context_handle hKey;
        uint32_t dwIndex;
        char *lpName;
        uint16_t lpName_max_length;
        char *lpClass;
        uint16_t lpClass_max_length;
        struct winreg_FILETIME *lpftLastWriteTime;
};

struct winreg_BaseRegEnumKey_rep {
        uint32_t status;

        char *lpName;
        char *lpClass;
        struct winreg_FILETIME lpftLastWriteTime;
};












struct winreg_BaseRegOpenKey_req {
        struct dcerpc_context_handle hKey;
        char *lpSubKey;
        uint32_t dwOptions;
        uint32_t samDesired;
};

struct winreg_BaseRegOpenKey_rep {
        uint32_t status;

        struct dcerpc_context_handle phkResult;
};
















struct winreg_BaseRegCreateKey_req {
        struct dcerpc_context_handle hKey;
        char *lpSubKey;
        char *lpClass;
        uint32_t dwOptions;
        uint32_t samDesired;

        uint32_t disposition;
};

struct winreg_BaseRegCreateKey_rep {
        uint32_t status;

        struct dcerpc_context_handle phkResult;
        uint32_t disposition;
};










struct winreg_BaseRegDeleteKey_req {
        struct dcerpc_context_handle hKey;
        char *lpSubKey;
};

struct winreg_BaseRegDeleteKey_rep {
        uint32_t status;
};









struct winreg_BaseRegDeleteValue_req {
        struct dcerpc_context_handle hKey;
        char *lpValueName;
};

struct winreg_BaseRegDeleteValue_rep {
        uint32_t status;
};













struct winreg_BaseRegSetValue_req {
        struct dcerpc_context_handle hKey;
        char *lpValueName;
        uint32_t dwType;
        uint8_t *lpData;
        uint32_t cbData;
};

struct winreg_BaseRegSetValue_rep {
        uint32_t status;
};




















struct winreg_BaseRegEnumValue_req {
        struct dcerpc_context_handle hKey;
        uint32_t dwIndex;
        char *lpValueName;
        uint16_t lpValueName_max_length;
        uint32_t type;
        uint8_t *lpData;
        uint32_t cbData;
        uint32_t cbLen;
};

struct winreg_BaseRegEnumValue_rep {
        uint32_t status;

        char *lpValueName;
        uint32_t type;
        uint8_t *lpData;
        uint32_t cbData;
        uint32_t cbLen;
};

int winreg_OpenClassesRoot_rep_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);
int winreg_OpenClassesRoot_req_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);
int winreg_OpenCurrentUser_rep_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);
int winreg_OpenCurrentUser_req_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);
int winreg_OpenLocalMachine_rep_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr);
int winreg_OpenLocalMachine_req_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr);
int winreg_OpenUsers_rep_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr);
int winreg_OpenUsers_req_coder(char *name, struct dcerpc_context *dce,
                               struct dcerpc_pdu *pdu,
                               struct dcerpc_iovec *iov, int *offset,
                               void *ptr);
int winreg_OpenCurrentConfig_rep_coder(char *name, struct dcerpc_context *dce,
                                       struct dcerpc_pdu *pdu,
                                       struct dcerpc_iovec *iov, int *offset,
                                       void *ptr);
int winreg_OpenCurrentConfig_req_coder(char *name, struct dcerpc_context *dce,
                                       struct dcerpc_pdu *pdu,
                                       struct dcerpc_iovec *iov, int *offset,
                                       void *ptr);
int winreg_BaseRegCloseKey_rep_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);
int winreg_BaseRegCloseKey_req_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);
int winreg_BaseRegQueryInfoKey_rep_coder(char *name, struct dcerpc_context *dce,
                                         struct dcerpc_pdu *pdu,
                                         struct dcerpc_iovec *iov, int *offset,
                                         void *ptr);
int winreg_BaseRegQueryInfoKey_req_coder(char *name, struct dcerpc_context *dce,
                                         struct dcerpc_pdu *pdu,
                                         struct dcerpc_iovec *iov, int *offset,
                                         void *ptr);
int winreg_BaseRegEnumKey_rep_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr);
int winreg_BaseRegEnumKey_req_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr);
int winreg_BaseRegOpenKey_rep_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr);
int winreg_BaseRegOpenKey_req_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr);
int winreg_BaseRegCreateKey_rep_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr);
int winreg_BaseRegCreateKey_req_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr);
int winreg_BaseRegDeleteKey_rep_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr);
int winreg_BaseRegDeleteKey_req_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr);
int winreg_BaseRegDeleteValue_rep_coder(char *name, struct dcerpc_context *dce,
                                        struct dcerpc_pdu *pdu,
                                        struct dcerpc_iovec *iov, int *offset,
                                        void *ptr);
int winreg_BaseRegDeleteValue_req_coder(char *name, struct dcerpc_context *dce,
                                        struct dcerpc_pdu *pdu,
                                        struct dcerpc_iovec *iov, int *offset,
                                        void *ptr);
int winreg_BaseRegSetValue_rep_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);
int winreg_BaseRegSetValue_req_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);
int winreg_BaseRegEnumValue_rep_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr);
int winreg_BaseRegEnumValue_req_coder(char *name, struct dcerpc_context *dce,
                                      struct dcerpc_pdu *pdu,
                                      struct dcerpc_iovec *iov, int *offset,
                                      void *ptr);

extern struct dcerpc_procedure winreg_procs[];

#ifdef __cplusplus
}
#endif

#endif
