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

#include <sys/ioctl.h>
#include <dev/usb/usb.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "process.h"
#include "net.h"

void process_dev_list_request(int s, char *addr)
{
  struct usb_interface_desc idesc;
  usb_device_descriptor_t ddesc;
  struct usb_config_desc cdesc;
  struct usb_device_info dinfo;
  struct net_usb_dev dev;
  struct net_usb_if uif;
  uint32_t ndev;
  int conf;
  int fd;
  int i;

  if (net_send_op(s, NET_OP_SDEVLIST, NET_RES_OK))
  {
    printf("%s: error sending devlist answer\n", addr);
    return;
  }

  fd = open("/dev/ugen0.00", O_RDWR);
  if (fd == -1)
    ndev = htonl(0);
  else
    ndev = htonl(1);
  if (net_send(s, &ndev, sizeof(ndev)))
  {
    printf("%s: error sending devlist\n", addr);
    return;
  }
  if (fd == -1)
    return;

  if (ioctl(fd, USB_GET_DEVICE_DESC, &ddesc) == -1)
  {
    printf("%s: error getting device desc\n", addr);
    return;
  }

  if (ioctl(fd, USB_GET_CONFIG, &conf) == -1)
  {
    printf("%s: error getting device conf\n", addr);
    return;
  }

  if (ioctl(fd, USB_GET_DEVICEINFO, &dinfo) == -1)
  {
    printf("%s: error getting device info\n", addr);
    return;
  }

  cdesc.ucd_config_index = USB_CURRENT_CONFIG_INDEX;
  if (ioctl(fd, USB_GET_CONFIG_DESC, &cdesc) == -1)
  {
    printf("%s: error getting device config desc\n", addr);
    return;
  }

  strlcpy(dev.dev, "usb0", NET_USB_DEV_MAX);
  snprintf(dev.bus, NET_USB_BUS_MAX, "usb0");
  dev.bus_n = htonl(dinfo.udi_bus);
  dev.dev_n = htonl(dinfo.udi_addr);
  dev.dev_speed = htonl(dinfo.udi_speed);
  dev.vid = htons(dinfo.udi_vendorNo);
  dev.pid = htons(dinfo.udi_productNo);
  dev.bcd = htons(dinfo.udi_releaseNo);
  dev.class = ddesc.bDeviceClass;
  dev.sub_class = ddesc.bDeviceSubClass;
  dev.proto = ddesc.bDeviceProtocol;
  dev.conf = conf;
  dev.conf_n = ddesc.bNumConfigurations;
  dev.if_n = cdesc.ucd_desc.bNumInterface;
  if (net_send(s, &dev, sizeof(dev)))
  {
    printf("%s: error sending dev info\n", addr);
    return;
  }

  for (i = 0; i < dev.if_n; i++)
  {
    idesc.uid_config_index = USB_CURRENT_CONFIG_INDEX;
    idesc.uid_interface_index = i;
    idesc.uid_alt_index = USB_CURRENT_ALT_INDEX;
    if (ioctl(fd, USB_GET_INTERFACE_DESC, &idesc) == -1)
    {
      printf("%s: error getting interface %d:%d\n",
	     addr, cdesc.ucd_desc.iConfiguration, i);
      return;
    }
    uif.class = idesc.uid_desc.bInterfaceClass;
    uif.sub_class = idesc.uid_desc.bInterfaceSubClass;
    uif.proto = idesc.uid_desc.bInterfaceProtocol;
    if (net_send(s, &uif, sizeof(uif)))
    {
      printf("%s: error sending interface\n", addr);
      return;
    }
  }
}

void process_get_desc_dev(int s, usb_device_descriptor_t *ddesc,
			 struct net_submit *submit, char *addr)
{
  struct net_submit_ret ret;

  ret.hdr.cmd = htonl(3);
  ret.hdr.seq = htonl(submit->hdr.seq);
  ret.hdr.dev = 0;
  ret.hdr.dir = 0;
  ret.hdr.endp = 0;
  ret.ret = 0;
  ret.len = htonl(sizeof(*ddesc));
  ret.sfrm = 0;
  ret.pkt_n = 0;
  ret.err_n = 0;

  if (net_send(s, &ret, sizeof(ret)))
    printf("%s: pgdd: error sending hdr\n", addr);
  if (net_send(s, ddesc, sizeof(*ddesc)))
    printf("%s: pgdd: error sending data\n", addr);
}

void process_get_desc_conf(int s, int *fd, struct net_submit *submit,
			   char *addr)
{
  struct net_submit_ret ret;
  struct usb_full_desc full;
  u_char buf[1024];
  int rlen;

  rlen = *(uint16_t *)(submit->setup + 6);
  if (rlen > 1024)
  {
    printf("%s: too big buffer requested\n", addr);
    return;
  }

  full.ufd_config_index = submit->setup[2];
  full.ufd_size = 1024;
  full.ufd_data = buf;
  if (ioctl(fd[0], USB_GET_FULL_DESC, &full) == -1)
  {
    printf("%s: cannot get full descriptors\n", addr);
    return;
  }

  ret.hdr.cmd = htonl(3);
  ret.hdr.seq = htonl(submit->hdr.seq);
  ret.hdr.dev = 0;
  ret.hdr.dir = 0;
  ret.hdr.endp = 0;
  ret.ret = 0;
  ret.len = htonl(rlen);
  ret.sfrm = 0;
  ret.pkt_n = 0;
  ret.err_n = 0;

  if (net_send(s, &ret, sizeof(ret)))
    printf("%s: pgdc: error sending hdr\n", addr);
  if (net_send(s, buf, rlen))
    printf("%s: pgdc: error sending data\n", addr);
}

void process_get_desc_str(int s, int *fd, struct net_submit *submit,
			   char *addr)
{
  struct usb_ctl_request req;
  struct net_submit_ret ret;
  char buf[1024];
  int rlen;

  rlen = *(uint16_t *)(submit->setup + 6);
  if (rlen > 1024)
  {
    printf("%s: string desc request too big\n", addr);
    return;
  }
  memcpy(&(req.ucr_request), submit->setup, 8); // copy the setup packet
  req.ucr_addr = 0;
  req.ucr_data = buf;
  req.ucr_flags = USBD_SHORT_XFER_OK;
  if (ioctl(fd[0], USB_DO_REQUEST, &req) == -1)
  {
    printf("%s: cannot get string descriptor\n", addr);
    return;
  }
  ret.hdr.cmd = htonl(3);
  ret.hdr.seq = htonl(submit->hdr.seq);
  ret.hdr.dev = 0;
  ret.hdr.dir = 0;
  ret.hdr.endp = 0;
  ret.ret = 0;
  ret.len = htonl(rlen);
  ret.sfrm = 0;
  ret.pkt_n = 0;
  ret.err_n = 0;

  if (net_send(s, &ret, sizeof(ret)))
    printf("%s: pgds: error sending hdr\n", addr);
  if (net_send(s, buf, rlen))
    printf("%s: pgds: error sending data\n", addr);
}

void process_get_desc(int s, int *fd, usb_device_descriptor_t *ddesc,
		      struct net_submit *submit, char *addr)
{
  switch(submit->setup[3])
  {
  case 1:
    process_get_desc_dev(s, ddesc, submit, addr);
    return;
  case 2:
    process_get_desc_conf(s, fd, submit, addr);
    return;
  case 3:
    process_get_desc_str(s, fd, submit, addr);
    return;
  }
  printf("%s: unknown descriptor requested: %x\n", addr, submit->setup[3]);
  return;
}

void process_set_conf(int s, int *fd, struct net_submit *submit, char *addr,
		      char *ugen)
{
  struct net_submit_ret ret;
  char *udev;
  int conf;
  int i;

  // first close all opened endpoints, except control
  for (i = 1; i < 16; i++)
    if (fd[i] != -1)
    {
      close(fd[i]);
      fd[i] = -1;
    }

  // setconf
  conf = *(uint16_t *)(submit->setup + 2);
  if (ioctl(fd[0], USB_SET_CONFIG, &conf) == -1)
  {
    printf("%s: cannot set conf %d\n", addr, conf);
    return;
  }
  ret.hdr.cmd = htonl(3);
  ret.hdr.seq = htonl(submit->hdr.seq);
  ret.hdr.dev = 0;
  ret.hdr.dir = 0;
  ret.hdr.endp = 0;
  ret.ret = 0;
  ret.len = 0;
  ret.sfrm = 0;
  ret.pkt_n = 0;
  ret.err_n = 0;

  if (net_send(s, &ret, sizeof(ret)))
    printf("%s: psc: error sending hdr\n", addr);

  // now re-open all endpoints
  for (i = 1; i < 16; i++)
  {
    asprintf(&udev, "/dev/ugen%s.%02d", ugen, i);
    if (udev == NULL)
    {
      printf("%s: malloc() error\n", addr);
      return;
    }
    fd[i] = open(udev, O_RDWR);
    if (fd[i] == -1)
      fd[i] = open(udev, O_RDONLY);
    if (fd[i] == -1)
      fd[i] = open(udev, O_WRONLY);
    conf = 1;
    if (fd[i] != -1)
      if (ioctl(fd[i], USB_SET_SHORT_XFER, &conf) == -1)
	printf("%s: cannot set short transfers on endp%d\n", addr, i);
    free(udev);
  }
}

void process_usb_ctl_req(int s, int *fd, struct net_submit *submit,
			 char *addr)
{
  struct usb_ctl_request req;
  struct net_submit_ret ret;
  char buf[1024];
  int rlen;
  int dir;

  rlen = *(uint16_t *)(submit->setup + 6);
  if (rlen > 1024)
  {
    printf("%s: ctl request too big\n", addr);
    return;
  }

  dir = ((submit->setup[0] >> 7) & 1);
  if (!dir) // host to device
  {
    if (net_read(s, buf, rlen))
    {
      printf("%s: cannot read ctl payload\n", addr);
      return;
    }
  }

  memcpy(&(req.ucr_request), submit->setup, 8); // copy the setup packet
  req.ucr_addr = 0;
  req.ucr_data = buf;
  req.ucr_flags = USBD_SHORT_XFER_OK;
  if (ioctl(fd[0], USB_DO_REQUEST, &req) == -1)
  {
    printf("%s: cannot do ctl request %02x%02x%02x%02x%02x%02x%02x%02x: %s\n",
	   addr,
	   submit->setup[0],
	   submit->setup[1],
	   submit->setup[2],
	   submit->setup[3],
	   submit->setup[4],
	   submit->setup[5],
	   submit->setup[6],
	   submit->setup[7],
	   strerror(errno));
    return;
  }
  ret.hdr.cmd = htonl(3);
  ret.hdr.seq = htonl(submit->hdr.seq);
  ret.hdr.dev = 0;
  ret.hdr.dir = 0;
  ret.hdr.endp = 0;
  ret.ret = 0;
  ret.len = dir ? htonl(rlen) : 0;
  ret.sfrm = 0;
  ret.pkt_n = 0;
  ret.err_n = 0;

  if (net_send(s, &ret, sizeof(ret)))
    printf("%s: pucr: error sending hdr\n", addr);
  if (dir)
    if (net_send(s, buf, rlen))
      printf("%s: pucr: error sending data\n", addr);
}

void process_usb_req(int s, int *fd, struct net_submit *submit,
		     char *addr)
{
  struct net_submit_ret ret;
  char buf[32768];
  int endp;
  int rlen;
  int len;
  int res;
  int dir;

  rlen = submit->len;
  if (rlen > 32768)
  {
    printf("%s: request too big\n", addr);
    return;
  }

  bzero(buf, rlen);
  res = 0;
  endp = submit->hdr.endp;
  dir = submit->hdr.dir;
  if (!dir) // host to device
  {
    if (net_read(s, buf, rlen))
    {
      printf("%s: cannot read payload\n", addr);
      return;
    }
    len = write(fd[endp], buf, rlen);
    if (len < 0)
    {
      printf("%s: cannot write to endpoint %d\n", addr, endp);
      len = 0;
      res = -errno;
    }
  }
  else // device to host
  {
    len = read(fd[endp], buf, rlen);
    if (len < 0)
    {
      printf("%s: cannot read from endpoint %d: %s %d\n", addr, endp,
	     strerror(errno), errno);
      len = 0;
      res = -errno;
    }
  }

  ret.hdr.cmd = htonl(3);
  ret.hdr.seq = htonl(submit->hdr.seq);
  ret.hdr.dev = 0;
  ret.hdr.dir = 0;
  ret.hdr.endp = 0;
  ret.ret = htonl(res);
  ret.len = dir ? htonl(len) : 0;
  ret.sfrm = 0;
  ret.pkt_n = 0;
  ret.err_n = 0;

  if (net_send(s, &ret, sizeof(ret)))
    printf("%s: pur: error sending hdr\n", addr);
  if (dir && len)
    if (net_send(s, buf, len))
      printf("%s: pur: error sending data\n", addr);
}

void process_submit(int s, int *fd, int conf,
		    usb_device_descriptor_t *ddesc,
		    struct usb_device_info *dinfo,
		    struct usb_config_desc *cdesc,
		    char *addr,
		    struct net_submit *submit,
		    char *ugen)
{
  submit->fl = ntohl(submit->fl);
  submit->len = ntohl(submit->len);
  submit->sfrm = ntohl(submit->sfrm);
  submit->pkt_n = ntohl(submit->pkt_n);
  submit->intv = ntohl(submit->intv);
  if (submit->hdr.endp == 0)
  {
    switch(submit->setup[1])
    {
    case 6:
      if (submit->setup[0] == 0x80)
      {
	process_get_desc(s, fd, ddesc, submit, addr);
	return;
      }
      break;
    case 9:
      if (submit->setup[0] == 0)
      {
	process_set_conf(s, fd, submit, addr, ugen);
	return;
      }
      break;
    }
    process_usb_ctl_req(s, fd, submit, addr);
    return;
  }
  process_usb_req(s, fd, submit, addr);
}

void process_unlink(int s, int *fd, char *addr, struct net_unlink *unlink)
{
}

void process_kern_client(int s, int *fd, int conf,
			 usb_device_descriptor_t *ddesc,
			 struct usb_device_info *dinfo,
			 struct usb_config_desc *cdesc,
			 char *addr, char *ugen)
{
  struct net_generic hdr;

  for (;;)
  {
    if (net_read_hdr(s, &hdr))
      return;
    switch(hdr.hdr.cmd)
    {
    case 1:
      process_submit(s, fd, conf, ddesc, dinfo, cdesc, addr,
		     (struct net_submit *)&hdr, ugen);
      break;
    case 2:
      process_unlink(s, fd, addr, (struct net_unlink *)&hdr);
      break;
    default:
      printf("%s: unknown request (%u)\n", addr, hdr.hdr.cmd);
      return;
    }
  }
}

void process_import_request(int s, char *addr)
{
  usb_device_descriptor_t ddesc;
  struct usb_config_desc cdesc;
  struct usb_device_info dinfo;
  struct net_usb_dev dev;
  char bus[NET_USB_BUS_MAX + 1];
  char *udev;
  int fd[16];
  int conf;
  int res;
  int i;

  bzero(bus, NET_USB_BUS_MAX + 1);
  if (net_read_import(s, bus))
    return;

  for (udev = bus + 3; *udev; udev++)
    if (!isdigit(*udev))
    {
      printf("%s: bad device requested\n", addr);
      return;
    }

  for (i = 0; i < 16; i++)
  {
    asprintf(&udev, "/dev/ugen%s.%02d", bus + 3, i);
    if (udev == NULL)
    {
      printf("%s: malloc() error\n", addr);
      return;
    }
    fd[i] = open(udev, O_RDWR);
    free(udev);
  }
  if (fd[0] == -1)
    res = NET_RES_NODEV;
  else
  {
    net_no_delay(s);
    res = NET_RES_OK;
  }

  if (net_send_op(s, NET_OP_SIMPORT, res))
  {
    printf("%s: error sending import answer\n", addr);
    return;
  }

  if (res != NET_RES_OK)
    return;

  if (ioctl(fd[0], USB_GET_DEVICE_DESC, &ddesc) == -1)
  {
    printf("%s: error getting device desc\n", addr);
    return;
  }

  if (ioctl(fd[0], USB_GET_CONFIG, &conf) == -1)
  {
    printf("%s: error getting device conf\n", addr);
    return;
  }

  if (ioctl(fd[0], USB_GET_DEVICEINFO, &dinfo) == -1)
  {
    printf("%s: error getting device info\n", addr);
    return;
  }

  cdesc.ucd_config_index = USB_CURRENT_CONFIG_INDEX;
  if (ioctl(fd[0], USB_GET_CONFIG_DESC, &cdesc) == -1)
  {
    printf("%s: error getting device config desc\n", addr);
    return;
  }

  strlcpy(dev.dev, "usb0", NET_USB_DEV_MAX);
  snprintf(dev.bus, NET_USB_BUS_MAX, "usb0");
  dev.bus_n = htonl(dinfo.udi_bus);
  dev.dev_n = htonl(dinfo.udi_addr);
  dev.dev_speed = htonl(dinfo.udi_speed);
  dev.vid = htons(dinfo.udi_vendorNo);
  dev.pid = htons(dinfo.udi_productNo);
  dev.bcd = htons(dinfo.udi_releaseNo);
  dev.class = ddesc.bDeviceClass;
  dev.sub_class = ddesc.bDeviceSubClass;
  dev.proto = ddesc.bDeviceProtocol;
  dev.conf = conf;
  dev.conf_n = ddesc.bNumConfigurations;
  dev.if_n = cdesc.ucd_desc.bNumInterface;
  if (net_send(s, &dev, sizeof(dev)))
  {
    printf("%s: error sending dev info\n", addr);
    return;
  }

  // we now receive requests from the kernel driver directly
  process_kern_client(s, fd, conf, &ddesc, &dinfo, &cdesc, addr, bus + 3);
}

void process_client(int s, char *addr)
{
  struct net_op op;
  int r;

  r = net_read_op(s, &op);
  if (r)
  {
    printf("%s: error reading op\n", addr);
    return;
  }

  switch (op.op)
  {
  case NET_OP_RDEVLIST:
    process_dev_list_request(s, addr);
    break;
  case NET_OP_RIMPORT:
    process_import_request(s, addr);
    break;
  default:
    printf("%s: unknown op (%hx)\n", addr, op.op);
    break;
  }
}
