// =================================================================================================================
// xs_compress.h - copyright (c) 2006 2014 Sree Kotay - All rights reserverd
// =================================================================================================================
#ifndef _xs_COMPRESS_H_
#define _xs_COMPRESS_H_


// =================================================================================================================
// function declarations
// =================================================================================================================
typedef struct xs_compress xs_compress;
enum {
    exs_Decompress_ZLIB = 0,
    exs_Decompress_GZIP,
    exs_Decompress_RAW,
    exs_Compress_ZLIB,
    exs_Compress_GZIP,
    exs_Compress_RAW
};


int             xs_compress_create      (xs_compress** xcp, int doCompress, int level);
int             xs_compress_setdict     (xs_compress* xc, const char* dict, int dictlen);
xs_compress*    xs_compress_destroy     (xs_compress* xc);
int             xs_compress_err         (xs_compress* xc);
int             xs_compress_in          (xs_compress* xc, char* buf, size_t len, char isLastInput);
size_t          xs_compress_out         (xs_compress* xc, char* buf, size_t len);


#endif //header









// =================================================================================================================
// implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_COMPRESS_IMPL_
#define _xs_COMPRESS_IMPL_

#undef _xs_IMPLEMENTATION_
#include "xs_crc.h"
#define _xs_IMPLEMENTATION_


#if 0
    #include "miniz_116.c"
#else
    #define TRUE_ZLIB
    #define crc32 xs_crc32
    //#define xs_NO_GZIP
    #include "zlib/adler32.c"
    //#include "zlib/crc32.c"
    #include "zlib/deflate.c"
    #include "zlib/inflate.c"
    #include "zlib/inftrees.c"
    #include "zlib/trees.c"
    #include "zlib/zutil.c"
    #include "zlib/inffast.c"
    #ifndef Z_DEFAULT_WINDOW_BITS
    #define Z_DEFAULT_WINDOW_BITS 15
    #endif //Z_DEFAULT_WINDOW_BITS
#endif
#define XS_CHUNK_SIZE   (8192)


// =========================================================================
// core compress definition and functions
// =========================================================================
struct xs_compress {
    char            out[XS_CHUNK_SIZE];
    z_stream        z;
    size_t          outsize, outavail, rtotal, wtotal, headermin;
    xsuint32        crc;
    int             errnum;
    size_t          outoffset, headeroffset;
    unsigned int    is_last:2, is_zlib:2, is_gzip:2, is_compress:2, skip_string:2;
};


int xs_compress_create (xs_compress** xcp, int compressType, int level) {
    xs_compress* xc;
    int ret, dwb;
    if (xcp==0) return -50; *xcp=0;
    if ((xc=calloc(sizeof(*xc), 1))==0) return -108;
    xc->outsize = sizeof(xc->out);
    switch (compressType) {
        case exs_Compress_ZLIB:
        case exs_Compress_GZIP:
        case exs_Compress_RAW:      xc->is_compress=1; break;
    }
    switch (compressType) {
        case exs_Decompress_ZLIB:
        case exs_Compress_ZLIB:     xc->is_zlib=1; break;
        case exs_Decompress_GZIP:
        case exs_Compress_GZIP:     xc->is_gzip=1; break;
    }
    dwb =xc->is_zlib ? Z_DEFAULT_WINDOW_BITS : -Z_DEFAULT_WINDOW_BITS;
    if (xc->is_compress)  ret = deflateInit2(&xc->z, level,  Z_DEFLATED, dwb, 9, Z_DEFAULT_STRATEGY);
    else                  ret = inflateInit2(&xc->z,                     dwb);
    *xcp = xc;
    return ret;
}

xs_compress* xs_compress_destroy (xs_compress* xc) {
    if (xc==0) return 0;
    inflateEnd (&xc->z);
    free(xc);
    return 0;
}

int xs_compress_setdict (xs_compress* xc, const char* dict, int dictlen) {
    if (xc==0) return -1;
#ifdef TRUE_ZLIB
    if (xc->is_compress)    return deflateSetDictionary (&xc->z, (const Bytef*)dict, dictlen);
    else                    return inflateSetDictionary (&xc->z, (const Bytef*)dict, dictlen);
#else
    assert(0);
    return -50;
#endif
}


int xs_compress_err(xs_compress* xc) {
    if (xc==0 || xc->errnum==Z_BUF_ERROR) return 0;
    return xc->errnum;
}


// =========================================================================
// core compress/decompress INPUT function 
// =========================================================================
int xs_compress_in (xs_compress* xc, char* buf, size_t len, char isLastInput) {
#ifndef xs_NO_GZIP
    size_t outlen, offset;
    unsigned char* in;
    if (xc->is_gzip) {
        if (xc->is_compress) 
            xc->crc = xs_crc32(xc->crc, buf, len);
        else if (xc->headeroffset==0) { 
        REPEAT:
            in = (unsigned char*)xc->out;
            if (xc->headermin==0) xc->headermin = 10;//10 is minimum header size
            if (xc->rtotal<xc->headermin) {
                if (len+xc->rtotal>xc->headermin)    outlen = xc->headermin-xc->rtotal;
                else                                 outlen = len;
                memcpy (in+xc->rtotal, buf, outlen);
                xc->rtotal += outlen;
                buf += outlen;
                len -= outlen;
            }
            #define _check_gz_hdr(a)    if ((size_t)(a)>xc->rtotal) {xc->headermin=(a); if (len) goto REPEAT; return isLastInput ? (xc->errnum=Z_DATA_ERROR) : 0;}
            _check_gz_hdr(4);
            if (in[0]!=0x1f || in[1]!=0x8b || in[2]!=0x08) return (xc->errnum=Z_DATA_ERROR);
            offset = 10;

            //check flags
            if (in[3]&0x02) {offset+=2;}                                                                                  //16-bit CRC multi-part gzip
            if (in[3]&0x04) {_check_gz_hdr(offset+2); offset += (in[offset+1]<<8) + in[offset+0] + 2;}                    //extra fields + 2 for length
            if (in[3]&0x08) {_check_gz_hdr(offset+1); while (in[offset]) {_check_gz_hdr(offset+1); offset++;} offset++;}  //name     - extra for NULL
            if (in[3]&0x10) {_check_gz_hdr(offset+1); while (in[offset]) {_check_gz_hdr(offset+1); offset++;} offset++;}  //comment  - extra for NULL
            _check_gz_hdr(offset);

            //skip header
            xc->headeroffset = offset;
            buf += offset-xc->rtotal;
            len -= offset-xc->rtotal;;
       }
    }
#endif
    xc->z.avail_in = len;
    xc->z.next_in = (Bytef*)buf;
    xc->is_last = (isLastInput!=0);
    xc->rtotal += len;
    return 0;
}

// =========================================================================
// core compress/decompress OUTPUT function 
// =========================================================================
size_t xs_compress_out (xs_compress* xc, char* buf, size_t len) {
#ifndef xs_NO_GZIP
    unsigned char ftr[8], hdr[10], *ch; //minimal gzip header
#endif //xs_NO_GZIP
    int ret=0;
    size_t outlen, totlen=0, cc;
    z_stream *z = &xc->z;
    if (buf==0 || len<=0) {xc->errnum=-50; return 0;}

    // ========================
    // write header
    // ========================
#ifndef xs_NO_GZIP
    if (xc->wtotal<sizeof(hdr) && xc->is_compress && xc->is_gzip) {
        //write gzip header
        memset(hdr, 0, sizeof(hdr));
        hdr[0]='\x1f'; hdr[1]='\x8b'; hdr[2]='\x08'; 
        if (len<(sizeof(hdr)-xc->wtotal))  outlen = len;
        else                               outlen = sizeof(hdr)-xc->wtotal;
        memcpy (buf, hdr+xc->wtotal, outlen);
        xc->wtotal += outlen;
        totlen += outlen;
        len -= outlen;
    }
#endif //xs_NO_GZIP

    // ===========================================
    // core compress/decompress loop 
    // ===========================================
    if (len && xc->is_last<2)
    do {
        //copy to output buffer
        if (xc->outavail) {
            if (0) {//debugging
                if (xc->outavail<xc->outsize) xc->out[xc->outavail]=0;
                fwrite (xc->out, 1, xc->outavail, stdout);
                xc->outavail =0;
                continue;
            }
            if (xc->outavail>=(size_t)len)  outlen = len;
            else                            outlen = (int)xc->outavail;
        #ifndef xs_NO_GZIP
            if (xc->is_gzip && xc->is_compress==0) 
                xc->crc = xs_crc32 (xc->crc, xc->out+xc->outoffset, outlen);
        #endif
            memcpy (buf+totlen, xc->out+xc->outoffset, outlen);
            xc->outavail -= outlen;
            xc->wtotal += outlen;
            totlen += outlen;
            len -= outlen;
            if (len==0) {
                //we are out of room, but have some left...
                if (xc->outavail) xc->outoffset += outlen;
                break;
            }
        }

        //avail
        if (ret || z->avail_in==0) break;

        //decompress
        xc->outoffset = 0;
        z->avail_out = xc->outsize;
        z->next_out = (Bytef*)xc->out;
        if (xc->is_compress)     ret = deflate(z, xc->is_last ? Z_FINISH : Z_NO_FLUSH);
        else                     ret = inflate(z, xc->is_last ? Z_FINISH : Z_NO_FLUSH);
        xc->outavail = xc->outsize-z->avail_out;
        if (ret<0)   xc->errnum = ret; //save error
        if (z->avail_out==0) 
            ret=1000;
    } while (1);


    // ========================
    // footer
    // ========================
#ifndef xs_NO_GZIP
    if (xc->is_last && totlen==0) {
        if (xc->is_last==1) {
            //in "footer mode"
            xc->outoffset = 0;
            xc->is_last = 2; //no more, even if we have a Z_BUF_ERROR
        }
        if (len && ret==0 && xc->outoffset<sizeof(ftr) &&
            xc->is_last && xc->is_compress && xc->is_gzip) {
            //write footer
            ftr[0] = (char)(xc->crc>>0);                ftr[4] = (char)(xc->rtotal>>0);
            ftr[1] = (char)(xc->crc>>8);                ftr[5] = (char)(xc->rtotal>>8);
            ftr[2] = (char)(xc->crc>>16);               ftr[6] = (char)(xc->rtotal>>16);
            ftr[3] = (char)(xc->crc>>24);               ftr[7] = (char)(xc->rtotal>>24);
            if (len<(sizeof(ftr)-xc->outoffset))        outlen = len;
            else                                        outlen = sizeof(ftr)-xc->outoffset;
            //printf ("crc %ld\n", (long)xc->crc); //for debugging
            memcpy (buf+totlen, ftr+xc->outoffset, outlen);
            xc->wtotal += outlen;
            totlen += outlen;
            len -= outlen;
            xc->outoffset += outlen;
        } else if (xc->is_last && xc->is_compress==0 && xc->is_gzip) {
            //check CRC and length
            if (z->avail_in<8)  xc->errnum = Z_DATA_ERROR;
            ch = (unsigned char*)z->next_in;
            cc = (((xsuint32)ch[0])<<0)|(((xsuint32)ch[1])<<8)|(((xsuint32)ch[2])<<16)|(((xsuint32)ch[3])<<24);
            if (cc!=xc->crc)  xc->errnum = Z_DATA_ERROR;
            cc = (((xsuint32)ch[4])<<0)|(((xsuint32)ch[5])<<8)|(((xsuint32)ch[6])<<16)|(((xsuint32)ch[7])<<24);
            if (cc!=xc->wtotal)  xc->errnum = Z_DATA_ERROR;
        }
    }
#endif //xs_NO_GZIP

     //need more data
    return totlen;
}



#endif //_xs_COMPRESS_IMPL_
#endif //_xs_IMPLEMENTATION_
