#include <winsock2.h>
#include <windows.h>
#include <stdio.h>

#include <net.h>

void clog_emptyCallback(Clog stream) { };

clog_callback callbacks[CLOG_CALLBACK_COUNT];

void clog_listener(int type, clog_callback callback) {
    if(callback == NULL)
        return;

    callbacks[type] = callback;
}

int clog_send(Clog *stream, char *data, int data_len) {
    int success = send(stream->sock_tcp, data, data_len, 0);
    if(success < 0)
        return WSAGetLastError();

    return success;
}

int clog_recvUdp(Clog *stream, char *buf, int data_len) {
    struct sockaddr_in from;
    int size = sizeof(from);
    recvfrom(stream->sock_udp, buf, data_len, 0, (struct sockaddr *)&from, &size);
    printf("fsdf\n");

    return 0;
}

int clog_recv(Clog str, char *msg, int size) {
    int num_bytes = recv(str.sock_tcp, msg, size, 0);
    if(num_bytes == SOCKET_ERROR)
        return SOCKET_ERROR;

    return num_bytes;
}

int clog_connect(char* host, int port, Clog *out) {
    WSADATA wsa;
    Clog this;

    // Socket Init
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return WSAGetLastError();

    if((this.sock_tcp = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
        return WSAGetLastError();

    // Socket Init
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_family      = AF_INET;
    server.sin_port        = htons(port);

    // Connect
    if (connect(this.sock_tcp, (struct sockaddr*)&server , sizeof(server)) < 0)
        return WSAGetLastError();

    *out = this;
    return 0;
}

DWORD WINAPI clog_wait(void* vargp) {
    while(1) {
        struct sockaddr_in client;
        SOCKET c_sock;
        SOCKET sock = *(SOCKET*)vargp;

        int c = sizeof(struct sockaddr_in);
        c_sock = accept(sock, (struct sockaddr*)&client, &c);

        Clog stream;
        stream.sock_tcp = c_sock;
        stream.host = inet_ntoa(client.sin_addr);
        stream.port = ntohs(client.sin_port);

	if(c_sock == INVALID_SOCKET)
	    stream.error = CLOG_ERR_INVAL_SOCK;

        callbacks[CLOG_ONCONNECTION](stream);
    }

    return 0;
}

int clog_listen(char *host, int port, int type, Clog *out) {
    WSADATA wsa;
    Clog this;

    struct sockaddr_in server;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family      = AF_INET;
    server.sin_port        = htons(port);

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return WSAGetLastError();

    /* TCP */
    if((type & CLOG_TCP) == CLOG_TCP) {
	if((this.sock_tcp = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	    return WSAGetLastError();

	if(bind(this.sock_tcp,(struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR)
	    return WSAGetLastError();

	listen(this.sock_tcp, SOMAXCONN);
        CreateThread(NULL, 0, clog_wait, &this.sock_tcp, 0, NULL);
    }

    /* UDP */
    if((type & CLOG_UDP) == CLOG_UDP) {
	if((this.sock_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	    return WSAGetLastError();

	if(bind(this.sock_udp, (struct sockaddr *)&server, sizeof(server)) < 0)
	    return WSAGetLastError();
    }

    // closesocket(sock);
	// WSACleanup();

    *out = this;
    return 0;
}

void clog_closeStream(Clog *stream) {
    closesocket(stream->sock_tcp);
}

void clog_exit() {
}
