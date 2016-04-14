/*
 * Copyright 2004 Bill Cheswick <ches@cheswick.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Cheswick.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
#include <assert.h>

#include <ppm.h>
#include <pnm.h>

#include <dev/usb/usb.h>

#include "libutil/util.h"
#include "ov.h"
#include "ov511.h"

extern int debug;

struct vidstate {
  enum {SKIPPING, READING, DONE} state;
  int width, height;		/* image dimensions */
  xel **xels;			/* output image data */

  int segsize;			/* size of each data segment */

  int iY, jY;			/* current horizontal/vertical Y coords */
  int iUV, jUV;			/* current horizontal/vertical UV coords */
  
  u_char buf[2048];		/* segment data being processed */
  int residue;			/* left over from last processing run */
};

static long long start = 0;

long long
now(void) {
	struct timeval tp;
	long long t;

	if (gettimeofday(&tp, 0) < 0)
		perror("gettimeofday");
	t = tp.tv_sec*1000000 + tp.tv_usec;
	if (start == 0)
		start = t;
	return t - start;
}

FILE *watchfile = NULL;		/* if write picture acquisition information */

void
start_watch(char *template) {
	char wfn[100];

	snprintf(wfn, sizeof(wfn), "%s.log", template);
	watchfile = fopen(wfn, "w");
	if (watchfile == NULL) {
		perror("start_watch: opening file");
		exit(1);
	}
}

void
end_watch(void) {
	if (watchfile == NULL)
		return;
	fclose(watchfile);
	watchfile = NULL;
}

long long last_time = -1;

#ifdef notdef
void
watch_write(char *fmt, ...) {
	char buf[400];
	long long t = now();
	long long diff;
	va_list ap;
	time_t tt = time(0);
	char *ts = ctime(&tt);
	char stamp[50];

	strncpy(stamp, &ts[14], 5);
	stamp[5] = '\0';

	if (watchfile == NULL)
		return;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (last_time < 0)
		diff = 0;
	else
		diff = t - last_time;
	last_time = t;

	fprintf(watchfile, "%s %5lld  %s\n", stamp, diff, buf);
}
#endif

static int
ov511_reg_read(int fd, int reg) {
  struct usb_ctl_request ur;
  unsigned char data[1024];
  
  ur.ucr_request.bmRequestType = UT_READ_VENDOR_INTERFACE;
  ur.ucr_request.bRequest = 2;
  
  USETW(ur.ucr_request.wValue, 0);	/* unused */
  USETW(ur.ucr_request.wIndex, reg); /* index */
  USETW(ur.ucr_request.wLength, 1);	/* payload len in bytes */
  ur.ucr_data = data;
  ur.ucr_flags = 0;
  ur.ucr_actlen = 0;
  
  if(ioctl(fd, USB_DO_REQUEST, &ur) < 0) {
    return -1;
  }

  return data[0] & 0xFF;
}

static int
ov511_reg_write(int fd, int reg, int val) {
  struct usb_ctl_request ur;
  unsigned char data[1024];

  data[0] = val;
  
  ur.ucr_request.bmRequestType = UT_WRITE_VENDOR_INTERFACE;
  ur.ucr_request.bRequest = 2;
  
  USETW(ur.ucr_request.wValue, 0);	/* unused */
  USETW(ur.ucr_request.wIndex, reg); /* index */
  USETW(ur.ucr_request.wLength, 1);	/* payload len in bytes */
  ur.ucr_data = data;
  ur.ucr_flags = 0;
  ur.ucr_actlen = 0;
  
  if(ioctl(fd, USB_DO_REQUEST, &ur) < 0) {
    return -1;
  }

  return 0;
}

static int
ov511_i2c_read(int fd, int reg) {
  int status = 0;
  int val = 0;
  int retries = OV7610_I2C_RETRIES;

  while(--retries >= 0) {
    /* wait until bus idle */
    do {
      if((status = ov511_reg_read(fd, OV511_REG_I2C_CONTROL)) < 0)
	return -1;
    } while((status & 0x01) == 0);

    /* perform a dummy write cycle to set the register */
    if(ov511_reg_write(fd, OV511_REG_SMA, reg) < 0)
      return -1;

    /* initiate the dummy write */
    if(ov511_reg_write(fd, OV511_REG_I2C_CONTROL, 0x03) < 0)
      return -1;

    /* wait until bus idle */
    do {
      if((status = ov511_reg_read(fd, OV511_REG_I2C_CONTROL)) < 0)
	return -1;
    } while((status & 0x01) == 0);

    if((status & 0x2) == 0)
      break;
  }

  if(retries < 0)
    return -1;

  retries = OV7610_I2C_RETRIES;
  while(--retries >= 0) {
    /* initiate read */
    if(ov511_reg_write(fd, OV511_REG_I2C_CONTROL, 0x05) < 0)
      return -1;

    /* wait until bus idle */
    do {
      if((status = ov511_reg_read(fd, OV511_REG_I2C_CONTROL)) < 0)
	return -1;
    } while ((status & 0x01) == 0);
    
    if((status & 0x2) == 0)
      break;

    /* abort I2C bus before retrying */
    if(ov511_reg_write(fd, OV511_REG_I2C_CONTROL, 0x10) < 0)
      return -1;
  }
  if(retries < 0)
    return -1;
  
  /* retrieve data */
  val = ov511_reg_read(fd, OV511_REG_SDA);

  /* issue another read for some weird reason */
  if(ov511_reg_write(fd, OV511_REG_I2C_CONTROL, 0x05) < 0)
    return -1;

  return val;
}

static int
ov511_i2c_write(int fd, int reg, int val) {
  int status = 0;
  int retries = OV7610_I2C_RETRIES;

  while(--retries >= 0) {
    if(ov511_reg_write(fd, OV511_REG_SWA, reg) < 0)
      return -1;
    
    if(ov511_reg_write(fd, OV511_REG_SDA, val) < 0)
      return -1;
    
    if(ov511_reg_write(fd, OV511_REG_I2C_CONTROL, 0x1) < 0)
      return -1;

    /* wait until bus idle */
    do {
      if((status = ov511_reg_read(fd, OV511_REG_I2C_CONTROL)) < 0)
	return -1;
    } while((status & 0x01) == 0);

    /* OK if ACK */
    if((status & 0x02) == 0)
      return 0;
  }

  return -1;
}

static struct vidstate vs;		/* current read state */

static void
progsegment(struct vidstate *vsp, u_char *data) {
  int i, j;
  u_char *Up = data, *Vp = data + 64, *Yp = data + 128;
  xel *xp;

  for(j = 0; j < 8; ++j) {
    xp = vsp->xels[vsp->jUV + j * 2] + vsp->iUV;

    for(i = 0; i < 8; ++i) {
      PPM_ASSIGN(*xp, *Up, PPM_GETG(*xp), *Vp);
      Up++;
      Vp++;
      xp += 2;
    }
  }

  for(j = 0; j < 8; ++j) {
    xp = vsp->xels[vsp->jY + j] + vsp->iY;
    
    for(i = 0; i < 8; ++i) {
      PPM_ASSIGN(xp[0], PPM_GETR(xp[0]), Yp[0], PPM_GETB(xp[0]));
      PPM_ASSIGN(xp[8], PPM_GETR(xp[8]), Yp[64], PPM_GETB(xp[8]));
      PPM_ASSIGN(xp[16], PPM_GETR(xp[16]), Yp[128], PPM_GETB(xp[16]));
      PPM_ASSIGN(xp[24], PPM_GETR(xp[24]), Yp[192], PPM_GETB(xp[24]));
      Yp++;
      xp++;
    }
  }
}

static void
procdata(struct vidstate *vsp, u_char *data, size_t datalen) {
  size_t len = vsp->residue + datalen;
  int offset = 0;

/*watch_write("procdata");*/
  memcpy(vsp->buf + vsp->residue, data, datalen);

  while(len - offset >= vsp->segsize) {
    if(vsp->jY >= vsp->height && vsp->jUV >= vsp->height) {
      vsp->state = DONE;
// watch_write("procdata, state=DONE");
      return;
    } else if(vsp->jY >= vsp->height || vsp->jUV >= vsp->height) {
      vsp->state = SKIPPING;
// watch_write("procdata, state=SKIPPING");
      return;
    }
       
    progsegment(vsp, vsp->buf + offset);

    vsp->iY += 32;		/* each segment contains 32x8 Y pixels */
    if(vsp->iY >= vsp->width) {
      vsp->iY = 0;
      vsp->jY += 8;
    }

    vsp->iUV += 16;		/* and 16x16 downsampled U and V pixels */
    if(vsp->iUV >= vsp->width) {
      vsp->iUV = 0;
      vsp->jUV += 16;
    }

    offset += vsp->segsize;
  }
  vsp->residue = len - offset;
  memmove(vsp->buf, vsp->buf + offset, len - offset);
/*watch_write("procdata, end");*/
}

/* The first 64 bytes of each segment are U, the next 64 are V.  The U
   and V are arranged as follows:

  0  1 ...  7
  8  9 ... 15
       ...   
 56 57 ... 63

 U and V are shipped at half resolution (1 U,V sample -> one 2x2 block).

 The next 256 bytes are full resolution Y data and represent 4 
 squares of 8x8 pixels as follows:

  0  1 ...  7    64  65 ...  71   ...  192 193 ... 199
  8  9 ... 15    72  73 ...  79        200 201 ... 207
       ...              ...                    ...
 56 57 ... 63   120 121     127        248 249 ... 255

 Note that the U and V data in one segment represents a 16 x 16 pixel
 area, but the Y data represents a 32 x 8 pixel area.
*/

#define LIMIT(x) ((x)>0xffffff?0xff: ((x)<=0xffff?0:((x)>>16)))

static void
postproc(struct vidstate *vsp) {
  int i, j;
  xel *xp, *xq;

  const double brightness = 1.0; /* 0->black; 1->full scale */
  const double saturation = 1.0; /* 0->greyscale; 1->full color */
  const double fixScale = brightness * 256 * 256;
  const int rvScale = (int)(1.402 * saturation * fixScale);
  const int guScale = (int)(-0.344136 * saturation * fixScale);
  const int gvScale = (int)(-0.714136 * saturation * fixScale);
  const int buScale = (int)(1.772 * saturation * fixScale);
  const int yScale = (int)(fixScale);	

  for(j = 0; j < vsp->height; j += 2) {
    xp = vsp->xels[j];
    xq = vsp->xels[j+1];
    for(i = 0; i < vsp->width; i += 2) {

      int u = PPM_GETR(xp[0]) - 128;
      int v = PPM_GETB(xp[0]) - 128;

      int yTL = PPM_GETG(xp[0]) * yScale;
      int yTR = PPM_GETG(xp[1]) * yScale;
      int yBL = PPM_GETG(xq[0]) * yScale;
      int yBR = PPM_GETG(xq[1]) * yScale;

      int r =               rvScale * v;
      int g = guScale * u + gvScale * v;
      int b = buScale * u;

      PPM_ASSIGN(xp[0], LIMIT(r+yTL), LIMIT(g+yTL), LIMIT(b+yTL));
      PPM_ASSIGN(xp[1], LIMIT(r+yTR), LIMIT(g+yTR), LIMIT(b+yTR));
      PPM_ASSIGN(xq[0], LIMIT(r+yBL), LIMIT(g+yBL), LIMIT(b+yBL));
      PPM_ASSIGN(xq[1], LIMIT(r+yBR), LIMIT(g+yBR), LIMIT(b+yBR));

      xp += 2;
      xq += 2;
    }
  }

  if(vsp->width > 400) {
    for(j = 0; j < vsp->height - 1; ++j) {
      xp = vsp->xels[j];
      xq = vsp->xels[j+1];
      for(i = 0; i < vsp->width; ++i) {
	PPM_PUTR(*xp, PPM_GETR(*xq));
	++xp;
	++xq;
      }
    }
    for(j = vsp->height - 1; j >= 1; --j) {
      xp = vsp->xels[j-1];
      xq = vsp->xels[j];
      for(i = 0; i < vsp->width; ++i) {
	PPM_PUTB(*xq, PPM_GETB(*xp));
	++xp;
	++xq;
      }
    }
  }
}

int fd = -1, isoc = -1;	/* control and isochronous input fds */
struct usb_device_info udi;	/* device info */
struct usb_alt_interface alt;	/* interface/alternate selection */
int cid = -1;			/* OV511 camera ID */
char dev[FILENAME_MAX];	/* for constructing device names */
char isocdev[FILENAME_MAX];	/* for constructing endpoint 1 device names */
int isplus;			/* bridge is OV511+ if true, else OV511 */
int is20;			/* sensor is OV7620 if true, else OV7610 */
int bufsize;			/* size of packet buffer */
int frmnm = 0;		/* cyclic frame number key */

void
ov511_new_frame(void) {
  if(ov511_reg_write(fd, OV511_REG_RST, 0x3F) < 0)
    exit(1);

  if(ov511_reg_write(fd, OV511_REG_RST, 0x00) < 0)
    exit(1);
// watch_write("new frame, SKIPPING");
  vs.state = SKIPPING;
}

void
setup_ov511(int small, char *devname) {
  if(devname) {
    if((fd = open(devname, O_RDWR)) < 0) {
      perror(devname);
      exit(1);
    }

    /* make sure it's an OV511 */
    if(ioctl(fd, USB_GET_DEVICEINFO, &udi) < 0) {
      perror("USB_GET_DEVICEINFO");
      exit(1);
    }

    if(udi.udi_vendorNo != 0x05A9 || (udi.udi_productNo != 0x0511 &&
				  udi.udi_productNo != 0xa511)) {
      fprintf(stderr, "device %s is not an OmniVision OV511 or OV511+\n", devname);
      exit(1);
    }
  } else {
    int i = 0;
    for(i = 0; i < 15; ++i) {
      snprintf(dev, sizeof(dev), "/dev/ugen%d", i);
      if((fd = open(dev, O_RDWR)) < 0) {
	fprintf(stderr, "vid: could not open %s for read/write\n", dev);
	continue;
      }
      if(ioctl(fd, USB_GET_DEVICEINFO, &udi) < 0
	 || udi.udi_vendorNo != 0x05A9 || (udi.udi_productNo != 0x0511 &&
				       udi.udi_productNo != 0xa511)) {
	fprintf(stderr, "vid: vendor 0x%.04x prod 0x%.04x\n",
		udi.udi_vendorNo, udi.udi_productNo);
	close(fd);
	fd = -1;
	continue;
      } else {
	break;
      }
    }

    if(fd < 0) {
      fprintf(stderr, "Could not locate an OV511 or OV511+ USB video camera\n");
      exit(1);
    }

    devname = dev;
  }

  isplus = udi.udi_productNo == 0xa511;
  bufsize = (isplus ? 961 : 993);
  
  /* reset the OV511 */
  if(ov511_reg_write(fd, OV511_REG_RST, 0x7f) < 0)
    exit(1);

  if(ov511_reg_write(fd, OV511_REG_RST, 0) < 0)
    exit(1);

  /* initialize system */
  if(ov511_reg_write(fd, OV511_REG_EN_SYS, 0x1) < 0)
    exit(1);

  /* determine the camera model */
  if((cid = ov511_reg_read(fd, OV511_REG_CID)) < 0)
    exit(1);

#ifdef notdef
  switch(cid) {
  case 0: /* This also means that no custom ID was set */
    fprintf(stderr, "vid: MediaForte MV300\n");
    break;
  case 3:
    fprintf(stderr, "vid: D-Link DSB-C300\n");
    break;
  case 4:
    fprintf(stderr, "vid: Generic OV511/OV7610\n");
    break;
  case 5:
    fprintf(stderr, "vid: Puretek PT-6007\n");
    break;
  case 21:
    fprintf(stderr, "vid: Creative Labs WebCam 3\n");
    break;
  case 36:
    fprintf(stderr, "vid: Koala-Cam\n");
    break;
  case 100:
    fprintf(stderr, "vid: Lifeview RoboCam\n");
    break;
  case 102:
    fprintf(stderr, "vid: AverMedia InterCam Elite\n");
    break;
  case 112: /* The OmniVision OV7110 evaluation kit uses this too */
    fprintf(stderr, "vid: MediaForte MV300\n");
    break;
  default:
    fprintf(stderr, "vid: Camera custom ID %d not recognized\n", cid);
    exit(1);
  }
#endif

  /* set I2C write slave ID for OV7601 */
  if(ov511_reg_write(fd, OV511_REG_SID, OV7610_I2C_WRITE_ID) < 0)
    exit(1);

  /* set I2C read slave ID for OV7601 */
  if(ov511_reg_write(fd, OV511_REG_SRA, OV7610_I2C_READ_ID) < 0)
    exit(1);

  if(ov511_reg_write(fd, OV511_REG_PKSZ, 0x1) < 0)
    exit(1);
  if(ov511_reg_write(fd, OV511_REG_PKFMT, 0x0) < 0)
    exit(1);

  if(ov511_reg_write(fd, OV511_REG_RST, 0x3d) < 0)
    exit(1);

  if(ov511_reg_write(fd, OV511_REG_RST, 0x0) < 0)
    exit(1);

  if(ov511_i2c_read(fd, 0x00) < 0)
    exit(1);

  /* set YUV 4:2:0 format, Y channel LPF */
  if(ov511_reg_write(fd, OV511_REG_M400, 0x01) < 0)
    exit(1);
  if(ov511_reg_write(fd, OV511_REG_M420_YFIR, 0x03) < 0)
    exit(1);

  /* disable snapshot */
  if(ov511_reg_write(fd, OV511_REG_SNAP, 0x0) < 0)
    exit(1);
  /* disable compression */
  if(ov511_reg_write(fd, OV511_REG_CE_EN, 0x0) < 0)
    exit(1);

#ifdef COMPRESSIONTEST
	/* set compress, from the linux kernel driver */

	ov511_reg_write(fd, OV511_REG_CE_EN, 0x03); // Turn on Y compression
	ov511_reg_write(fd, OV511_REG_LT_EN, 0x00); // Disable LUTs
#endif

  /* This returns 0 if we have an OV7620 sensor */
  if((is20 = ov511_i2c_read(fd, OV7610_REG_COMI)) < 0)
    exit(1);
  is20 = !is20;

  /* set up the OV7610/OV7620 */
  if(is20) {
    ov511_i2c_write(fd, OV7610_REG_EC,      0xff);
    ov511_i2c_write(fd, OV7610_REG_FD,      0x06);
    ov511_i2c_write(fd, OV7610_REG_COMH,    COMH_PROGRESSIVE|COMH_EN_YG);
    ov511_i2c_write(fd, OV7610_REG_EHSL,    0xac);
    ov511_i2c_write(fd, OV7610_REG_COMA,    0x00);
    ov511_i2c_write(fd, OV7610_REG_COMH,    COMH_PROGRESSIVE|COMH_EN_YG);
    ov511_i2c_write(fd, OV7610_REG_RWB,     0x85);
    ov511_i2c_write(fd, OV7610_REG_COMD,    0x01);
    ov511_i2c_write(fd, 0x23,               0x00);
    ov511_i2c_write(fd, OV7610_REG_ECW,     0x10);
    ov511_i2c_write(fd, OV7610_REG_ECB,     0x8a);
    ov511_i2c_write(fd, OV7610_REG_COMG,    0xe2);
    ov511_i2c_write(fd, OV7610_REG_EHSH,    0x00);
    ov511_i2c_write(fd, OV7610_REG_EXBK,    0xfe);
    ov511_i2c_write(fd, 0x30,               0x71);	/* marked 'reserved' */
    ov511_i2c_write(fd, 0x31,               0x60);	/* marked 'reserved' */
    ov511_i2c_write(fd, 0x32,               0x26);	/* marked 'reserved' */
    ov511_i2c_write(fd, OV7610_REG_YGAM,    0x20);
    ov511_i2c_write(fd, OV7610_REG_BADJ,    0x48);
    ov511_i2c_write(fd, OV7610_REG_COMA,    COMA_AGC_EN|COMA_AUTOBAL|COMA_OPTS);
    ov511_i2c_write(fd, OV7610_REG_SYN_CLK, 0x01);
    ov511_i2c_write(fd, OV7610_REG_BBS,     0x24);
    ov511_i2c_write(fd, OV7610_REG_RBS,     0x24);
  } else {
    ov511_i2c_write(fd, OV7610_REG_RWB, 0x5);
    ov511_i2c_write(fd, OV7610_REG_EC, 0xFF);
    /* COMB:
     *  COMB_AUTOADJUST: enable auto adjustment mode.  auto sets registers
     *    0x00 (gain)
     *    0x01 (blue channel balance)
     *    0x02 (red channel balance)
     *    0x10 manual exposure setting
     */
    ov511_i2c_write(fd, OV7610_REG_COMB, COMB_AUTOADJUST);
    ov511_i2c_write(fd, OV7610_REG_FD, 0x06);
    ov511_i2c_write(fd, OV7610_REG_COME, 0x1c);
    ov511_i2c_write(fd, OV7610_REG_COMF, 0x90);
    ov511_i2c_write(fd, OV7610_REG_ECW, 0x2e);
    ov511_i2c_write(fd, OV7610_REG_ECB, 0x7C);
    ov511_i2c_write(fd, OV7610_REG_COMH, COMH_PROGRESSIVE|COMH_EN_YG);
    ov511_i2c_write(fd, OV7610_REG_EHSH, 0x04);
    ov511_i2c_write(fd, OV7610_REG_EHSL, 0xAC);
    ov511_i2c_write(fd, OV7610_REG_EXBK, 0xFE);
    ov511_i2c_write(fd, OV7610_REG_COMJ, 0x93);
    ov511_i2c_write(fd, OV7610_REG_BADJ, 0x48);
    ov511_i2c_write(fd, OV7610_REG_COMK, 0x81);

    ov511_i2c_write(fd, OV7610_REG_GAM, 0x04);	/* gamma = 0.45 */
  }
  if(small) {
    vs.width = 320;
    vs.height = 240;
    ov511_reg_write(fd, OV511_REG_PXCNT, 0x27);
    ov511_reg_write(fd, OV511_REG_LNCNT, 0x1D);
    ov511_i2c_write(fd, OV7610_REG_SYN_CLK, 0x01);
    ov511_i2c_write(fd, OV7610_REG_COMA, COMA_AGC_EN|COMA_AUTOBAL|COMA_OPTS);
    ov511_i2c_write(fd, OV7610_REG_COMC, COMC_SETGAM|COMC_320x240);
    ov511_i2c_write(fd, OV7610_REG_COML, 0x9e);
  } else {
    vs.width = 640;
    vs.height = 480;
    ov511_reg_write(fd, OV511_REG_PXCNT, 0x4F);
    ov511_reg_write(fd, OV511_REG_LNCNT, 0x3D);
    ov511_i2c_write(fd, OV7610_REG_SYN_CLK, 0x06);
    ov511_i2c_write(fd, OV7610_REG_HE, 0x3a + (640>>2));
    ov511_i2c_write(fd, OV7610_REG_VE, 5 + (480>>1));
    ov511_i2c_write(fd, OV7610_REG_COMA, COMA_AGC_EN|COMA_AUTOBAL|COMA_OPTS);
    ov511_i2c_write(fd, OV7610_REG_COMC, COMC_SETGAM);
    ov511_i2c_write(fd, OV7610_REG_COML, 0x1e);	/* U,V, and Y alg. selection*/
  }

  ov511_reg_write(fd, OV511_REG_PXDV, 0x00);
  ov511_reg_write(fd, OV511_REG_LNDV, 0x00);

  /* set FIFO format (993-byte packets) */
  if(ov511_reg_write(fd, OV511_REG_PKSZ, bufsize/32) < 0)
    exit(1);
  /*
   *	0x08	insert zero packet after EOF frame
   *    0x02	append packet number byte to each packet
   *    0x01	"compressed data non-zero insertion"
   */
  if(ov511_reg_write(fd, OV511_REG_PKFMT, 0x03) < 0)
    exit(1);

  /* select the 993-byte alternative */
  alt.uai_interface_index = 0;
  alt.uai_alt_no = (isplus ? 7 : 1);
  if(ioctl(fd, USB_SET_ALTINTERFACE, &alt) < 0) {
    perror("USB_SET_ALTINTERFACE");
    exit(1);
  }

  vs.segsize = 384;
  vs.xels = pnm_allocarray(vs.width, vs.height);

  /* open the isochronous endpoint (endpoint 1) */
  snprintf(isocdev, sizeof(isocdev),  "%s.1", devname);
  if((isoc = open(isocdev, O_RDONLY)) < 0) {
    perror(isocdev);
    exit(1);
  }
}

char *
dump(int frameno, u_char buf[], int len, int bufsize) {
	static char display[300];
	char *state = "";

	switch (vs.state) {
	case SKIPPING:	state = "skipping";	break;
	case READING:	state = "reading ";	break;
	case DONE:	state = "done    ";	break;
	default:
		assert(0); /* unknown state */
	}
	snprintf(display, sizeof(display), "%s  %5d  %3d%s%3d  %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x %.02x  %.02x %.02x   %.02x",
		state, frameno, len,
		len != bufsize ? "!" : " ",
		bufsize,
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8],
		buf[9], buf[10],
		buf[bufsize-1]);
	return display;
}

int bufsize;			/* size of packet buffer */
int len = -1;			/* isochronous input read length */
int isplus;			/* bridge is OV511+ if true, else OV511 */
int is20;			/* sensor is OV7620 if true, else OV7610 */

#define ZEROHDR(b,l)	((b[0] == 0 && b[1] == 0 && b[2] == 0 && b[3] == 0\
       && b[4] == 0 && b[5] == 0 && b[6] == 0 && b[7] == 0))

#define FRAMECTL(b,l)	(ZEROHDR(b,l) && (b[8] & 0x08) == 0x08 && b[l-1] == 0)

#define SoF(b, l)	((b[8] & 0x80) == 0)

#define EoF(b, l)	((b[8] & 0x80) != 0)

xel **
grab_ov511(void) {
  u_char buf[1024];		/* isochronous input read buffer */
// watch_write("grabbing");
  /* read, looking for start and end frames */
  while(vs.state != DONE && (len = read(isoc, &buf, bufsize)) >= 0) {
// watch_write("read  %s", dump(frmnm, buf, len, bufsize));
    if(FRAMECTL(buf,len) && SoF(buf,len) && vs.state == SKIPPING) {
// watch_write("READING");
	vs.state = READING;
	vs.iY = vs.jY = vs.iUV = vs.jUV = 0;
	vs.residue = 0;
	procdata(&vs, buf + 9, bufsize - 10);
	frmnm = 1;
    } else if(FRAMECTL(buf,len) && EoF(buf, len) && vs.state == READING) {
// watch_write("DONE");
      vs.state = DONE;
    } else if(vs.state == READING) {
      if (ZEROHDR(buf,len)) {
// watch_write("zero image");
	vs.state = SKIPPING;
      } else {
	procdata(&vs, buf, bufsize - 1);

	  /*
	   * abort the capture and start over if packets come in
	   * out-of-order
	   */
	if(buf[bufsize-1] != frmnm /* && buf[bufsize-1] != 1*/ ) {
// watch_write("aborted");
	  vs.state = SKIPPING;
        }
	frmnm = buf[bufsize-1] + 1;
	if(frmnm == 256)
	  frmnm = 1;
      }
    } else if(buf[bufsize-1] != 0) {
      frmnm = buf[bufsize-1] + 1;
      if(frmnm == 256)
	frmnm = 1;
    }
  }

  /* convert from CIE YCbCr color to RGB */
// watch_write("postproc");
  postproc(&vs);
// watch_write("done");
	return vs.xels;
}

/* reset and close the OV511 */

void
finish_ov511(void) {
	ov511_reg_write(fd, OV511_REG_RST, 0x7f);
	close(isoc);
	close(fd);
}

int
get_bright(void) {
	int x = ov511_i2c_read(fd, OV7610_REG_BRT);
	if (x < 0)
		fprintf(stderr, "get_bright error\n");
	return x;
}

int
get_sat(void) {
	int x = ov511_i2c_read(fd, OV7610_REG_SAT);
	if (x < 0)
		fprintf(stderr, "get_sat error\n");
	return x;
}

void
set_bright(int b) {
	if (b >= 0 && b <= 0xff) {
		if (ov511_i2c_write(fd, OV7610_REG_BRT, b) >= 0)
			fprintf(stderr, "bright -> 0x%.02x\n", b);
		else if (debug)
			fprintf(stderr, "set_bright failed\n");
	}
}

int
get_cont(void) {
	int x = ov511_i2c_read(fd, OV7610_REG_CTR);
	if (x < 0)
		fprintf(stderr, "get_cont error\n");
	return x;
}

void
set_cont(int c) {
	if (c >= 0 && c <= 0xff) {
		if (ov511_i2c_write(fd, OV7610_REG_CTR, c) >= 0)
			fprintf(stderr, "cont -> 0x%.02x\n", c);
		else if (debug)
			fprintf(stderr, "set_cont failed\n");
	}
}

void
enable_agc(void) {
/*	ov511_i2c_write(fd, OV7610_REG_COMA, COMA_AGC_EN|COMA_AUTOBAL|COMA_OPTS);*/
	ov511_i2c_write(fd, OV7610_REG_COMB, COMB_AUTOADJUST);
}

void
disable_agc(void) {
/*	ov511_i2c_write(fd, OV7610_REG_COMA, COMA_AUTOBAL|COMA_OPTS);*/
	ov511_i2c_write(fd, OV7610_REG_COMB, 0x00);
}

void
get_exp(int *gain, int *bluecb, int *redcb, int *exposure) {
	*gain = ov511_i2c_read(fd, OV7610_REG_GC);
	*bluecb = ov511_i2c_read(fd, OV7610_REG_BLU);
	*redcb = ov511_i2c_read(fd, OV7610_REG_RED);
	*exposure = ov511_i2c_read(fd, OV7610_REG_EC);
}

void
set_gain(int gain) {
	ov511_i2c_write(fd, OV7610_REG_GC, gain);
}

void
set_bluecb(int bluecb) {
	ov511_i2c_write(fd, OV7610_REG_BLU, bluecb);
}

void
set_redcb(int redcb) {
	ov511_i2c_write(fd, OV7610_REG_RED, redcb);
}

void
set_exposure(int exposure) {
	ov511_i2c_write(fd, OV7610_REG_EC, exposure);
}

/*
 * set/reset single frame mode
 */
void
set_single(int on) {
	if (on) {
		ov511_i2c_write(fd, OV7610_REG_FD,      0x04);
		ov511_i2c_write(fd, OV7610_REG_COMB, COMB_AUTOADJUST|COMB_SINGLE);
	} else {
		ov511_i2c_write(fd, OV7610_REG_FD,      0x06);
		ov511_i2c_write(fd, OV7610_REG_COMB, COMB_AUTOADJUST);
	}
}

void
dump_stuff(void) {
	int i, n;

	for (i=0x36; i<=0x3f; i++)
		fprintf (stderr, "%.02x ",
			(n=ov511_i2c_read(fd, i)) >= 0 ? n : 0xff);
	fprintf(stderr, "\n");
}
