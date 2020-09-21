#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char opt_host[256] = "localhost";
static uint16_t opt_port = 0;


void resolve_port(const char* port)
{
    if (*port == '@')
    {
        char linebuffer[256];
        FILE *portfile = fopen(port + 1, "r");
        if (NULL == portfile)
        {
            perror(port + 1);
            exit(1);
        }
        if (NULL == fgets(linebuffer, sizeof(linebuffer), portfile))
        {
            perror(port + 1);
            exit(1);
        }
        fclose(portfile);
        resolve_port(linebuffer);
        return;
    }
    if (strchr(port, ':'))
    {
        strcpy(opt_host, port);
        *strchr(opt_host, ':') = '\0';
        port = strchr(port, ':') + 1;
    }
    opt_port = atoi(port);
}

const struct option LONG_OPTIONS[] = {
    { "port", 1, NULL, 'p' },
    { NULL,   0, NULL, 0 }
};

int main(int argc, char *argv[])
{
    int opt;
    _Bool has_port = false;
    while ((opt = getopt_long(argc, argv, "p:", LONG_OPTIONS, NULL)) != -1)
    {
        switch (opt)
        {
        case 'p':
            resolve_port(optarg);
            has_port = true;
            break;
        }
    }

    if (!has_port)
        resolve_port("@.nrepl-port");
    printf("hostname = %s, port = %u\n", opt_host, opt_port);
    exit(0);
}
