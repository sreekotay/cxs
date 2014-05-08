// =================================================================================================================
// xs_utils.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_UTILS_H_
#define _xs_UTILS_H_

#include "xs_types.h"

// =================================================================================================================
// function declarations
// =================================================================================================================
static int              xs_lower_           (int a);
static char*            xs_strlcpy          (char *d, const char *s, size_t n); 
static char*            xs_strlcat          (char *d, const char *s, size_t n);
static char*            xs_strndup          (const char *s, size_t n);
static char*            xs_strdup           (const char *str);          
static int              xs_strncmp_case     (const char *s1, const char *s2, size_t n);
static int              xs_strcmp_case      (const char *s1, const char *s2);
static const char*      xs_strstr_case      (const char *bstr, const char *sstr);
static int              xs_isalnum          (int c);
static int              xs_isalpha          (int c);
static char*            xs_itoa             (long val, char* str, int maxlength, int rad);
static char*            xs_atod             (const char *str, double *value, int len);
static int              xs_iround           (const double val);
static int              xs_fromhex          (const char c);
static int              xs_tohex            (int v, int upper);
static int              xs_strappend        (char* d, int n, const char* s);
static int              xs_strlappend       (char* d, int n, const char* s, int l);
static int              xs_b64_encode       (char *dst, const unsigned char *src, int count);
//xsuint32                xs_rand             ();
//static xsuint32         xs_rand_i           (int range)     {return xs_rand()%range;}
#define                 xs_rot32(x,k)       ((((xsuint32)x)<<(k))|(((xsuint32)x)>>(32-(k))))
static char	            xs_isspace	        (const char* str)					{return (*str) && ((*str=='\t') || (*str==' ') || (*str=='\r') || (*str=='\n'));}
static char*	        xs_skipspaces	    (const char* str, char skipSpace)	{if (str) while (*str && xs_isspace(str)==skipSpace)	str++;	return (char*)str;}


/*
typedef struct xs_conststr xs_conststr;

typedef struct xs_sstrpool {
    char*       pool;
    xs_atomic   space, count;
} xs_sstrpool;

typedef struct xs_conststr {
    xs_sstrpool* pool
    char         data[];
} xs_conststr;
*/







// =================================================================================================================
// simple string functions implementation
// =================================================================================================================
static int   xs_lower_(int a)                               {return (unsigned int)(a-'A')<=(unsigned int)('Z'-'A') ? a-('A'-'a') : a;} //lame, but fine 
static int   xs_isalpha(int c)                              {return (((c>='a') && (c<='z')) || ((c>='A') && (c<='Z')));} //lamer even, but fine 
static int   xs_isalnum(int c)                              {return (((c>='a') && (c<='z')) || ((c>='A') && (c<='Z')) || ((c>='0') && (c<='9')));} //lamer even, but fine 
static char* xs_strlcpy(char *d, const char *s, size_t n)   {char* i=d; if (n<1||d==0||s==d) return i; if (s) for (;*s!=0&&n>1; n--) *d++=*s++; *d=0; return i;}
static char* xs_strlcat(char *d, const char *s, size_t n)   {char* i=d; if (n<1||d==0||s==d) return i;        for (;*d!=0&&n>1; n--)  d++; if (n>1) xs_strlcpy(d,s,n); return i;}
static char* xs_strndup(const char *s, size_t n)            {return xs_strlcpy((char*)malloc(n+1),s,n+1);}
static char* xs_strdup(const char *str)                     {return xs_strndup(str, strlen(str));}
static void* xs_datadup(const void* p, size_t n)            {void* r=malloc(n); if (r) memcpy(r,p,n); return r;}
static void* xs_datandup(const void* p, size_t n, size_t c) {void* r=malloc(n>c?n:c); if (r) memcpy(r,p,c); return r;}

static int xs_strappend(char* d, int n, const char* s) {
    int l = (int)strlen(s);
    if (l>=n) l=n-1;
    if (l<0) return 0;
    memcpy(d,s,l); d[l]=0;
    return l;
}
static int xs_strlappend(char* d, int n, const char* s, int l) {
    if (l>=n) l=n-1;
    if (l<0) return 0;
    memcpy(d,s,l); d[l]=0;
    return l;
}

static int xs_fromhex(const char c) {
    if (c>='0' && c<='9') return c-'0'; 
    if (c>='A' && c<='F') return c-'A'+10; 
    if (c>='a' && c<='f') return c-'a'+10;
    return -1;
}

static int xs_tohex(int v, int upper) {
    return v<10 ? v+'0' : (upper?v+'A':v+'a');
}

static int xs_strncmp_case(const char *s1, const char *s2, size_t n) {
    //const char* si=s1; //needed below IF we return the count offset
    int v = 0;
    if (s1==s2) return 0;
    if (s2==0)  return 1;
    if (s1==0)  return -1;
    for (; n>0; s1++,s2++,n--) {
        v = xs_lower_(*s1)-xs_lower_(*s2);
        if (v || *s1==0 || *s2==0) return v;// v<0 ? (int)(si-s1-1) ? (int)(s1-si+1); //should we return the count offset?
    }
    return 0;
}

static int xs_strcmp_case(const char *s1, const char *s2) {
    return xs_strncmp_case(s1,s2,INT_MAX);
}

static const char *xs_strstr_case(const char *bstr, const char *sstr) {
    int slen = strlen(sstr), i;
    int dlen = strlen(bstr)-slen;
    int lc   = sstr ? xs_lower_(*sstr) : 0;
    for (i=0; i<=dlen; i++) {
        if (xs_lower_(bstr[i])==lc &&
        xs_strncmp_case(bstr+i, sstr, slen)==0)
            return bstr + i;
    }
    return NULL;
}

static int xs_strncmp(const char *s1, const char *s2, size_t n) {
    //const char* si=s1; //needed below IF we return the count offset
    int v = 0;
    if (s1==s2) return 0;
    if (s2==0)  return 1;
    if (s1==0)  return -1;
    for (; n>0; s1++,s2++,n--) {
        v = (*s1)-(*s2);
        if (v || *s1==0 || *s2==0) return v;// v<0 ? (int)(si-s1-1) ? (int)(s1-si+1); //should we return the count offset?
    }
    return 0;
}

static int xs_strcmp(const char *s1, const char *s2) {
    return xs_strncmp(s1,s2,INT_MAX);
}

static int xs_b64_encode(char *dst, const unsigned char *src, int count) {
    const char *b64_tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i=0, j=0;
    int a, b, c;
    
    while (i < count) {
        a = src[i];
        b = (i+1>=count) ? 0 : src[i+1];
        c = (i+2>=count) ? 0 : src[i+2];
        
        dst[j++]                    = b64_tab [a>>2];
        dst[j++]                    = b64_tab [((a&3 )<<4) | (b>>4)];
        if (i+1<count)     dst[j++] = b64_tab [((b&15)<<2) | (c>>6)];
        if (i+2<count)     dst[j++] = b64_tab [c&63];

        i+=3;
    }
    while (j&3) dst[j++] = '=';
    dst[j] = 0; //terminate
    return j;
}

// =================================================================================================================
// xs_itoa
// =================================================================================================================
static char* xs_itoa (long val, char* str, int maxlength, int rad)
{
    unsigned long uval;
    long oval, digval;
    char *beg, *end, temp;
    if (str==0&&maxlength!=0)       return 0;
    if (str!=0&&maxlength<2)        {if (maxlength) *str=0; return 0;}

    //negative
    if (val<0)  {*str++ = '-';  uval = (unsigned long)(-((long)val)); maxlength--;}
    else        uval = val;
    
    //first char (for inversion)
    end = (beg=str) + maxlength - 1; //-1 for termination

    //loop for digits
    do {//always do at least a 0
        oval            = uval;
        uval            = uval/rad;
        digval          = oval-(uval*rad); //same as: dival = uint32 (uval%rad);

        if (digval>9)   *str++ = (char)(digval-10+'a');
        else            *str++ = (char)(digval   +'0');
        }
    while (uval>0 && str<end);

    //terminate
    *str = 0;

    //reverse digits
    end  = str-1;   //-1 for termination, -1 for last digit
    do {
        temp    = *end;
        *end--  = *beg;
        *beg++  = temp;
        }
    while (beg<end);

    return str;
}


// ================================================================================================================
//  xs_atod
// ================================================================================================================
static float xs_floatHex(xsuint32 v)    {union { float f; xsuint32 d; } x; x.d=v;  return x.f;}
static double xs_nan()					{return xs_floatHex (0x7FFFFFFF);}
static double xs_snan()					{return xs_floatHex (0x7FFBFFFF);}
static double xs_infinity()				{return xs_floatHex (0x7F800000);}
static double xs_neg_infinity()			{return xs_floatHex (0xFF800000);}

    
static double xs_pow10 (int count) {
    //iterate
    double gxs_pow10R[] =  {1,10,100,1000,10000,100000,1000000,10000000,100000000};
    double val = 100000000;
    if (count<9) return gxs_pow10R[count];
    count = count-8; while (count-->0)  val*=10;
    return val;
}

static double xs_raisenegpow10 (double v, int count)	{return v/xs_pow10(count);}
static double xs_raisepow10 (double v, int count)		{return v*xs_pow10(count);}

static int xs_iround(const double val) {
    const double floatutil_xs_doublemagic = (6755399441055744.0); // 2^52 * 1.5
    union {xsuint64 i; double d;} dunion;
    dunion.d = val + floatutil_xs_doublemagic;
    return (int)dunion.i; // just cast to grab the bottom bits
}

static char* xs_atod (const char *str, double *value, int len) {
    char *estr		 = len ? (char*)str+len : (char*)str+65536;	//should be plenty
    int maxdigits	 = 29;
    double curval	 = 0;
    double floatval	 = 0;
    int digits	     = 0;
    char neg		 = 0;
    char negExp	     = 0;
    char signOK	     = 1;
    char dotOK	     = 1;
    char expOK	     = 1;
    char retval	     = 0;
    if (str==0)		 return 0;
    str				 = xs_skipspaces (str, 1);
    while (*str && str<estr && xs_isspace(str)==0)  {
        if ((*str>='0') && (*str<='9')) {
            signOK = 0;
            retval = 1;
            do {
                curval *= 10;
                curval += *str - '0';
                digits++;
                if (digits>maxdigits) estr=(char*)str; //error case
                str++;
            } while (str<estr && (*str>='0') && (*str<='9'));
            continue;
        } else if (signOK && ((*str == '-') || (*str == '+'))) {
            if (expOK)	neg		= (*str == '-');
            else		negExp	= (*str == '-');
            signOK		= 0;
        } else if (dotOK && (*str == '.')) {
            floatval	= curval;
            curval		= 0;
            digits		= 0;
            dotOK		= 0;
            signOK		= 0;
        } else if (expOK && ((*str == 'e') || (*str == 'E'))) {
            if (dotOK==0)
                 floatval	+= xs_raisenegpow10 (curval, digits);	//means there was a decimal point
            else floatval	 = curval;
            curval		 = 0;
            digits		 = 0;
            dotOK		 = 0;
            expOK		 = 0;
            signOK		 = 1;
        } else {
            if (digits==0) {
                if (*str=='#')		 {str++; if (*str==0 || str>=estr || xs_isspace(str))	break;}

                if      (xs_strncmp_case (str, "NaN",   3)==0)	{str+=3;	curval = xs_nan();}
                else if (xs_strncmp_case (str, "INF",   3)==0)	{str+=3;	curval = xs_infinity();}
                else if (xs_strncmp_case (str, "IND",   3)==0)	{str+=3;	curval = xs_nan();}
                else if (xs_strncmp_case (str, "QNaN",  4)==0)	{str+=4;	curval = xs_nan();}
                else if (xs_strncmp_case (str, "SNaN",  4)==0)	{str+=4;	curval = xs_snan();}
                else if (xs_strncmp_case (str, "NaNQ",  4)==0)	{str+=4;	curval = xs_nan();}
                else if (xs_strncmp_case (str, "NaNS",  4)==0)	{str+=4;	curval = xs_snan();}
                else break;

                retval	= 1;
                continue;
            }
            break;
        }
        str++;
    }

    if (1)/*(expOK+dotOK)!=2)*/ {
        xsint32 icv				      = expOK==0 ? xs_iround(curval) : 0;
        if (expOK==0)		floatval  = negExp ? xs_raisenegpow10(floatval, icv) : xs_raisepow10(floatval, icv);
        else if (dotOK==0)	floatval += xs_raisenegpow10 (curval, digits);	//means there was a decimal point
        else				floatval  = curval; 
        }
    else floatval  = curval; 

    if (neg) {
        if (floatval!=0)	floatval  = -floatval;
    #if _xs_doublesetnegative
        else				_xs_doublesetnegative (floatval);
    #endif
    }

    if (value)	*value	= floatval;
    return retval ? (char*)str : 0;
}

#endif //for header



// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_UTILS_IMPL_
#define _xs_UTILS_IMPL_

//http://burtleburtle.net/bob/rand/smallprng.html
//By Bob Jenkins, public domain
typedef struct xs_ranctx    {xsuint32 a, b, c, d;} xs_ranctx;
xsuint32 xs_ranval( xs_ranctx *x ) {
    xsuint32 e = x->a - xs_rot32(x->b, 27);
    x->a = x->b ^ xs_rot32(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

void xs_raninit( xs_ranctx *x, xsuint32 seed ) {
    xsuint32 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i=0; i<20; i++) xs_ranval(x);
}

int         gxs_raninit=0;
xs_ranctx   gxs_seed={0};
void        xs_rand_init(xsuint32 seed)   {gxs_raninit=1; xs_raninit(&gxs_seed, seed);}
xsuint32    xs_rand()                     {if (gxs_raninit==0) xs_rand_init (0x12345678); return xs_ranval(&gxs_seed);}


#endif //_xs_UTILS_IMPL_
#endif //_xs_IMPLEMENTATION_
