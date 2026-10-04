
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_SYS__IOVEC_H
#include <sys/_iovec.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_UNISTD_H
#include <sys/unistd.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <errno.h>

#include "compat.h"

#include "slist.h"
#include "smb2.h"
#include "libsmb2.h"
#include "smb3-seal.h"
#include "libsmb2-private.h"
#include "portable-endian.h"
#include <errno.h>




#ifdef BMC_SOCK_CIRCLE
#define SO_ERROR_DI(fd, err, size) ((err) = 0, (void)(size), 0)
#else
#define SO_ERROR_DI(fd, err, size) \
        getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&(err), &(size))
#endif


#define MAX_URL_SIZE 1024





#define HAPPY_EYEBALLS_TIMEOUT 100
#if !defined(HAVE_LINGER)
struct linger
{
        int     l_onoff;
        int     l_linger;
};
#endif
static int
smb2_connect_async_next_addr(struct smb2_context *smb2, const struct addrinfo *base);

void
smb2_close_connecting_fds(struct smb2_context *smb2)
{
        size_t i;
        for (i = 0; i < smb2->connecting_fds_count; ++i) {
                t_socket fd = smb2->connecting_fds[i];


                if (fd == smb2->fd || !SMB2_VALID_SOCKET(fd))
                        continue;

                if (smb2->change_fd) {
                        smb2->change_fd(smb2, fd, SMB2_DEL_FD);
                }
                close(fd);
        }
        free(smb2->connecting_fds);
        smb2->connecting_fds = NULL;
        smb2->connecting_fds_count = 0;

        if (smb2->addrinfos != NULL) {
                freeaddrinfo(smb2->addrinfos);
                smb2->addrinfos = NULL;
        }
        smb2->next_addrinfo = NULL;
}

static int
smb2_get_real_credit_charge_for_one_pdu(struct smb2_context *smb2, struct smb2_header *hdr)
{
        int credits;

        credits = hdr->credit_charge;
        if (hdr->command == SMB2_NEGOTIATE) {

        } else if (smb2->dialect <= SMB2_VERSION_0202) {
                ++ credits;
        }

        return credits;
}

static int
smb2_get_credit_charge(struct smb2_context *smb2, struct smb2_pdu *pdu)
{
        int credits = 0;

        while (pdu) {
                credits += smb2_get_real_credit_charge_for_one_pdu(smb2, &pdu->header);
                pdu = pdu->next_compound;
        }

        return credits;
}

int
smb2_which_events(struct smb2_context *smb2)
{
        int events = SMB2_VALID_SOCKET(smb2->fd) ? POLLIN : POLLOUT;

        if (smb2->outqueue != NULL &&
            smb2_get_credit_charge(smb2, smb2->outqueue) <= smb2->credits) {
                events |= POLLOUT;
        }

        return events;
}

t_socket smb2_get_fd(struct smb2_context *smb2)
{
        if (SMB2_VALID_SOCKET(smb2->fd)) {
                return smb2->fd;
        } else if (smb2->connecting_fds_count > 0) {
                return smb2->connecting_fds[0];
        } else {
                return -1;
        }
}

const t_socket *
smb2_get_fds(struct smb2_context *smb2, size_t *fd_count, int *timeout)
{
        if (SMB2_VALID_SOCKET(smb2->fd)) {
                *fd_count = 1;
                *timeout = -1;
                return &smb2->fd;
        } else {
                *fd_count = smb2->connecting_fds_count;
                *timeout = smb2->next_addrinfo != NULL ? HAPPY_EYEBALLS_TIMEOUT : -1;
                return smb2->connecting_fds;
        }
}

int
smb2_write_to_socket(struct smb2_context *smb2)
{
        struct smb2_pdu *pdu;

        if (!SMB2_VALID_SOCKET(smb2->fd)) {
                smb2_set_error(smb2, "trying to write but not connected");
                return -1;
        }
        while ((pdu = smb2->outqueue) != NULL) {
                struct iovec iov[SMB2_MAX_VECTORS] _U_;
                struct iovec *tmpiov;
                struct smb2_pdu *tmp_pdu;
                size_t num_done = pdu->out.num_done;
                int i, niov = 1;
                ssize_t count;
                uint32_t spl = 0, tmp_spl, credit_charge;

                credit_charge = smb2_get_credit_charge(smb2, pdu);
                if (credit_charge > (uint32_t)smb2->credits) {
                        return 0;
                }

                if (pdu->seal) {
                        niov = 2;
                        spl = pdu->crypt_len;
                        iov[1].iov_base = pdu->crypt;
                        iov[1].iov_len  = pdu->crypt_len;
                } else {



                        for (tmp_pdu = pdu; tmp_pdu;
                             tmp_pdu = tmp_pdu->next_compound) {
                                for (i = 0; i < tmp_pdu->out.niov;
                                     i++, niov++) {
                                        iov[niov].iov_base = tmp_pdu->out.iov[i].buf;
#if defined(_WIN32) || defined(_XBOX)
                                        iov[niov].iov_len = (unsigned long)tmp_pdu->out.iov[i].len;
#else
                                        iov[niov].iov_len = (size_t)tmp_pdu->out.iov[i].len;
#endif
                                        spl += (uint32_t)tmp_pdu->out.iov[i].len;
                                }
                        }
                }


                tmp_spl = htobe32(spl);
                iov[0].iov_base = &tmp_spl;
                iov[0].iov_len = SMB2_SPL_SIZE;

                tmpiov = iov;


                while (num_done >= tmpiov->iov_len) {
                        num_done -= tmpiov->iov_len;
                        tmpiov++;
                        niov--;
                }


                tmpiov->iov_base = (char *)tmpiov->iov_base + num_done;
#if defined(_WIN32) || defined(_XBOX)
                tmpiov->iov_len -= (unsigned long)num_done;
#else
                tmpiov->iov_len -= (size_t)num_done;
#endif
                count = writev(smb2->fd, tmpiov, niov);

                if (count == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                return 0;
                        }
                        smb2_set_error(smb2, "Error when writing to "
                                       "socket :%d %s", errno,
                                       smb2_get_error(smb2));
                        return -1;
                }

                pdu->out.num_done += (size_t)count;

                if (pdu->out.num_done == SMB2_SPL_SIZE + spl) {
                        SMB2_LIST_REMOVE(&smb2->outqueue, pdu);
                        smb2_change_events(smb2, smb2->fd, smb2_which_events(smb2));
                        while (pdu) {
                                tmp_pdu = pdu->next_compound;






                                pdu->next_compound = NULL;

                                if (!smb2_is_server(smb2)) {
                                        smb2->credits -= smb2_get_real_credit_charge_for_one_pdu(smb2, &pdu->header);
                                        if (pdu->header.command == SMB2_CANCEL) {






                                                if (pdu->cb) {
                                                        pdu->cb(smb2, 0, NULL, pdu->cb_data);
                                                }
                                                smb2_free_pdu(smb2, pdu);
                                        } else {

                                                SMB2_LIST_ADD_END(&smb2->waitqueue, pdu);
                                        }
                                }
                                else {

                                        smb2->credits = 128;

                                        smb2_free_pdu(smb2, pdu);
                                }
                                pdu = tmp_pdu;
                        }
                }
        }
        return 0;
}

typedef ssize_t (*read_func)(struct smb2_context *smb2,
                             const struct iovec *iov, int iovcnt);

static int smb2_read_data(struct smb2_context *smb2, read_func func,
                          int has_xfrmhdr)
{
        struct iovec iov[SMB2_MAX_VECTORS] _U_;
        struct iovec *tmpiov;
        int i, niov, is_chained;
        size_t num_done;
        size_t iov_offset = 0;
        static char smb3tfrm[4] = {0xFD, 'S', 'M', 'B'};
        struct smb2_pdu *pdu = smb2->pdu;
        ssize_t count;
        int len;

read_more_data:
        num_done = smb2->in.num_done;


        niov = smb2->in.niov;
        for (i = 0; i < niov; i++) {
                iov[i].iov_base = smb2->in.iov[i].buf;
#if defined(_WIN32) || defined(_XBOX)
                iov[i].iov_len = (unsigned long)smb2->in.iov[i].len;
#else
                iov[i].iov_len = (size_t)smb2->in.iov[i].len;
#endif
        }
        tmpiov = iov;











        while (niov > 0 && num_done >= tmpiov->iov_len) {
                num_done -= tmpiov->iov_len;
                tmpiov++;
                niov--;
        }
        if (niov <= 0) {
                smb2_set_error(smb2, "No io vectors left to read into");
                return -1;
        }


        tmpiov->iov_base = (char *)tmpiov->iov_base + num_done;
#if defined(_WIN32) || defined(_XBOX)
        tmpiov->iov_len -= (unsigned long)num_done;
#else
        tmpiov->iov_len -= (size_t)num_done;
#endif

        count = func(smb2, tmpiov, niov);
        if (count < 0) {
#if defined(_WIN32) || defined(_XBOX)
                int err = WSAGetLastError();
                if (err == WSAEINTR || err == WSAEWOULDBLOCK) {
#else
                int err = errno;
                if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK) {
#endif
                        return -EAGAIN;
                }
                smb2_set_error(smb2, "Read from socket failed, "
                               "errno:%d. Closing socket.", err);
                return -1;
        }
        if (count == 0) {

                smb2_set_error(smb2, "Read from socket failed, "
                               "remote closed connection.");
                return -1;
        }
        smb2->in.num_done += (size_t)count;

        if (smb2->in.num_done < smb2->in.total_size) {
                goto read_more_data;
        }


        switch (smb2->recv_state) {
        case SMB2_RECV_SPL:
                smb2->spl = be32toh(smb2->spl);







                if (smb2->spl < SMB2_HEADER_SIZE ||
                    smb2->spl > SMB2_MAX_PDU_SIZE) {
                        smb2_set_error(smb2, "Invalid session packet length "
                                       "%u in PDU", smb2->spl);
                        return -1;
                }
                smb2->recv_state = SMB2_RECV_HEADER;
                if (smb2_add_iovector(smb2, &smb2->in, &smb2->header[0],
                                  SMB2_HEADER_SIZE, NULL) == NULL) {
                        smb2_set_error(smb2, "Too many I/O vectors when adding header");
                        return -1;
                }
                goto read_more_data;
        case SMB2_RECV_HEADER:
                if (!memcmp(smb2->in.iov[smb2->in.niov - 1].buf, smb3tfrm, 4)) {








                        if (smb2->spl < 52 + 12) {
                                smb2_set_error(smb2, "Transform header PDU is "
                                               "too short: %u", smb2->spl);
                                return -1;
                        }
                        smb2->in.iov[smb2->in.niov - 1].len = 52;
                        len = smb2->spl - 52;
                        smb2->in.total_size -= 12;
                        {
                                uint8_t *tmp = malloc(len);
                                if (tmp == NULL) {
                                        smb2_set_error(smb2, "malloc failed while adding TRFM payload");
                                        return -1;
                                }
                                if (smb2_add_iovector(smb2, &smb2->in,tmp,len, free) == NULL) {
                                        free(tmp);
                                        smb2_set_error(smb2, "Failed to add iovector for TRFM payload");
                                        return -1;
                                }
                        }
                        memcpy(smb2->in.iov[smb2->in.niov - 1].buf,
                               &smb2->in.iov[smb2->in.niov - 2].buf[52], 12);
                        smb2->recv_state = SMB2_RECV_TRFM;
                        goto read_more_data;
                }
                if (smb2_decode_header(smb2, &smb2->in.iov[smb2->in.niov - 1],
                                       &smb2->hdr) != 0) {
                        smb2_set_error(smb2, "Failed to decode smb2 "
                                       "header: %s", smb2_get_error(smb2));
                        return -1;
                }


                if (smb2_is_server(smb2) && smb2->hdr.command == SMB1_NEGOTIATE) {
                        uint8_t flusher[32];
                        struct iovec fiov;
                        fiov.iov_base = (char *)flusher;
                        fiov.iov_len = sizeof(flusher);
                        do {
                                count = func(smb2, &fiov, 1);
                                if (count < 0) {
#if defined(_WIN32) || defined(_XBOX)
                                        int err = WSAGetLastError();
                                        if (err == WSAEINTR || err == WSAEWOULDBLOCK) {
#else
                                        int err = errno;
                                        if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK) {
#endif
                                                count = 0;
                                        }
                                }
                        }
                        while (count > 0);


                        SMB2_LIST_ADD_END(&smb2->waitqueue, pdu);

                        smb2->in.num_done = 0;
                        pdu->cb(smb2, smb2->hdr.status, pdu->payload, pdu->cb_data);
                        smb2->pdu = NULL;
                        smb2->pdu = smb2->next_pdu;
                        smb2->next_pdu = NULL;
                        return 0;
                }

                smb2->payload_offset = smb2->in.num_done;

                if (!smb2_is_server(smb2)) {
                        smb2->credits += smb2->hdr.credit_request_response;

                        smb2_change_events(smb2, smb2->fd, smb2_which_events(smb2));
                }

                if (!smb2_is_server(smb2) && !(smb2->hdr.flags & SMB2_FLAGS_SERVER_TO_REDIR)) {
                        smb2_set_error(smb2, "received non-reply");
                        return -1;
                }
                else if (smb2_is_server(smb2) && (smb2->hdr.flags & SMB2_FLAGS_SERVER_TO_REDIR)) {
                        smb2_set_error(smb2, "received non-request");
                        return -1;
                }
                if (smb2->hdr.status == SMB2_STATUS_PENDING) {






                        len = smb2->spl - smb2->in.num_done;




                        if (!has_xfrmhdr) {
                                len += SMB2_SPL_SIZE;
                        }








                        if (len < 0) {
                                smb2_set_error(smb2, "Negative number of PAD "
                                               "bytes in PENDING reply");
                                return -1;
                        }
                        if (len == 0) {








                                if (!smb2_is_server(smb2)) {
                                        pdu = smb2_find_pdu(smb2,
                                                        smb2->hdr.message_id);
                                        if (pdu != NULL &&
                                            (smb2->hdr.flags & SMB2_FLAGS_ASYNC_COMMAND)) {









                                                pdu->header.flags |= SMB2_FLAGS_ASYNC_COMMAND;
                                                pdu->header.async.async_id = smb2->hdr.async.async_id;
                                        }
                                        if (smb2->passthrough && pdu) {
                                                pdu->cb(smb2, smb2->hdr.status,
                                                        pdu->payload,
                                                        pdu->cb_data);
                                        }
                                }
                                smb2->in.num_done = 0;
                                return 0;
                        }

                        smb2->recv_state = SMB2_RECV_PAD;
                        {
                                uint8_t *tmp = malloc(len);
                                if (tmp == NULL) {
                                        smb2_set_error(smb2, "malloc failed while adding PENDING padding");
                                        return -1;
                                }
                                if (smb2_add_iovector(smb2, &smb2->in,tmp, len, free) == NULL) {
                                        free(tmp);
                                        return -1;
                                }
                        }
                        goto read_more_data;
                }

                if (smb2_is_server(smb2)) {
                        pdu = smb2->pdu;
                        if (!pdu) {
                                smb2_set_error(smb2, "no pdu for request");
                                return -1;
                        }



                        pdu->header.message_id = smb2->hdr.message_id;
                        if (!(smb2->hdr.flags & SMB2_FLAGS_ASYNC_COMMAND)) {
                                pdu->header.sync.tree_id = smb2->hdr.sync.tree_id;
                        }




                        if (smb2->hdr.command > SMB2_SESSION_SETUP) {
                                pdu->header.command = smb2->hdr.command;
                        }
                        pdu->header.credit_charge = smb2->hdr.credit_charge;
                        pdu->header.credit_request_response = smb2->hdr.credit_request_response;
                } else {
                        if ((smb2->hdr.command != SMB2_OPLOCK_BREAK) ||
                                        (smb2->hdr.message_id != 0xffffffffffffffffULL)) {
                                if (smb2->pdu) {
                                        smb2_free_pdu(smb2, smb2->pdu);
                                        smb2->pdu = NULL;
                                }
                                pdu = smb2->pdu = smb2_find_pdu(smb2, smb2->hdr.message_id);
                                if (pdu == NULL) {
                                        len = smb2->spl - smb2->in.num_done;
                                        if (!has_xfrmhdr) {
                                                len += SMB2_SPL_SIZE;
                                        }
                                        if (len < 0 || len > SMB2_MAX_PDU_SIZE) {
                                                smb2_set_error(smb2, "no matching PDU found");
                                                return -1;
                                        }





                                        if (len == 0) {
                                                smb2->in.num_done = 0;
                                                return 0;
                                        }
                                        smb2->recv_state = SMB2_RECV_UNKNOWN;
                                        {
                                                uint8_t *tmp = malloc(len);
                                                if (tmp == NULL) {
                                                        smb2_set_error(smb2, "malloc failed while adding UNKNOWN padding");
                                                        return -1;
                                                }
                                                if (smb2_add_iovector(smb2, &smb2->in, tmp, len, free) == NULL) {
                                                        free(tmp);
                                                        return -1;
                                                }
                                        }
                                        goto read_more_data;
                                }







                                if (!pdu->unrelated_compound &&
                                    pdu->prev_compound_mid &&
                                    smb2_find_pdu(smb2, pdu->prev_compound_mid)) {
                                        smb2_set_error(smb2, "compound reply received out of order");
                                        return -1;
                                }

                                SMB2_LIST_REMOVE(&smb2->waitqueue, pdu);
                        } else {



                                pdu = smb2->pdu;
                                if (!pdu) {
                                        pdu = smb2->pdu = smb2_allocate_pdu(smb2, SMB2_OPLOCK_BREAK,
                                                smb2_oplock_break_notify,  NULL);
                                }
                                if (pdu == NULL) {
                                       smb2_set_error(smb2, "can not alloc pdu");
                                       return -1;
                                }
                        }
                }

                len = smb2_get_fixed_size(smb2, pdu);
                if (((int)len) < 0) {
                        smb2_set_error(smb2, "can not determine fixed size");
                        return -1;
                }

                smb2->recv_state = SMB2_RECV_FIXED;
                {
                        size_t alen = len & 0xfffe;
                        uint8_t *tmp = malloc(alen);
                        if (tmp == NULL) {
                                smb2_set_error(smb2, "malloc failed while adding FIXED payload");
                                return -1;
                        }
                        if (smb2_add_iovector(smb2, &smb2->in,
                                  tmp,
                                  alen, free) == NULL) {
                                free(tmp);
                                return -1;
                        }
                }
                goto read_more_data;
        case SMB2_RECV_FIXED:
                len = smb2_process_payload_fixed(smb2, pdu);
                if (len < 0) {
                        smb2_set_error(smb2, "Failed to parse fixed part of "
                                       "command payload. %s",
                                       smb2_get_error(smb2));
                        return -1;
                }


                if (len) {
                        for (i = 0; i < pdu->in.niov; i++) {
                                size_t num = pdu->in.iov[i].len;

                                if (num > (size_t)len) {
                                        num = (size_t)len;
                                }
                                if (smb2_add_iovector(smb2, &smb2->in,
                                                  pdu->in.iov[i].buf,
                                                  num, NULL) == NULL) {
                                        return -1;
                                }
                                len -= num;

                                if (len == 0) {
                                        smb2->recv_state = SMB2_RECV_VARIABLE;
                                        goto read_more_data;
                                }
                        }
                        if (len > 0) {
                                smb2->recv_state = SMB2_RECV_VARIABLE;
                                {
                                        uint8_t *tmp = malloc(len);
                                        if (tmp == NULL) {
                                                smb2_set_error(smb2, "malloc failed while adding VARIABLE tail");
                                                return -1;
                                        }
                                        if (smb2_add_iovector(smb2, &smb2->in,
                                                  tmp,
                                                  len, free) == NULL) {
                                                free(tmp);
                                                return -1;
                                        }
                                }
                                goto read_more_data;
                        }
                }


                if (smb2->hdr.next_command) {
                        len = smb2->hdr.next_command - (SMB2_HEADER_SIZE +
                                  smb2->in.num_done - smb2->payload_offset);
                } else {
                        len = smb2->spl + SMB2_SPL_SIZE - smb2->in.num_done;




                        if (smb2->enc) {
                                len -= SMB2_SPL_SIZE;
                        }
                }
                if (len < 0) {
                        smb2_set_error(smb2, "Negative number of PAD bytes "
                                       "encountered during PDU decode of"
                                       "fixed payload");
                        return -1;
                }
                if (len > 0) {

                        smb2->recv_state = SMB2_RECV_PAD;
                        {
                                uint8_t *tmp = malloc(len);
                                if (tmp == NULL) {
                                        smb2_set_error(smb2, "malloc failed while adding PAD");
                                        return -1;
                                }
                                if (smb2_add_iovector(smb2, &smb2->in,
                                          tmp,
                                          len, free) == NULL) {
                                        free(tmp);
                                        return -1;
                                }
                        }
                        goto read_more_data;
                }



                break;
        case SMB2_RECV_VARIABLE:
                if (smb2_process_payload_variable(smb2, pdu) < 0) {
                        smb2_set_error(smb2, "Failed to parse variable part of "
                                       "command payload. %s",
                                       smb2_get_error(smb2));
                        return -1;
                }


                if (smb2->hdr.next_command) {
                        len = smb2->hdr.next_command - (SMB2_HEADER_SIZE +
                                  smb2->in.num_done - smb2->payload_offset);
                } else {
                        len = smb2->spl + SMB2_SPL_SIZE - smb2->in.num_done;




                        if (smb2->enc) {
                                len -= SMB2_SPL_SIZE;
                        }
                }
                if (len < 0) {
                        smb2_set_error(smb2, "Negative number of PAD bytes "
                                       "encountered during PDU decode of"
                                       "variable payload");
                        return -1;
                }
                if (len > 0) {

                        smb2->recv_state = SMB2_RECV_PAD;
                        uint8_t * tmp = malloc(len);
                        if (tmp == NULL) {
                            smb2_set_error(smb2, "malloc failed while adding PAD");
                            return -1;
                        }
                        if (smb2_add_iovector(smb2, &smb2->in,
                                              tmp,
                                              len, free) == NULL) {
                                free(tmp);
                                smb2_set_error(smb2, "Failed to add iovector for PAD");
                                return -1;
                        }
                        goto read_more_data;
                }



                break;
        case SMB2_RECV_PAD:



                break;
        case SMB2_RECV_TRFM:



                smb2->in.num_done = 0;
                if (smb3_decrypt_pdu(smb2)) {
                        smb2_set_error(smb2, "Failed to decrypyt pdu");
                        return -1;
                }



                return 0;
        case SMB2_RECV_UNKNOWN:





                smb2->in.num_done = 0;
                return 0;
        }

        if (smb2->in.niov < 2) {
                smb2_set_error(smb2, "Too few io vectors in received PDU.");
                return -1;
        }

        if (smb2->hdr.status == SMB2_STATUS_PENDING) {



                if (!smb2_is_server(smb2)) {
                        pdu = smb2_find_pdu(smb2, smb2->hdr.message_id);
                        if (pdu != NULL &&
                            (smb2->hdr.flags & SMB2_FLAGS_ASYNC_COMMAND)) {








                                pdu->header.flags |= SMB2_FLAGS_ASYNC_COMMAND;
                                pdu->header.async.async_id = smb2->hdr.async.async_id;
                        }
                        if (smb2->passthrough) {
                                if (pdu == NULL) {
                                        smb2_set_error(smb2, "no matching PDU found");
                                }
                                else
                                {


                                        pdu->cb(smb2, smb2->hdr.status, pdu->payload, pdu->cb_data);
                                }
                        }
                }
                smb2->in.num_done = 0;
                return 0;
        }















        if (smb2->sign &&
            (smb2->hdr.command != SMB2_NEGOTIATE) &&
            (smb2->hdr.command != SMB2_SESSION_SETUP) &&
            !smb2->enc) {
                uint8_t signature[16] _U_;

                if (!(smb2->hdr.flags & SMB2_FLAGS_SIGNED)) {
                        smb2_set_error(smb2, "PDU for command %d is not "
                                       "signed but signing is required",
                                       smb2->hdr.command);
                        return -1;
                }
                memcpy(&signature[0], &smb2->in.iov[1 + iov_offset].buf[48], 16);
                if (smb2_calc_signature(smb2, &smb2->in.iov[1 + iov_offset].buf[48],
                                        &smb2->in.iov[1 + iov_offset],
                                        smb2->in.niov - 1 - iov_offset) < 0) {
                        smb2_set_error(smb2, "Signature calc failed.");
                        return -1;
                }
                if (memcmp(&signature[0], &smb2->in.iov[1 + iov_offset].buf[48], 16)) {
                        smb2_set_error(smb2, "Wrong signature in received "
                                       "PDU");
                        return -1;
                }
        }

        is_chained = smb2->hdr.next_command;

        if (smb2_is_server(smb2)) {

                SMB2_LIST_ADD_END(&smb2->waitqueue, pdu);
                pdu->cb(smb2, smb2->hdr.status, pdu->payload, pdu->cb_data);
                smb2->pdu = smb2->next_pdu;
                smb2->next_pdu = NULL;
        } else {
                pdu->cb(smb2, smb2->hdr.status, pdu->payload, pdu->cb_data);
                if (!pdu->caller_frees_pdu) {
                        smb2_free_pdu(smb2, pdu);
                }
                smb2->pdu = NULL;
        }

        if (is_chained) {

                iov_offset = smb2->in.niov - 1;
                smb2->recv_state = SMB2_RECV_HEADER;
                if (smb2_add_iovector(smb2, &smb2->in, &smb2->header[0],
                                  SMB2_HEADER_SIZE, NULL) == NULL) {
                        smb2_set_error(smb2, "Too many I/O vectors when adding chained header");
                        return -1;
                }
                goto read_more_data;
        }




        smb2->in.num_done = 0;

        return 0;
}

static ssize_t smb2_readv_from_socket(struct smb2_context *smb2,
                                      const struct iovec *iov, int iovcnt)
{
        ssize_t rc = readv(smb2->fd, (struct iovec*) iov, iovcnt);
        return rc;
}

static int
smb2_read_from_socket(struct smb2_context *smb2)
{
        int count;

        while(1) {





                if (smb2->in.num_done == 0) {
                        smb2->recv_state = SMB2_RECV_SPL;
                        smb2->spl = 0;

                        smb2_free_iovector(smb2, &smb2->in);
                        if (smb2_add_iovector(smb2, &smb2->in, (uint8_t *)&smb2->spl,
                                          SMB2_SPL_SIZE, NULL) == NULL) {
                                smb2_set_error(smb2, "Too many I/O vectors when adding SPL");
                                return -1;
                        }
                }

                count = smb2_read_data(smb2, smb2_readv_from_socket, 0);
                if (count == -EAGAIN) {
                        return 0;
                }
                if (count) {
                        return count;
                }
        }
}

static ssize_t smb2_readv_from_buf(struct smb2_context *smb2,
                                   const struct iovec *iov, int iovcnt)
{
        size_t i, len;
        ssize_t count = 0;

        for (i=0;(int)i<iovcnt;i++){
                len = iov[i].iov_len;
                if (len > smb2->enc_len - smb2->enc_pos) {
                        len = smb2->enc_len - smb2->enc_pos;
                }
                memcpy(iov[i].iov_base, &smb2->enc[smb2->enc_pos], len);
                smb2->enc_pos += (int)len;
                count += len;
        }
        return count;
}

int
smb2_read_from_buf(struct smb2_context *smb2)
{
        return smb2_read_data(smb2, smb2_readv_from_buf, 1);
}

static void
smb2_close_connecting_fd(struct smb2_context *smb2, t_socket fd)
{
        size_t i;

        close(fd);

        for (i = 0; i < smb2->connecting_fds_count; ++i) {
                if (fd == smb2->connecting_fds[i]) {
                        memmove(&smb2->connecting_fds[i],
                                &smb2->connecting_fds[i + 1],
                                (smb2->connecting_fds_count - i - 1)
                                * sizeof(smb2->connecting_fds[0]));
                        smb2->connecting_fds_count--;
                        return;
                }
        }
}

int
smb2_service_fd(struct smb2_context *smb2, t_socket fd, int revents)
{
        int ret = 0;

        if (!SMB2_VALID_SOCKET(fd)) {

                if (smb2->next_addrinfo != NULL) {
                    int err = smb2_connect_async_next_addr(smb2,
                                                           smb2->next_addrinfo);
                    return err == 0 ? 0 : -1;
                }
                goto out;
        } else if (fd != smb2->fd) {
                int fd_found = 0;
                size_t i;
                for (i = 0; i < smb2->connecting_fds_count; ++i) {
                        if (fd == smb2->connecting_fds[i])
                        {
                                fd_found = 1;
                                break;
                        }
                }
                if (fd_found == 0) {




                        return 0;
                }
        }

        if (revents & POLLERR) {
                int err = 0;
                socklen_t err_size = sizeof(err);

                if (!SMB2_VALID_SOCKET(smb2->fd) && smb2->next_addrinfo != NULL) {

                        smb2_close_connecting_fd(smb2, fd);

                        err = smb2_connect_async_next_addr(smb2, smb2->next_addrinfo);

                        if (err == 0) {
                                return 0;
                        }
                } else if (SO_ERROR_DI(fd, err, err_size) != 0 || err != 0) {
                        if (err == 0) {
                                err = errno;
                        }
                        smb2_set_error(smb2, "smb2_service: socket error "
                                        "%s(%d).",
                                        strerror(err), err);
                } else {
                        smb2_set_error(smb2, "smb2_service: POLLERR, "
                                        "Unknown socket error.");
                }

                if (smb2->connect_cb) {
                        smb2->connect_cb(smb2, err, NULL, smb2->connect_data);
                        smb2->connect_cb = NULL;
                }
                ret = -1;
                goto out;
        }
        if (revents & POLLHUP) {
                smb2_set_error(smb2, "smb2_service: POLLHUP, "
                                "socket error.");
                ret = -1;
                goto out;
        }

        if (!SMB2_VALID_SOCKET(smb2->fd) && revents & POLLOUT) {
                int err = 0;
                socklen_t err_size = sizeof(err);

                if (SO_ERROR_DI(fd, err, err_size) != 0 || err != 0) {
                        if (err == 0) {
                                err = errno;
                        }
                        if (smb2->next_addrinfo != NULL) {

                                smb2_close_connecting_fd(smb2, fd);

                                err = smb2_connect_async_next_addr(smb2, smb2->next_addrinfo);

                                if (err == 0) {
                                        return 0;
                                }
                        } else {
                                smb2_set_error(smb2, "smb2_service: socket error "
                                                "%s(%d) while connecting.",
                                                strerror(err), err);
                        }

                        if (smb2->connect_cb) {
                                smb2->connect_cb(smb2, err,
                                                 NULL, smb2->connect_data);
                                smb2->connect_cb = NULL;
                        }
                        ret = -1;
                        goto out;
                }
                smb2->fd = fd;

                smb2_close_connecting_fds(smb2);

                smb2_change_events(smb2, smb2->fd, smb2_which_events(smb2));
                if (smb2->connect_cb) {
                        smb2->connect_cb(smb2, 0, NULL,        smb2->connect_data);
                        smb2->connect_cb = NULL;
                }
                goto out;
        }

        if (revents & POLLIN) {
                if (smb2_read_from_socket(smb2) != 0) {
                        ret = -1;
                        goto out;
                }
        }

        if (revents & POLLOUT && smb2->outqueue != NULL) {
                if (smb2_write_to_socket(smb2) != 0) {
                        ret = -1;
                        goto out;
                }
        }

  out:
        if (smb2->timeout) {
                smb2_timeout_pdus(smb2);
        }
        return ret;
}

int
smb2_service(struct smb2_context *smb2, int revents)
{
        if (smb2->connecting_fds_count > 0) {
                return smb2_service_fd(smb2, smb2->connecting_fds[0], revents);
        } else {
                return smb2_service_fd(smb2, smb2->fd, revents);
        }
}

static void
set_nonblocking(t_socket fd)
{
#if defined(WIN32) || defined(_XBOX)
        unsigned long opt = 1;
        ioctlsocket(fd, FIONBIO, &opt);
#elif (defined(__AMIGA__) || defined(__AROS__)) && !defined(__amigaos4__) && !defined(__amigaos3__)
        unsigned long opt = 0;
        IoctlSocket(fd, FIONBIO, (char *)&opt);
#elif defined(__wii__) || defined(__gamecube__)








        int one = 1;
        net_ioctl(fd, FIONBIO, &one);
#else
        unsigned v;
        v = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, v | O_NONBLOCK);
#endif
}

static int
set_tcp_sockopt(t_socket sockfd, int optname, int value)
{
        int level;
#if !defined(SOL_TCP)
        struct protoent *buf;
        if ((buf = getprotobyname("tcp")) != NULL) {
                level = buf->p_proto;
        } else {
                return -1;
        }
#else
        level = SOL_TCP;
#endif

        return setsockopt(sockfd, level, optname, (char *)&value, sizeof(value));
}

static int
connect_async_ai(struct smb2_context *smb2, const struct addrinfo *ai, int *fd_out)
{
        int family;
        t_socket fd;
        socklen_t socksize;
        struct sockaddr_storage ss;
#if 0 == CONFIGURE_OPTION_TCP_LINGER
        int const yes = 1;
        struct linger const lin = { 1, 0 };
#endif
#ifdef _XBOX
        BOOL bBroadcast = TRUE;
#endif
        memset(&ss, 0, sizeof(ss));
        switch (ai->ai_family) {
        case AF_INET:
                socksize = sizeof(struct sockaddr_in);
                memcpy(&ss, ai->ai_addr, socksize);
#ifdef HAVE_SOCK_SIN_LEN
                ((struct sockaddr_in *)&ss)->sin_len = socksize;
#endif
                break;
#ifdef AF_INET6
        case AF_INET6:
#if !defined(PICO_PLATFORM) || defined(LWIP_INETV6)
                socksize = sizeof(struct sockaddr_in6);
                memcpy(&ss, ai->ai_addr, socksize);
#ifdef HAVE_SOCK_SIN_LEN
                ((struct sockaddr_in6 *)&ss)->sin6_len = socksize;
#endif
#endif
                break;
#endif
        default:
                smb2_set_error(smb2, "Unknown address family :%d. "
                                "Only IPv4/IPv6 supported so far.",
                                ai->ai_family);
                return -EINVAL;

        }
        family = ai->ai_family;

        fd = socket(family, SOCK_STREAM, 0);
        if (!SMB2_VALID_SOCKET(fd)) {
                smb2_set_error(smb2, "Failed to open smb2 socket. "
                               "Errno:%s(%d).", strerror(errno), errno);
                return -EIO;
        }

#ifdef _XBOX
        if(setsockopt(fd, SOL_SOCKET, 0x5801, (PCSTR)&bBroadcast, sizeof(BOOL) ) != 0 )
        {
#if 0
                return 0;
#endif
        }
        if(setsockopt(fd, SOL_SOCKET, 0x5802, (PCSTR)&bBroadcast, sizeof(BOOL)) != 0)
        {
#if 0
                return 0;
#endif
        }
#endif

        set_nonblocking(fd);
        set_tcp_sockopt(fd, TCP_NODELAY, 1);
#if 0 == CONFIGURE_OPTION_TCP_LINGER
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&yes, sizeof yes);
        setsockopt(fd, SOL_SOCKET, SO_LINGER, (const void*)&lin, sizeof lin);
#endif

        if (connect(fd, (struct sockaddr *)&ss, socksize) != 0
#if defined(_WIN32) || defined(_MSC_VER)
                  && WSAGetLastError() != WSAEWOULDBLOCK) {
#else
                  && errno != EINPROGRESS) {
#endif
                smb2_set_error(smb2, "Connect failed with errno : "
                        "%s(%d)", strerror(errno), errno);
                close(fd);
                return -EIO;
        }

        *fd_out = (int)fd;
        return 0;
}

static int
smb2_connect_async_next_addr(struct smb2_context *smb2, const struct addrinfo *base)
{
        int err = -1;
        const struct addrinfo *ai;
        for (ai = base; ai != NULL; ai = ai->ai_next) {
                int fd;
                err = connect_async_ai(smb2, ai, &fd);

                if (err == 0) {


                        smb2_set_error(smb2, "");
                        smb2->connecting_fds[smb2->connecting_fds_count++] = fd;
                        if (smb2->change_fd) {
                                smb2->change_fd(smb2, fd, SMB2_ADD_FD);
                                smb2_change_events(smb2, fd, POLLOUT);
                        }

                        smb2->next_addrinfo = ai->ai_next;
                        break;
                }
        }

        return err;
}


static void interleave_addrinfo(struct addrinfo *base)
{
        struct addrinfo **next = &base->ai_next;
        while (*next) {
                struct addrinfo *cur = *next;

                if (cur->ai_family == base->ai_family) {
                        next = &cur->ai_next;
                        continue;
                }
                if (cur == base->ai_next) {




                        base = cur;
                        next = &base->ai_next;
                        continue;
                }

                *next = cur->ai_next;

                cur->ai_next = base->ai_next;
                base->ai_next = cur;







                base = cur->ai_next;
        }
}

int
smb2_connect_async(struct smb2_context *smb2, const char *server,
                   smb2_command_cb cb, void *private_data)
{
        char *addr, *host, *port;
        int err;
        size_t addr_count = 0;
        const struct addrinfo *ai;

        if (SMB2_VALID_SOCKET(smb2->fd)) {
                smb2_set_error(smb2, "Trying to connect but already "
                               "connected.");
                return -EINVAL;
        }

        addr = strdup(server);
        if (addr == NULL) {
                smb2_set_error(smb2, "Out-of-memory: "
                               "Failed to strdup server address.");
                return -ENOMEM;
        }
        host = addr;
        port = host;


        if (host[0] == '[') {
                char *str;

                host++;
                str = strchr(host, ']');
                if (str == NULL) {
                        free(addr);
                        smb2_set_error(smb2, "Invalid address:%s  "
                                "Missing ']' in IPv6 address", server);
                        return -EINVAL;
                }
                *str = 0;
                port = str + 1;
        }

        port = strchr(port, ':');
        if (port != NULL) {
                *port++ = 0;
        } else {
                port = (char*)"445";
        }


        err = getaddrinfo(host, port, NULL, &smb2->addrinfos);
        if (err != 0) {
                free(addr);
#if defined(_WINDOWS) || defined(_XBOX)
                if (err == WSANOTINITIALISED)
                {
                        smb2_set_error(smb2, "Winsock was not initialized. "
                                "Please call WSAStartup().");
                        return -WSANOTINITIALISED;
                }
                else
#endif
                {
                        smb2_set_error(smb2, "Invalid address:%s  "
                                "Can not resolve into IPv4/v6.", server);
                }
                switch (err) {
                    case EAI_AGAIN:
                        return -EAGAIN;
                    case EAI_NONAME:
#ifdef EAI_NODATA
#if EAI_NODATA != EAI_NONAME
                    case EAI_NODATA:
#endif
#endif
                    case EAI_SERVICE:
                    case EAI_FAIL:
#ifdef EAI_ADDRFAMILY
                    case EAI_ADDRFAMILY:
#endif
                        return -EIO;
                    case EAI_MEMORY:
                        return -ENOMEM;
#ifdef EAI_SYSTEM
                    case EAI_SYSTEM:
                        return -errno;
#endif
                    default:
                        return -EINVAL;
                }
        }
        free(addr);

        interleave_addrinfo(smb2->addrinfos);


        for (ai = smb2->addrinfos; ai != NULL; ai = ai->ai_next)
                addr_count++;
        smb2->connecting_fds = malloc(sizeof(t_socket) * addr_count);
        if (smb2->connecting_fds == NULL) {
                freeaddrinfo(smb2->addrinfos);
                smb2->addrinfos = NULL;
                return -ENOMEM;
        }

        err = smb2_connect_async_next_addr(smb2, smb2->addrinfos);

        if (err == 0) {
                smb2->connect_cb   = cb;
                smb2->connect_data = private_data;
        } else {
                free(smb2->connecting_fds);
                smb2->connecting_fds = NULL;
                freeaddrinfo(smb2->addrinfos);
                smb2->addrinfos = NULL;
                smb2->next_addrinfo = NULL;
        }

        return err;
}

int
smb2_bind_and_listen(const uint16_t port, const int max_connections, int *out_fd)
{
        t_socket fd;
        socklen_t socksize;
        struct sockaddr_in serv_addr;
#if defined(SO_REUSEADDR) && !defined(_WIN32) && !defined(_MSC_VER)
        int const yes = 1;
#endif

             *out_fd = -1;

        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (!SMB2_VALID_SOCKET(fd)) {
                return -EIO;
        }

        set_nonblocking(fd);
        set_tcp_sockopt(fd, TCP_NODELAY, 1);

#if defined(SO_REUSEADDR) && !defined(_WIN32) && !defined(_MSC_VER)








        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&yes, sizeof(yes));
#endif

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_port = htons(port);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        socksize = sizeof(serv_addr);

        if (bind(fd, (struct sockaddr *)&serv_addr, socksize) != 0
#if defined(_WIN32) || defined(_MSC_VER)
                  && WSAGetLastError() != WSAEWOULDBLOCK) {
#else
                  && errno != EINPROGRESS) {
#endif
                close(fd);
                return -EIO;
        }

        if (listen(fd, max_connections) != 0) {
                close(fd);
                return -EIO;
        }

        *out_fd = (int)fd;
        return 0;
}

int smb2_accept_connection_async(const int fd, const int to_msec, smb2_accepted_cb cb, void *cb_data)
{
        int err = -1;
        struct sockaddr_in client_addr;
        socklen_t socklen;
        t_socket clientfd;
        struct pollfd pfd;
#if 0 == CONFIGURE_OPTION_TCP_LINGER
        int const yes = 1;
        struct linger const lin = { 1, 0 };
#endif

        if (!SMB2_VALID_SOCKET(fd)) {
                return -EINVAL;
        }

        memset(&pfd, 0, sizeof(struct pollfd));
        pfd.fd = fd;
        pfd.events = POLLIN;

        err = poll(&pfd, 1, to_msec);
        if (err > 0) {
                socklen = sizeof(client_addr);
                clientfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);

                if (clientfd >= 0) {
                        set_nonblocking(clientfd);
                        set_tcp_sockopt(clientfd, TCP_NODELAY, 1);
#if 0 == CONFIGURE_OPTION_TCP_LINGER
                        setsockopt(clientfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&yes, sizeof yes);
                        setsockopt(clientfd, SOL_SOCKET, SO_LINGER, (const void*)&lin, sizeof lin);
#endif
                        err = cb(clientfd, cb_data);
                }
                else {
                        err = -EIO;
                }
        }

        return err;
}

void smb2_change_events(struct smb2_context *smb2, t_socket fd, int events)
{
        if (smb2->events == events) {
                return;
        }

        if (smb2->change_events) {
                smb2->change_events(smb2, fd, events);
                smb2->events = events;
        }
}
