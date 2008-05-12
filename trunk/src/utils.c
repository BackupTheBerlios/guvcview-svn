/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

/*******************************************************************************#
#                                                                               #
#  MJpeg decoding and frame capture taken from luvcview                         #
#                                                                               # 
#                                                                               #
********************************************************************************/

#include "utils.h"
#include "v4l2uvc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <string.h>
#include <fcntl.h>
#include <wait.h>
#include <time.h>
#include <limits.h>
#include "huffman.h"
#include <png.h>
#include "prototype.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>    


#define ISHIFT 11

#define IFIX(a) ((int)((a) * (1 << ISHIFT) + .5))

#ifndef __P
# define __P(x) x
#endif

/* special markers */
#define M_BADHUFF	-1
#define M_EOF		0x80

/*yuv to rgb formula (set to one to use to 1)*/
#define _F1 0
#define _F2 0
#define _F3 1


struct jpeg_decdata {
    int dcts[6 * 64 + 16];
    int out[64 * 6];
    int dquant[3][64];
};



struct in {
    unsigned char *p;
    unsigned int bits;
    int left;
    int marker;
    int (*func) __P((void *));
    void *data;
};

/*********************************/
struct dec_hufftbl;
struct enc_hufftbl;

union hufftblp {
    struct dec_hufftbl *dhuff;
    struct enc_hufftbl *ehuff;
};

struct scan {
    int dc;			/* old dc value */

    union hufftblp hudc;
    union hufftblp huac;
    int next;			/* when to switch to next scan */

    int cid;			/* component id */
    int hv;			/* horiz/vert, copied from comp */
    int tq;			/* quant tbl, copied from comp */
};

/*********************************/

#define DECBITS 10		/* seems to be the optimum */

struct dec_hufftbl {
    int maxcode[17];
    int valptr[16];
    unsigned char vals[256];
    unsigned int llvals[1 << DECBITS];
};
static int huffman_init(void);
static void decode_mcus
__P((struct in *, int *, int, struct scan *, int *));
static int dec_readmarker __P((struct in *));
static void dec_makehuff
__P((struct dec_hufftbl *, int *, unsigned char *));

static void setinput __P((struct in *, unsigned char *));
/*********************************/

#undef PREC
#define PREC int

static void idctqtab __P((unsigned char *, PREC *));

inline static void idct(int *in, int *out, int *quant, long off, int max);

/*********************************/

//static void col221111 __P((int *, unsigned char *, int));

static void yuv420pto422(int * out,unsigned char *pic,int width);
static void yuv422pto422(int * out,unsigned char *pic,int width);
static void yuv444pto422(int * out,unsigned char *pic,int width);
static void yuv400pto422(int * out,unsigned char *pic,int width);
typedef void (*ftopict) ( int *out, unsigned char *pic, int width) ;
/*********************************/

#define M_SOI	0xd8
#define M_APP0	0xe0
#define M_DQT	0xdb
#define M_SOF0	0xc0
#define M_DHT   0xc4
#define M_DRI	0xdd
#define M_SOS	0xda
#define M_RST0	0xd0
#define M_EOI	0xd9
#define M_COM	0xfe

static unsigned char *datap;

static int getbyte(void)
{
    return *datap++;
}

static int getword(void)
{
    int c1, c2;
    c1 = *datap++;
    c2 = *datap++;
    return c1 << 8 | c2;
}

struct comp {
    int cid;
    int hv;
    int tq;
};

#define MAXCOMP 4
struct jpginfo {
    int nc;			/* number of components */
    int ns;			/* number of scans */
    int dri;			/* restart interval */
    int nm;			/* mcus til next marker */
    int rm;			/* next restart marker */
};

static struct jpginfo info;
static struct comp comps[MAXCOMP];

static struct scan dscans[MAXCOMP];

static unsigned char quant[4][64];

static struct dec_hufftbl dhuff[4];

#define dec_huffdc (dhuff + 0)
#define dec_huffac (dhuff + 2)

static struct in in;

static int readtables(int till, int *isDHT)
{
    int m, l, i, j, lq, pq, tq;
    int tc, th, tt;

    for (;;) {
	if (getbyte() != 0xff)
	    return -1;
	if ((m = getbyte()) == till)
	    break;

	switch (m) {
	case 0xc2:
	    return 0;

	case M_DQT:
	//printf("find DQT \n");
	    lq = getword();
	    while (lq > 2) {
		pq = getbyte();
		tq = pq & 15;
		if (tq > 3)
		    return -1;
		pq >>= 4;
		if (pq != 0)
		    return -1;
		for (i = 0; i < 64; i++)
		    quant[tq][i] = getbyte();
		lq -= 64 + 1;
	    }
	    break;

	case M_DHT:
	//printf("find DHT \n");
	    l = getword();
	    while (l > 2) {
		int hufflen[16], k;
		unsigned char huffvals[256];

		tc = getbyte();
		th = tc & 15;
		tc >>= 4;
		tt = tc * 2 + th;
		if (tc > 1 || th > 1)
		    return -1;
		for (i = 0; i < 16; i++)
		    hufflen[i] = getbyte();
		l -= 1 + 16;
		k = 0;
		for (i = 0; i < 16; i++) {
		    for (j = 0; j < hufflen[i]; j++)
			huffvals[k++] = getbyte();
		    l -= hufflen[i];
		}
		dec_makehuff(dhuff + tt, hufflen, huffvals);
	    }
	    *isDHT= 1;
	    break;

	case M_DRI:
	//printf("find DRI \n");
	    l = getword();
	    info.dri = getword();
	    break;

	default:
	    l = getword();
	    while (l-- > 2)
		getbyte();
	    break;
	}
    }

    return 0;
}

static void dec_initscans(void)
{
    int i;

    info.nm = info.dri + 1;
    info.rm = M_RST0;
    for (i = 0; i < info.ns; i++)
	dscans[i].dc = 0;
}

static int dec_checkmarker(void)
{
    int i;

    if (dec_readmarker(&in) != info.rm)
	return -1;
    info.nm = info.dri;
    info.rm = (info.rm + 1) & ~0x08;
    for (i = 0; i < info.ns; i++)
	dscans[i].dc = 0;
    return 0;
}


int jpeg_decode(unsigned char **pic, unsigned char *buf, int *width,
		int *height)
{
    struct jpeg_decdata *decdata;
    int i, j, m, tac, tdc;
    int intwidth, intheight;
    int mcusx, mcusy, mx, my;
    int ypitch ,xpitch,bpp,pitch,x,y;
    int mb;
    int max[6];
    ftopict convert;
    int err = 0;
    int isInitHuffman = 0;
    decdata = (struct jpeg_decdata *) malloc(sizeof(struct jpeg_decdata));
    
    if (!decdata) {
	err = -1;
	goto error;
    }
    if (buf == NULL) {
	err = -1;
	goto error;
    }
    datap = buf;
    if (getbyte() != 0xff) {
	err = ERR_NO_SOI;
	goto error;
    }
    if (getbyte() != M_SOI) {
	err = ERR_NO_SOI;
	goto error;
    }
    if (readtables(M_SOF0, &isInitHuffman)) {
	err = ERR_BAD_TABLES;
	goto error;
    }
    getword();
    i = getbyte();
    if (i != 8) {
	err = ERR_NOT_8BIT;
	goto error;
    }
    intheight = getword();
    intwidth = getword();
    
    if ((intheight & 7) || (intwidth & 7)) {
	err = ERR_BAD_WIDTH_OR_HEIGHT;
	goto error;
    }
    info.nc = getbyte();
    if (info.nc > MAXCOMP) {
	err = ERR_TOO_MANY_COMPPS;
	goto error;
    }
    for (i = 0; i < info.nc; i++) {
	int h, v;
	comps[i].cid = getbyte();
	comps[i].hv = getbyte();
	v = comps[i].hv & 15;
	h = comps[i].hv >> 4;
	comps[i].tq = getbyte();
	if (h > 3 || v > 3) {
	    err = ERR_ILLEGAL_HV;
	    goto error;
	}
	if (comps[i].tq > 3) {
	    err = ERR_QUANT_TABLE_SELECTOR;
	    goto error;
	}
    }
    if (readtables(M_SOS,&isInitHuffman)) {
	err = ERR_BAD_TABLES;
	goto error;
    }
    getword();
    info.ns = getbyte();
    if (!info.ns){
    printf("info ns %d/n",info.ns);
	err = ERR_NOT_YCBCR_221111;
	goto error;
    }
    for (i = 0; i < info.ns; i++) {
	dscans[i].cid = getbyte();
	tdc = getbyte();
	tac = tdc & 15;
	tdc >>= 4;
	if (tdc > 1 || tac > 1) {
	    err = ERR_QUANT_TABLE_SELECTOR;
	    goto error;
	}
	for (j = 0; j < info.nc; j++)
	    if (comps[j].cid == dscans[i].cid)
		break;
	if (j == info.nc) {
	    err = ERR_UNKNOWN_CID_IN_SCAN;
	    goto error;
	}
	dscans[i].hv = comps[j].hv;
	dscans[i].tq = comps[j].tq;
	dscans[i].hudc.dhuff = dec_huffdc + tdc;
	dscans[i].huac.dhuff = dec_huffac + tac;
    }

    i = getbyte();
    j = getbyte();
    m = getbyte();

    if (i != 0 || j != 63 || m != 0) {
    	printf("hmm FW error,not seq DCT ??\n");
    }
   // printf("ext huffman table %d \n",isInitHuffman);
    if(!isInitHuffman) {
    	if(huffman_init() < 0)
		return -ERR_BAD_TABLES;
	}
/*
    if (dscans[0].cid != 1 || dscans[1].cid != 2 || dscans[2].cid != 3) {
	err = ERR_NOT_YCBCR_221111;
	goto error;
    }

    if (dscans[1].hv != 0x11 || dscans[2].hv != 0x11) {
	err = ERR_NOT_YCBCR_221111;
	goto error;
    }
*/    
    /* if internal width and external are not the same or heigth too 
       and pic not allocated realloc the good size and mark the change 
       need 1 macroblock line more ?? */
    if (intwidth != *width || intheight != *height || *pic == NULL) {
	*width = intwidth;
	*height = intheight;
	// BytesperPixel 2 yuyv , 3 rgb24 
	*pic =
	    (unsigned char *) realloc((unsigned char *) *pic,
				      (size_t) intwidth * (intheight +
							   8) * 2);
    }


    switch (dscans[0].hv) {
    case 0x22: // 411
    	mb=6;
	mcusx = *width >> 4;
	mcusy = *height >> 4;
	bpp=2;
	xpitch = 16 * bpp;
	pitch = *width * bpp; // YUYV out
	ypitch = 16 * pitch;
	convert = yuv420pto422;	
	break;
    case 0x21: //422
   // printf("find 422 %dx%d\n",*width,*height);
    	mb=4;
	mcusx = *width >> 4;
	mcusy = *height >> 3;
	bpp=2;	
	xpitch = 16 * bpp;
	pitch = *width * bpp; // YUYV out
	ypitch = 8 * pitch;
	convert = yuv422pto422;	
	break;
    case 0x11: //444
	mcusx = *width >> 3;
	mcusy = *height >> 3;
	bpp=2;
	xpitch = 8 * bpp;
	pitch = *width * bpp; // YUYV out
	ypitch = 8 * pitch;
	 if (info.ns==1) {
    		mb = 1;
		convert = yuv400pto422;
	} else {
		mb=3;
		convert = yuv444pto422;	
	}
        break;
    default:
	err = ERR_NOT_YCBCR_221111;
	goto error;
	break;
    }

    idctqtab(quant[dscans[0].tq], decdata->dquant[0]);
    idctqtab(quant[dscans[1].tq], decdata->dquant[1]);
    idctqtab(quant[dscans[2].tq], decdata->dquant[2]);
    setinput(&in, datap);
    dec_initscans();

    dscans[0].next = 2;
    dscans[1].next = 1;
    dscans[2].next = 0;	/* 4xx encoding */
    for (my = 0,y=0; my < mcusy; my++,y+=ypitch) {
	for (mx = 0,x=0; mx < mcusx; mx++,x+=xpitch) {
	    if (info.dri && !--info.nm)
		if (dec_checkmarker()) {
		    err = ERR_WRONG_MARKER;
		    goto error;
		}
	switch (mb){
	    case 6: {
		decode_mcus(&in, decdata->dcts, mb, dscans, max);
		idct(decdata->dcts, decdata->out, decdata->dquant[0],
		     IFIX(128.5), max[0]);
		idct(decdata->dcts + 64, decdata->out + 64,
		     decdata->dquant[0], IFIX(128.5), max[1]);
		idct(decdata->dcts + 128, decdata->out + 128,
		     decdata->dquant[0], IFIX(128.5), max[2]);
		idct(decdata->dcts + 192, decdata->out + 192,
		     decdata->dquant[0], IFIX(128.5), max[3]);
		idct(decdata->dcts + 256, decdata->out + 256,
		     decdata->dquant[1], IFIX(0.5), max[4]);
		idct(decdata->dcts + 320, decdata->out + 320,
		     decdata->dquant[2], IFIX(0.5), max[5]);
	  
	    } break;
	    case 4:
	    {
		decode_mcus(&in, decdata->dcts, mb, dscans, max);
		idct(decdata->dcts, decdata->out, decdata->dquant[0],
		     IFIX(128.5), max[0]);
		idct(decdata->dcts + 64, decdata->out + 64,
		     decdata->dquant[0], IFIX(128.5), max[1]);
		idct(decdata->dcts + 128, decdata->out + 256,
		     decdata->dquant[1], IFIX(0.5), max[4]);
		idct(decdata->dcts + 192, decdata->out + 320,
		     decdata->dquant[2], IFIX(0.5), max[5]);
	   	   
	    }
	    break;
	    case 3:
	    	 decode_mcus(&in, decdata->dcts, mb, dscans, max);
		idct(decdata->dcts, decdata->out, decdata->dquant[0],
		     IFIX(128.5), max[0]);		     
		idct(decdata->dcts + 64, decdata->out + 256,
		     decdata->dquant[1], IFIX(0.5), max[4]);
		idct(decdata->dcts + 128, decdata->out + 320,
		     decdata->dquant[2], IFIX(0.5), max[5]);
	    
		         
	    break;
	    case 1:
	    	 decode_mcus(&in, decdata->dcts, mb, dscans, max);
		idct(decdata->dcts, decdata->out, decdata->dquant[0],
		     IFIX(128.5), max[0]);
		  
	    break;
	    
	} // switch enc411
	convert(decdata->out,*pic+y+x,pitch); 
	}
    }

    m = dec_readmarker(&in);
    if (m != M_EOI) {
	err = ERR_NO_EOI;
	goto error;
    }
    if (decdata)
	free(decdata);
    return 0;
  error:
    if (decdata)
	free(decdata);
    return err;
}

/****************************************************************/
/**************       huffman decoder             ***************/
/****************************************************************/
static int huffman_init(void)
{    	int tc, th, tt;
 	unsigned char *ptr= (unsigned char *) JPEGHuffmanTable ;
	int i, j, l;
	l = JPG_HUFFMAN_TABLE_LENGTH ;
	    while (l > 0) {
		int hufflen[16], k;
		unsigned char huffvals[256];

		tc = *ptr++;
		th = tc & 15;
		tc >>= 4;
		tt = tc * 2 + th;
		if (tc > 1 || th > 1)
		    return -ERR_BAD_TABLES;
		for (i = 0; i < 16; i++)
		    hufflen[i] = *ptr++;
		l -= 1 + 16;
		k = 0;
		for (i = 0; i < 16; i++) {
		    for (j = 0; j < hufflen[i]; j++)
			huffvals[k++] = *ptr++;
		    l -= hufflen[i];
		}
		dec_makehuff(dhuff + tt, hufflen, huffvals);
	    }
	    return 0;
}

static int fillbits __P((struct in *, int, unsigned int));
static int dec_rec2
__P((struct in *, struct dec_hufftbl *, int *, int, int));

static void setinput(in, p)
struct in *in;
unsigned char *p;
{
    in->p = p;
    in->left = 0;
    in->bits = 0;
    in->marker = 0;
}

static int fillbits(in, le, bi)
struct in *in;
int le;
unsigned int bi;
{
    int b, m;

    if (in->marker) {
	if (le <= 16)
	    in->bits = bi << 16, le += 16;
	return le;
    }
    while (le <= 24) {
	b = *in->p++;
	if (b == 0xff && (m = *in->p++) != 0) {
	    if (m == M_EOF) {
		if (in->func && (m = in->func(in->data)) == 0)
		    continue;
	    }
	    in->marker = m;
	    if (le <= 16)
		bi = bi << 16, le += 16;
	    break;
	}
	bi = bi << 8 | b;
	le += 8;
    }
    in->bits = bi;		/* tmp... 2 return values needed */
    return le;
}

static int dec_readmarker(in)
struct in *in;
{
    int m;

    in->left = fillbits(in, in->left, in->bits);
    if ((m = in->marker) == 0)
	return 0;
    in->left = 0;
    in->marker = 0;
    return m;
}

#define LEBI_DCL	int le, bi
#define LEBI_GET(in)	(le = in->left, bi = in->bits)
#define LEBI_PUT(in)	(in->left = le, in->bits = bi)

#define GETBITS(in, n) (					\
  (le < (n) ? le = fillbits(in, le, bi), bi = in->bits : 0),	\
  (le -= (n)),							\
  bi >> le & ((1 << (n)) - 1)					\
)

#define UNGETBITS(in, n) (	\
  le += (n)			\
)


static int dec_rec2(in, hu, runp, c, i)
struct in *in;
struct dec_hufftbl *hu;
int *runp;
int c, i;
{
    LEBI_DCL;

    LEBI_GET(in);
    if (i) {
	UNGETBITS(in, i & 127);
	*runp = i >> 8 & 15;
	i >>= 16;
    } else {
	for (i = DECBITS;
	     (c = ((c << 1) | GETBITS(in, 1))) >= (hu->maxcode[i]); i++);
	if (i >= 16) {
	    in->marker = M_BADHUFF;
	    return 0;
	}
	i = hu->vals[hu->valptr[i] + c - hu->maxcode[i - 1] * 2];
	*runp = i >> 4;
	i &= 15;
    }
    if (i == 0) {		/* sigh, 0xf0 is 11 bit */
	LEBI_PUT(in);
	return 0;
    }
    /* receive part */
    c = GETBITS(in, i);
    if (c < (1 << (i - 1)))
	c += (-1 << i) + 1;
    LEBI_PUT(in);
    return c;
}

#define DEC_REC(in, hu, r, i)	 (	\
  r = GETBITS(in, DECBITS),		\
  i = hu->llvals[r],			\
  i & 128 ?				\
    (					\
      UNGETBITS(in, i & 127),		\
      r = i >> 8 & 15,			\
      i >> 16				\
    )					\
  :					\
    (					\
      LEBI_PUT(in),			\
      i = dec_rec2(in, hu, &r, r, i),	\
      LEBI_GET(in),			\
      i					\
    )					\
)

static void decode_mcus(in, dct, n, sc, maxp)
struct in *in;
int *dct;
int n;
struct scan *sc;
int *maxp;
{
    struct dec_hufftbl *hu;
    int i, r, t;
    LEBI_DCL;

    memset(dct, 0, n * 64 * sizeof(*dct));
    LEBI_GET(in);
    while (n-- > 0) {
	hu = sc->hudc.dhuff;
	*dct++ = (sc->dc += DEC_REC(in, hu, r, t));

	hu = sc->huac.dhuff;
	i = 63;
	while (i > 0) {
	    t = DEC_REC(in, hu, r, t);
	    if (t == 0 && r == 0) {
		dct += i;
		break;
	    }
	    dct += r;
	    *dct++ = t;
	    i -= r + 1;
	}
	*maxp++ = 64 - i;
	if (n == sc->next)
	    sc++;
    }
    LEBI_PUT(in);
}

static void dec_makehuff(hu, hufflen, huffvals)
struct dec_hufftbl *hu;
int *hufflen;
unsigned char *huffvals;
{
    int code, k, i, j, d, x, c, v;
    for (i = 0; i < (1 << DECBITS); i++)
	hu->llvals[i] = 0;

/*
 * llvals layout:
 *
 * value v already known, run r, backup u bits:
 *  vvvvvvvvvvvvvvvv 0000 rrrr 1 uuuuuuu
 * value unknown, size b bits, run r, backup u bits:
 *  000000000000bbbb 0000 rrrr 0 uuuuuuu
 * value and size unknown:
 *  0000000000000000 0000 0000 0 0000000
 */
    code = 0;
    k = 0;
    for (i = 0; i < 16; i++, code <<= 1) {	/* sizes */
	hu->valptr[i] = k;
	for (j = 0; j < hufflen[i]; j++) {
	    hu->vals[k] = *huffvals++;
	    if (i < DECBITS) {
		c = code << (DECBITS - 1 - i);
		v = hu->vals[k] & 0x0f;	/* size */
		for (d = 1 << (DECBITS - 1 - i); --d >= 0;) {
		    if (v + i < DECBITS) {	/* both fit in table */
			x = d >> (DECBITS - 1 - v - i);
			if (v && x < (1 << (v - 1)))
			    x += (-1 << v) + 1;
			x = x << 16 | (hu->vals[k] & 0xf0) << 4 |
			    (DECBITS - (i + 1 + v)) | 128;
		    } else
			x = v << 16 | (hu->vals[k] & 0xf0) << 4 |
			    (DECBITS - (i + 1));
		    hu->llvals[c | d] = x;
		}
	    }
	    code++;
	    k++;
	}
	hu->maxcode[i] = code;
    }
    hu->maxcode[16] = 0x20000;	/* always terminate decode */
}

/****************************************************************/
/**************             idct                  ***************/
/****************************************************************/


#define IMULT(a, b) (((a) * (b)) >> ISHIFT)
#define ITOINT(a) ((a) >> ISHIFT)

#define S22 ((PREC)IFIX(2 * 0.382683432))
#define C22 ((PREC)IFIX(2 * 0.923879532))
#define IC4 ((PREC)IFIX(1 / 0.707106781))

static unsigned char zig2[64] = {
    0, 2, 3, 9, 10, 20, 21, 35,
    14, 16, 25, 31, 39, 46, 50, 57,
    5, 7, 12, 18, 23, 33, 37, 48,
    27, 29, 41, 44, 52, 55, 59, 62,
    15, 26, 30, 40, 45, 51, 56, 58,
    1, 4, 8, 11, 19, 22, 34, 36,
    28, 42, 43, 53, 54, 60, 61, 63,
    6, 13, 17, 24, 32, 38, 47, 49
};

inline static void idct(int *in, int *out, int *quant, long off, int max)
{
    long t0, t1, t2, t3, t4, t5, t6, t7;	// t ;
    long tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
    long tmp[64], *tmpp;
    int i, j, te;
    unsigned char *zig2p;

    t0 = off;
    if (max == 1) {
	t0 += in[0] * quant[0];
	for (i = 0; i < 64; i++)
	    out[i] = ITOINT(t0);
	return;
    }
    zig2p = zig2;
    tmpp = tmp;
    for (i = 0; i < 8; i++) {
	j = *zig2p++;
	t0 += in[j] * (long) quant[j];
	j = *zig2p++;
	t5 = in[j] * (long) quant[j];
	j = *zig2p++;
	t2 = in[j] * (long) quant[j];
	j = *zig2p++;
	t7 = in[j] * (long) quant[j];
	j = *zig2p++;
	t1 = in[j] * (long) quant[j];
	j = *zig2p++;
	t4 = in[j] * (long) quant[j];
	j = *zig2p++;
	t3 = in[j] * (long) quant[j];
	j = *zig2p++;
	t6 = in[j] * (long) quant[j];


	if ((t1 | t2 | t3 | t4 | t5 | t6 | t7) == 0) {

	    tmpp[0 * 8] = t0; /*t0 is DC*/
	    tmpp[1 * 8] = t0;
	    tmpp[2 * 8] = t0;
	    tmpp[3 * 8] = t0;
	    tmpp[4 * 8] = t0;
	    tmpp[5 * 8] = t0;
	    tmpp[6 * 8] = t0;
	    tmpp[7 * 8] = t0;

	    tmpp++;
	    t0 = 0;
	    continue;
	}
	//IDCT;
	tmp0 = t0 + t1; /*adds AC*/
	t1 = t0 - t1;
	tmp2 = t2 - t3;
	t3 = t2 + t3;
	tmp2 = IMULT(tmp2, IC4) - t3;
	tmp3 = tmp0 + t3;
	t3 = tmp0 - t3;
	tmp1 = t1 + tmp2;
	tmp2 = t1 - tmp2;
	tmp4 = t4 - t7;
	t7 = t4 + t7;
	tmp5 = t5 + t6;
	t6 = t5 - t6;
	tmp6 = tmp5 - t7;
	t7 = tmp5 + t7;
	tmp5 = IMULT(tmp6, IC4);
	tmp6 = IMULT((tmp4 + t6), S22);
	tmp4 = IMULT(tmp4, (C22 - S22)) + tmp6;
	t6 = IMULT(t6, (C22 + S22)) - tmp6;
	t6 = t6 - t7;
	t5 = tmp5 - t6;
	t4 = tmp4 - t5;

	tmpp[0 * 8] = tmp3 + t7;	//t0;
	tmpp[1 * 8] = tmp1 + t6;	//t1;
	tmpp[2 * 8] = tmp2 + t5;	//t2;
	tmpp[3 * 8] = t3 + t4;	//t3;
	tmpp[4 * 8] = t3 - t4;	//t4;
	tmpp[5 * 8] = tmp2 - t5;	//t5;
	tmpp[6 * 8] = tmp1 - t6;	//t6;
	tmpp[7 * 8] = tmp3 - t7;	//t7;
	tmpp++;
	t0 = 0;
    }
    for (i = 0, j = 0; i < 8; i++) {
	t0 = tmp[j + 0];
	t1 = tmp[j + 1];
	t2 = tmp[j + 2];
	t3 = tmp[j + 3];
	t4 = tmp[j + 4];
	t5 = tmp[j + 5];
	t6 = tmp[j + 6];
	t7 = tmp[j + 7];
	if ((t1 | t2 | t3 | t4 | t5 | t6 | t7) == 0) {
	    te = ITOINT(t0);
	    out[j + 0] = te;
	    out[j + 1] = te;
	    out[j + 2] = te;
	    out[j + 3] = te;
	    out[j + 4] = te;
	    out[j + 5] = te;
	    out[j + 6] = te;
	    out[j + 7] = te;
	    j += 8;
	    continue;
	}
	//IDCT;
	tmp0 = t0 + t1;
	t1 = t0 - t1;
	tmp2 = t2 - t3;
	t3 = t2 + t3;
	tmp2 = IMULT(tmp2, IC4) - t3;
	tmp3 = tmp0 + t3;
	t3 = tmp0 - t3;
	tmp1 = t1 + tmp2;
	tmp2 = t1 - tmp2;
	tmp4 = t4 - t7;
	t7 = t4 + t7;
	tmp5 = t5 + t6;
	t6 = t5 - t6;
	tmp6 = tmp5 - t7;
	t7 = tmp5 + t7;
	tmp5 = IMULT(tmp6, IC4);
	tmp6 = IMULT((tmp4 + t6), S22);
	tmp4 = IMULT(tmp4, (C22 - S22)) + tmp6;
	t6 = IMULT(t6, (C22 + S22)) - tmp6;
	t6 = t6 - t7;
	t5 = tmp5 - t6;
	t4 = tmp4 - t5;

	out[j + 0] = ITOINT(tmp3 + t7);
	out[j + 1] = ITOINT(tmp1 + t6);
	out[j + 2] = ITOINT(tmp2 + t5);
	out[j + 3] = ITOINT(t3 + t4);
	out[j + 4] = ITOINT(t3 - t4);
	out[j + 5] = ITOINT(tmp2 - t5);
	out[j + 6] = ITOINT(tmp1 - t6);
	out[j + 7] = ITOINT(tmp3 - t7);
	j += 8;
    }

}

static unsigned char zig[64] = {
    0, 1, 5, 6, 14, 15, 27, 28,
    2, 4, 7, 13, 16, 26, 29, 42,
    3, 8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

static PREC aaidct[8] = {
    IFIX(0.3535533906), IFIX(0.4903926402),
    IFIX(0.4619397663), IFIX(0.4157348062),
    IFIX(0.3535533906), IFIX(0.2777851165),
    IFIX(0.1913417162), IFIX(0.0975451610)
};


static void idctqtab(qin, qout)
unsigned char *qin;
PREC *qout;
{
    int i, j;

    for (i = 0; i < 8; i++)
	for (j = 0; j < 8; j++)
	    qout[zig[i * 8 + j]] = qin[zig[i * 8 + j]] *
		IMULT(aaidct[i], aaidct[j]);
}


static void yuv420pto422(int * out,unsigned char *pic,int width)
{
    int j, k;
    unsigned char *pic0, *pic1;
    int *outy, *outu, *outv;
    int outy1 = 0;
    int outy2 = 8;


    pic0 = pic;
    pic1 = pic + width;
    outy = out;
    outu = out + 64 * 4;
    outv = out + 64 * 5;    
	for (j = 0; j < 8; j++) {
	    for (k = 0; k < 8; k++) {
	    if( k == 4) { 
	    	outy1 += 56;
	    	outy2 += 56;
	    }
	    *pic0++ = CLIP(outy[outy1]);
	    *pic0++ = CLIP(128 + *outu);
	    *pic0++ = CLIP(outy[outy1+1]);
	    *pic0++ = CLIP(128 + *outv);
	    *pic1++ = CLIP(outy[outy2]);
	    *pic1++ = CLIP(128 + *outu);
	    *pic1++ = CLIP(outy[outy2+1]);
	    *pic1++ = CLIP(128 + *outv);
	   outy1 +=2; outy2 += 2; outu++; outv++;
	  }
	  if(j==3) {
	  outy = out + 128;
	  } else {
	  outy += 16;
	  }
	    outy1 = 0;
	    outy2 = 8;
	    pic0 += 2 * (width -16);
	    pic1 += 2 * (width -16);
	    
	}
    
}
static void yuv422pto422(int * out,unsigned char *pic,int width)
{
    int j, k;
    unsigned char *pic0, *pic1;
    int *outy, *outu, *outv;
    int outy1 = 0;
    int outy2 = 8;
    int outu1 = 0;
    int outv1 = 0;
 

    pic0 = pic;
    pic1 = pic + width;
    outy = out;
    outu = out + 64 * 4;
    outv = out + 64 * 5;    
	for (j = 0; j < 4; j++) {
	    for (k = 0; k < 8; k++) {
	    if( k == 4) { 
	    	outy1 += 56;
	    	outy2 += 56;
	    }
	    *pic0++ = CLIP(outy[outy1]);
	    *pic0++ = CLIP(128 + outu[outu1]);
	    *pic0++ = CLIP(outy[outy1+1]);
	    *pic0++ = CLIP(128 + outv[outv1]);
	    *pic1++ = CLIP(outy[outy2]);
	    *pic1++ = CLIP(128 + outu[outu1+8]);
	    *pic1++ = CLIP(outy[outy2+1]);
	    *pic1++ = CLIP(128 + outv[outv1+8]);
	    outv1 += 1; outu1 += 1;
	    outy1 +=2; outy2 +=2;
	   
	  }
	  
	    outy += 16;outu +=8; outv +=8;
	    outv1 = 0; outu1=0;
	    outy1 = 0;
	    outy2 = 8;
	    pic0 += 2 * (width -16);
	    pic1 += 2 * (width -16);
	    
	}
    
}

static void yuv444pto422(int * out,unsigned char *pic,int width)
{
    int j, k;
    unsigned char *pic0, *pic1;
    int *outy, *outu, *outv;
    int outy1 = 0;
    int outy2 = 8;
    int outu1 = 0;
    int outv1 = 0;

    pic0 = pic;
    pic1 = pic + width;
    outy = out;
    outu = out + 64 * 4; // Ooops where did i invert ??
    outv = out + 64 * 5;    
	for (j = 0; j < 4; j++) {
	    for (k = 0; k < 4; k++) {
	    
	    *pic0++ =CLIP( outy[outy1]);
	    *pic0++ =CLIP( 128 + outu[outu1]);
	    *pic0++ =CLIP( outy[outy1+1]);
	    *pic0++ =CLIP( 128 + outv[outv1]);
	    *pic1++ =CLIP( outy[outy2]);
	    *pic1++ =CLIP( 128 + outu[outu1+8]);
	    *pic1++ =CLIP( outy[outy2+1]);
	    *pic1++ =CLIP( 128 + outv[outv1+8]);
	    outv1 += 2; outu1 += 2;
	    outy1 +=2; outy2 +=2;	   
	  }	  
	    outy += 16;outu +=16; outv +=16;
	    outv1 = 0; outu1=0;
	    outy1 = 0;
	    outy2 = 8;
	    pic0 += 2 * (width -8);
	    pic1 += 2 * (width -8);	    
	}
    
}

static void yuv400pto422(int * out,unsigned char *pic,int width)
{
    int j, k;
    unsigned char *pic0, *pic1;
    int *outy ;
    int outy1 = 0;
    int outy2 = 8;
    pic0 = pic;
    pic1 = pic + width;
    outy = out;
      
	for (j = 0; j < 4; j++) {
	    for (k = 0; k < 4; k++) {	    
	    *pic0++ = CLIP(outy[outy1]);
	    *pic0++ = 128 ;
	    *pic0++ = CLIP(outy[outy1+1]);
	    *pic0++ = 128 ;
	    *pic1++ = CLIP(outy[outy2]);
	    *pic1++ = 128 ;
	    *pic1++ = CLIP(outy[outy2+1]);
	    *pic1++ = 128 ;
	     outy1 +=2; outy2 +=2;  
	  }	  
	    outy += 16;
	    outy1 = 0;
	    outy2 = 8;
	    pic0 += 2 * (width -8);
	    pic1 += 2 * (width -8);	    
	}
    
}

int 
is_huffman(unsigned char *buf)
{
unsigned char *ptbuf;
int i = 0;
ptbuf = buf;
while (((ptbuf[0] << 8) | ptbuf[1]) != 0xffda){	
	if(i++ > 2048) 
		return 0;
	if(((ptbuf[0] << 8) | ptbuf[1]) == 0xffc4)
		return 1;
	ptbuf++;
}
return 0;
}

int initGlobals (struct GLOBAL *global) {
	
    	global->debug=0;
    
	if((global->videodevice = (char *) calloc(1, 16 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->videodevice\n");
		goto error;
	}
	
	snprintf(global->videodevice, 15, "/dev/video0");

	if((global->confPath = (char *) calloc(1, 80 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->confPath\n");
		goto error;
	}
	snprintf(global->confPath, 79, "%s/.guvcviewrc",getenv("HOME"));
	
	if((global->aviFPath = (pchar *) calloc(1, 2 * sizeof(pchar)))==NULL){
		printf("couldn't calloc memory for:global->aviFPath\n");
		goto error;
	}
	if((global->imgFPath = (pchar *) calloc(1, 2 * sizeof(pchar)))==NULL){
		printf("couldn't calloc memory for:global->imgFPath\n");
		goto error;
	}
	if((global->profile_FPath = (pchar *) calloc(1, 2 * sizeof(pchar)))==NULL){
		printf("couldn't calloc memory for:global->profile_FPath\n");
		goto error;
	}
	
	if((global->aviFPath[1] = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->aviFPath[1]\n");
		goto error;
	}
	snprintf(global->aviFPath[1], 2, "~");
	
	if((global->imgFPath[1] = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->imgFPath[1]\n");
		goto error;
	}
	snprintf(global->imgFPath[1], 99, "%s",getenv("HOME"));
	
	if((global->imgFPath[0] = (char *) calloc(1, 20 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->imageName\n");
		goto error;
	}
	snprintf(global->imgFPath[0],10,DEFAULT_IMAGE_FNAME);
	
	if((global->aviFPath[0] = (char *) calloc(1, 20 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->aviFPath[0]\n");
		goto error;
	}
	snprintf(global->aviFPath[0],12,DEFAULT_AVI_FNAME);
	
	if((global->sndfile= (char *) calloc(1, 32 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->sndfile\n");
		goto error;
	}
	//snprintf(global->sndfile,32,"%s",tempnam (NULL, "gsnd_"));/*generates a temporary file name*/
	snprintf(global->sndfile,32,"/tmp/guvc_sound_XXXXXX");/*template for temp sound file name*/
	
	if((global->profile_FPath[1] = (char *) calloc(1, 100 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->profile_FPath[1]\n");
		goto error;
	}
	snprintf(global->profile_FPath[1], 100, "%s",getenv("HOME"));
	
	if((global->profile_FPath[0] = (char *) calloc(1, 20 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->profile_FPath[0]\n");
		goto error;
	}
	snprintf(global->profile_FPath[0], 20, "default.gpfl");
	
	
	if((global->WVcaption= (char *) calloc(1, 32 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->WVcaption\n");
		goto error;
	}
	snprintf(global->WVcaption,10,"GUVCVIdeo");
	
   	global->image_inc=0;
   
	if((global->imageinc_str= (char *) calloc(1, 25 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->imageinc_str\n");
		goto error;
	}
	snprintf(global->imageinc_str,20,_("File num:%d"),global->image_inc);
	
	global->vid_sleep=0;
	global->avifile=NULL; /*avi filename passed through argument options with -n */
	global->Capture_time=0; /*avi capture time passed through argument options with -t */
	global->lprofile=0; /* flag for -l command line option*/
	global->imgFormat=0; /* 0 -JPG 1-BMP 2-PNG*/
	global->AVIFormat=0; /*0-"MJPG"  1-"YUY2" 2-"DIB "(rgb32)*/ 
	global->snd_begintime=0;/*begin time for audio capture*/
	global->currtime=0;
	global->lasttime=0;
	global->AVIstarttime=0;
	global->AVIstoptime=0;
	global->framecount=0;
	global->frmrate=5;
	global->Sound_enable=1; /*Enable Sound by Default*/
	global->Sound_SampRate=SAMPLE_RATE;
	global->Sound_SampRateInd=0;
	global->Sound_numInputDev=0;
	//Sound_IndexDev[20]; /*up to 20 input devices (should be alocated dinamicly)*/
	global->Sound_DefDev=0; 
	global->Sound_UseDev=0;
	global->Sound_NumChan=NUM_CHANNELS;
	global->Sound_NumChanInd=0;
	global->Sound_NumSec=NUM_SECONDS;
	global->Sound_BuffFactor=BUFF_FACTOR;
	global->FpsCount=0;
	global->timer_id=0;
	global->image_timer_id=0;
	global->image_timer=0;
	global->image_npics=999;/*default max number of captures*/
	global->frmCount=0;
	global->PanStep=2;/*2 degree step for Pan*/
	global->TiltStep=2;/*2 degree step for Tilt*/
	global->DispFps=0;
	global->fps = DEFAULT_FPS;
	global->fps_num = DEFAULT_FPS_NUM;
	global->bpp = 0; //current bytes per pixel
	global->hwaccel = 1; //use hardware acceleration
	global->grabmethod = 1;//default mmap(1) or read(0)
	global->width = DEFAULT_WIDTH;
	global->height = DEFAULT_HEIGHT;
	global->winwidth=WINSIZEX;
	global->winheight=WINSIZEY;
    	global->spinbehave=0;
	global->boxvsize=0;
	if((global->mode = (char *) calloc(1, 5 * sizeof(char)))==NULL){
		printf("couldn't calloc memory for:global->mode\n");
		goto error;
	}
	snprintf(global->mode, 4, "jpg");
	global->format = V4L2_PIX_FMT_MJPEG;
	global->formind = 0; /*0-MJPG 1-YUYV*/
	global->Frame_Flags = YUV_NOFILT;
	global->jpeg=NULL;
   	global->jpeg_size = 0;
	/* reset with videoIn parameters */
	global->jpeg_bufsize = 0; 
	return (0);
	
error:
	return(-1); /*no mem should exit*/

}

int closeGlobals(struct GLOBAL *global){
	if (global->videodevice) free(global->videodevice);
	if (global->confPath) free(global->confPath);
	if (global->aviFPath[1]) free(global->aviFPath[1]);
	if (global->imgFPath[1]) free(global->imgFPath[1]);
	if (global->imgFPath[0]) free(global->imgFPath[0]);
	if (global->aviFPath[0]) free(global->aviFPath[0]);
	if (global->sndfile) free(global->sndfile);
	if (global->profile_FPath[1]) free(global->profile_FPath[1]);
	if (global->profile_FPath[0]) free(global->profile_FPath[0]);
	if (global->aviFPath) free(global->aviFPath);
	if (global->imgFPath) free(global->imgFPath);
	if (global->profile_FPath) free(global->profile_FPath);
	if (global->WVcaption) free (global->WVcaption);
	if (global->imageinc_str) free(global->imageinc_str);
	if (global->avifile) free (global->avifile);
	if (global->mode) free(global->mode);
	global->videodevice=NULL;
	global->confPath=NULL;
	global->sndfile=NULL;
	global->avifile=NULL;
	global->mode=NULL;
	if(global->jpeg) free (global->jpeg);
	global->jpeg=NULL;
	free(global);
	return (0);
}

/* split fullpath in Path and filename*/
pchar* splitPath(char *FullPath, pchar* splited) 
{
	int i;
	int j;
	int k;
	int l;
	int FPSize;
	int FDSize;
	int FSize;
	char *tmpstr;
	tmpstr=FullPath;
	
	FPSize=strlen(FullPath);
	tmpstr+=FPSize;
	for(i=0;i<FPSize;i++) {
		if((*tmpstr--)=='/') {
			FSize=i;
			tmpstr+=2;/*must increment by 2 because of '/'*/
			if((FSize>19) && (FSize>strlen(splited[0]))) {
				// printf("realloc Filename to %d chars.\n",FSize);
				splited[0]=realloc(splited[0],(FSize+1)*sizeof(char));
			} 
			
			splited[0]=strncpy(splited[0],tmpstr,FSize);
			splited[0][FSize]='\0';
			/* cut spaces at begin of Filename String*/
			j=0;
			l=0;
			while((splited[0][j]==' ') && (j<100)) j++;/*count*/
			if (j>0) {
				for(k=j;k<strlen(splited[0]);k++) {
					splited[0][l++]=splited[0][k];
				}
				splited[0][l++]='\0';
			}
			
			FDSize=FPSize-FSize;
			if ((FDSize>99) && (FDSize>strlen(splited[1]))) {
				// printf("realloc FileDir to %d chars.\n",FDSize);
				splited[1]=realloc(splited[1],(FDSize+1)*sizeof(char));
			} 
			if(FDSize>0) {
				splited[1]=strncpy(splited[1],FullPath,FDSize);
				splited[1][FDSize]='\0';
				/* cut spaces at begin of Dir String*/
				j=0;
				l=0;
				while((splited[1][j]==' ') && (j<100)) j++;
				if (j>0) {
					for(k=j;k<strlen(splited[1]);k++) {
						splited[1][l++]=splited[1][k];
					}
					splited[1][l++]='\0';
				}
				/* check for "~" and replace with home dir*/
				//printf("FileDir[0]=%c\n",FileDir[0]);
				if(splited[1][0]=='~') {
					for(k=0;k<strlen(splited[1]);k++) {
						splited[1][k]=splited[1][k+1];
					}
					char path_str[100];
					char *home=getenv("HOME");
					sprintf(path_str,"%s%s",home,splited[1]);
					//printf("path is %s\n",path_str);
					FDSize=strlen(path_str);
					if (FDSize<99) {
						strncpy (splited[1],path_str,FDSize);
						splited[1][FDSize]='\0';
					}
					else printf("Error: Home Path(~) too big, keeping last.\n");
				}
			}
				
			break;
		}
	}
	
	if(i>=FPSize) { /* no dir specified */
		if ((FPSize>19) && (FPSize>strlen(splited[0])))  {
			// printf("realloc Filename to %d chars.\n",FPSize);
			splited[0]=realloc(splited[0],(FPSize+1)*sizeof(char));
		} 
		splited[0]=strncpy(splited[0],FullPath,FPSize);
		splited[0][FPSize]='\0';
	}
	//printf("Dir:%s File:%s\n",splited[1],splited[0]);
	tmpstr=NULL;/*clean up*/
	return (splited);
}


void cleanBuff(BYTE* Buff,int size)
{
	int i=0;
	for(i=0;i<size;i++) Buff[i]=0x00;
}
/*------------------------------- Color space conversions --------------------*/
/* regular yuv (YUYV) to rgb24*/
void 
yuyv2rgb (BYTE *pyuv, BYTE *prgb, int width, int height){
	int l=0;
	int SizeYUV=height * width * 2; /* 2 bytes per pixel*/
	for(l=0;l<SizeYUV;l=l+4) { /*iterate every 4 bytes*/
		/* b = y0 + 1.772 (u-128) */
		*prgb++=CLIP(pyuv[l] + 1.772 *( pyuv[l+1]-128)); 
		/* g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
		*prgb++=CLIP(pyuv[l] - 0.34414 * (pyuv[l+1]-128) -0.71414*(pyuv[l+3]-128));
		/* r =y0 + 1.402 (v-128) */
		*prgb++=CLIP(pyuv[l] + 1.402 * (pyuv[l+3]-128));                                                        
		/* b1 = y1 + 1.772 (u-128) */
		*prgb++=CLIP(pyuv[l+2] + 1.772*(pyuv[l+1]-128));
		/* g1 = y1 - 0.34414 (u-128) - 0.71414 (v-128)*/
		*prgb++=CLIP(pyuv[l+2] - 0.34414 * (pyuv[l+1]-128) -0.71414 * (pyuv[l+3]-128)); 
		/* r1 =y1 + 1.402 (v-128) */
		*prgb++=CLIP(pyuv[l+2] + 1.402 * (pyuv[l+3]-128));
	}
	
}

/* yuv (YUYV) to bgr with lines upsidedown */
/* used for bitmap files (DIB24)           */
void 
yuyv2bgr (BYTE *pyuv, BYTE *pbgr, int width, int height){

	int l=0;
	int k=0;
	BYTE *preverse;
	int bytesUsed;
	int SizeBGR=height * width * 3; /* 3 bytes per pixel*/
	/* BMP byte order is bgr and the lines start from last to first*/
	preverse=pbgr+SizeBGR;/*start at the end and decrement*/
	//printf("preverse addr:%d | pbgr addr:%d | diff:%d\n",preverse,pbgr,preverse-pbgr);
	for(l=0;l<height;l++) { /*iterate every 1 line*/
		preverse-=width*3;/*put pointer at begin of unprocessed line*/
		bytesUsed=l*width*2;
		for (k=0;k<((width)*2);k=k+4)/*iterate every 4 bytes in the line*/
		{                              
		/* b = y0 + 1.772 (u-128) */
		*preverse++=CLIP(pyuv[k+bytesUsed] + 1.772 *( pyuv[k+1+bytesUsed]-128)); 
		/* g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
		*preverse++=CLIP(pyuv[k+bytesUsed] - 0.34414 * (pyuv[k+1+bytesUsed]-128) -0.71414*(pyuv[k+3+bytesUsed]-128));
		/* r =y0 + 1.402 (v-128) */
		*preverse++=CLIP(pyuv[k+bytesUsed] + 1.402 * (pyuv[k+3+bytesUsed]-128));                                                        
		/* b1 = y1 + 1.772 (u-128) */
		*preverse++=CLIP(pyuv[k+2+bytesUsed] + 1.772*(pyuv[k+1+bytesUsed]-128));
		/* g1 = y1 - 0.34414 (u-128) - 0.71414 (v-128)*/
		*preverse++=CLIP(pyuv[k+2+bytesUsed] - 0.34414 * (pyuv[k+1+bytesUsed]-128) -0.71414 * (pyuv[k+3+bytesUsed]-128)); 
		/* r1 =y1 + 1.402 (v-128) */
		*preverse++=CLIP(pyuv[k+2+bytesUsed] + 1.402 * (pyuv[k+3+bytesUsed]-128));
		}
		preverse-=width*3;/*get it back at the begin of processed line*/
	}
	//printf("preverse addr:%d | pbgr addr:%d | diff:%d\n",preverse,pbgr,preverse-pbgr);
	preverse=NULL;

}
/*-------------------------------- YUV Filters -------------------------------*/
/* Flip YUYV frame horizontal*/
void 
yuyv_mirror (BYTE *frame, int width, int height){
	
	int h=0;
	int w=0;
	int sizeline = width*2; /* 2 bytes per pixel*/ 
	BYTE *pframe;
	pframe=frame;
	BYTE line[sizeline-1];/*line buffer*/
    	for (h=0; h < height; h++) { /*line iterator*/
        	for(w=sizeline-1; w > 0; w = w - 4) { /* pixel iterator */
            	line[w-1]=*pframe++;
	    		line[w-2]=*pframe++;
	    		line[w-3]=*pframe++;
	    		line[w]=*pframe++;
		}
        	memcpy(frame+(h*sizeline), line, sizeline); /*copy reversed line to frame buffer*/           
    }
	
}


void 
yuyv_negative(BYTE* frame, int width, int height)
{
	int size=width*height*2;
	int i=0;
    for(i=0; i < size; i++)
        frame[i] = ~frame[i];     
}

void 
yuyv_upturn(BYTE* frame, int width, int height)
{   
	int h=0;
	int sizeline = width*2; /* 2 bytes per pixel*/ 
	BYTE *pframe;
	pframe=frame;
	BYTE line1[sizeline-1];/*line1 buffer*/
	BYTE line2[sizeline-1];/*line2 buffer*/
    for (h=0; h < height/2; h++) { /*line iterator*/	
       	memcpy(line1,frame+h*sizeline,sizeline);
		memcpy(line2,frame+(height-1-h)*sizeline,sizeline);
			
		memcpy(frame+h*sizeline, line2, sizeline);
		memcpy(frame+(height-1-h)*sizeline, line1, sizeline);
	}      
}


void
yuyv_monochrome(BYTE* frame, int width, int height) 
{
    int size=width*height*2;
	int i=0;

	for(i=0; i < size; i = i + 4) { /* keep Y - luma */
        frame[i+1]=0x80;/*U - median (half the max value)=128*/
        frame[i+3]=0x80;/*V - median (half the max value)=128*/        
    }
}


/*----------------------------------- Image Files ----------------------------*/ 
int 
SaveJPG(const char *Filename,int imgsize,BYTE *ImagePix) {
int ret=0;
int jpgsize=0;
BYTE *jpgtmp=NULL;
BYTE *Pimg=NULL;
BYTE *Pjpg=NULL;
BYTE *tp=NULL;
int Hlength;	
JPGFILEHEADER JpgFileh;
int jpghsize=sizeof(JpgFileh);
FILE *fp;
jpgsize=imgsize+sizeof(JPEGHuffmanTable)+4;/*huffman table +marker +size*/
if((jpgtmp=malloc(jpgsize))!=NULL) {
	
	Pjpg=jpgtmp;
	Pimg=ImagePix;
	memmove(&JpgFileh,Pimg,jpghsize);
	JpgFileh.JFIF[0]=0x4a;//JFIF0
	JpgFileh.JFIF[1]=0x46;
	JpgFileh.JFIF[2]=0x49;
	JpgFileh.JFIF[3]=0x46;
	JpgFileh.JFIF[4]=0x00;
	JpgFileh.VERS[0]=0x01;//version 1.2
	JpgFileh.VERS[1]=0x02;
	
	memmove(Pjpg,&JpgFileh,jpghsize);
	/*moves to the end of the header struct*/
	Pjpg+=jpghsize;
	Pimg+=jpghsize;
	/*adds the rest of the header if any*/
	Hlength=4+JpgFileh.length[0]*255+JpgFileh.length[1]+69+69;
	memmove(Pjpg,Pimg,(Hlength-jpghsize));
	Pjpg+=(Hlength-jpghsize); 
	Pimg+=(Hlength-jpghsize);
	/*adds Quantization tables and everything else until   * 
	 * start of frame marker (FFC0)                        */
	tp=Pimg;
	while(((*tp!=0xff) && (*tp++!=0xc0)) && tp!=NULL ) {
		tp+=2;
	}
	int qtsize=tp-Pimg;
	memmove(Pjpg,Pimg,qtsize);
	Pjpg+=qtsize; 
	Pimg+=qtsize;
	/*insert huffman table with marker (FFC4) and length(x01a2)*/
	BYTE HUFMARK[4];
	HUFMARK[0]=0xff;
	HUFMARK[1]=0xc4;
	HUFMARK[2]=0x01;
	HUFMARK[3]=0xa2;
	memmove(Pjpg,&HUFMARK,4);
	Pjpg+=4;
	memmove(Pjpg,&JPEGHuffmanTable,JPG_HUFFMAN_TABLE_LENGTH);/*0x01a0*/
	Pjpg+=JPG_HUFFMAN_TABLE_LENGTH;
	
    memmove(Pjpg,Pimg,(imgsize-(Pimg-ImagePix)));
	
	
	
	if ((fp = fopen(Filename,"wb"))!=NULL) {
		  
		 //fwrite(ImagePix,imgsize,1,fp);/*raw*/
		fwrite(jpgtmp,jpgsize,1,fp);/*jpeg - jfif*/
		
		fclose(fp);
	} else {
		ret=1;
	}

	free(jpgtmp);
	jpgtmp=NULL;
	Pimg=NULL;
	Pjpg=NULL;
	tp=NULL;
	
} else {
	printf("could not allocate memmory for jpg file\n");
	ret=1;
}

return ret;
}


int 
SaveBuff(const char *Filename,int imgsize,BYTE *data) {
	FILE *fp;
	int ret = 0;
	if ((fp = fopen(Filename,"wb"))!=NULL) {
	
		fwrite(data,imgsize,1,fp);/*jpeg - jfif*/
		
		fclose(fp);
	} else {
		ret = 1;
	}
	return (ret);
}

int 
SaveBPM(const char *Filename, long width, long height, int BitCount, BYTE *ImagePix) {

int ret=0;
BITMAPFILEHEADER BmpFileh;
BITMAPINFOHEADER BmpInfoh;
DWORD imgsize;
FILE *fp;

imgsize=width*height*BitCount/8;

BmpFileh.bfType=0x4d42;//must be BM (x4d42) 
BmpFileh.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+imgsize; //Specifies the size, in bytes, of the bitmap file
BmpFileh.bfReserved1=0; //Reserved; must be zero
BmpFileh.bfReserved2=0; //Reserved; must be zero
BmpFileh.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER); /*Specifies the offset, in bytes, 
			    from the beginning of the BITMAPFILEHEADER structure 
			    to the bitmap bits*/

BmpInfoh.biSize=40;
BmpInfoh.biWidth=width; 
BmpInfoh.biHeight=height; 
BmpInfoh.biPlanes=1; 
BmpInfoh.biBitCount=BitCount; 
BmpInfoh.biCompression=0; // 0 
BmpInfoh.biSizeImage=imgsize; 
BmpInfoh.biXPelsPerMeter=0; 
BmpInfoh.biYPelsPerMeter=0; 
BmpInfoh.biClrUsed=0; 
BmpInfoh.biClrImportant=0;

if ((fp = fopen(Filename,"wb"))!=NULL) {  // (wb) write in binary mode
		fwrite(&BmpFileh, sizeof(BITMAPFILEHEADER), 1, fp);
		fwrite(&BmpInfoh, sizeof(BITMAPINFOHEADER),1,fp);
		fwrite(ImagePix,imgsize,1,fp);
		
		fclose(fp);
} else {
	ret=1;
	printf("ERROR: Could not open file %s for write \n",Filename);
}


return ret;
}

/* write a png file */
int write_png(char *file_name, int width, int height,BYTE *prgb_data)
{
   int l=0;
   FILE *fp;
   png_structp png_ptr;
   png_infop info_ptr;
   png_text text_ptr[3];
   
   png_bytep row_pointers[height];
   /* open the file */
   fp = fopen(file_name, "wb");
   if (fp == NULL)
      return (1);

   /* Create and initialize the png_struct with the desired error handler
    * functions.  If you want to use the default stderr and longjump method,
    * you can supply NULL for the last three parameters.  We also check that
    * the library version is compatible with the one used at compile time,
    * in case we are using dynamically linked libraries.
    */
   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
      NULL, NULL, NULL);

   if (png_ptr == NULL)
   {
      fclose(fp);
      return (2);
   }
	
   /* Allocate/initialize the image information data. */
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fclose(fp);
      png_destroy_write_struct(&png_ptr,  png_infopp_NULL);
      return (3);
   }
  
   /* Set error handling.  REQUIRED if you aren't supplying your own
    * error handling functions in the png_create_write_struct() call.
    */
   if (setjmp(png_jmpbuf(png_ptr)))
   {
      /* If we get here, we had a problem reading the file */
      fclose(fp);
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return (4);
   }
   
   /* set up the output control using standard C streams */
   png_init_io(png_ptr, fp);
	
	/* turn on or off filtering, and/or choose
       specific filters.  You can use either a single
       PNG_FILTER_VALUE_NAME or the bitwise OR of one
       or more PNG_FILTER_NAME masks. */
    /* png_set_filter(png_ptr, 0,
       PNG_FILTER_NONE  | PNG_FILTER_VALUE_NONE |
       PNG_FILTER_SUB   | PNG_FILTER_VALUE_SUB  |
       PNG_FILTER_UP    | PNG_FILTER_VALUE_UP   |
       PNG_FILTER_AVE   | PNG_FILTER_VALUE_AVE  |
       PNG_FILTER_PAETH | PNG_FILTER_VALUE_PAETH|
       PNG_ALL_FILTERS);*/
	 /* set the zlib compression level */
    /*png_set_compression_level(png_ptr,
        Z_BEST_COMPRESSION);*/

    /* set other zlib parameters */
    /*png_set_compression_mem_level(png_ptr, 8);
    png_set_compression_strategy(png_ptr,
        Z_DEFAULT_STRATEGY);
    png_set_compression_window_bits(png_ptr, 15);
    png_set_compression_method(png_ptr, 8);
    png_set_compression_buffer_size(png_ptr, 8192);*/
	
	png_set_IHDR(png_ptr, info_ptr, width, height,
       8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
       PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  
	/* Optional gamma chunk is strongly suggested if you have any guess
    * as to the correct gamma of the image.
    */
   //png_set_gAMA(png_ptr, info_ptr, gamma);

   /* Optionally write comments into the image */
   text_ptr[0].key = "Title";
   text_ptr[0].text = file_name;
   text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
   text_ptr[1].key = "Software";
   text_ptr[1].text = "guvcview";
   text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
   text_ptr[2].key = "Description";
   text_ptr[2].text = "File generated by guvcview <http://guvcview.berlios.de>";
   text_ptr[2].compression = PNG_TEXT_COMPRESSION_NONE;
#ifdef PNG_iTXt_SUPPORTED
   text_ptr[0].lang = NULL;
   text_ptr[1].lang = NULL;
   text_ptr[2].lang = NULL;
#endif
   png_set_text(png_ptr, info_ptr, text_ptr, 3);

   /* Write the file header information.  REQUIRED */
   png_write_info(png_ptr, info_ptr);
	
	
	
   /* flip BGR pixels to RGB */
   png_set_bgr(png_ptr);
	
   /* Write the image data.*/
   for (l = 0; l < height; l++)
     row_pointers[l] = prgb_data + l*width*3;
  
	png_write_image(png_ptr, row_pointers);
	
	/* You can write optional chunks like tEXt, zTXt, and tIME at the end
    * as well.  Shouldn't be necessary in 1.1.0 and up as all the public
    * chunks are supported and you can use png_set_unknown_chunks() to
    * register unknown chunks into the info structure to be written out.
    */

   /* It is REQUIRED to call this to finish writing the rest of the file */
   png_write_end(png_ptr, info_ptr);

	/* If you png_malloced a palette, free it here (don't free info_ptr->palette,
      as recommended in versions 1.0.5m and earlier of this example; if
      libpng mallocs info_ptr->palette, libpng will free it).  If you
      allocated it with malloc() instead of png_malloc(), use free() instead
      of png_free(). */
   //~ png_free(png_ptr, palette);
   //~ palette=NULL;

   /* Similarly, if you png_malloced any data that you passed in with
      png_set_something(), such as a hist or trans array, free it here,
      when you can be sure that libpng is through with it. */
   //~ png_free(png_ptr, trans);
   //~ trans=NULL;

   /* clean up after the write, and free any memory allocated */
   png_destroy_write_struct(&png_ptr, &info_ptr);

   /* close the file */
   fclose(fp);
   for(l=0;l<height;l++) {
   		row_pointers[l]=NULL;
   }

   /* that's it */
   return (0);
}
	
