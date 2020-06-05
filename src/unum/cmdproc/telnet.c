// (c) 2018-2019 minim.co
// unum fetch_urls generic command handling

#include "unum.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

// From: http://pcmicro.com/netfoss/telnet.html
#define SE 0xF0
#define SB 0xFA
#define WILL 0xFB
#define WONT 0xFC
#define DO 0xFD
#define IAC 0xFF
#define NAWS 31

int negotiate_window_size(int socket, unsigned char *buffer, int len)
{
    if (buffer[0] == IAC && buffer[1] == DO && buffer[2] == NAWS) {
        // If server requests NAWS, tell it yes
        unsigned char agree_naws[] = { IAC, WILL, NAWS };
        if (send(socket, agree_naws, sizeof(agree_naws), 0) < 0) {
            return -1;
        }

        unsigned char window_width_height[] = { IAC, SB, NAWS, 0, 80, 0, 24, IAC, SE };
        if (send(socket, window_width_height, sizeof(window_width_height), 0) < 0) {
            return -1;
        }
    } else {
        // If the server asks for anything else, tell it to go fish
        int i;
        for (i = 0; i < len; i++) {
            if (buffer[i] == DO) {
                buffer[i] = WONT;
            } else if (buffer[i] == WILL) {
                buffer[i] = DO;
            }
        }
        if (send(socket, buffer, len, 0) < 0) {
            return -1;
        }
    }

    return 0;
}

int manage_telnet_socket(int socket, TELNET_INFO_t *info)
{
    struct timeval ts;
    unsigned char buffer[3];
    int len;
    fd_set file_descriptor;
    int pauses = 0;

    while ((pauses < 10) && (info->length < sizeof(info->output) - 1))
    {
        FD_ZERO(&file_descriptor);
        if (socket != 0) {
            FD_SET(socket, &file_descriptor);
        }
        FD_SET(0, &file_descriptor);

        ts.tv_sec = 1;
        ts.tv_usec = 0;
        int nready = select(socket + 1, &file_descriptor, NULL, NULL, &ts);
        if (nready < 0) {
            log("%s: Error: select() returned negative length!\n", __func__);
            return -1;
        } else if (nready == 0) {
            if (pauses == 0) {
                send(socket, info->username, strlen(info->username), 0);
                send(socket, "\n", 1, 0);
            } else if (pauses == 1) {
                send(socket, info->password, strlen(info->password), 0);
                send(socket, "\n", 1, 0);
            }
            pauses += 1;
        } else if (nready > 0) {
            if (socket != 0 && FD_ISSET(socket, &file_descriptor)) {
                if ((len = recv(socket, buffer, 1, 0)) < 0) {
                    log("%s: Error: recv() returned negative length!\n", __func__);
                    return -1;
                } else if (len == 0) {
                    return 0;
                }

                if (buffer[0] == IAC) {
                    len = recv(socket, buffer + 1, 2, 0);
                    if (len < 0) {
                        log("%s: Error: recv() returned negative length!\n", __func__);
                        return -1;
                    } else if (len == 0) {
                        return 0;
                    }
                    if (negotiate_window_size(socket, buffer, 3) != 0) {
                        log("%s: Error: Could not negotiate a window size correctly!\n", __func__);
                        return -1;
                    }
                } else if (pauses > 1) {
                    info->output[info->length] = buffer[0];
                    info->length += 1;
                }
            }
        }
    }
    return 0;
}

int attempt_telnet_login(TELNET_INFO_t *info)
{
    int retval = 0;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        log("%s: Error: Could not create socket!\n", __func__);
        return -1;
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(info->ip_addr);
    server.sin_family = AF_INET;
    server.sin_port = htons(info->port);
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        log("%s: Error: Could not connect to server!\n", __func__);
        retval = -1;
    }

    if (retval == 0) {
        retval = manage_telnet_socket(sock, info);
        info->output[info->length] = '\0';
    }
    close(sock);
    return retval;
}

int split_telnet_uri(TELNET_INFO_t *info, char *uri, int length)
{
    info->port = 23;
    info->ip_addr[0] = '\0';
    info->username[0] = '\0';
    info->password[0] = '\0';

    char *offset = memchr(uri, ':', length);
    if (!offset) {
        return -1;
    }
    int len = offset - uri;
    strncat(info->username, uri, MIN(MIN(len, MAX_URI_PART - 1), length - 1));
    length -= len + 1;
    uri = offset + 1;

    offset = memchr(uri, '@', length);
    if (!offset) {
        return -1;
    }
    len = offset - uri;
    strncat(info->password, uri, MIN(MIN(len, MAX_URI_PART - 1), length - 1));
    length -= len + 1;
    uri = offset + 1;

    offset = memchr(uri, ':', length);
    if (offset) {
        len = offset - uri;
        strncat(info->ip_addr, uri, MIN(MIN(len, MAX_URI_PART - 1), length - 1));
        length -= len + 1;

        uri = offset + 1;
        info->port = atoi(uri);
    } else {
        strncat(info->ip_addr, uri, MIN(length, MAX_URI_PART - 1));
    }

    return 0;
}
