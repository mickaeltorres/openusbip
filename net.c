/*
BSD 3-Clause License

Copyright (c) 2019, Mickael Torres
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "net.h"

int net_listen(unsigned short port, char *addr)
{
  struct sockaddr_in saddr;
  int opt = 1;
  int s;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1)
    perror("socket()");

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    perror("setsockopt()");

  saddr.sin_family = AF_INET;
  saddr.sin_port = htons(port);
  saddr.sin_addr.s_addr = inet_addr(addr);

  if (bind(s, (struct sockaddr*)&saddr, sizeof(saddr)) == -1)
    perror("bind()");

  if (listen(s, 10) == -1)
    perror("listen()");

  return s;
}

void net_serve(int s, void (*serve_fct)(int s, char *addr))
{
  struct sockaddr_in addr;
  socklen_t alen;
  char *caddr;
  int pid;
  int cs;

  signal(SIGCHLD, SIG_IGN);
  for (;;)
  {
    alen = sizeof(addr);
    cs = accept(s, (struct sockaddr *)&addr, &alen);
    if (cs == -1)
      continue;

    if (asprintf(&caddr, "%s:%hu", inet_ntoa(addr.sin_addr),
		 ntohs(addr.sin_port)) < 0)
      perror("asprintf()");

    pid = fork();
    switch (pid)
    {
    case 0:
      close(s);
      serve_fct(cs, caddr);
      exit(EXIT_SUCCESS);
      break;
    default:
      close(cs);
      break;
    }

    free(caddr);
  }
}

int net_read_op(int s, struct net_op *op)
{
  int len;

  len = read(s, op, sizeof(*op));
  if (len != sizeof(*op))
    return -1;

  op->v = ntohs(op->v);
  op->op = ntohs(op->op);
  op->res = ntohl(op->res);

  if (op->v != NET_VERSION)
    return -2;

  return 0;
}

int net_send_op(int s, uint16_t op, uint32_t res)
{
  struct net_op n_op;
  int len;

  n_op.v = htons(NET_VERSION);
  n_op.op = htons(op);
  n_op.res = htonl(res);
  len = write(s, &n_op, sizeof(n_op));
  if (len != sizeof(n_op))
    return -1;
  return 0;
}

int net_send(int s, void *buf, int len)
{
  if (write(s, buf, len) == len)
    return 0;
  return -1;
}

int net_read(int s, void *buf, int len)
{
  int toread;
  int ret;

  toread = 0;

  while (len > 0)
  {
    ret = read(s, buf + toread, len);
    if (ret < 0)
      return -1;
    toread += ret;
    len -= ret;
  }
  return 0;
}

int net_read_import(int s, char *bus)
{
  if (read(s, bus, NET_USB_BUS_MAX) < 1)
    return -1;
  return 0;
}

int net_read_hdr(int s, struct net_generic *hdr)
{
  int len;

  len = read(s, hdr, sizeof(*hdr));
  if (len != sizeof(*hdr))
    return -1;

  hdr->hdr.cmd = ntohl(hdr->hdr.cmd);
  hdr->hdr.seq = ntohl(hdr->hdr.seq);
  hdr->hdr.dev = ntohl(hdr->hdr.dev);
  hdr->hdr.dir = ntohl(hdr->hdr.dir);
  hdr->hdr.endp = ntohl(hdr->hdr.endp);

  return 0;
}

void net_no_delay(int s)
{
  int opt = 1;

  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
}
