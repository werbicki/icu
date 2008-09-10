/*
*******************************************************************************
*
*   Copyright (C) 2004-2008, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ubidi_props.c
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004dec30
*   created by: Markus W. Scherer
*
*   Low-level Unicode bidi/shaping properties access.
*/

#include "unicode/utypes.h"
#include "unicode/uset.h"
#include "unicode/udata.h" /* UDataInfo */
#include "ucmndata.h" /* DataHeader */
#include "udatamem.h"
#include "umutex.h"
#include "uassert.h"
#include "cmemory.h"
#include "utrie.h"
#include "utrie2.h"
#include "ubidi_props.h"
#include "ucln_cmn.h"

struct UBiDiProps {
    UDataMemory *mem;
    const int32_t *indexes;
    const uint32_t *mirrors;
    const uint8_t *jgArray;

    UTrie trie;
    uint8_t formatVersion[4];
};

/* data loading etc. -------------------------------------------------------- */

#if UBIDI_HARDCODE_DATA

/* ubidi_props_data.c is machine-generated by genbidi --csource */
#include "ubidi_props_data.c"

#else

static UBool U_CALLCONV
isAcceptable(void *context,
             const char *type, const char *name,
             const UDataInfo *pInfo) {
    if(
        pInfo->size>=20 &&
        pInfo->isBigEndian==U_IS_BIG_ENDIAN &&
        pInfo->charsetFamily==U_CHARSET_FAMILY &&
        pInfo->dataFormat[0]==UBIDI_FMT_0 &&    /* dataFormat="BiDi" */
        pInfo->dataFormat[1]==UBIDI_FMT_1 &&
        pInfo->dataFormat[2]==UBIDI_FMT_2 &&
        pInfo->dataFormat[3]==UBIDI_FMT_3 &&
        pInfo->formatVersion[0]==1 &&
        pInfo->formatVersion[2]==UTRIE_SHIFT &&
        pInfo->formatVersion[3]==UTRIE_INDEX_SHIFT
    ) {
        UBiDiProps *bdp=(UBiDiProps *)context;
        uprv_memcpy(bdp->formatVersion, pInfo->formatVersion, 4);
        return TRUE;
    } else {
        return FALSE;
    }
}

static UBiDiProps *
ubidi_openData(UBiDiProps *bdpProto,
               const uint8_t *bin, int32_t length, UErrorCode *pErrorCode) {
    UBiDiProps *bdp;
    int32_t size;

    bdpProto->indexes=(const int32_t *)bin;
    if( (length>=0 && length<16*4) ||
        bdpProto->indexes[UBIDI_IX_INDEX_TOP]<16
    ) {
        /* length or indexes[] too short for minimum indexes[] length of 16 */
        *pErrorCode=U_INVALID_FORMAT_ERROR;
        return NULL;
    }
    size=bdpProto->indexes[UBIDI_IX_INDEX_TOP]*4;
    if(length>=0) {
        if(length>=size && length>=bdpProto->indexes[UBIDI_IX_LENGTH]) {
            length-=size;
        } else {
            /* length too short for indexes[] or for the whole data length */
            *pErrorCode=U_INVALID_FORMAT_ERROR;
            return NULL;
        }
    }
    bin+=size;
    /* from here on, assume that the sizes of the items fit into the total length */

    /* unserialize the trie, after indexes[] */
    size=bdpProto->indexes[UBIDI_IX_TRIE_SIZE];
    utrie_unserialize(&bdpProto->trie, bin, size, pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }
    bin+=size;

    /* get mirrors[] */
    size=4*bdpProto->indexes[UBIDI_IX_MIRROR_LENGTH];
    bdpProto->mirrors=(const uint32_t *)bin;
    bin+=size;

    /* get jgArray[] */
    size=bdpProto->indexes[UBIDI_IX_JG_LIMIT]-bdpProto->indexes[UBIDI_IX_JG_START];
    bdpProto->jgArray=bin;
    bin+=size;

    /* allocate, copy, and return the new UBiDiProps */
    bdp=(UBiDiProps *)uprv_malloc(sizeof(UBiDiProps));
    if(bdp==NULL) {
        *pErrorCode=U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    } else {
        uprv_memcpy(bdp, bdpProto, sizeof(UBiDiProps));
        return bdp;
    }
}

U_CFUNC UBiDiProps *
ubidi_openProps(UErrorCode *pErrorCode) {
    UBiDiProps bdpProto={ NULL }, *bdp;

    bdpProto.mem=udata_openChoice(NULL, UBIDI_DATA_TYPE, UBIDI_DATA_NAME, isAcceptable, &bdpProto, pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }

    bdp=ubidi_openData(
            &bdpProto,
            udata_getMemory(bdpProto.mem),
            udata_getLength(bdpProto.mem),
            pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        udata_close(bdpProto.mem);
        return NULL;
    } else {
        return bdp;
    }
}

U_CFUNC UBiDiProps *
ubidi_openBinary(const uint8_t *bin, int32_t length, UErrorCode *pErrorCode) {
    UBiDiProps bdpProto={ NULL };
    const DataHeader *hdr;

    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }
    if(bin==NULL) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    /* check the header */
    if(length>=0 && length<20) {
        *pErrorCode=U_INVALID_FORMAT_ERROR;
        return NULL;
    }
    hdr=(const DataHeader *)bin;
    if(
        !(hdr->dataHeader.magic1==0xda && hdr->dataHeader.magic2==0x27 &&
          hdr->info.isBigEndian==U_IS_BIG_ENDIAN &&
          isAcceptable(&bdpProto, UBIDI_DATA_TYPE, UBIDI_DATA_NAME, &hdr->info))
    ) {
        *pErrorCode=U_INVALID_FORMAT_ERROR;
        return NULL;
    }

    bin+=hdr->dataHeader.headerSize;
    if(length>=0) {
        length-=hdr->dataHeader.headerSize;
    }
    return ubidi_openData(&bdpProto, bin, length, pErrorCode);
}

#endif

U_CFUNC void
ubidi_closeProps(UBiDiProps *bdp) {
    if(bdp!=NULL) {
#if !UBIDI_HARDCODE_DATA
        udata_close(bdp->mem);
#endif
        uprv_free(bdp);
    }
}

/* UBiDiProps singleton ----------------------------------------------------- */

#if !UBIDI_HARDCODE_DATA
static UBiDiProps *gBdpDummy=NULL;
static UBiDiProps *gBdp=NULL;
static UErrorCode gErrorCode=U_ZERO_ERROR;
static int8_t gHaveData=0;

static UBool U_CALLCONV
ubidi_cleanup(void) {
    ubidi_closeProps(gBdpDummy);
    gBdpDummy=NULL;
    ubidi_closeProps(gBdp);
    gBdp=NULL;
    gErrorCode=U_ZERO_ERROR;
    gHaveData=0;
    return TRUE;
}
#endif

static UTrie2 bidiTrie2={ 0 };
static void *bidiTrie2Memory=NULL;

U_CAPI void U_EXPORT2
ubidi_initUTrie2(UErrorCode *pErrorCode) {
    bidiTrie2Memory=utrie2_fromUTrie(&bidiTrie2, &ubidi_props_singleton.trie, 0, FALSE, pErrorCode);
}

U_CFUNC const UBiDiProps *
ubidi_getSingleton(UErrorCode *pErrorCode) {
#if UBIDI_HARDCODE_DATA
    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }
    return &ubidi_props_singleton;
#else
    int8_t haveData;

    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }

    UMTX_CHECK(NULL, gHaveData, haveData);

    if(haveData>0) {
        /* data was loaded */
        return gBdp;
    } else if(haveData<0) {
        /* data loading failed */
        *pErrorCode=gErrorCode;
        return NULL;
    } else /* haveData==0 */ {
        /* load the data */
        UBiDiProps *bdp=ubidi_openProps(pErrorCode);
        if(U_FAILURE(*pErrorCode)) {
            gHaveData=-1;
            gErrorCode=*pErrorCode;
            return NULL;
        }

        /* set the static variables */
        umtx_lock(NULL);
        if(gBdp==NULL) {
            gBdp=bdp;
            bdp=NULL;
            gHaveData=1;
            ucln_common_registerCleanup(UCLN_COMMON_UBIDI, ubidi_cleanup);
        }
        umtx_unlock(NULL);

        ubidi_closeProps(bdp);
        return gBdp;
    }
#endif
}

#if !UBIDI_HARDCODE_DATA
U_CAPI const UBiDiProps *
ubidi_getDummy(UErrorCode *pErrorCode) {
    UBiDiProps *bdp;

    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }

    UMTX_CHECK(NULL, gBdpDummy, bdp);

    if(bdp!=NULL) {
        /* the dummy object was already created */
        return bdp;
    } else /* bdp==NULL */ {
        /* create the dummy object */
        int32_t *indexes;
        
        bdp=(UBiDiProps *)uprv_malloc(sizeof(UBiDiProps)+UBIDI_IX_TOP*4+UTRIE_DUMMY_SIZE);
        if(bdp==NULL) {
            *pErrorCode=U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        uprv_memset(bdp, 0, sizeof(UBiDiProps)+UBIDI_IX_TOP*4);

        bdp->indexes=indexes=(int32_t *)(bdp+1);
        indexes[UBIDI_IX_INDEX_TOP]=UBIDI_IX_TOP;

        indexes[UBIDI_IX_TRIE_SIZE]=
            utrie_unserializeDummy(&bdp->trie, indexes+UBIDI_IX_TOP, UTRIE_DUMMY_SIZE, 0, 0, TRUE, pErrorCode);
        if(U_FAILURE(*pErrorCode)) {
            uprv_free(bdp);
            return NULL;
        }

        bdp->formatVersion[0]=1;
        bdp->formatVersion[2]=UTRIE_SHIFT;
        bdp->formatVersion[3]=UTRIE_INDEX_SHIFT;

        /* set the static variables */
        umtx_lock(NULL);
        if(gBdpDummy==NULL) {
            gBdpDummy=bdp;
            bdp=NULL;
            ucln_common_registerCleanup(UCLN_COMMON_UBIDI, ubidi_cleanup);
        }
        umtx_unlock(NULL);

        uprv_free(bdp);
        return gBdpDummy;
    }
}
#endif

/* set of property starts for UnicodeSet ------------------------------------ */

static UBool U_CALLCONV
_enumPropertyStartsRange(const void *context, UChar32 start, UChar32 limit, uint32_t value) {
    /* add the start code point to the USet */
    const USetAdder *sa=(const USetAdder *)context;
    sa->add(sa->set, start);
    return TRUE;
}

U_CFUNC void
ubidi_addPropertyStarts(const UBiDiProps *bdp, const USetAdder *sa, UErrorCode *pErrorCode) {
    int32_t i, length;
    UChar32 c, start, limit;

    const uint8_t *jgArray;
    uint8_t prev, jg;

    if(U_FAILURE(*pErrorCode)) {
        return;
    }

    /* add the start code point of each same-value range of the trie */
    utrie_enum(&bdp->trie, NULL, _enumPropertyStartsRange, sa);

    /* add the code points from the bidi mirroring table */
    length=bdp->indexes[UBIDI_IX_MIRROR_LENGTH];
    for(i=0; i<length; ++i) {
        c=UBIDI_GET_MIRROR_CODE_POINT(bdp->mirrors[i]);
        sa->addRange(sa->set, c, c+1);
    }

    /* add the code points from the Joining_Group array where the value changes */
    start=bdp->indexes[UBIDI_IX_JG_START];
    limit=bdp->indexes[UBIDI_IX_JG_LIMIT];
    jgArray=bdp->jgArray;
    prev=0;
    while(start<limit) {
        jg=*jgArray++;
        if(jg!=prev) {
            sa->add(sa->set, start);
            prev=jg;
        }
        ++start;
    }
    if(prev!=0) {
        /* add the limit code point if the last value was not 0 (it is now start==limit) */
        sa->add(sa->set, limit);
    }

    /* add code points with hardcoded properties, plus the ones following them */

    /* (none right now) */
}

/* data access primitives --------------------------------------------------- */

/* UTRIE_GET16() itself validates c */
#define GET_PROPS(bdp, c, result) \
    UTRIE_GET16(&(bdp)->trie, c, result);

/* property access functions ------------------------------------------------ */

U_CFUNC int32_t
ubidi_getMaxValue(const UBiDiProps *bdp, UProperty which) {
    int32_t max;

    if(bdp==NULL) {
        return -1;
    }

    max=bdp->indexes[UBIDI_MAX_VALUES_INDEX];
    switch(which) {
    case UCHAR_BIDI_CLASS:
        return (max&UBIDI_CLASS_MASK);
    case UCHAR_JOINING_GROUP:
        return (max&UBIDI_MAX_JG_MASK)>>UBIDI_MAX_JG_SHIFT;
    case UCHAR_JOINING_TYPE:
        return (max&UBIDI_JT_MASK)>>UBIDI_JT_SHIFT;
    default:
        return -1; /* undefined */
    }
}

U_CAPI UCharDirection
ubidi_getClass(const UBiDiProps *bdp, UChar32 c) {
    uint32_t props;
    GET_PROPS(bdp, c, props);
    return (UCharDirection)UBIDI_GET_CLASS(props);
}

U_CFUNC UBool
ubidi_isMirrored(const UBiDiProps *bdp, UChar32 c) {
    uint32_t props;
    GET_PROPS(bdp, c, props);
    return (UBool)UBIDI_GET_FLAG(props, UBIDI_IS_MIRRORED_SHIFT);
}

U_CFUNC UChar32
ubidi_getMirror(const UBiDiProps *bdp, UChar32 c) {
    uint32_t props;
    int32_t delta;

    GET_PROPS(bdp, c, props);
    delta=((int16_t)props)>>UBIDI_MIRROR_DELTA_SHIFT;
    if(delta!=UBIDI_ESC_MIRROR_DELTA) {
        return c+delta;
    } else {
        /* look for mirror code point in the mirrors[] table */
        const uint32_t *mirrors;
        uint32_t m;
        int32_t i, length;
        UChar32 c2;

        mirrors=bdp->mirrors;
        length=bdp->indexes[UBIDI_IX_MIRROR_LENGTH];

        /* linear search */
        for(i=0; i<length; ++i) {
            m=mirrors[i];
            c2=UBIDI_GET_MIRROR_CODE_POINT(m);
            if(c==c2) {
                /* found c, return its mirror code point using the index in m */
                return UBIDI_GET_MIRROR_CODE_POINT(mirrors[UBIDI_GET_MIRROR_INDEX(m)]);
            } else if(c<c2) {
                break;
            }
        }

        /* c not found, return it itself */
        return c;
    }
}

U_CFUNC UBool
ubidi_isBidiControl(const UBiDiProps *bdp, UChar32 c) {
    uint32_t props;
    GET_PROPS(bdp, c, props);
    return (UBool)UBIDI_GET_FLAG(props, UBIDI_BIDI_CONTROL_SHIFT);
}

U_CFUNC UBool
ubidi_isJoinControl(const UBiDiProps *bdp, UChar32 c) {
    uint32_t props;
    GET_PROPS(bdp, c, props);
    return (UBool)UBIDI_GET_FLAG(props, UBIDI_JOIN_CONTROL_SHIFT);
}

U_CFUNC UJoiningType
ubidi_getJoiningType(const UBiDiProps *bdp, UChar32 c) {
    uint32_t props;
    GET_PROPS(bdp, c, props);
    return (UJoiningType)((props&UBIDI_JT_MASK)>>UBIDI_JT_SHIFT);
}

U_CFUNC UJoiningGroup
ubidi_getJoiningGroup(const UBiDiProps *bdp, UChar32 c) {
    UChar32 start, limit;

    start=bdp->indexes[UBIDI_IX_JG_START];
    limit=bdp->indexes[UBIDI_IX_JG_LIMIT];
    if(start<=c && c<limit) {
        return (UJoiningGroup)bdp->jgArray[c-start];
    } else {
        return U_JG_NO_JOINING_GROUP;
    }
}

/* public API (see uchar.h) ------------------------------------------------- */

U_CFUNC UCharDirection
u_charDirection(UChar32 c) {   
    UErrorCode errorCode=U_ZERO_ERROR;
    const UBiDiProps *bdp=ubidi_getSingleton(&errorCode);
    if(bdp!=NULL) {
        return ubidi_getClass(bdp, c);
    } else {
        return U_LEFT_TO_RIGHT;
    }
}

U_CFUNC UBool
u_isMirrored(UChar32 c) {
    UErrorCode errorCode=U_ZERO_ERROR;
    const UBiDiProps *bdp=ubidi_getSingleton(&errorCode);
    return (UBool)(bdp!=NULL && ubidi_isMirrored(bdp, c));
}

U_CFUNC UChar32
u_charMirror(UChar32 c) {
    UErrorCode errorCode=U_ZERO_ERROR;
    const UBiDiProps *bdp=ubidi_getSingleton(&errorCode);
    if(bdp!=NULL) {
        return ubidi_getMirror(bdp, c);
    } else {
        return c;
    }
}
