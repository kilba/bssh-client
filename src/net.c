#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdbool.h>

#include <net.h>
#include <auth.h>

void clog_emptyCallback(Clog stream) { };

clog_callback callbacks[CLOG_CALLBACK_COUNT];
int error = 0;

char* clog_readFile(char *path, int *content_len, int *errcode) {
    if(path == 0) {
        *errcode = 1;
        return NULL;
    }

    char *buffer = 0;
    long length;
    FILE * f = fopen (path, "rb");

    if (f)
    {
      fseek (f, 0, SEEK_END);
      length = ftell (f) + 1;
      fseek (f, 0, SEEK_SET);
      buffer = malloc (length);
      if (buffer)
      {
        fread (buffer, 1, length, f);
      }
      fclose (f);
    } else {
        *errcode = 2;
        return NULL;
    }

    *errcode = 0;
    *content_len = length;
    buffer[length - 1] = '\0';
    return buffer;
}

void clog_writeFile(char *name, char *data) {
    FILE *fp = fopen(name, "w");
    if (fp != NULL) {
	fprintf(fp, "%s", data);
        fclose(fp);
	return;
    }
}

int clog_GET(clog_HTTP *http) {
    int err, siz;

    Clog chost;
    err = clog_conn(http->host, http->port, &chost);
    if(err < 0)
	return err;

    err = clog_send(&chost, http->data, http->offset);
    if(err < 0)
	return err;

    siz = clog_recv(chost, http->data, 2048);
    if(siz < 0)
	return siz;

    printf("received : %d\n", siz);

    http->data[siz] = '\0';
    http->body = strstr(http->data, "\r\n\r\n") + 4;
    int total_size = http->body - http->data;
    http->content_len = siz - total_size;
    return 0;
}

clog_HTTP clog_InitGET(char *host, char *path, int port) {
    if(path == NULL)
	path = "";

    clog_HTTP http;
    http.host = host;
    http.port = port;
    http.offset = 0;

    http.data = malloc(2048);
    http.offset += sprintf(http.data, "GET /%s HTTP/1.1\r\n", path);
    clog_AddHeader(&http, "Host", host);

    return http;
}

void clog_AddHeader(clog_HTTP *http, char *key, char *value) {
    http->offset += sprintf(
	http->data + http->offset,
	"%s: %s\r\n",
	key, value
    );
}

void clog_AddCookieF(clog_HTTP *http, char *path) {
    int err, len;
    char *fdata = clog_readFile(path, &len, &err);
    fdata[strcspn(fdata, "\r\n")] = 0;
    http->offset += sprintf(
	http->data + http->offset,
	"Cookie: %s\r\n",
	fdata
    );
    free(fdata);
}

void clog_saveCookies(clog_HTTP *http, char *path) {
    char *cookie_data = strstr(http->data, "Set-Cookie:");
    if(cookie_data == NULL)
	return;
    cookie_data += sizeof("Set-Cookie:");
    int cookie_len = strcspn(cookie_data, "\r\n");

    char save[cookie_len + 1];
    memcpy(save, cookie_data, cookie_len);
    save[cookie_len] = '\0';

    clog_writeFile(path, save);
}

void clog_AddBody(clog_HTTP *http, char *body) {
    if(body == NULL)
	body = "";

    int len = strlen(body);

    // Set Content-Length header before body
    char content_length[8];
    sprintf(content_length, "%d", len);
    clog_AddHeader(http, "Content-Length", content_length);
    
    // Set body
    memcpy(http->data + http->offset, "\r\n", 2);
    memcpy(http->data + http->offset + 2, body, len);
    http->offset += 2 + len;
}

void clog_listener(int type, clog_callback callback) {
    if(callback == NULL)
        return;

    callbacks[type] = callback;
}

int clog_lastError() {
    int last = error;
    error = 0;
    return last;
}

int clog_send(Clog *stream, char *data, int data_len) {
    int success = send(stream->sock_tcp, data, data_len, 0);
    if(success < 0) {
	error = WSAGetLastError();
	return -1;
    }

    return success;
}

int clog_recvUdp(Clog *stream, char *buf, int data_len) {
    struct sockaddr_in from;
    int size = sizeof(from);
    recvfrom(stream->sock_udp, buf, data_len, 0, (struct sockaddr *)&from, &size);

    return 0;
}

int clog_recv(Clog str, char *msg, int size) {
    int num_bytes = recv(str.sock_tcp, msg, size, 0);
    if(num_bytes == SOCKET_ERROR) {
	error = WSAGetLastError();
        return -1;
    }

    return num_bytes;
}

int clog_conn(char* host, int port, Clog *out) {
    authenticate();
    return 0;
    WSADATA wsa;
    Clog this;

    // Socket Init
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
	error = WSAGetLastError();
	return -1;
    }

    if((this.sock_tcp = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
	error = WSAGetLastError();
	return -1;
    }

    // Convert domain to ip
    bool is_domain = false;
    for(int i = 0; i < strlen(host); i++) {
	if(host[i] >= 'A' && host[i] <= 'z') {
	    is_domain = true;
	    break;
	}
    }

    struct sockaddr_in server;
    if(is_domain) {
	struct addrinfo* res = NULL;
	char port_str[16];
	sprintf(port_str, "%d", port);
	int err = getaddrinfo(host, port_str, NULL, &res);
	if(err != 0)
	    return -1;

	server = *(struct sockaddr_in *)res->ai_addr;
    } else {
	server.sin_addr.s_addr = inet_addr(host);
	server.sin_family      = AF_INET;
	server.sin_port        = htons(port);
    }

    // Connect
    if (connect(this.sock_tcp, (struct sockaddr*)&server , sizeof(server)) < 0) {
	error = WSAGetLastError();
        return -1;
    }

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
