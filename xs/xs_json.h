// =================================================================================================================
// xs_json.h - copyright (c) 2006 2014 Sree Kotay - All rights reserved
// =================================================================================================================
#ifndef _xs_JSON_H_
#define _xs_JSON_H_

// ====================================================================================================================
//  JSON Enums
// ====================================================================================================================
enum 
{
    // ==========================
    //  Tag Types
    // ==========================
    exs_JSON_Key=1,
	exs_JSON_Value,
    exs_JSON_ObjectStart,
    exs_JSON_ArrayStart,
    exs_JSON_ObjectEnd,
    exs_JSON_ArrayEnd,

    exs_JSON_String,
    exs_JSON_Number,
    exs_JSON_Int,
    exs_JSON_Boolean,
    exs_JSON_Null,
    

    // ==========================
    //  Parser return values
    // ==========================
    exs_JSON_EndOfFile		=  1,
    exs_JSON_Incomplete		= -1, //unterminated tag, section, comment, or the like
    exs_JSON_Err_Syntax		= -2,
};

typedef struct xs_json xs_json;
typedef struct xs_json_tag {
	char	type, stype, has_escape;
    char*	ptr;
    int		len;
    union	{int i; double	d;};
} xs_json_tag;


xs_json*	xs_json_create		(const char* str, int len);
xs_json*	xs_json_destroy		(xs_json* js);
int			xs_json_next		(xs_json* js, xs_json_tag* tag, int permissive);
int			xs_json_unescape	(char* dst, int dlen, const char* src, int slen);	//for reading (note: does not force null termination)
int			xs_json_escape		(char* dst, int dlen, const char* src, int slen);	//for writing: TODO

#endif //header




// =================================================================================================================
//  implementation
// =================================================================================================================
#ifdef _xs_IMPLEMENTATION_
#ifndef _xs_JSON_IMPL_
#define _xs_JSON_IMPL_

#undef _xs_IMPLEMENTATION_
#include "xs_arr.h"
#include "xs_utils.h"
#define _xs_IMPLEMENTATION_


typedef struct xs_json {
    char	*mptr, *msrc, *mend;
    int		mstate;
    xs_arr	mstack;
} xs_json;


// =================================================================================================================
// basic string handling  
// =================================================================================================================
char*   xs_JSONReadString   (const char *str, char start, char end, char escapeChar, char* hasEscape) {
    int ret=0, hasstart;

    //skip the spaces
    str								= xs_skipspaces (str, 1);
    if (str==0)						return 0;
    if (hasEscape)					*hasEscape = 0;
    
    //abort if it's not the start;
    hasstart = (*str == start);
    if (hasstart) str++;

    //read the string
    while (*str && ret==0)	{
        //abort properly if we hit the end
        if (*str==escapeChar)						{str++; str++; if (hasEscape) *hasEscape = 1;}	//escape char
        else if (*str==end)							{str++; ret=1;}
		else if (hasstart==0 &&
			    (*str==':' || *str==','))			ret=1;
        else										str++;
    }

    //return
    return ret||hasstart==0 ? (char*)str : 0;
}

// ====================================================================================================================
//  xs_json_create
// ====================================================================================================================
xs_json* xs_json_create(const char* str, int len) {
	xs_json* js = (xs_json*)calloc (sizeof(xs_json), 1);
	if (js==0) return 0;

	js->mptr = js->msrc = (char*)str;
	js->mend = js->msrc + len;
	xs_arr_makespace(int, js->mstack, 16);	//make room
	xs_arr_push(int, js->mstack, -1);		//one object only
	return js;
}

// ====================================================================================================================
//  xs_json_destroy
// ====================================================================================================================
xs_json* xs_json_destroy(xs_json* js) {
	if (js) free (js);
	return 0;
}

// ====================================================================================================================
//  xs_json_next
// ====================================================================================================================
int xs_json_next(xs_json* js, xs_json_tag* tag, int permissive) {
    char* pend = js->mend, *sptr, *nptr, *vptr, hasEscape;
    int sv;

    //are we done?
	if (js==0 || tag==0)								return -1;
    if (js->mptr==0 || js->mptr==0 || js->mptr>=pend)	return xs_arr_count(js->mstack)==1 ? exs_JSON_EndOfFile : exs_JSON_Incomplete;

    //read...
    sptr							= xs_skipspaces (js->mptr, 1);
    nptr							= xs_JSONReadString (sptr, '"', '"', '\\', &hasEscape);

    if (nptr==0 || nptr<=sptr)		return xs_arr_count(js->mstack)==1 ? exs_JSON_EndOfFile : exs_JSON_Incomplete;

	tag->ptr = 0;
	tag->type = tag->stype = tag->i = tag->len = 0;
	tag->has_escape = hasEscape;

	
    if (js->mstate==0 && permissive && *sptr!='{' && *sptr!='[') js->mstate = exs_JSON_ObjectStart;	//implicit object

    //state
    if (js->mstate==-1)	 {
        //multiple objects, or too many "ends"
        return exs_JSON_Err_Syntax;

    } else if (js->mstate==exs_JSON_ObjectStart && *sptr!='}') {
        //end parsing object
        nptr--;						
        if (nptr<sptr)						return exs_JSON_Err_Syntax; //must be a key (string)
        if (*sptr!='\"' && permissive==0)	return exs_JSON_Err_Syntax; //must be a key (string)
        if (*nptr!='\"' && permissive==0)	return exs_JSON_Err_Syntax; //must be a key (string)
		if (*nptr!='\"') nptr++;
        js->mstate = exs_JSON_Value;

        //success
        tag->ptr	= sptr+(*sptr=='\"');
        tag->len	= (int)(nptr-sptr-(*nptr=='\"'));
        tag->type	= exs_JSON_Key;
		if (*nptr=='\"') nptr++;

        //next token must be colon
        vptr						= xs_skipspaces (nptr, 1);
        if (vptr==0 || *vptr!=':')	return exs_JSON_Err_Syntax;
        nptr = vptr+1;				//skip the colon

    } else if (*sptr=='{') {
        //starting object
        nptr = sptr+1;
        js->mstate = exs_JSON_ObjectStart;
        xs_arr_push(int, js->mstack, js->mstate);
        tag->type = exs_JSON_ObjectStart;

    } else if (*sptr=='}') {
        nptr = sptr+1;
        if (xs_arr_pop(int, js->mstack, sv)==0)		return exs_JSON_Err_Syntax; 
        if (sv!=exs_JSON_ObjectStart)				return exs_JSON_Err_Syntax; 

        if (xs_arr_count(js->mstack)==0)			return exs_JSON_Err_Syntax;
        js->mstate = xs_arr_last(int, js->mstack);
        tag->type  = exs_JSON_ObjectEnd;

        //skip trailing comma
        vptr				= xs_skipspaces (nptr, 1);
        if (*vptr==',')		nptr = vptr+1;
        else				nptr = vptr;

    } else if (*sptr=='[') {
        nptr = sptr+1;
        js->mstate = exs_JSON_ArrayStart;
        xs_arr_push(int, js->mstack, js->mstate);
        tag->type  = exs_JSON_ArrayStart;

    } else if (*sptr==']') {
        nptr = sptr+1;
        if (xs_arr_pop(int, js->mstack, sv)==0)		return exs_JSON_Err_Syntax; 
        if (sv!=exs_JSON_ArrayStart)				return exs_JSON_Err_Syntax; 

        if (xs_arr_count(js->mstack)==0)			return exs_JSON_Err_Syntax;
        js->mstate = xs_arr_last(int, js->mstack);
        tag->type  = exs_JSON_ArrayEnd;

        //skip trailing comma
        vptr				= xs_skipspaces (nptr, 1);
        if (*vptr==',')		nptr = vptr+1;
        else				nptr = vptr;

    } else {
        if ((js->mstate==exs_JSON_ArrayStart) || 
            (js->mstate==exs_JSON_Value) || 
            (js->mstate==0 && permissive)) {
            //must be array, value, or first "object"
        } else return exs_JSON_Err_Syntax;


        //parsing value
        if (*sptr!='\"') {
            //could be true, false, null, or must be number
            int ch = xs_lower_ (*sptr);
			tag->ptr  = sptr;
			tag->len  = (int)(nptr-sptr);
			tag->type = exs_JSON_Value;
            if (ch=='t' || ch=='f' || ch=='n') {
                if		(xs_strncmp_case (sptr, "true", 4)==0)	{nptr=sptr+4; tag->stype = exs_JSON_Boolean;  tag->i = 1;}
                else if (xs_strncmp_case (sptr, "false", 5)==0)	{nptr=sptr+5; tag->stype = exs_JSON_Boolean;  tag->i = 0;}
                else if	(xs_strncmp_case (sptr, "null", 4)==0)	{nptr=sptr+4; tag->stype = exs_JSON_Null;	  tag->i = 0;}
                else return exs_JSON_Err_Syntax;
            } else if (permissive==0) {
                //must be number
				/*
                double value	= 0;
                nptr			= xs_atod (sptr, &value, (int)(pend-sptr));
                if (nptr==0 || nptr<=sptr) return exs_JSON_Err_Syntax;

                //value
                tag->d			= value;
				*/
                tag->stype		= exs_JSON_Number;
            } else tag->stype	= exs_JSON_String;

        } else {
            //parsing string
            nptr--;						
            if (nptr<sptr)								return exs_JSON_Err_Syntax; //must be a key (string)
            if (*sptr!='\"' && permissive==0)			return exs_JSON_Err_Syntax; //must be a key (string)
            if (*nptr!='\"' && permissive==0)			return exs_JSON_Err_Syntax; //must be a key (string)
			if (*nptr!='\"') nptr++;

            //success
            tag->ptr	= sptr+(*sptr=='\"');
            tag->len	= (int)(nptr-sptr-(*nptr=='\"'));
            tag->type	= exs_JSON_Value;
			tag->stype	= exs_JSON_String;
			if (*nptr=='\"') nptr++;
        }

        //next token must be comma or container terminator
        if (js->mstate != 0) {
            char* vptr = xs_skipspaces (nptr, 1);
            if (vptr==0 || 
                (*vptr!=',' &&
                 *vptr!=']' &&
                 *vptr!='}' ))
                 return exs_JSON_Err_Syntax;

            //skip the comma
            if (*vptr==',')					nptr = vptr+1;	
        }

        //get ready to parse the next element
        if (xs_arr_count(js->mstack)==0)	return exs_JSON_Err_Syntax;
        js->mstate = xs_arr_last(int, js->mstack);
    }

	//success
	js->mptr = nptr;
	return 0;
}


int  xs_json_unescape (char* dst, int dlen, const char* src, int slen) {
	//copy the rest of the text
	char* d = dst;
	const char* s = src, *e = src+(slen>dlen?dlen:slen);
	int ha, hb, hc, hd;
	if (slen==0 || dlen==0 || src==0 || dst==0) return 0;
	while (s<e && *s) {
		if (*s=='\\') {
			//detected an escape
			s++;
			if (s>=e)	return exs_JSON_Err_Syntax;
			switch (*s) {
				case 'r':	*d++ = '\r';	break;
				case 'n':	*d++ = '\n';	break;
				case 'f':	*d++ = '\f';	break;
				case 'b':	*d++ = '\b';	break;
				case 't':	*d++ = '\t';	break;
				case '"':	*d++ = '"';		break;
				case '\\':	*d++ = '\\';	break;
				case '/':	*d++ = '/';		break;
				case 'u':
					if (s+3>=e)	return exs_JSON_Err_Syntax;
					ha = xs_fromhex(s[0]);	 if (ha<0)	return exs_JSON_Err_Syntax;
					hb = xs_fromhex(s[1]);	 if (hb<0)	return exs_JSON_Err_Syntax;
					hc = xs_fromhex(s[2]);	 if (hc<0)	return exs_JSON_Err_Syntax;
					hd = xs_fromhex(s[3]);	 if (hd<0)	return exs_JSON_Err_Syntax;
					*d++ = (ha<<12) + (hb<<8) + (hc<<4) + hd;
					s+=3; //4th will be added below
					break;
				default:
					return exs_JSON_Err_Syntax;
					break;
			}
		} else *d++ = *s;
		s++;
	}
	return 0;
}

#endif //_xs_JSON_IMPL_
#endif //_xs_IMPLEMENTATION_