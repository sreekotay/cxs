// =================================================================================================================
// xs_utils.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_UTILS_H_
#define _xs_UTILS_H_


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
static int              xs_fromhex          (const char c);
static int              xs_tohex            (int v, int upper);
static int              xs_strappend        (char* d, int n, const char* s);
static int              xs_strlappend       (char* d, int n, const char* s, int l);
static int              xs_b64_encode       (char *dst, const unsigned char *src, int count);


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


#endif //for entire file

