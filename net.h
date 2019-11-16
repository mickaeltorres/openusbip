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

#ifndef NET_H
#define NET_H

#define NET_VERSION 0x111

#define NET_RES_OK 0x00
#define NET_RES_NODEV 0x04

#define NET_OP_RDEVLIST 0x8005
#define NET_OP_SDEVLIST 0x0005

#define NET_OP_RIMPORT 0x8003
#define NET_OP_SIMPORT 0x0003

struct net_op
{
  uint16_t v;
  uint16_t op;
  uint32_t res;
} __attribute__((packed));

#define NET_USB_DEV_MAX 256
#define NET_USB_BUS_MAX 32

struct net_usb_dev
{
  char dev[NET_USB_DEV_MAX];
  char bus[NET_USB_BUS_MAX];
  uint32_t bus_n;
  uint32_t dev_n;
  uint32_t dev_speed;
  uint16_t vid;
  uint16_t pid;
  uint16_t bcd;
  uint8_t class;
  uint8_t sub_class;
  uint8_t proto;
  uint8_t conf;
  uint8_t conf_n;
  uint8_t if_n;
} __attribute__((packed));

struct net_usb_if
{
  uint8_t class;
  uint8_t sub_class;
  uint8_t proto;
  uint8_t pad;
} __attribute__((packed));

struct net_hdr
{
  uint32_t cmd;
  uint32_t seq;
  uint32_t dev;
  uint32_t dir;
  uint32_t endp;
} __attribute__((packed));

struct net_generic
{
  struct net_hdr hdr;
  char pad[28];
} __attribute__((packed));

struct net_submit
{
  struct net_hdr hdr;
  uint32_t fl;
  int32_t len;
  int32_t sfrm;
  int32_t pkt_n;
  int32_t intv;
  uint8_t setup[8];
} __attribute__((packed));

struct net_unlink
{
  struct net_hdr hdr;
  uint32_t seq;
} __attribute__((packed));

struct net_submit_ret
{
  struct net_hdr hdr;
  uint32_t ret;
  int32_t len;
  int32_t sfrm;
  int32_t pkt_n;
  int32_t err_n;
  uint8_t setup[8];
};

struct net_unlink_ret
{
  struct net_hdr hdr;
  int32_t ret;
};

int net_listen(unsigned short port, char *addr);
void net_serve(int s, void (*serve_fct)(int s, char *addr));
int net_read_op(int s, struct net_op *op);
int net_send_op(int s, uint16_t op, uint32_t res);
int net_send(int s, void *buf, int len);
int net_read(int s, void *buf, int len);
int net_read_import(int s, char *bus);
int net_read_hdr(int s, struct net_generic *hdr);
void net_no_delay(int s);

#endif
