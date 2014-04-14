// =================================================================================================================
// xs_printf.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_PRINTF_H_
#define _xs_PRINTF_H_

#include "xs_types.h"

// =================================================================================================================
//  interface and functions -- declarations
// =================================================================================================================
typedef int (xs_sprintflush) (void* userdata, const char* str, int len);

static int xs_printf (const char* fmt, ...);
static int xs_sprintf (char* str, int len, const char* fmt, ...);
static int xs_fprintf (FILE* f, const char* fmt, ...);
static int xs_printf_va (const char* fmt, va_list apin);
static int xs_sprintf_va (char* str, int len, const char* fmt, va_list apin);
static int xs_fprintf_va (FILE* f, const char* fmt, va_list apin);
static int xs_sprintf_core (xs_sprintflush* sflush, void* userdata, char* str, int len, const char* fmt, va_list apin);



//helper
#if !defined(va_copy)
#define va_copy(x, y)       x=y
#endif





// =================================================================================================================
//  implementation
// =================================================================================================================
// =================================================================================================================
//  utilities
// =================================================================================================================
#define xs_min(a,b)     (((a)<(b)) ? (a) : (b))
#define xs_max(a,b)     (((a)>(b)) ? (a) : (b))

static int convert_i32_to_backwards_str(char*s, int ssize, unsigned long v, int r, int c) {
    unsigned long d;
    int i=0;
    do {
        d=v;  v/=r;  d= d-v*r;
        if (d>9) s[i++] = (char)(d - 10 + ((c=='x') ? 'a' : 'A'));
        else     s[i++] = (char)(d + '0');
    } while (v && i<ssize);
    return i;
}

static int convert_i64_to_backwards_str(char*s, int ssize, xsuint64 v, int r, int c) {
    xsuint64 d;
    int i=0;
    do {
        d=v;  v/=r;  d= d-v*r;
        if (d>9) s[i++] = (char)(d - 10 + ((c=='x') ? 'a' : 'A'));
        else     s[i++] = (char)(d + '0');
    } while (v && i<ssize);
    return i;
}


static int xs_filep(void* userdata, const char* buf, int len) {return (int)fwrite (buf, 1, len, (FILE*)userdata);}

static int xs_printf (const char* fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = xs_sprintf_core (xs_filep, (void*)stdout, 0, 0, fmt, ap);
    va_end(ap);
    return result;
}
static int xs_printf_va (const char* fmt, va_list apin) {
    va_list ap;
    int result;
    va_copy(ap, apin);
    result = xs_sprintf_core (xs_filep, (void*)stdout, 0, 0, fmt, ap);
    va_end(ap);
    return result;
}

static int xs_fprintf (FILE* f, const char* fmt, ...) {
    va_list ap;
    int result;
    va_start(ap, fmt);
    result = xs_sprintf_core (xs_filep, (void*)f, 0, 0, fmt, ap);
    va_end(ap);
    return result;
}

static int xs_fprintf_va (FILE* f, const char* fmt, va_list apin) {
    va_list ap;
    int result;
    va_copy(ap, apin);
    result = xs_sprintf_core (f ? xs_filep : 0, (void*)f, 0, 0, fmt, ap);
    va_end(ap);
    return result;
}

static int xs_sprintf (char* str, int len, const char* fmt, ...) {
    va_list ap;
    int result; 
    va_start(ap, fmt);
    result = xs_sprintf_core (0, 0, str, len?len-1:0, fmt, ap); //len-1 = leave room for termination
    if (result>=0 && result<len && str) str[result]=0; //always terminate
    else if (str&&len)                  str[0]=0; //some bad error          
    va_end(ap);
    return result;
}
static int xs_sprintf_va (char* str, int len, const char* fmt, va_list apin) {
    va_list ap;
    int result; 
    va_copy(ap, apin);
    result = xs_sprintf_core (0, 0, str, len?len-1:0, fmt, ap); //len-1 = leave room for termination
    if (result>=0 && result<len && str) str[result]=0; //always terminate
    else if (str&&len)                  str[0]=0; //some bad error          
    va_end(ap);
    return result;
}


// =================================================================================================================
//  core function
// =================================================================================================================
static int xs_sprintf_core (xs_sprintflush* sflush, void* userdata, char* str, int len, const char* fmt, va_list apin) {
    va_list ap;
    #define exs_buf_room_sprintf    1024
    char c, *p, s[64], o, t[exs_buf_room_sprintf]; //t array must be larger than longest int format (binary 64-bit)
    int i, j, f, fo, w, pr, l, r, vs, *ip, neg, ind=0, tot=0, sroom=len, cc;
    if (str==0)             {len=INT_MAX; str=t; sroom=exs_buf_room_sprintf;} //in case we redefine char, use sizeof
    else if (len==0)        return 0;
    if (fmt==0)             return 0;
    #define _xspf_flush     do {if (sflush && (cc=(*sflush)(userdata, str, ind))!=ind) {ind=cc; goto END;} tot+=ind; ind=0;} while(0)
    va_copy(ap, apin);
    while (*fmt && ind<len) {
        if (ind==sroom)     _xspf_flush;
        c = *fmt++;
        if (c!='%') { //copy blocks of "static" text
            p = (char*)fmt--; while (*p && *p!='%') p++;
            //p = (char*)strchr(fmt,'%'); if (p==0) p=(char*)fmt+strlen(fmt);
            l = (int)(p-fmt);
            if (l==1) {
                str[ind++]=c;
                fmt++;
                continue;
            } else if (l+ind>sroom) {
                do {
                    _xspf_flush;
                    i = (l>sroom) ? sroom : l;
                    memcpy (str+ind, fmt, i*sizeof(t[0]));
                    ind += i;
                    fmt += i;
                    l -= i;
                } while (l);
            } else {
                memcpy (str+ind, fmt, l*sizeof(t[0]));
                ind += l;
                fmt += l;
            }
            continue;
        }

        if (c!='%')         {str[ind++]=c; continue;}
        c = *fmt++;
        if (c=='%')         {str[ind++]=c; continue;}

        //flags
        f = 0; fo = 1;
        do {
            switch (c) {
                case '0':   {f |= 1;  c = *fmt++;} break; // fill with zeroes
                case '-':   {f |= 2;  c = *fmt++;} break; // left-justify
                case '+':   {f |= 4;  c = *fmt++;} break; // force '+' before non-negative
                case ' ':   {f |= 8;  c = *fmt++;} break; // force ' ' before non-negative
                case '#':   {f |= 16; c = *fmt++;} break; // force '0', '0x' or '0X' before binary/hex
                default:    fo=0;                  break; //get ALL the flags -- dupes seem to be allowed in the spec
            }
        } while (fo);

        //width
        w = 0; //default
        if (c=='*') {
            c = *fmt++;
            w = va_arg(ap, int);
        } else for (; c>='0' && c<='9'; c=*fmt++)           
            w = w*10 + c - '0';
        if (w>sroom) w=sroom; //cap it

        //precision
        pr = -1; //default -- note that 'pr==0' means something specific
        if (c=='.') {
            pr = 0;
            c = *fmt++;
            if (c=='*') {
                c = *fmt++;
                pr = va_arg(ap, int);
            } else for (; c>='0' && c<='9'; c=*fmt++)           
                pr = pr*10 + c - '0';
        }
        if (pr>sroom) pr=sroom; //cap it

        //format modifiers
        vs = sizeof(int); //default for "int"
        switch (c) {
            case 'h':
                c = *fmt++;
                if (c=='h') {vs = 1; c = *fmt++;} 
                else         vs = 2;
                break;
            case 'L':
                c = *fmt++;
                vs = -(int)sizeof(long double);
                break;
            case 'l':
                c = *fmt++;
                if (c=='l') {vs = 8; c = *fmt++;} 
                else         vs = 4;
                break;
            case 'I':
                c = *fmt++;
                if (c=='6') {
                    if (*fmt!='4') {c=0; break;} //special case - error?
                    fmt++; vs = 8;  c = *fmt++;
                } else if (c=='3') {
                    if (*fmt!='2') {c=0; break;} //special case - error?
                    fmt++; vs = 4;  c = *fmt++;
                }
                break;
            case 'z':
                c = *fmt++;
                vs = sizeof(size_t);
                break;
        }

        //ready
        if (!c) break; //error!!

        //format handling
        r=10; //default
        switch (c) {
            case 'a':   case 'A':   case 'e':   //fall through
            case 'E':   case 'f':   case 'g':   r=-1;       break;
            case 'b':                           r=2;        break;
            case 'o':                           r=8;        break;
            case 'i':   case 'd':   case 'u':   r=10;       break;
            case 'X':   case 'x':               r=16;       break;
            case 'p':   vs = sizeof(fmt);       r=16;       break; //sizeof ptr, use hex
            case 'n':
                ip = va_arg(ap, int*);
                if (ip) *ip = ind;
                continue;  //will goto while
            case 's':
                p = va_arg(ap, char*);
                l = (int)strlen(p);
                if (pr>=0) l = xs_min (l, pr);
                l = j = xs_min(len-ind, l);

                //write with padding
                if ((f&2)==0 && w-j+ind>sroom)  _xspf_flush;
                if ((f&2)==0)                   while (j++<w) str[ind++]=' ';
                if (l+ind>sroom)                _xspf_flush;
                if (vs==8)  {}// to do -- wide char to char
                else if (l+ind>sroom) {
                    do {
                        _xspf_flush;
                        i = (l>sroom) ? sroom : l;
                        memcpy (str+ind, p, i*sizeof(t[0]));
                        ind += i;
                        p += i;
                        l -= i;
                    } while (l);
                } else {
                    memcpy (str+ind, p, l*sizeof(t[0]));
                    ind += l;
                }
                if (w-j+ind>sroom)  _xspf_flush;
                while (j++<w)       str[ind++]=' ';
                continue;   //will goto while
            case 'c':
                if (vs==8)  {}// to do -- wide char to char
                else        str[ind++] = ((char)va_arg(ap, int)); 
                continue;   //will goto while
            default:
                //error?
                str[ind++]=c; 
                continue;   //will goto while
        }

        //write number
        if (r>0) {
            //integers
            xsuint64 v64 = 0;
            unsigned long v = 0;
            if (vs==8)                      v64 = va_arg(ap, xsint64);
            else if (vs==4)                 v   = va_arg(ap, xsint32);
            else if (vs==2)                 v   = va_arg(ap, int);
            else if (vs==1)                 v   = va_arg(ap, int);
            if (vs==8)                      neg = ((v64&0x8000000000000000)!=0);
            else                            neg = ((v&0x80000000)!=0);
            if (neg) {
                v = 0 - v;
                v64 = 0 - v64;
                str[ind++] = '-';
            } else {
                if (f&4)                    str[ind++] = '+';
                else if (f&8)               str[ind++] = ' ';
                else if ((f&16) && v)   {
                    if (r==8 || r==16)      str[ind++] = '0';
                    if (ind==sroom)         _xspf_flush;
                    if (r==16 && ind<len)   str[ind++] =  c; //write 'x' or 'X' :)
                }
                if (pr==0 && v==0 && v64==0) continue; //goto while... special case
            }

            if (ind==sroom)                 _xspf_flush;
            if (vs==8)                      i = convert_i64_to_backwards_str(s, sizeof(s), v64, r, c);
            else                            i = convert_i32_to_backwards_str(s, sizeof(s), v,   r, c);

            //write it with padding
            if (pr-i>=0 && pr-i+ind>sroom)  _xspf_flush;
            pr -= i; while (ind<len && pr-->0) str[ind++]='0';
            j = i = xs_min(len-ind, i); 
            o = (f&1) ? '0' : ' ';
            if ((f&2)==0 && w-j+ind>sroom)  _xspf_flush;
            if ((f&2)==0)                   while (j++<w) str[ind++]=o;
            if (i+ind>sroom)    _xspf_flush;
            do str[ind++] = s[--i]; while(i);
            if (w-j+ind>sroom)  _xspf_flush;
            while (j++<w)       str[ind++]=o;
        } else {
            /* to do -- place holder for floating point support */
            xsuint64 v64 = 0;
            unsigned long v = 0;
            if (vs==8)                      v64 = va_arg(ap, xsint64);
            else if (vs==4)                 v   = va_arg(ap, xsint32);
            else                            v   = va_arg(ap, int);
            (void)v64; (void)v;
        }
    }
END:
    va_end(ap);
    if (sflush && (cc=(*sflush)(userdata, str, ind))!=ind) ind=cc;      //make sure we flush the remaining stuff
    return ind+tot;
}


#endif //_xs_PRINTF_H_
