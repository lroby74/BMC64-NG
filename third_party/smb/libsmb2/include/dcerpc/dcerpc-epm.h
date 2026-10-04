
/*
   Copyright (C) 2026 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _DCERPC_EPM_H_
#define _DCERPC_EPM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <dcerpc/dcerpc.h>










#define EPM_INSERT               0x00
#define EPM_DELETE               0x01
#define EPM_LOOKUP               0x02
#define EPM_MAP                  0x03
#define EPM_LOOKUP_HANDLE_FREE   0x04
#define EPM_INQ_OBJECT           0x05
#define EPM_MGMT_DELETE          0x06


#define EPMAPPER_STATUS_OK                 0x00000000
#define EPMAPPER_STATUS_CANT_PERFORM_OP    0x000006d8
#define EPMAPPER_STATUS_NO_MEMORY          0x16c9a012
#define EPMAPPER_STATUS_NO_MORE_ENTRIES    0x16c9a0d6


#define RPC_C_EP_ALL_ELTS          0x00000000
#define RPC_C_EP_MATCH_BY_IF       0x00000001
#define RPC_C_EP_MATCH_BY_OBJ      0x00000002
#define RPC_C_EP_MATCH_BY_BOTH     0x00000003


#define RPC_C_VERS_ALL             0x00000000
#define RPC_C_VERS_COMPATIBLE      0x00000001
#define RPC_C_VERS_EXACT           0x00000002
#define RPC_C_VERS_MAJOR_ONLY      0x00000003
#define RPC_C_VERS_UPTO            0x00000004


#define EPM_PROTOCOL_TCP           0x07
#define EPM_PROTOCOL_UDP           0x08
#define EPM_PROTOCOL_IP            0x09
#define EPM_PROTOCOL_NCADG         0x0a
#define EPM_PROTOCOL_NCACN         0x0b
#define EPM_PROTOCOL_NCALRPC       0x0c
#define EPM_PROTOCOL_UUID          0x0d
#define EPM_PROTOCOL_SMB           0x0f
#define EPM_PROTOCOL_NAMED_PIPE    0x10
#define EPM_PROTOCOL_NETBIOS       0x11
#define EPM_PROTOCOL_HTTP          0x1f

#define EPM_MAX_ANNOTATION_SIZE    64

struct dcerpc_context;
struct dcerpc_pdu;









struct epm_twr_t {
        uint32_t tower_length;
        uint8_t *tower_octet_string;
};




struct epm_rpc_if_id {
        dcerpc_uuid_t uuid;
        uint16_t vers_major;
        uint16_t vers_minor;
};








struct epm_entry_t {
        dcerpc_uuid_t object;
        struct epm_twr_t tower;
        uint32_t tower_null;
        char *annotation;
};










struct epm_Insert_req {
        uint32_t num_ents;
        struct epm_entry_t *entries;
        uint32_t replace;
};

struct epm_Insert_rep {
        uint32_t status;
};









struct epm_Delete_req {
        uint32_t num_ents;
        struct epm_entry_t *entries;
};

struct epm_Delete_rep {
        uint32_t status;
};


















struct epm_Lookup_req {
        uint32_t inquiry_type;
        dcerpc_uuid_t object;
        uint32_t object_null;
        struct epm_rpc_if_id interface_id;
        uint32_t interface_id_null;
        uint32_t vers_option;
        struct dcerpc_context_handle entry_handle;
        uint32_t max_ents;
};

struct epm_Lookup_rep {
        struct dcerpc_context_handle entry_handle;
        uint32_t num_ents;
        struct epm_entry_t *entries;
        uint32_t status;
};













struct epm_Map_req {
        dcerpc_uuid_t object;
        uint32_t object_null;
        struct epm_twr_t map_tower;
        uint32_t map_tower_null;
        struct dcerpc_context_handle entry_handle;
        uint32_t max_towers;
};






struct epm_Map_rep {
        struct dcerpc_context_handle entry_handle;
        uint32_t num_towers;
        struct epm_twr_t *towers;
        uint32_t status;
};




struct epm_LookupHandleFree_req {
        struct dcerpc_context_handle entry_handle;
};

struct epm_LookupHandleFree_rep {
        struct dcerpc_context_handle entry_handle;
        uint32_t status;
};








struct epm_InqObject_req {
        int unused;
};

struct epm_InqObject_rep {
        dcerpc_uuid_t ept_object;
        uint32_t status;
};




struct epm_MgmtDelete_req {
        uint32_t object_speced;
        dcerpc_uuid_t object;
        uint32_t object_null;
        struct epm_twr_t tower;
        uint32_t tower_null;
};

struct epm_MgmtDelete_rep {
        uint32_t status;
};


int epm_twr_coder(char *name, struct dcerpc_context *ctx,
                  struct dcerpc_pdu *pdu,
                  struct dcerpc_iovec *iov, int *offset,
                  void *ptr);
int epm_entry_coder(char *name, struct dcerpc_context *ctx,
                    struct dcerpc_pdu *pdu,
                    struct dcerpc_iovec *iov, int *offset,
                    void *ptr);
int epm_rpc_if_id_coder(char *name, struct dcerpc_context *ctx,
                        struct dcerpc_pdu *pdu,
                        struct dcerpc_iovec *iov, int *offset,
                        void *ptr);

int epm_Insert_req_coder(char *name, struct dcerpc_context *ctx,
                         struct dcerpc_pdu *pdu,
                         struct dcerpc_iovec *iov, int *offset,
                         void *ptr);
int epm_Insert_rep_coder(char *name, struct dcerpc_context *ctx,
                         struct dcerpc_pdu *pdu,
                         struct dcerpc_iovec *iov, int *offset,
                         void *ptr);
int epm_Delete_req_coder(char *name, struct dcerpc_context *ctx,
                         struct dcerpc_pdu *pdu,
                         struct dcerpc_iovec *iov, int *offset,
                         void *ptr);
int epm_Delete_rep_coder(char *name, struct dcerpc_context *ctx,
                         struct dcerpc_pdu *pdu,
                         struct dcerpc_iovec *iov, int *offset,
                         void *ptr);
int epm_Lookup_req_coder(char *name, struct dcerpc_context *ctx,
                         struct dcerpc_pdu *pdu,
                         struct dcerpc_iovec *iov, int *offset,
                         void *ptr);
int epm_Lookup_rep_coder(char *name, struct dcerpc_context *ctx,
                         struct dcerpc_pdu *pdu,
                         struct dcerpc_iovec *iov, int *offset,
                         void *ptr);
int epm_Map_req_coder(char *name, struct dcerpc_context *ctx,
                      struct dcerpc_pdu *pdu,
                      struct dcerpc_iovec *iov, int *offset,
                      void *ptr);
int epm_Map_rep_coder(char *name, struct dcerpc_context *ctx,
                      struct dcerpc_pdu *pdu,
                      struct dcerpc_iovec *iov, int *offset,
                      void *ptr);
int epm_LookupHandleFree_req_coder(char *name, struct dcerpc_context *ctx,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr);
int epm_LookupHandleFree_rep_coder(char *name, struct dcerpc_context *ctx,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr);
int epm_InqObject_req_coder(char *name, struct dcerpc_context *ctx,
                            struct dcerpc_pdu *pdu,
                            struct dcerpc_iovec *iov, int *offset,
                            void *ptr);
int epm_InqObject_rep_coder(char *name, struct dcerpc_context *ctx,
                            struct dcerpc_pdu *pdu,
                            struct dcerpc_iovec *iov, int *offset,
                            void *ptr);
int epm_MgmtDelete_req_coder(char *name, struct dcerpc_context *ctx,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr);
int epm_MgmtDelete_rep_coder(char *name, struct dcerpc_context *ctx,
                             struct dcerpc_pdu *pdu,
                             struct dcerpc_iovec *iov, int *offset,
                             void *ptr);










int epm_build_tower_ncacn_np(const dcerpc_uuid_t *iface,
                             uint16_t vers_major, uint16_t vers_minor,
                             const char *host,
                             struct epm_twr_t *out);
int epm_build_tower_ncacn_ip_tcp(const dcerpc_uuid_t *iface,
                                 uint16_t vers_major, uint16_t vers_minor,
                                 const char *ipv4, uint16_t port,
                                 struct epm_twr_t *out);




void epm_free_tower(struct epm_twr_t *twr);







int epm_tower_get_ncacn_np(const struct epm_twr_t *twr,
                           char **pipe_out, char **host_out);
int epm_tower_get_ncacn_ip_tcp(const struct epm_twr_t *twr,
                               char **ipv4_out, uint16_t *port_out);

extern p_syntax_id_t epm_interface;
extern struct dcerpc_procedure epm_procs[];

#ifdef __cplusplus
}
#endif

#endif
