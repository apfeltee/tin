
#pragma once

/*
* fnmatch compatible functions, adapted from musl-libc.
* musl license:
*
*  Musl Libc
*  Copyright © 2005-2014 Rich Felker, et al.
*
*  Permission is hereby granted, free of charge, to any person obtaining
*  a copy of this software and associated documentation files (the
*  "Software"), to deal in the Software without restriction, including
*  without limitation the rights to use, copy, modify, merge, publish,
*  distribute, sublicense, and/or sell copies of the Software, and to
*  permit persons to whom the Software is furnished to do so, subject to
*  the following conditions:
*
*  The above copyright notice and this permission notice shall be
*  included in all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
*  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
*  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
*  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*/


#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#if !defined(MB_CUR_MAX)
    #define MB_CUR_MAX 4
#endif


#define FNM_PATHNAME 0x1
#define FNM_NOESCAPE 0x2
#define FNM_PERIOD 0x4
#define FNM_LEADING_DIR 0x8
#define FNM_CASEFOLD 0x10

/*
* these both are unused. probably used only in actual UNIX-esque glob().
* not needed here anyway.
*/
/*
#define FNM_FILE_NAME FNM_PATHNAME
#define FNM_NOSYS (-1)
*/
#define FNM_NOMATCH 1

#define FNSTATE_END 0
#define FNSTATE_UNMATCHABLE -2
#define FNSTATE_BRACKET -3
#define FNSTATE_QUESTION -4
#define FNSTATE_STAR -5

/* main.c */
static inline int strglobint_nextstring(const char* str, size_t n, size_t* step);
static inline int strglobint_nextpattern(const char* pat, size_t ofs, size_t* step, int flags);
static inline int strglobint_casefold(int k);
static inline int strglobint_dobracket(const char* p, int k, int kfold);
static inline int strglobint_perform(const char* pat, size_t ofs, const char* str, size_t n, int flags);
static inline int strglob_textmatch(const char* pat, size_t patlen, const char* str, size_t slen, int flags);
static inline int strglob_fnmatch(const char* pat, const char* str, int flags);

static inline int strglobint_nextstring(const char* str, size_t n, size_t* step)
{
    int k;
    wchar_t wc;
    if(!n)
    {
        *step = 0;
        return 0;
    }
    if(((unsigned int)str[0]) >= 128U)
    {
        k = mbtowc(&wc, str, n);
        if(k < 0)
        {
            *step = 1;
            return -1;
        }
        *step = k;
        return wc;
    }
    *step = 1;
    return str[0];
}

static inline int strglobint_nextpattern(const char* pat, size_t ofs, size_t* step, int flags)
{
    int chv;
    int esc;
    size_t cidx;
    wchar_t wc;
    esc = 0;
    if(!ofs || !*pat)
    {
        *step = 0;
        return FNSTATE_END;
    }
    *step = 1;
    if(pat[0] == '\\' && pat[1] && !(flags & FNM_NOESCAPE))
    {
        *step = 2;
        pat++;
        esc = 1;
        goto escaped;
    }
    if(pat[0] == '[')
    {
        cidx = 1;
        if(cidx < ofs)
        {
            if(pat[cidx] == '^' || pat[cidx] == '!')
            {
                cidx++;
            }
        }
        if(cidx < ofs)
        {
            if(pat[cidx] == ']')
            {
                cidx++;
            }
        }
        for(; cidx < ofs && pat[cidx] && pat[cidx] != ']'; cidx++)
        {
            if(cidx + 1 < ofs && pat[cidx + 1] && pat[cidx] == '[' && (pat[cidx + 1] == ':' || pat[cidx + 1] == '.' || pat[cidx + 1] == '='))
            {
                int nowidx = pat[cidx + 1];
                cidx += 2;
                if(cidx < ofs && pat[cidx])
                {
                    cidx++;
                }
                while(cidx < ofs && pat[cidx] && (pat[cidx - 1] != nowidx || pat[cidx] != ']'))
                {
                    cidx++;
                }
                if(cidx == ofs || !pat[cidx])
                {
                    break;
                }
            }
        }
        if(cidx == ofs || !pat[cidx])
        {
            *step = 1;
            return '[';
        }
        *step = cidx + 1;
        return FNSTATE_BRACKET;
    }
    if(pat[0] == '*')
    {
        return FNSTATE_STAR;
    }
    if(pat[0] == '?')
    {
        return FNSTATE_QUESTION;
    }
escaped:
    if(((unsigned int)pat[0]) >= 128U)
    {
        chv = mbtowc(&wc, pat, ofs);
        if(chv < 0)
        {
            *step = 0;
            return FNSTATE_UNMATCHABLE;
        }
        *step = chv + esc;
        return wc;
    }
    return pat[0];
}

static inline int strglobint_casefold(int k)
{
    int c = towupper(k);
    if(c == k)
    {
        return towlower(k);
    }
    return c;
}

static inline int strglobint_dobracket(const char* patptr, int k, int kfold)
{
    int nowch;
    int inv;
    int lch;
    int r1;
    int r2;
    unsigned int rv1a;
    unsigned int rv2a;
    unsigned int rv1b;
    unsigned int rv2b;
    wchar_t wc;
    wchar_t wc2;
    char buf[16];
    const char* p0;
    inv = 0;
    patptr++;
    if(*patptr == '^' || *patptr == '!')
    {
        inv = 1;
        patptr++;
    }
    if(*patptr == ']')
    {
        if(k == ']')
        {
            return !inv;
        }
        patptr++;
    }
    else if(*patptr == '-')
    {
        if(k == '-')
        {
            return !inv;
        }
        patptr++;
    }
    wc = patptr[-1];
    for(; *patptr != ']'; patptr++)
    {
        if(patptr[0] == '-' && patptr[1] != ']')
        {
            lch = mbtowc(&wc2, patptr + 1, 4);
            if(lch < 0)
            {
                return 0;
            }
            if(wc <= wc2)
            {
                rv1a = (unsigned)(k - wc);
                rv1b = ((unsigned int)(wc2 - wc));
                r1 = (rv1a <= rv1b);
                rv2a = ((unsigned)(kfold - wc));
                rv2b = ((unsigned int)(wc2 - wc));
                r2 = (rv2a <= rv2b);
                if(r1 || r2)
                {
                    return !inv;
                }
            }
            patptr += lch - 1;
            continue;
        }
        if(patptr[0] == '[' && (patptr[1] == ':' || patptr[1] == '.' || patptr[1] == '='))
        {
            p0 = patptr + 2;
            nowch = patptr[1];
            patptr += 3;
            while(patptr[-1] != nowch || patptr[0] != ']')
            {
                patptr++;
            }
            if(nowch == ':' && patptr - 1 - p0 < 16)
            {
                memcpy(buf, p0, patptr - 1 - p0);
                buf[patptr - 1 - p0] = 0;
                if(iswctype(k, wctype(buf)) || iswctype(kfold, wctype(buf)))
                {
                    return !inv;
                }
            }
            continue;
        }
        if(((unsigned int)*patptr) < 128U)
        {
            wc = (unsigned char)*patptr;
        }
        else
        {
            lch = mbtowc(&wc, patptr, 4);
            if(lch < 0)
            {
                return 0;
            }
            patptr += lch - 1;
        }
        if(wc == k || wc == kfold)
        {
            return !inv;
        }
    }
    return inv;
}

static inline int strglobint_perform(const char* pat, size_t ofs, const char* str, size_t n, int flags)
{
    const char* patptr;
    const char* plast;
    const char* endpat;
    const char* s;
    const char* stail;
    const char* endstr;
    size_t pinc;
    size_t sinc;
    size_t tailcnt = 0;
    int c;
    int k;
    int kfold;
    if(flags & FNM_PERIOD)
    {
        if(*str == '.' && *pat != '.')
        {
            return FNM_NOMATCH;
        }
    }
    while(true)
    {
        c = strglobint_nextpattern(pat, ofs, &pinc, flags);
        switch(c)
        {
            case FNSTATE_UNMATCHABLE:
                {
                    return FNM_NOMATCH;
                }
                break;
            case FNSTATE_STAR:
                {
                    pat++;
                    ofs--;
                }
                break;
            default:
                {
                    k = strglobint_nextstring(str, n, &sinc);
                    if(k <= 0)
                    {
                        if(c == FNSTATE_END)
                        {
                            return 0;
                        }
                        return FNM_NOMATCH;
                    }
                    str += sinc;
                    n -= sinc;
                    kfold = k;
                    if(flags & FNM_CASEFOLD)
                    {
                        kfold = strglobint_casefold(k);
                    }
                    if(c == FNSTATE_BRACKET)
                    {
                        if(!strglobint_dobracket(pat, k, kfold))
                        {
                            return FNM_NOMATCH;
                        }
                    }
                    else if(c != FNSTATE_QUESTION && k != c && kfold != c)
                    {
                        return FNM_NOMATCH;
                    }
                    pat += pinc;
                    ofs -= pinc;
                }
                continue;
        }
        break;
    }
    /* Compute real pat length if it was initially unknown/-1 */
    ofs = strnlen(pat, ofs);
    endpat = pat + ofs;
    /* Find the last * in pat and count chars needed after it */
    for(patptr = plast = pat; patptr < endpat; patptr += pinc)
    {
        switch(strglobint_nextpattern(patptr, endpat - patptr, &pinc, flags))
        {
            case FNSTATE_UNMATCHABLE:
                {
                    return FNM_NOMATCH;
                }
            case FNSTATE_STAR:
                {
                    tailcnt = 0;
                    plast = patptr + 1;
                }
                break;
            default:
                {
                    tailcnt++;
                }
                break;
        }
    }
    /*
    * Past this point we need not check for FNSTATE_UNMATCHABLE in pat,
    * because all of pat has already been parsed once.
    */
    /* Compute real str length if it was initially unknown/-1 */
    n = strnlen(str, n);
    endstr = str + n;
    if(n < tailcnt)
    {
        return FNM_NOMATCH;
    }
    /*
    * Find the final tailcnt chars of str, accounting for UTF-8.
    * On illegal sequences we may get it wrong, but in that case
    * we necessarily have a matching failure anyway.
    */
    for(s = endstr; s > str && tailcnt; tailcnt--)
    {
        if((((unsigned int)s[-1]) < 128U) || (MB_CUR_MAX == 1))
        {
            s--;
        }
        else
        {
            while((((unsigned char)*--s - 0x80U) < 0x40) && (s > str))
            {
            }
        }
    }
    if(tailcnt)
    {
        return FNM_NOMATCH;
    }
    stail = s;
    /* Check that the pat and str tails match */
    patptr = plast;
    while(true)
    {
        c = strglobint_nextpattern(patptr, endpat - patptr, &pinc, flags);
        patptr += pinc;
        if((k = strglobint_nextstring(s, endstr - s, &sinc)) <= 0)
        {
            if(c != FNSTATE_END)
            {
                return FNM_NOMATCH;
            }
            break;
        }
        s += sinc;
        kfold = k;
        if(flags & FNM_CASEFOLD)
        {
            kfold = strglobint_casefold(k);
        }
        if(c == FNSTATE_BRACKET)
        {
            if(!strglobint_dobracket(patptr - pinc, k, kfold))
            {
                return FNM_NOMATCH;
            }
        }
        else if(c != FNSTATE_QUESTION && k != c && kfold != c)
        {
            return FNM_NOMATCH;
        }
    }

    /* We're all done with the tails now, so throw them out */
    endstr = stail;
    endpat = plast;

    /* Match pattern components until there are none left */
    while(pat < endpat)
    {
        patptr = pat;
        s = str;
        while(true)
        {
            c = strglobint_nextpattern(patptr, endpat - patptr, &pinc, flags);
            patptr += pinc;
            /* Encountering * completes/commits a component */
            if(c == FNSTATE_STAR)
            {
                pat = patptr;
                str = s;
                break;
            }
            k = strglobint_nextstring(s, endstr - s, &sinc);
            if(!k)
            {
                return FNM_NOMATCH;
            }
            kfold = k; 
            if(flags & FNM_CASEFOLD)
            {
                kfold = strglobint_casefold(k);
            }
            if(c == FNSTATE_BRACKET)
            {
                if(!strglobint_dobracket(patptr - pinc, k, kfold))
                {
                    break;
                }
            }
            else if(c != FNSTATE_QUESTION && k != c && kfold != c)
            {
                break;
            }
            s += sinc;
        }
        if(c == FNSTATE_STAR)
        {
            continue;
        }
        /*
        * If we failed, advance str, by 1 char if it's a valid
        * char, or past all invalid bytes otherwise.
        */
        k = strglobint_nextstring(str, endstr - str, &sinc);
        if(k > 0)
        {
            str += sinc;
        }
        else
        {
            str++;
            while(strglobint_nextstring(str, endstr - str, &sinc) < 0)
            {
                str++;
            }
        }
    }
    return 0;
}

/**
 * Matches filename.
 *
 *   - `*` for wildcard
 *   - `?` for single character
 *   - `[abc]` to match character within set
 *   - `[!abc]` to match character not within set
 *   - `\*\?\[\]` for escaping above special syntax
 *
 * @see glob()
 */
static inline int strglob_textmatch(const char* pat, size_t patlen, const char* str, size_t slen, int flags)
{
    const char* s;
    const char* patptr;
    size_t inc;
    int c;
    /*
    * not used right now; assumes both are null-terminated.
    * good luck if they ain't.
    */
    (void)patlen;
    (void)slen;
    if(flags & FNM_PATHNAME)
    {
        while(true)
        {
            for(s = str; *s && *s != '/'; s++)
            {
            }
            for(patptr = pat; (c = strglobint_nextpattern(patptr, -1, &inc, flags)) != FNSTATE_END && c != '/'; patptr += inc)
            {
            }
            if(c != *s && (!*s || !(flags & FNM_LEADING_DIR)))
            {
                return FNM_NOMATCH;
            }
            if(strglobint_perform(pat, patptr - pat, str, s - str, flags))
            {
                return FNM_NOMATCH;
            }
            if(!c)
            {
                return 0;
            }
            str = s + 1;
            pat = patptr + inc;
        }
    }
    else if(flags & FNM_LEADING_DIR)
    {
        for(s = str; *s; s++)
        {
            if(*s != '/')
            {
                continue;
            }
            if(!strglobint_perform(pat, -1, str, s - str, flags))
            {
                return 0;
            }
        }
    }
    return strglobint_perform(pat, -1, str, -1, flags);
}

static inline int strglob_fnmatch(const char* pat, const char* str, int flags)
{
    return strglob_textmatch(pat, strlen(pat), str, strlen(str), flags);
}

static inline bool strglob_strglob(const char* pat, const char* str, size_t slen, int flags)
{
    return (strglob_textmatch(pat, strlen(pat), str, slen, flags) == 0);
}

