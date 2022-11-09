#ifndef NET_H
#define NET_H

#include<winsock2.h>

#define CLOG_TCP 1
#define CLOG_UDP 2

/* Errors */
enum {
    CLOG_ERR_INVAL_SOCK
};

/* Actions */
enum {
    CLOG_ACT_IGNORE = 1
};

/* Callback Types */
enum {
    CLOG_ONCONNECTION = 0,

    CLOG_CALLBACK_COUNT
};

typedef struct {
    SOCKET sock_tcp;
    SOCKET sock_udp;

    char *host;
    int port;

    int error;
} Clog;

void clog_getStream(Clog *str);

void clog_listener(int type, void (*callback)());
int clog_send(Clog *stream, char *data, int data_len);
int clog_connect(char *host, int port, Clog *out);
int clog_listen(char *host, int port, int type, Clog *out);
int clog_recv(Clog str, char *msg, int size);
void clog_closeStream(Clog *stream);

/* UDP */
int clog_recvUdp(Clog *stream, char *buf, int data_len);

typedef void(*clog_callback)(Clog stream);

#endif
