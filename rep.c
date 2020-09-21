#include <alloca.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

struct sockaddr_in opt_port =
{
    .sin_family = AF_INET,
    .sin_addr = htonl(INADDR_LOOPBACK),
    .sin_port = 0,
};

int nrepl_sock = -1;

void nrepl_exec(const char* code)
{
    printf("::0\n");
    fflush(stdout);

    nrepl_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nrepl_sock == -1)
    {
        perror("socket");
        exit(1);
    }

    printf("::00 %s %u\n", inet_ntoa(opt_port.sin_addr), ntohs(opt_port.sin_port));
    fflush(stdout);

    if (-1 == connect(nrepl_sock, (struct sockaddr*)&opt_port, sizeof(opt_port)))
    {
        perror("connect");
        exit(1);
    }

    printf("::1\n");
    fflush(stdout);

    char message[512];
    //snprintf(message, sizeof(message), "d2:op4:eval4:code%lu:%se\n", strlen(code), code);
    snprintf(message, sizeof(message), "d2:op5:clonee");

    printf("::2 %s\n", message);
    fflush(stdout);

    if (send(nrepl_sock, message, strlen(message), 0) != strlen(message))
    {
        perror("send");
        exit(1);
    }

    int message_offset = 0;
    for (;;)
    {
        int result = recv(nrepl_sock, message + message_offset, sizeof(message) - message_offset, 0);
        if (result < 0)
        {
            perror("recv");
            exit(1);
        }

        fwrite(message + message_offset, 1, result, stdout);
        fflush(stdout);

        message_offset += result;
    }

    close(nrepl_sock);
}


char *read_file(const char* filename, char* buffer, size_t buffer_size)
{
    FILE *portfile = fopen(filename, "r");
    if (NULL == portfile)
    {
        fclose(portfile);
        return NULL;
    }
    if (NULL == fgets(buffer, buffer_size, portfile))
    {
        fclose(portfile);
        return NULL;
    }
    fclose(portfile);
    return buffer;
}

void resolve_port_option(const char* port)
{
    if (*port == '@')
    {
        char linebuffer[256];
        if (!read_file(port + 1, linebuffer, sizeof(linebuffer)))
        {
            perror(port + 1);
            exit(1);
        }
        resolve_port_option(linebuffer);
        return;
    }
    if (strchr(port, ':'))
    {
        char *host_part = alloca(strlen(port));
        strcpy(host_part, port);
        *strchr(host_part, ':') = '\0';

        opt_port.sin_addr.s_addr = inet_addr(host_part);
        if (opt_port.sin_addr.s_addr == INADDR_NONE)
        {
            struct hostent *ent = gethostbyname(host_part);
            if (NULL == ent)
            {
                perror(host_part);
                exit(1);
            }
            if (NULL == ent->h_addr)
            {
                fprintf(stderr, "%s has no addresses\n", host_part);
                exit(1);
            }
            opt_port.sin_addr = *(struct in_addr*)ent->h_addr;
        }

        port = strchr(port, ':') + 1;
    }
    opt_port.sin_port = htons(atoi(port));
}

const struct option LONG_OPTIONS[] =
{
    { "port", 1, NULL, 'p' },
    { NULL,   0, NULL, 0 }
};

char* collect_code(int argc, char *argv[], int start)
{
    size_t code_size = 0;
    for (int i = optind; i < argc; ++i)
        code_size += strlen(argv[i]) + 1;
    char *code = (char *)malloc(code_size);
    code[0] = '\0';
    for (int i = optind; i < argc; ++i)
    {
        if (code[0])
            strcat(code, " ");
        strcat(code, argv[i]);
    }
    return code;
}

int main(int argc, char *argv[])
{
    int opt;
    _Bool has_port = false;
    while ((opt = getopt_long(argc, argv, "p:", LONG_OPTIONS, NULL)) != -1)
    {
        switch (opt)
        {
        case 'p':
            resolve_port_option(optarg);
            has_port = true;
            break;

        case '?':
            exit(1);
        }
    }

    if (!has_port)
        resolve_port_option("@.nrepl-port");

    char* code = collect_code(argc, argv, optind);
    nrepl_exec(code);
    free(code);
    exit(0);
}
