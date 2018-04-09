/*-
 * Copyright (c) 2018 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef CHERIOS_SOCKETS_H
#define CHERIOS_SOCKETS_H

#include "object.h"

#define SOCKET_CONNECT_IPC_NO       (0xbeef)

#define E_AGAIN                     (-1)
#define E_MSG_SIZE                  (-2)
#define E_COPY_NEEDED               (-11)
#define E_UNSUPPORTED               (-12)

#define E_CONNECT_FAIL              (-3)
#define E_CONNECT_FAIL_WRONG_PORT   (-9)
#define E_CONNECT_FAIL_WRONG_TYPE   (-10)

#define E_BUFFER_SIZE_NOT_POWER_2   (-4)
#define E_ALREADY_CLOSED            (-5)
#define E_SOCKET_CLOSED             (-6)
#define E_SOCKET_WRONG_TYPE         (-7)
#define E_SOCKET_NO_DIRECTION       (-8)

#define E_IN_PROXY                  (-13)
#define E_NO_DATA_BUFFER            (-14)
#define E_ALREADY_CONNECTED         (-15)
#define E_NOT_CONNECTED             (-16)

enum SOCKET_FLAGS {
    MSG_NONE = 0,
    MSG_DONT_WAIT = 1,
    MSG_NO_CAPS = 2,
    MSG_NO_COPY = 4,
    MSG_PEEK = 8,
};

#define SOCK_TYPE_PUSH 0
#define SOCK_TYPE_PULL 1

typedef enum {
    REQUEST_IM = 0,
    REQUEST_IND = 1,
    REQUEST_PROXY = 2,
    REQUEST_OUT_BAND = 3,
} request_type_e;

/* A socket is formed of a single requester and fulfiller. Between them they manage a ring buffer. The requester
 * enqueues requests, which are to remain valid until the fulfiller marks them as fulfilled, at which point the
 * fulfiller should no longer use them. There are currently 3 types of request. IM has some immediate data.
 * IND points to a buffer of data. Proxy points to another fulfiller, and asks to fulfill for them instead */

typedef struct request {
    uint64_t length;
    uint32_t drb_fullfill_inc; // When this request is fulfilled, automagically bump a fulfillment for a data buffer
    request_type_e type;
    union {
        struct uni_dir_socket_fulfiller* proxy_for;
        char* ind;
        char im[CHERICAP_SIZE];
    } request;
} request_t;

_Static_assert(sizeof(request_t) ==  2*sizeof(capability), "Make sure each request type is small enough");

// Uni-directional socket.
typedef struct uni_dir_socket_requester_fulfiller_component  {
    volatile act_kt fulfiller_waiting;
    volatile act_kt requester_waiting;
    volatile uint16_t fulfill_ptr;
    volatile uint8_t  fulfiller_closed;
} uni_dir_socket_requester_fulfiller_component;

typedef struct uni_dir_socket_requester {
    uni_dir_socket_requester_fulfiller_component fulfiller_component;
    volatile uint8_t requester_closed;
    uint8_t socket_type;
    uint8_t connected;
    uint16_t buffer_size;           // Power of 2
    volatile uint16_t requeste_ptr;
    volatile uint64_t* drb_fulfill_ptr;      // a pointer to a fulfilment pointer for a data buffer
    uni_dir_socket_requester_fulfiller_component* access;
    request_t request_ring_buffer[]; // Variable sized, is buffer_size.
} uni_dir_socket_requester;

#define SIZE_OF_request(buffer_size) ((sizeof(request_t) * buffer_size) + sizeof(uni_dir_socket_requester))

typedef struct uni_dir_socket_fulfiller {
    uni_dir_socket_requester* requester;  // Read only
    volatile uint64_t partial_fulfill_bytes;
    uint8_t socket_type;
    uint8_t connected;

    volatile uint16_t proxy_state;          // 0, not proxied, 1 proxied
    uni_dir_socket_requester* proxyied_in;  // set if proxied
} uni_dir_socket_fulfiller;

// Ring buffer for copy in for unix abstraction
typedef struct data_ring_buffer {
    volatile uint64_t requeste_ptr;
    volatile uint64_t fulfill_ptr;
    uint32_t buffer_size;
    char* buffer;
} data_ring_buffer;

enum socket_connect_type {
    CONNECT_NONE = 0,
    CONNECT_PULL_READ = 1,
    CONNECT_PULL_WRITE = 2,
    CONNECT_PUSH_READ = 4,
    CONNECT_PUSH_WRITE = 8
};

typedef union {
    uni_dir_socket_requester* pull_reader;
    uni_dir_socket_fulfiller push_reader;
} socket_reader_t;
typedef union {
    uni_dir_socket_requester* push_writer;
    uni_dir_socket_fulfiller pull_writer;
} socket_writer_t;

// Bi directional unix like socket
typedef struct unix_like_socket {
    enum SOCKET_FLAGS flags;
    enum socket_connect_type con_type;
    data_ring_buffer write_copy_buffer; // If we push write and are worried about delays we need a buffer
    // data_ring_buffer read_copy_buffer; // Do we copy for pull reading? Otherwise how do we know when consume has happened?
    socket_reader_t read;
    socket_writer_t write;
} unix_like_socket;


typedef uni_dir_socket_requester* requester_ptr_t;
DEC_ERROR_T(requester_ptr_t);


// Init //
int socket_internal_fulfiller_init(uni_dir_socket_fulfiller* fulfiller, uint8_t socket_type);
int socket_internal_requester_init(uni_dir_socket_requester* requester, uint16_t buffer_size, uint8_t socket_type, data_ring_buffer* paired_drb);


// Connection //
int socket_internal_listen(register_t port,
                           uni_dir_socket_requester* requester,
                           uni_dir_socket_fulfiller* fulfiller);
int socket_internal_connect(act_kt target, register_t port,
                            uni_dir_socket_requester* requester,
                            uni_dir_socket_fulfiller* fulfiller);

// Closing //
ssize_t socket_internal_close_requester(uni_dir_socket_requester* requester, int wait_finish, int dont_wait);
ssize_t socket_internal_close_fulfiller(uni_dir_socket_fulfiller* fulfiller, int wait_finish, int dont_wait);


// Call these to check number of REQUESTS (not bytes) //

// Wait for enough space for 'need_space' requests
int socket_internal_requester_space_wait(uni_dir_socket_requester* requester, uint16_t need_space, int dont_wait, int delay_sleep);
// Wait for 'amount' requests to be outstanding
int socket_internal_fulfill_outstanding_wait(uni_dir_socket_fulfiller* fulfiller, uint16_t amount, int dont_wait, int delay_sleep);
// Wait for all requests to be marked as fulfilled
ssize_t socket_internal_requester_wait_all_finish(uni_dir_socket_requester* requester, int dont_wait);




// These all enqueue requests. All lengths in BYTES //

// Request length (< cap_size) number of bytes. Bytes will be copied in from buf_in, and *buf_out will return the buffer
ssize_t socket_internal_request_im(uni_dir_socket_requester* requester, uint8_t length, char** buf_out, char* buf_in, uint32_t drb_off);
// Requests length bytes, bytes are put in / taken from buf
ssize_t socket_internal_request_ind(uni_dir_socket_requester* requester, char* buf, uint64_t length, uint32_t drb_off);
// Requests length bytes to be proxied as fulfillment to fulfiller
ssize_t socket_internal_request_proxy(uni_dir_socket_requester* requester, uni_dir_socket_fulfiller* fulfiller, uint64_t, uint32_t drb_off);
// Requests length bytes as an ind request, but uses a data buffer instead of the provided buf.
// Will call space_wait itself.
// For a write this does what you expect, copying the source buffer
// For a read this will only use your buffer as an alignment hint. Copying of data back is still a TODO
ssize_t socket_internal_request_ind_db(uni_dir_socket_requester* requester, const char* buf, uint32_t size,
                                       data_ring_buffer* data_buffer,
                                       int dont_wait, register_t perms);

void socket_internal_dump_requests(uni_dir_socket_requester* requester);

// This is for fulfilling requests //

// Call this to fulfill and / or progress. ful_func will be called on every char* to be fulfilled, providing its length
// an offset that is offset plus all previous lengths. arg is a user argument that is passed through
typedef ssize_t ful_func(capability arg, char* buf, uint64_t offset, uint64_t length);
ssize_t socket_internal_fulfill_progress_bytes(uni_dir_socket_fulfiller* fulfiller, size_t bytes,
                                               int check, int progress, int dont_wait, int in_proxy,
                                               ful_func* visit, capability arg, uint64_t offset);
ssize_t socket_internal_fulfiller_wait_proxy(uni_dir_socket_fulfiller* fulfiller, int dont_wait, int delay_sleep);


// Unix like interface. Either copies or waits for fulfill to return //

// This only inits the unix_socket struct. Up to you to provide underlying.
int socket_init(unix_like_socket* sock, enum SOCKET_FLAGS flags,
                char* data_buffer, uint32_t data_buffer_size,
                enum socket_connect_type con_type);
ssize_t socket_close(unix_like_socket* sock);
ssize_t socket_recv(unix_like_socket* sock, char* buf, size_t length, enum SOCKET_FLAGS flags);
ssize_t socket_send(unix_like_socket* sock, const char* buf, size_t length, enum SOCKET_FLAGS flags);

ssize_t socket_sendfile(unix_like_socket* sockout, unix_like_socket* sockin, size_t count);

enum poll_events {
    POLL_NONE = 0,
    POLL_IN = 1,
    POLL_OUT = 2,
    POLL_ER = 4,
    POLL_HUP = 8,
    POLL_NVAL = 16,
};

typedef struct poll_sock {
    unix_like_socket* sock;
    enum poll_events events;
    enum poll_events revents;
} poll_sock_t;

int socket_poll(poll_sock_t* socks, size_t nsocks);

#endif //CHERIOS_SOCKETS_H
