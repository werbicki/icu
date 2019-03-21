// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2000-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ushape.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2000jun29
*   created by: Markus W. Scherer
*
*   Contributions:
*   Arabic letter shaping implemented by Ayman Roshdy
*   UText enhancements by Paul Werbicki
*/

#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/ushape.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "putilimp.h"
#include "ustr_imp.h"
#include "ubidi_props.h"
#include "uassert.h"

/*
 * ### TODO in general for letter shaping:
 * - needs to handle the "Arabic Tail" that is used in some legacy codepages
 *   as a glyph fragment of wide-glyph letters
 *   + IBM Unicode conversion tables map it to U+200B (ZWSP)
 *   + IBM Egypt has proposed to encode the tail in Unicode among Arabic Presentation Forms
 *   + Unicode 3.2 added U+FE73 ARABIC TAIL FRAGMENT
 */

static int32_t
utext_replace32(UText *ut, int32_t start, int32_t *limit, UChar32 uchar, UBool forward, UErrorCode *pErrorCode) {
    UChar uchars[2] = { (UChar)uchar, 0 };
    int32_t length = (uchar == U_SENTINEL ? 0 : 1);
    int64_t nativeIndex = 0;
    int32_t nativeLength = 0;

    if (pErrorCode == NULL) {
        return 0;
    }

    if ((uchar != U_SENTINEL) &&
        ((!U16_IS_SINGLE(uchar)) || (U_IS_SUPPLEMENTARY(uchar)))
        )
    {
        uchars[0] = U16_LEAD(uchar);
        uchars[1] = U16_TRAIL(uchar);
        length = 2;
    };

    if ((!forward) && (!U_FAILURE(*pErrorCode))) {
        nativeIndex = UTEXT_GETNATIVEINDEX(ut);
    }
    if ((!U_FAILURE(*pErrorCode))) {
        nativeLength = utext_replace(ut, start, *limit, uchars, length, pErrorCode);
        if (uchar == U_SENTINEL)
            *limit = start;
        else
            *limit += nativeLength;
    }
    if ((!forward) && (!U_FAILURE(*pErrorCode))) {
        UTEXT_SETNATIVEINDEX(ut, nativeIndex);
    }

    if (*pErrorCode == U_BUFFER_OVERFLOW_ERROR) {
        nativeLength = length;
    }

    return nativeLength;
}

/* definitions for Arabic letter shaping ------------------------------------ */

#define IRRELEVANT 4
#define LAMTYPE    16
#define ALEFTYPE   32
#define LINKR      1
#define LINKL      2
#define APRESENT   8
#define SHADDA     64
#define CSHADDA    128
#define COMBINE    (SHADDA+CSHADDA)

#define HAMZAFE_CHAR       0xfe80
#define HAMZA06_CHAR       0x0621
#define YEH_HAMZA_CHAR     0x0626
#define YEH_HAMZAFE_CHAR   0xFE89
#define LAMALEF_SPACE_SUB  0xFFFF
#define TASHKEEL_SPACE_SUB 0xFFFE
#define NEW_TAIL_CHAR      0xFE73
#define OLD_TAIL_CHAR      0x200B
#define LAM_CHAR           0x0644
#define SPACE_CHAR         0x0020
#define SHADDA_CHAR        0xFE7C
#define TATWEEL_CHAR       0x0640
#define SHADDA_TATWEEL_CHAR  0xFE7D
#define SHADDA06_CHAR      0x0651

#define SHAPE_MODE   0
#define DESHAPE_MODE 1

struct uShapeVariables {
     UChar tailChar;
     uint32_t uShapeLamalefBegin;
     uint32_t uShapeLamalefEnd;
     uint32_t uShapeTashkeelBegin;
     uint32_t uShapeTashkeelEnd;
     int spacesRelativeToTextBeginEnd;
};

static const uint8_t tailFamilyIsolatedFinal[] = {
    /* FEB1 */ 1,
    /* FEB2 */ 1,
    /* FEB3 */ 0,
    /* FEB4 */ 0,
    /* FEB5 */ 1,
    /* FEB6 */ 1,
    /* FEB7 */ 0,
    /* FEB8 */ 0,
    /* FEB9 */ 1,
    /* FEBA */ 1,
    /* FEBB */ 0,
    /* FEBC */ 0,
    /* FEBD */ 1,
    /* FEBE */ 1
};

static const uint8_t tashkeelMedial[] = {
    /* FE70 */ 0,
    /* FE71 */ 1,
    /* FE72 */ 0,
    /* FE73 */ 0,
    /* FE74 */ 0,
    /* FE75 */ 0,
    /* FE76 */ 0,
    /* FE77 */ 1,
    /* FE78 */ 0,
    /* FE79 */ 1,
    /* FE7A */ 0,
    /* FE7B */ 1,
    /* FE7C */ 0,
    /* FE7D */ 1,
    /* FE7E */ 0,
    /* FE7F */ 1
};

static const UChar yehHamzaToYeh[] =
{
/* isolated*/ 0xFEEF,
/* final   */ 0xFEF0
};

static const uint8_t IrrelevantPos[] = {
    0x0, 0x2, 0x4, 0x6,
    0x8, 0xA, 0xC, 0xE
};


static const UChar convertLamAlef[] =
{
/*FEF5*/    0x0622,
/*FEF6*/    0x0622,
/*FEF7*/    0x0623,
/*FEF8*/    0x0623,
/*FEF9*/    0x0625,
/*FEFA*/    0x0625,
/*FEFB*/    0x0627,
/*FEFC*/    0x0627
};

static const UChar araLink[178]=
{
  1           + 32 + 256 * 0x11,/*0x0622*/
  1           + 32 + 256 * 0x13,/*0x0623*/
  1                + 256 * 0x15,/*0x0624*/
  1           + 32 + 256 * 0x17,/*0x0625*/
  1 + 2            + 256 * 0x19,/*0x0626*/
  1           + 32 + 256 * 0x1D,/*0x0627*/
  1 + 2            + 256 * 0x1F,/*0x0628*/
  1                + 256 * 0x23,/*0x0629*/
  1 + 2            + 256 * 0x25,/*0x062A*/
  1 + 2            + 256 * 0x29,/*0x062B*/
  1 + 2            + 256 * 0x2D,/*0x062C*/
  1 + 2            + 256 * 0x31,/*0x062D*/
  1 + 2            + 256 * 0x35,/*0x062E*/
  1                + 256 * 0x39,/*0x062F*/
  1                + 256 * 0x3B,/*0x0630*/
  1                + 256 * 0x3D,/*0x0631*/
  1                + 256 * 0x3F,/*0x0632*/
  1 + 2            + 256 * 0x41,/*0x0633*/
  1 + 2            + 256 * 0x45,/*0x0634*/
  1 + 2            + 256 * 0x49,/*0x0635*/
  1 + 2            + 256 * 0x4D,/*0x0636*/
  1 + 2            + 256 * 0x51,/*0x0637*/
  1 + 2            + 256 * 0x55,/*0x0638*/
  1 + 2            + 256 * 0x59,/*0x0639*/
  1 + 2            + 256 * 0x5D,/*0x063A*/
  0, 0, 0, 0, 0,                /*0x063B-0x063F*/
  1 + 2,                        /*0x0640*/
  1 + 2            + 256 * 0x61,/*0x0641*/
  1 + 2            + 256 * 0x65,/*0x0642*/
  1 + 2            + 256 * 0x69,/*0x0643*/
  1 + 2       + 16 + 256 * 0x6D,/*0x0644*/
  1 + 2            + 256 * 0x71,/*0x0645*/
  1 + 2            + 256 * 0x75,/*0x0646*/
  1 + 2            + 256 * 0x79,/*0x0647*/
  1                + 256 * 0x7D,/*0x0648*/
  1                + 256 * 0x7F,/*0x0649*/
  1 + 2            + 256 * 0x81,/*0x064A*/
         4         + 256 * 1,   /*0x064B*/
         4 + 128   + 256 * 1,   /*0x064C*/
         4 + 128   + 256 * 1,   /*0x064D*/
         4 + 128   + 256 * 1,   /*0x064E*/
         4 + 128   + 256 * 1,   /*0x064F*/
         4 + 128   + 256 * 1,   /*0x0650*/
         4 + 64    + 256 * 3,   /*0x0651*/
         4         + 256 * 1,   /*0x0652*/
         4         + 256 * 7,   /*0x0653*/
         4         + 256 * 8,   /*0x0654*/
         4         + 256 * 8,   /*0x0655*/
         4         + 256 * 1,   /*0x0656*/
  0, 0, 0, 0, 0,                /*0x0657-0x065B*/
  1                + 256 * 0x85,/*0x065C*/
  1                + 256 * 0x87,/*0x065D*/
  1                + 256 * 0x89,/*0x065E*/
  1                + 256 * 0x8B,/*0x065F*/
  0, 0, 0, 0, 0,                /*0x0660-0x0664*/
  0, 0, 0, 0, 0,                /*0x0665-0x0669*/
  0, 0, 0, 0, 0, 0,             /*0x066A-0x066F*/
         4         + 256 * 6,   /*0x0670*/
  1        + 8     + 256 * 0x00,/*0x0671*/
  1            + 32,            /*0x0672*/
  1            + 32,            /*0x0673*/
  0,                            /*0x0674*/
  1            + 32,            /*0x0675*/
  1, 1,                         /*0x0676-0x0677*/
  1 + 2,                        /*0x0678*/
  1 + 2 + 8        + 256 * 0x16,/*0x0679*/
  1 + 2 + 8        + 256 * 0x0E,/*0x067A*/
  1 + 2 + 8        + 256 * 0x02,/*0x067B*/
  1+2, 1+2,                     /*0x67C-0x067D*/
  1+2+8+256 * 0x06, 1+2, 1+2, 1+2, 1+2, 1+2, /*0x067E-0x0683*/
  1+2, 1+2, 1+2+8+256 * 0x2A, 1+2,           /*0x0684-0x0687*/
  1     + 8        + 256 * 0x38,/*0x0688*/
  1, 1, 1,                      /*0x0689-0x068B*/
  1     + 8        + 256 * 0x34,/*0x068C*/
  1     + 8        + 256 * 0x32,/*0x068D*/
  1     + 8        + 256 * 0x36,/*0x068E*/
  1, 1,                         /*0x068F-0x0690*/
  1     + 8        + 256 * 0x3C,/*0x0691*/
  1, 1, 1, 1, 1, 1, 1+8+256 * 0x3A, 1,       /*0x0692-0x0699*/
  1+2, 1+2, 1+2, 1+2, 1+2, 1+2, /*0x069A-0x06A3*/
  1+2, 1+2, 1+2, 1+2,           /*0x069A-0x06A3*/
  1+2, 1+2, 1+2, 1+2, 1+2, 1+2+8+256 * 0x3E, /*0x06A4-0x06AD*/
  1+2, 1+2, 1+2, 1+2,           /*0x06A4-0x06AD*/
  1+2, 1+2+8+256 * 0x42, 1+2, 1+2, 1+2, 1+2, /*0x06AE-0x06B7*/
  1+2, 1+2, 1+2, 1+2,           /*0x06AE-0x06B7*/
  1+2, 1+2,                     /*0x06B8-0x06B9*/
  1     + 8        + 256 * 0x4E,/*0x06BA*/
  1 + 2 + 8        + 256 * 0x50,/*0x06BB*/
  1+2, 1+2,                     /*0x06BC-0x06BD*/
  1 + 2 + 8        + 256 * 0x5A,/*0x06BE*/
  1+2,                          /*0x06BF*/
  1     + 8        + 256 * 0x54,/*0x06C0*/
  1 + 2 + 8        + 256 * 0x56,/*0x06C1*/
  1, 1, 1,                      /*0x06C2-0x06C4*/
  1     + 8        + 256 * 0x90,/*0x06C5*/
  1     + 8        + 256 * 0x89,/*0x06C6*/
  1     + 8        + 256 * 0x87,/*0x06C7*/
  1     + 8        + 256 * 0x8B,/*0x06C8*/
  1     + 8        + 256 * 0x92,/*0x06C9*/
  1,                            /*0x06CA*/
  1     + 8        + 256 * 0x8E,/*0x06CB*/
  1 + 2 + 8        + 256 * 0xAC,/*0x06CC*/
  1,                            /*0x06CD*/
  1+2, 1+2,                     /*0x06CE-0x06CF*/
  1 + 2 + 8        + 256 * 0x94,/*0x06D0*/
  1+2,                          /*0x06D1*/
  1     + 8        + 256 * 0x5E,/*0x06D2*/
  1     + 8        + 256 * 0x60 /*0x06D3*/
};

static const uint8_t presALink[] = {
/***********0*****1*****2*****3*****4*****5*****6*****7*****8*****9*****A*****B*****C*****D*****E*****F*/
/*FB5*/    0,    1,    0,    0,    0,    0,    0,    1,    2,1 + 2,    0,    0,    0,    0,    0,    0,
/*FB6*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FB7*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    2,1 + 2,    0,    0,
/*FB8*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    0,    0,    0,    1,
/*FB9*/    2,1 + 2,    0,    1,    2,1 + 2,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FBA*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FBB*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FBC*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FBD*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FBE*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FBF*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    1,    2,1 + 2,
/*FC0*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FC1*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FC2*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FC3*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FC4*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
/*FC5*/    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    4,    4,
/*FC6*/    4,    4,    4
};

static const uint8_t presBLink[]=
{
/***********0*****1*****2*****3*****4*****5*****6*****7*****8*****9*****A*****B*****C*****D*****E*****F*/
/*FE7*/1 + 2,1 + 2,1 + 2,    0,1 + 2,    0,1 + 2,1 + 2,1 + 2,1 + 2,1 + 2,1 + 2,1 + 2,1 + 2,1 + 2,1 + 2,
/*FE8*/    0,    0,    1,    0,    1,    0,    1,    0,    1,    0,    1,    2,1 + 2,    0,    1,    0,
/*FE9*/    1,    2,1 + 2,    0,    1,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,
/*FEA*/1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    0,    1,    0,    1,    0,
/*FEB*/    1,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,
/*FEC*/1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,
/*FED*/1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,
/*FEE*/1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    2,1 + 2,    0,    1,    0,
/*FEF*/    1,    0,    1,    2,1 + 2,    0,    1,    0,    1,    0,    1,    0,    1,    0,    0,    0
};

static const UChar convertFBto06[] =
{
/***********0******1******2******3******4******5******6******7******8******9******A******B******C******D******E******F***/
/*FB5*/   0x671, 0x671, 0x67B, 0x67B, 0x67B, 0x67B, 0x67E, 0x67E, 0x67E, 0x67E,     0,     0,     0,     0, 0x67A, 0x67A,
/*FB6*/   0x67A, 0x67A,     0,     0,     0,     0, 0x679, 0x679, 0x679, 0x679,     0,     0,     0,     0,     0,     0,
/*FB7*/       0,     0,     0,     0,     0,     0,     0,     0,     0,     0, 0x686, 0x686, 0x686, 0x686,     0,     0,
/*FB8*/       0,     0, 0x68D, 0x68D, 0x68C, 0x68C, 0x68E, 0x68E, 0x688, 0x688, 0x698, 0x698, 0x691, 0x691, 0x6A9, 0x6A9,
/*FB9*/   0x6A9, 0x6A9, 0x6AF, 0x6AF, 0x6AF, 0x6AF,     0,     0,     0,     0,     0,     0,     0,     0, 0x6BA, 0x6BA,
/*FBA*/   0x6BB, 0x6BB, 0x6BB, 0x6BB, 0x6C0, 0x6C0, 0x6C1, 0x6C1, 0x6C1, 0x6C1, 0x6BE, 0x6BE, 0x6BE, 0x6BE, 0x6d2, 0x6D2,
/*FBB*/   0x6D3, 0x6D3,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
/*FBC*/       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
/*FBD*/       0,     0,     0,     0,     0,     0,     0, 0x6C7, 0x6C7, 0x6C6, 0x6C6, 0x6C8, 0x6C8,     0, 0x6CB, 0x6CB,
/*FBE*/   0x6C5, 0x6C5, 0x6C9, 0x6C9, 0x6D0, 0x6D0, 0x6D0, 0x6D0,     0,     0,     0,     0,     0,     0,     0,     0,
/*FBF*/       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0, 0x6CC, 0x6CC, 0x6CC, 0x6CC
};

static const UChar convertFEto06[] =
{
/***********0******1******2******3******4******5******6******7******8******9******A******B******C******D******E******F***/
/*FE7*/   0x64B, 0x64B, 0x64C, 0x64C, 0x64D, 0x64D, 0x64E, 0x64E, 0x64F, 0x64F, 0x650, 0x650, 0x651, 0x651, 0x652, 0x652,
/*FE8*/   0x621, 0x622, 0x622, 0x623, 0x623, 0x624, 0x624, 0x625, 0x625, 0x626, 0x626, 0x626, 0x626, 0x627, 0x627, 0x628,
/*FE9*/   0x628, 0x628, 0x628, 0x629, 0x629, 0x62A, 0x62A, 0x62A, 0x62A, 0x62B, 0x62B, 0x62B, 0x62B, 0x62C, 0x62C, 0x62C,
/*FEA*/   0x62C, 0x62D, 0x62D, 0x62D, 0x62D, 0x62E, 0x62E, 0x62E, 0x62E, 0x62F, 0x62F, 0x630, 0x630, 0x631, 0x631, 0x632,
/*FEB*/   0x632, 0x633, 0x633, 0x633, 0x633, 0x634, 0x634, 0x634, 0x634, 0x635, 0x635, 0x635, 0x635, 0x636, 0x636, 0x636,
/*FEC*/   0x636, 0x637, 0x637, 0x637, 0x637, 0x638, 0x638, 0x638, 0x638, 0x639, 0x639, 0x639, 0x639, 0x63A, 0x63A, 0x63A,
/*FED*/   0x63A, 0x641, 0x641, 0x641, 0x641, 0x642, 0x642, 0x642, 0x642, 0x643, 0x643, 0x643, 0x643, 0x644, 0x644, 0x644,
/*FEE*/   0x644, 0x645, 0x645, 0x645, 0x645, 0x646, 0x646, 0x646, 0x646, 0x647, 0x647, 0x647, 0x647, 0x648, 0x648, 0x649,
/*FEF*/   0x649, 0x64A, 0x64A, 0x64A, 0x64A, 0x65C, 0x65C, 0x65D, 0x65D, 0x65E, 0x65E, 0x65F, 0x65F
};

static const uint8_t shapeTable[4][4][4]=
{
  { {0,0,0,0}, {0,0,0,0}, {0,1,0,3}, {0,1,0,1} },
  { {0,0,2,2}, {0,0,1,2}, {0,1,1,2}, {0,1,1,3} },
  { {0,0,0,0}, {0,0,0,0}, {0,1,0,3}, {0,1,0,3} },
  { {0,0,1,2}, {0,0,1,2}, {0,1,1,2}, {0,1,1,3} }
};

/*
 * Converts the Alef characters into an equivalent LamAlef location in
 * the 0x06xx Range, this is an intermediate stage in the operation of
 * the program later it'll be converted into the 0xFExx LamAlefs in the
 * shaping function.
 */
static inline UChar32
changeLamAlef(UChar32 ch)
{
    switch (ch) {
    case 0x0622:
        return 0x065C;
    case 0x0623:
        return 0x065D;
    case 0x0625:
        return 0x065E;
    case 0x0627:
        return 0x065F;
    }
    return 0;
}

/*
 * Resolves the link between the characters as Arabic characters have four
 * forms: Isolated, Initial, Middle and Final Form
 */
static UChar32
getLink(UChar32 ch)
{
    if (ch >= 0x0622 && ch <= 0x06D3) {
        return(araLink[ch - 0x0622]);
    }
    else if (ch == 0x200D) {
        return(3);
    }
    else if (ch >= 0x206D && ch <= 0x206F) {
        return(4);
    }
    else if (ch >= 0xFB50 && ch <= 0xFC62) {
        return(presALink[ch - 0xFB50]);
    }
    else if (ch >= 0xFE70 && ch <= 0xFEFC) {
        return(presBLink[ch - 0xFE70]);
    }
    else {
        return(0);
    }
}

/*
 * Returns TRUE for Tashkeel characters in 06 range
 * else return FALSE.
 */
static inline UBool
isTashkeelChar(UChar32 ch)
{
    return (int32_t)(ch >= 0x064B && ch <= 0x0652);
}

/*
 * Returns TRUE for Tashkeel characters in FE range
 * else return FALSE.
 */
static inline UBool
isTashkeelCharFE(UChar32 ch)
{
    return (int32_t)(ch >= 0xFE70 && ch <= 0xFE7F);
}

/*
 * Returns TRUE for Alef characters else return FALSE.
 */
static inline UBool
isAlefChar(UChar32 ch)
{
    return (int32_t)((ch == 0x0622) || (ch == 0x0623) || (ch == 0x0625) || (ch == 0x0627));
}

/*
 * Returns TRUE for LamAlef characters else return FALSE.
 */
static inline UBool
isLamAlefChar(UChar32 ch)
{
    return (int32_t)((ch >= 0xFEF5) && (ch <= 0xFEFC));
}

/*
 * Returns TRUE if the character matches one of the tail
 * characters (0xfe73 or 0x200b) otherwise returns FALSE
 */
static inline UBool
isTailChar(UChar32 ch)
{
    if (ch == OLD_TAIL_CHAR || ch == NEW_TAIL_CHAR) {
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * Returns TRUE if the character is a seen family isolated
 * character in the FE range otherwise returns FALSE.
 */
static inline UBool
isSeenTailFamilyChar(UChar32 ch)
{
    if (ch >= 0xfeb1 && ch < 0xfebf) {
        return tailFamilyIsolatedFinal[ch - 0xFEB1];
    }
    else {
        return 0;
    }
}

/* 
 * Returns TRUE if the character is a seen family character in the
 * Unicode 06 range otherwise returns FALSE
 */
static inline UBool
isSeenFamilyChar(UChar32 ch)
{
    if (ch >= 0x633 && ch <= 0x636) {
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * Returns TRUE if the character is a Alef Maksoura Final or isolated
 * else returns FALSE.
 */
static inline UBool
isAlefMaksouraChar(UChar32 ch)
{
    return (int32_t)((ch == 0xFEEF) || (ch == 0xFEF0) || (ch == 0x0649));
}

/*
 * Returns TRUE if the character is a yehHamza isolated or yehhamza
 * final is found otherwise returns FALSE.
 */
static inline UBool
isYehHamzaChar(UChar32 ch)
{
    if ((ch == 0xFE89) || (ch == 0xFE8A)) {
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * Checks if the Tashkeel Character is on Tatweel or not,if the
 * Tashkeel on tatweel (FE range), it returns 1 else if the
 * Tashkeel with shadda on tatweel (FC range) return 2 otherwise
 * returns 0.
 */
static inline int32_t
isTashkeelOnTatweelChar(UChar32 ch)
{
    if (ch >= 0xfe70 && ch <= 0xfe7f && ch != NEW_TAIL_CHAR && ch != 0xFE75 && ch != SHADDA_TATWEEL_CHAR)
    {
        return tashkeelMedial[ch - 0xFE70];
    }
    else if ((ch >= 0xfcf2 && ch <= 0xfcf4) || (ch == SHADDA_TATWEEL_CHAR)) {
        return 2;
    }
    else {
        return 0;
    }
}

/*
 * Checks if the Tashkeel Character is in the isolated form
 * (i.e. Unicode FE range) returns 1 else if the Tashkeel
 * with shadda is in the isolated form (i.e. Unicode FC range)
 * returns 2 otherwise returns 0
 */
static inline int32_t
isIsolatedTashkeelChar(UChar32 ch)
{
    if (ch >= 0xfe70 && ch <= 0xfe7f && ch != NEW_TAIL_CHAR && ch != 0xFE75) {
        return (1 - tashkeelMedial[ch - 0xFE70]);
    }
    else if (ch >= 0xfc5e && ch <= 0xfc63) {
        return 1;
    }
    else {
        return 0;
    }
}

/*
 * This function counts the number of spaces at each end of
 * the logical buffer. The count is in the native index of
 * the UText.
 */
static void
countSpaces(UText *ut,
    uint32_t /*options*/,
    int32_t *nativeSpacesLeft,
    int32_t *nativeSpacesRight)
{
    int32_t spacesLeft = 0;
    int32_t spacesRight = 0;

    utext_setNativeIndex(ut, 0);
    int32_t nativeStart = 0;
    UChar32 uchar = UTEXT_NEXT32(ut);
    int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    for (; (uchar != U_SENTINEL) && (uchar == SPACE_CHAR);
        nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
    {
        spacesLeft += nativeLimit - nativeStart;
    }

    if (uchar != U_SENTINEL)
    {
        utext_setNativeIndex(ut, utext_nativeLength(ut));
        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        uchar = UTEXT_PREVIOUS32(ut);
        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        for (; (uchar != U_SENTINEL) && (uchar == SPACE_CHAR);
            nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(ut), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            spacesRight += nativeLimit - nativeStart;
        }
    }

    if (nativeSpacesLeft)
        *nativeSpacesLeft = spacesLeft;
    if (nativeSpacesRight)
        *nativeSpacesRight = spacesRight;
}

/*
 * This function inverts the buffer, it's used in case the user
 * specifies the buffer to be U_SHAPE_TEXT_DIRECTION_LOGICAL. The
 * inversion is done in-place using codepoints.
 */
static void
invertBuffer(UText *ut,
    uint32_t /*options*/,
    int32_t nativeStart,
    int32_t nativeLimit,
    UErrorCode *pErrorCode)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return;
    }

    utext_setNativeIndex(ut, nativeStart);
    UChar32 uchar = UTEXT_NEXT32(ut);
    for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeLimit > nativeStart);
        uchar = UTEXT_NEXT32(ut))
    {
        int32_t length = (int32_t)UTEXT_GETNATIVEINDEX(ut) - nativeStart;

        utext_copy(ut, nativeStart, nativeStart + length, nativeLimit, TRUE, pErrorCode);
        UTEXT_SETNATIVEINDEX(ut, nativeStart);

        nativeLimit -= length;
    }
}

static int32_t
handleAggregateTashkeel(UText *ut, 
    uint32_t options, 
    UErrorCode *pErrorCode)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    UBool isLogical = (options & U_SHAPE_TEXT_DIRECTION_MASK) == U_SHAPE_TEXT_DIRECTION_LOGICAL;
    UBool isAggregateTashkeel =
        (options & (U_SHAPE_AGGREGATE_TASHKEEL_MASK + U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED)) ==
        (U_SHAPE_AGGREGATE_TASHKEEL + U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED);
    UBool isAggregationPossible = TRUE;
    UChar32 currLink = 0;
    UChar32 prevLink = 0;
    UChar32 prev = 0;

    if (isLogical)
    {
        utext_setNativeIndex(ut, 0);
        UTEXT_NEXT32(ut);

        int32_t nativeStart = 0;
        int32_t lastNativeStart = nativeStart;
        UChar32 uchar = UTEXT_NEXT32(ut);
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        int32_t lastNativeLimit = nativeLimit;
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            prevLink = currLink;
            currLink = getLink(uchar);

            if (isAggregateTashkeel && ((prevLink | currLink) & COMBINE) == COMBINE && isAggregationPossible) {
                isAggregationPossible = FALSE;

                uchar = (prev < uchar ? prev : uchar) - 0x064C + 0xFC5E;
                currLink = getLink(uchar);

                int32_t prevLastNativeLimit = lastNativeLimit;
                utext_replace32(ut, lastNativeStart, &lastNativeLimit, uchar, FALSE, pErrorCode);
                nativeStart += lastNativeLimit - prevLastNativeLimit;
                nativeLimit += lastNativeLimit - prevLastNativeLimit;

                utext_replace32(ut, nativeStart, &nativeLimit, U_SENTINEL, FALSE, pErrorCode);

                nativeStart = lastNativeStart;
                nativeLimit = lastNativeLimit;
            }
            else {
                isAggregationPossible = TRUE;

                prev = uchar;
            }

            lastNativeStart = nativeStart;
            lastNativeLimit = nativeLimit;
        }

        return nativeLimit;
    }
    else
    {
        utext_setNativeIndex(ut, utext_nativeLength(ut));
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        int32_t lastNativeLimit = nativeLimit;
        UChar32 uchar = UTEXT_PREVIOUS32(ut);
        int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        int32_t lastNativeStart = nativeStart;
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
            nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(ut), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            prevLink = currLink;
            currLink = getLink(uchar);

            if (isAggregateTashkeel && ((prevLink | currLink) & COMBINE) == COMBINE && isAggregationPossible) {
                isAggregationPossible = FALSE;

                uchar = (prev < uchar ? prev : uchar) - 0x064C + 0xFC5E;
                currLink = getLink(uchar);

                utext_replace32(ut, lastNativeStart, &lastNativeLimit, uchar, FALSE, pErrorCode);

                int32_t prevNativeLimit = nativeLimit;
                utext_replace32(ut, nativeStart, &nativeLimit, U_SENTINEL, FALSE, pErrorCode);
                lastNativeStart += nativeLimit - prevNativeLimit;
                lastNativeLimit += nativeLimit - prevNativeLimit;

                nativeStart = lastNativeStart;
                nativeLimit = lastNativeLimit;
            }
            else {
                isAggregationPossible = TRUE;

                prev = uchar;
            }

            lastNativeStart = nativeStart;
            lastNativeLimit = nativeLimit;
        }

        return (int32_t)utext_nativeLength(ut);
    }
}

/*
 * Replaces Tashkeel as following:
 * Case 1: if the Tashkeel on tatweel, replace it with Tatweel.
 * Case 2: if the Tashkeel aggregated with Shadda on Tatweel, replace
 *         it with Shadda on Tatweel.
 * Case 3: if the Tashkeel is isolated replace it with Space.
 */
static int32_t
handleTashkeelWithTatweel(UText *ut, 
    uint32_t /*options*/, 
    UErrorCode *pErrorCode)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    utext_setNativeIndex(ut, 0);
    int32_t nativeStart = 0;
    UChar32 uchar = UTEXT_NEXT32(ut);
    int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
        nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
    {
        if ((isTashkeelOnTatweelChar(uchar) == 1)) {
            utext_replace32(ut, nativeStart, &nativeLimit, TATWEEL_CHAR, TRUE, pErrorCode);
        }
        else if ((isTashkeelOnTatweelChar(uchar) == 2)) {
            utext_replace32(ut, nativeStart, &nativeLimit, SHADDA_TATWEEL_CHAR, TRUE, pErrorCode);
        }
        else if (isIsolatedTashkeelChar(uchar) && uchar != SHADDA_CHAR) {
            utext_replace32(ut, nativeStart, &nativeLimit, SPACE_CHAR, TRUE, pErrorCode);
        }
    }

    return nativeLimit;
}

/*
 * The shapeUnicode function converts Lam + Alef into LamAlef + space,
 * and Tashkeel to space.
 *
 * handleGeneratedSpaces function puts these generated spaces
 * according to the options the user specifies. LamAlef and Tashkeel
 * spaces can be replaced at begin, at end, at near or decrease the
 * buffer size.
 *
 * There is also Auto option for LamAlef and tashkeel, which will put
 * the spaces at end of the buffer (or end of text if the user used
 * the option U_SHAPE_SPACES_RELATIVE_TO_TEXT_BEGIN_END).
 *
 * If the text type was visual_LTR and the option
 * U_SHAPE_SPACES_RELATIVE_TO_TEXT_BEGIN_END was selected the END
 * option will place the space at the beginning of the buffer and
 * BEGIN will place the space at the end of the buffer.
 */
static int32_t
handleGeneratedSpaces(UText *ut,
    uint32_t options,
    UErrorCode *pErrorCode,
    struct uShapeVariables shapeVars)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    int lamAlefOption = 0;
    int tashkeelOption = 0;
    int shapingMode = SHAPE_MODE;

    if (shapingMode == 0) {
        if ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_RESIZE) {
            lamAlefOption = 1;
        }
        if ((options & U_SHAPE_TASHKEEL_MASK) == U_SHAPE_TASHKEEL_RESIZE) {
            tashkeelOption = 1;
        }
    }

    if (lamAlefOption || tashkeelOption)
    {
        utext_setNativeIndex(ut, 0);
        int32_t nativeStart = 0;
        UChar32 uchar = UTEXT_NEXT32(ut);
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL); 
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            if ((lamAlefOption && uchar == LAMALEF_SPACE_SUB) || (tashkeelOption && uchar == TASHKEEL_SPACE_SUB)) {
                utext_replace32(ut, nativeStart, &nativeLimit, U_SENTINEL, TRUE, pErrorCode);
            }
        }
    }

    lamAlefOption = 0;

    if (shapingMode == 0) {
        if ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_NEAR) {
            lamAlefOption = 1;
        }
    }

    if (lamAlefOption)
    {
        // Lam+Alef is already shaped into LamAlef + FFFF
        utext_setNativeIndex(ut, 0);
        int32_t nativeStart = 0;
        UChar32 uchar = UTEXT_NEXT32(ut);
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            if (lamAlefOption && uchar == LAMALEF_SPACE_SUB) {
                utext_replace32(ut, nativeStart, &nativeLimit, SPACE_CHAR, TRUE, pErrorCode);
            }
        }
    }

    lamAlefOption = 0;
    tashkeelOption = 0;

    if (shapingMode == 0) {
        if (((options & U_SHAPE_LAMALEF_MASK) == shapeVars.uShapeLamalefBegin) ||
            (((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_AUTO)
                && (shapeVars.spacesRelativeToTextBeginEnd == 1))) {
            lamAlefOption = 1;
        }
        if ((options & U_SHAPE_TASHKEEL_MASK) == shapeVars.uShapeTashkeelBegin) {
            tashkeelOption = 1;
        }
    }

    if (lamAlefOption || tashkeelOption)
    {
        utext_setNativeIndex(ut, utext_nativeLength(ut));
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        UChar32 uchar = UTEXT_PREVIOUS32(ut);
        int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
            nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(ut), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            if ((lamAlefOption && uchar == LAMALEF_SPACE_SUB) ||
                (tashkeelOption && uchar == TASHKEEL_SPACE_SUB)) {
                utext_replace32(ut, nativeStart, &nativeLimit, U_SENTINEL, TRUE, pErrorCode);
                int32_t nativeInsert = 0;
                utext_replace32(ut, 0, &nativeInsert, SPACE_CHAR, TRUE, pErrorCode);
                UTEXT_SETNATIVEINDEX(ut, nativeStart);
            }
        }
    }

    lamAlefOption = 0;
    tashkeelOption = 0;

    if (shapingMode == 0) {
        if (((options & U_SHAPE_LAMALEF_MASK) == shapeVars.uShapeLamalefEnd) ||
            (((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_AUTO)
                && (shapeVars.spacesRelativeToTextBeginEnd == 0))) {
            lamAlefOption = 1;
        }
        if ((options & U_SHAPE_TASHKEEL_MASK) == shapeVars.uShapeTashkeelEnd) {
            tashkeelOption = 1;
        }
    }

    if (lamAlefOption || tashkeelOption)
    {
        utext_setNativeIndex(ut, 0);
        int32_t nativeLength = (int32_t)utext_nativeLength(ut);
        int32_t nativeStart = 0;
        UChar32 uchar = UTEXT_NEXT32(ut);
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            if ((lamAlefOption && uchar == LAMALEF_SPACE_SUB) ||
                (tashkeelOption && uchar == TASHKEEL_SPACE_SUB)) {
                nativeLength -= (nativeLimit - nativeStart);
                utext_replace32(ut, nativeStart, &nativeLimit, U_SENTINEL, TRUE, pErrorCode);
                utext_replace32(ut, nativeLength, &nativeLength, SPACE_CHAR, TRUE, pErrorCode);
                UTEXT_SETNATIVEINDEX(ut, nativeStart);
            }
        }
    }

    return (int32_t)utext_nativeLength(ut);
}

/*
 * Expands the LamAlef character to Lam and Alef consuming the required
 * space from beginning of the buffer. If the text type was visual_LTR
 * and the option U_SHAPE_SPACES_RELATIVE_TO_TEXT_BEGIN_END was selected
 * the spaces will be located at end of buffer.
 * If there are no spaces to expand the LamAlef, an error
 * will be set to U_NO_SPACE_AVAILABLE as defined in utypes.h
 */
static int32_t
expandCompositCharAtBegin(UText *ut,
    UErrorCode *pErrorCode)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    int32_t nativeSpacesLeft = 0;
    int32_t nativeSpacesRight = 0;

    countSpaces(ut, 0, &nativeSpacesLeft, &nativeSpacesRight);

    int32_t startNativeStart = 0;
    utext_setNativeIndex(ut, startNativeStart);
    UTEXT_NEXT32(ut);
    int32_t startNativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);

    utext_setNativeIndex(ut, utext_nativeLength(ut));
    int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    UChar32 uchar = UTEXT_PREVIOUS32(ut);
    int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
        nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(ut), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut))
    {
        if (nativeSpacesLeft > 0 && isLamAlefChar(uchar)) {
            utext_copy(ut, startNativeStart, startNativeLimit, nativeLimit, TRUE, pErrorCode);
            nativeStart -= startNativeLimit - startNativeStart;
            nativeLimit -= startNativeLimit - startNativeStart;

            utext_replace32(ut, nativeStart, &nativeLimit, convertLamAlef[uchar - 0xFEF5], TRUE, pErrorCode);
            int32_t nativeLimit2 = nativeLimit + (startNativeLimit - startNativeStart);
            utext_replace32(ut, nativeLimit, &nativeLimit2, LAM_CHAR, FALSE, pErrorCode);
            nativeLimit = nativeLimit2;

            utext_setNativeIndex(ut, startNativeStart);
            UTEXT_NEXT32(ut);
            startNativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);

            UTEXT_SETNATIVEINDEX(ut, nativeStart);

            nativeSpacesLeft--;
        }
        else {
            if (nativeSpacesLeft == 0 && isLamAlefChar(uchar)) {
                *pErrorCode = U_NO_SPACE_AVAILABLE;
            }
        }
    }

    return (int32_t)utext_nativeLength(ut);
}

/*
 * Expands the LamAlef character to Lam and Alef consuming the
 * required space from end of the buffer. If the text type was
 * Visual LTR and the option U_SHAPE_SPACES_RELATIVE_TO_TEXT_BEGIN_END
 * was used, the spaces will be consumed from begin of buffer. If
 * there are no spaces to expand the LamAlef, an error
 * will be set to U_NO_SPACE_AVAILABLE as defined in utypes.h
 */
static int32_t
expandCompositCharAtEnd(UText *ut,
    UErrorCode *pErrorCode)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    int32_t nativeSpacesLeft = 0;
    int32_t nativeSpacesRight = 0;

    countSpaces(ut, 0, &nativeSpacesLeft, &nativeSpacesRight);

    int32_t endNativeLimit = (int32_t)utext_nativeLength(ut);
    utext_setNativeIndex(ut, endNativeLimit);
    UTEXT_PREVIOUS32(ut);
    int32_t endNativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);

    utext_setNativeIndex(ut, endNativeLimit - nativeSpacesRight);
    int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    UChar32 uchar = UTEXT_PREVIOUS32(ut);
    int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
        nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(ut), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut))
    {
        if (nativeSpacesRight > 0 && isLamAlefChar(uchar)) {
            utext_copy(ut, endNativeStart, endNativeLimit, nativeLimit, TRUE, pErrorCode);
            utext_replace32(ut, nativeStart, &nativeLimit, convertLamAlef[uchar - 0xFEF5], TRUE, pErrorCode);
            int32_t nativeLimit2 = nativeLimit + (endNativeLimit - endNativeStart);
            utext_replace32(ut, nativeLimit, &nativeLimit2, LAM_CHAR, FALSE, pErrorCode);
            nativeLimit = nativeLimit2;

            utext_setNativeIndex(ut, endNativeLimit);
            UTEXT_PREVIOUS32(ut);
            endNativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);

            UTEXT_SETNATIVEINDEX(ut, nativeStart);

            nativeSpacesRight--;
        }
        else {
            if ((nativeSpacesRight == 0) && isLamAlefChar(uchar)) {
                *pErrorCode = U_NO_SPACE_AVAILABLE;
            }
        }
    }

    return (int32_t)utext_nativeLength(ut);
}

/*
 * Expands the LamAlef character into Lam + Alef, YehHamza character
 * into Yeh + Hamza, SeenFamily character into SeenFamily character
 * + Tail, while consuming the space next to the character.
 * If there are no spaces next to the character, an error
 * will be set to U_NO_SPACE_AVAILABLE as defined in utypes.h
 */
static int32_t
expandCompositCharAtNear(UText *ut,
    UErrorCode *pErrorCode,
    int yehHamzaOption,
    int seenTailOption,
    int lamAlefOption,
    struct uShapeVariables shapeVars)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    utext_setNativeIndex(ut, 0);
    int32_t nativeStart = 0;
    int32_t lastNativeStart = (-1);
    UChar32 uchar = UTEXT_NEXT32(ut);
    UChar32 uchar1 = U_SENTINEL;
    int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    int32_t lastNativeLimit = nativeLimit;
    for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
        nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
    {
        UChar32 uchar2 = UTEXT_NEXT32(ut);
        if (uchar2 != U_SENTINEL)
            UTEXT_PREVIOUS32(ut);

        if (seenTailOption && isSeenTailFamilyChar(uchar)) {
            if (uchar1 == SPACE_CHAR) {
                utext_replace32(ut, lastNativeStart, &lastNativeLimit, shapeVars.tailChar, TRUE, pErrorCode);
                UTEXT_SETNATIVEINDEX(ut, nativeStart);
            }
            else {
                *pErrorCode = U_NO_SPACE_AVAILABLE;
            }
        }
        else if (yehHamzaOption && (isYehHamzaChar(uchar))) {
            if (uchar1 == SPACE_CHAR) {
                UChar32 yehhamzaChar = uchar;

                int32_t prevLastNativeLimit = lastNativeLimit;
                utext_replace32(ut, lastNativeStart, &lastNativeLimit, HAMZAFE_CHAR, TRUE, pErrorCode);
                nativeStart += lastNativeLimit - prevLastNativeLimit;
                nativeLimit += lastNativeLimit - prevLastNativeLimit;

                utext_replace32(ut, nativeStart, &nativeLimit, yehHamzaToYeh[yehhamzaChar - YEH_HAMZAFE_CHAR], TRUE, pErrorCode);

                UTEXT_SETNATIVEINDEX(ut, nativeStart);
            }
            else {
                *pErrorCode = U_NO_SPACE_AVAILABLE;
            }
        }
        else if (lamAlefOption && isLamAlefChar(uchar2)) {
            if (uchar == SPACE_CHAR) {
                UChar32 lamalefChar = uchar2;
                utext_replace32(ut, nativeStart, &nativeLimit, convertLamAlef[lamalefChar - 0xFEF5], TRUE, pErrorCode);

                nativeStart = nativeLimit;
                uchar = UTEXT_NEXT32(ut);
                nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                utext_replace32(ut, nativeStart, &nativeLimit, LAM_CHAR, TRUE, pErrorCode);
            }
            else {
                *pErrorCode = U_NO_SPACE_AVAILABLE;
            }
        }

        lastNativeStart = nativeStart;
        lastNativeLimit = nativeLimit;
        uchar1 = uchar;
    }

    return nativeLimit;
}

/*
 * LamAlef, need special handling, since it expands from one
 * character into two characters while shaping or deshaping.
 * In order to expand it, near or far spaces according to the
 * options user specifies. Also buffer size can be increased.
 *
 * For SeenFamily characters and YehHamza only the near option is
 * supported, while for LamAlef we can take spaces from begin, end,
 * near or even increase the buffer size.
 * There is also the Auto option for LamAlef only, which will first
 * search for a space at end, begin then near, respectively.
 * If there are no spaces to expand these characters, an error will be set to
 * U_NO_SPACE_AVAILABLE as defined in utypes.h
 */
static int32_t
expandCompositChar(UText *ut,
    uint32_t options,
    UErrorCode *pErrorCode,
    int shapingMode,
    struct uShapeVariables shapeVars)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    int32_t nativeLength = (int32_t)utext_nativeLength(ut);
    int yehHamzaOption = 0;
    int seenTailOption = 0;
    int lamAlefOption = 0;

    if (shapingMode == 1) {
        if ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_AUTO) {

            if (shapeVars.spacesRelativeToTextBeginEnd == 0) {
                nativeLength = expandCompositCharAtEnd(ut, pErrorCode);

                if (*pErrorCode == U_NO_SPACE_AVAILABLE) {
                    *pErrorCode = U_ZERO_ERROR;
                    nativeLength = expandCompositCharAtBegin(ut, pErrorCode);
                }
            }
            else {
                nativeLength = expandCompositCharAtBegin(ut, pErrorCode);

                if (*pErrorCode == U_NO_SPACE_AVAILABLE) {
                    *pErrorCode = U_ZERO_ERROR;
                    nativeLength = expandCompositCharAtEnd(ut, pErrorCode);
                }
            }

            if (*pErrorCode == U_NO_SPACE_AVAILABLE) {
                *pErrorCode = U_ZERO_ERROR;
                nativeLength = expandCompositCharAtNear(ut, pErrorCode, yehHamzaOption, seenTailOption, 1, shapeVars);
            }
        }
    }

    if (shapingMode == 1) {
        if ((options & U_SHAPE_LAMALEF_MASK) == shapeVars.uShapeLamalefEnd) {
            nativeLength = expandCompositCharAtEnd(ut, pErrorCode);
        }
    }

    if (shapingMode == 1) {
        if ((options & U_SHAPE_LAMALEF_MASK) == shapeVars.uShapeLamalefBegin) {
            nativeLength = expandCompositCharAtBegin(ut, pErrorCode);
        }
    }

    if (shapingMode == 0) {
        if ((options & U_SHAPE_YEHHAMZA_MASK) == U_SHAPE_YEHHAMZA_TWOCELL_NEAR) {
            yehHamzaOption = 1;
        }
        if ((options & U_SHAPE_SEEN_MASK) == U_SHAPE_SEEN_TWOCELL_NEAR) {
            seenTailOption = 1;
        }
    }
    if (shapingMode == 1) {
        if ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_NEAR) {
            lamAlefOption = 1;
        }
    }

    if (yehHamzaOption || seenTailOption || lamAlefOption) {
        nativeLength = expandCompositCharAtNear(ut, pErrorCode, yehHamzaOption, seenTailOption, lamAlefOption, shapeVars);
    }

    if (shapingMode == 1) {
        if ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_RESIZE) {
            utext_setNativeIndex(ut, 0);
            int32_t nativeStart = 0;
            UChar32 uchar = UTEXT_NEXT32(ut);
            int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
            for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
                nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
            {
                if (isLamAlefChar(uchar))
                {
                    utext_replace32(ut, nativeStart, &nativeLimit, convertLamAlef[uchar - 0xFEF5], TRUE, pErrorCode);
                    utext_replace32(ut, nativeLimit, &nativeLimit, LAM_CHAR, TRUE, pErrorCode);

                    nativeStart = nativeLimit;
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                }
            }

            nativeLength = nativeLimit;
        }
    }

    return nativeLength;
}

static int32_t
shapeUnicode(UText *ut,
    uint32_t options,
    UErrorCode *pErrorCode,
    int tashkeelFlag,
    struct uShapeVariables shapeVars)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    /*
     * Converts the input buffer from FExx Range into 06xx Range
     * to make sure that all characters are in the 06xx range
     * even the lamalef is converted to the special region in
     * the 06xx range
     */
    if ((options & U_SHAPE_PRESERVE_PRESENTATION_MASK) == U_SHAPE_PRESERVE_PRESENTATION_NOOP) {
        utext_setNativeIndex(ut, 0);
        int32_t nativeStart = 0;
        UChar32 uchar = UTEXT_NEXT32(ut);
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        for (; (uchar != U_SENTINEL);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            if ((uchar >= 0xFB50) && (uchar <= 0xFBFF)) {
                UChar32 c = convertFBto06[(uchar - 0xFB50)];
                if (c != 0)
                    utext_replace32(ut, nativeStart, &nativeLimit, c, TRUE, pErrorCode);
            }
            else if ((uchar >= 0xFE70) && (uchar <= 0xFEFC)) {
                utext_replace32(ut, nativeStart, &nativeLimit, convertFEto06[(uchar - 0xFE70)], TRUE, pErrorCode);
            }
        }
    }

    /*
     * This function resolves the link between the characters .
     * Arabic characters have four forms :
     * Isolated Form, Initial Form, Middle Form and Final Form
     */

    utext_setNativeIndex(ut, utext_nativeLength(ut));
    int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    int32_t lastNativeLimit = nativeLimit;
    UChar32 uchar = UTEXT_PREVIOUS32(ut);
    int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    int32_t lastNativeStart = nativeStart;
    int32_t nativeNext = -1;

    UBool lamalefFound = FALSE;
    UBool seenfamFound = FALSE;
    UBool yehhamzaFound = FALSE;
    UBool tashkeelFound = FALSE;
    UChar32 prevLink = 0;
    UChar32 lastLink = 0;
    UChar32 currLink = getLink(uchar);
    UChar32 nextLink = 0;

    for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL); )
    {
        // If high byte of currLink > 0 then more than one shape
        if ((currLink & 0xFF00) > 0 || (getLink(uchar) & IRRELEVANT) != 0)
        {
            // We need to know about next char
            if (nativeNext < 0)
            {
                UChar32 uchar2 = UTEXT_PREVIOUS32(ut);
                nativeNext = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                for (; (uchar2 != U_SENTINEL);
                    uchar2 = UTEXT_PREVIOUS32(ut), nativeNext = (int32_t)UTEXT_GETNATIVEINDEX(ut))
                {
                    nextLink = getLink(uchar2);
                    if ((nextLink & IRRELEVANT) == 0) {
                        break;
                    }
                }

                if (uchar2 == U_SENTINEL) {
                    nextLink = 0;
                }

                UTEXT_SETNATIVEINDEX(ut, nativeStart);
            }

            if (((currLink & ALEFTYPE) > 0) && ((lastLink & LAMTYPE) > 0)) {
                UChar32 wLamalef = changeLamAlef(uchar); // get from 0x065C-0x065f
                if (wLamalef != 0) {
                    // The default case is to drop the Alef and replace
                    // It by LAMALEF_SPACE_SUB which is the last character in the
                    // unicode private use area, this is done to make
                    // sure that removeLamAlefSpaces() handles only the
                    // spaces generated during lamalef generation.
                    // LAMALEF_SPACE_SUB is added here and is replaced by spaces
                    // in removeLamAlefSpaces()

                    utext_replace32(ut, lastNativeStart, &lastNativeLimit, wLamalef, TRUE, pErrorCode);

                    int32_t prevNativeLimit = nativeLimit;
                    utext_replace32(ut, nativeStart, &nativeLimit, LAMALEF_SPACE_SUB, FALSE, pErrorCode);
                    lastNativeStart += nativeLimit - prevNativeLimit;
                    lastNativeLimit += nativeLimit - prevNativeLimit;

                    nativeStart = lastNativeStart;
                    nativeLimit = lastNativeLimit;

                    UTEXT_SETNATIVEINDEX(ut, nativeLimit);
                    uchar = UTEXT_PREVIOUS32(ut);
                }

                lamalefFound = TRUE;

                lastLink = prevLink;
                currLink = getLink(wLamalef);
            }

            UChar32 uchar2 = UTEXT_PREVIOUS32(ut);
            if (uchar2 != U_SENTINEL)
                UTEXT_NEXT32(ut);

            if ((uchar2 == SPACE_CHAR) || (uchar2 == U_SENTINEL)) {
                if (isSeenFamilyChar(uchar)) {
                    seenfamFound = TRUE;
                }
                else if (uchar == YEH_HAMZA_CHAR) {
                    yehhamzaFound = TRUE;
                }
            }

            /*
             * get the proper shape according to link ability of neighbors
             * and of character; depends on the order of the shapes
             * (isolated, initial, middle, final) in the compatibility area
             */
            unsigned int Shape = shapeTable[nextLink & (LINKR + LINKL)][lastLink & (LINKR + LINKL)][currLink & (LINKR + LINKL)];

            if ((currLink & (LINKR + LINKL)) == 1) {
                Shape &= 1;
            }
            else if (isTashkeelChar(uchar)) {
                if ((lastLink & LINKL) && (nextLink & LINKR) && (tashkeelFlag == 1) &&
                    uchar != 0x064C && uchar != 0x064D)
                {
                    Shape = 1;
                    if ((nextLink & ALEFTYPE) == ALEFTYPE && (lastLink & LAMTYPE) == LAMTYPE) {
                        Shape = 0;
                    }
                }
                else if (tashkeelFlag == 2 && uchar == SHADDA06_CHAR) {
                    Shape = 1;
                }
                else {
                    Shape = 0;
                }
            }

            if ((uchar ^ 0x0600) < 0x100) {
                if (isTashkeelChar(uchar)) {
                    if (tashkeelFlag == 2 && uchar != SHADDA06_CHAR) {
                        int32_t prevNativeLimit = nativeLimit;
                        utext_replace32(ut, nativeStart, &nativeLimit, TASHKEEL_SPACE_SUB, FALSE, pErrorCode);
                        lastNativeStart += nativeLimit - prevNativeLimit;
                        lastNativeLimit += nativeLimit - prevNativeLimit;

                        tashkeelFound = TRUE;
                    }
                    else {
                        // To ensure the array index is within the range
                        U_ASSERT(uchar >= 0x064Bu && uchar - 0x064Bu < UPRV_LENGTHOF(IrrelevantPos));

                        int32_t prevNativeLimit = nativeLimit;
                        utext_replace32(ut, nativeStart, &nativeLimit, 0xFE70 + IrrelevantPos[(uchar - 0x064B)] + static_cast<UChar>(Shape), FALSE, pErrorCode);
                        lastNativeStart += nativeLimit - prevNativeLimit;
                        lastNativeLimit += nativeLimit - prevNativeLimit;
                    }
                }
                else if ((currLink & APRESENT) > 0) {
                    int32_t prevNativeLimit = nativeLimit;
                    utext_replace32(ut, nativeStart, &nativeLimit, 0xFB50 + (currLink >> 8) + Shape, FALSE, pErrorCode);
                    lastNativeStart += nativeLimit - prevNativeLimit;
                    lastNativeLimit += nativeLimit - prevNativeLimit;
                }
                else if ((currLink >> 8) > 0 && (currLink & IRRELEVANT) == 0) {
                    int32_t prevNativeLimit = nativeLimit;
                    utext_replace32(ut, nativeStart, &nativeLimit, 0xFE70 + (currLink >> 8) + Shape, FALSE, pErrorCode);
                    lastNativeStart += nativeLimit - prevNativeLimit;
                    lastNativeLimit += nativeLimit - prevNativeLimit;
                }
            }
        }

        // Move one notch forward
        if ((currLink & IRRELEVANT) == 0) {
            prevLink = lastLink;
            lastLink = currLink;
            lastNativeStart = nativeStart;
            lastNativeLimit = nativeLimit;
        }

        nativeLimit = nativeStart;

        uchar = UTEXT_PREVIOUS32(ut);
        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);

        if (nativeStart == nativeNext) {
            currLink = nextLink;
            nativeNext = -1;
        }
        else {
            currLink = getLink(uchar);
        }
    }

    int32_t nativeLength = (int32_t)utext_nativeLength(ut);

    if ((lamalefFound) || (tashkeelFound)) {
        nativeLength = handleGeneratedSpaces(ut, options, pErrorCode, shapeVars);
    }

    if ((seenfamFound) || (yehhamzaFound)) {
        nativeLength = expandCompositChar(ut, options, pErrorCode, SHAPE_MODE, shapeVars);
    }

    return nativeLength;
}

/*
 * Converts an Arabic Unicode buffer in FExx Range into unshaped
 * arabic Unicode buffer in 06xx Range
 */
static int32_t
deShapeUnicode(UText *ut,
    uint32_t options,
    UErrorCode *pErrorCode,
    struct uShapeVariables shapeVars)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    int32_t lamalefFound = 0;
    int32_t yehHamzaComposeEnabled = 0;
    int32_t seenComposeEnabled = 0;

    yehHamzaComposeEnabled = ((options & U_SHAPE_YEHHAMZA_MASK) == U_SHAPE_YEHHAMZA_TWOCELL_NEAR) ? 1 : 0;
    seenComposeEnabled = ((options & U_SHAPE_SEEN_MASK) == U_SHAPE_SEEN_TWOCELL_NEAR) ? 1 : 0;

    /*
     * This for loop changes the buffer from the Unicode FE range to
     * the Unicode 06 range
     */

    utext_setNativeIndex(ut, 0);
    int32_t nativeStart = 0;
    UChar32 uchar = UTEXT_NEXT32(ut);
    int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL);
        nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
    {
        UChar32 uchar2 = UTEXT_NEXT32(ut);
        if (uchar2 != U_SENTINEL)
            UTEXT_PREVIOUS32(ut);

        if ((uchar >= 0xFB50) && (uchar <= 0xFBFF)) { // FBxx Arabic range
            UChar c = convertFBto06[(uchar - 0xFB50)];
            if (c != 0)
                utext_replace32(ut, nativeStart, &nativeLimit, c, TRUE, pErrorCode);
        }
        else if ((yehHamzaComposeEnabled == 1) && ((uchar == HAMZA06_CHAR) || (uchar == HAMZAFE_CHAR))
            && isAlefMaksouraChar(uchar2)) {
            utext_replace32(ut, nativeStart, &nativeLimit, SPACE_CHAR, TRUE, pErrorCode);

            nativeStart = nativeLimit;
            uchar = UTEXT_NEXT32(ut);
            nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
            utext_replace32(ut, nativeStart, &nativeLimit, YEH_HAMZA_CHAR, TRUE, pErrorCode);
        }
        else if ((seenComposeEnabled == 1) && (isTailChar(uchar)) && (isSeenTailFamilyChar(uchar2))) {
            utext_replace32(ut, nativeStart, &nativeLimit, SPACE_CHAR, TRUE, pErrorCode);
        }
        else if ((uchar >= 0xFE70) && (uchar <= 0xFEF4)) { // FExx Arabic range
            utext_replace32(ut, nativeStart, &nativeLimit, convertFEto06[(uchar - 0xFE70)], TRUE, pErrorCode);
        }

        if (isLamAlefChar(uchar))
            lamalefFound = 1;
    }

    if (lamalefFound != 0) {
        nativeLimit = expandCompositChar(ut, options, pErrorCode, DESHAPE_MODE, shapeVars);
    }

    return nativeLimit;
}

static int32_t
shapeToArabicDigits(UText *ut,
    UChar32 digitBase,
    uint32_t options,
    UErrorCode *pErrorCode)
{
    //if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
    //    return 0;
    //}

    utext_setNativeIndex(ut, 0);
    int32_t nativeStart = 0;
    UChar32 uchar = UTEXT_NEXT32(ut);
    int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
    for (; /*(!U_FAILURE(*pErrorCode)) &&*/ (uchar != U_SENTINEL);
        nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
    {
        switch (options & U_SHAPE_DIGITS_MASK) {
        case U_SHAPE_DIGITS_EN2AN:
            // Add (digitBase-'0') to each European (ASCII) digit code point
            if (((uchar - 0x30) >= 0) && ((uchar - 0x30) < 10)) {
                utext_replace32(ut, nativeStart, &nativeLimit, uchar + (digitBase - 0x30), TRUE, pErrorCode);
            }
            break;
        case U_SHAPE_DIGITS_AN2EN:
            // Subtract (digitBase-'0') from each Arabic digit code point
            if (((uchar - digitBase) >= 0) && ((uchar - digitBase) < 10)) {
                utext_replace32(ut, nativeStart, &nativeLimit, uchar - (digitBase - 0x30), TRUE, pErrorCode);
            }
            break;
        }
    }

    return nativeLimit;
}

/*
 * This function shapes European digits to Arabic-Indic digits
 * in-place, writing over the input characters.
 */
static int32_t
shapeToArabicDigitsWithContext(UText *ut,
    UChar32 digitBase,
    UBool isLogical,
    UBool lastStrongWasAL,
    UErrorCode *pErrorCode)
{
    //if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
    //    return 0;
    //}

    digitBase -= 0x30;

    // Iteration direction depends on the type of input
    if (isLogical)
    {
        utext_setNativeIndex(ut, 0);
        int32_t nativeStart = 0;
        UChar32 uchar = UTEXT_NEXT32(ut);
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        for (; /*(!U_FAILURE(*pErrorCode)) &&*/ (uchar != U_SENTINEL);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            switch (ubidi_getClass(uchar))
            {
            case U_LEFT_TO_RIGHT: // L
            case U_RIGHT_TO_LEFT: // R
                lastStrongWasAL = FALSE;
                break;
            case U_RIGHT_TO_LEFT_ARABIC: // AL
                lastStrongWasAL = TRUE;
                break;
            case U_EUROPEAN_NUMBER: // EN
                if ((lastStrongWasAL) && ((uchar - 0x30) >= 0) && ((uchar - 0x30) < 10)) {
                    utext_replace32(ut, nativeStart, &nativeLimit, digitBase + uchar, TRUE, pErrorCode);
                }
                break;
            default:
                break;
            }
        }

        return nativeLimit;
    }
    else
    {
        utext_setNativeIndex(ut, utext_nativeLength(ut));
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        UChar32 uchar = UTEXT_PREVIOUS32(ut);
        int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
        for (; /*(!U_FAILURE(*pErrorCode)) &&*/ (uchar != U_SENTINEL);
            nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(ut), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut))
        {
            switch (ubidi_getClass(uchar))
            {
            case U_LEFT_TO_RIGHT: // L
            case U_RIGHT_TO_LEFT: // R
                lastStrongWasAL = FALSE;
                break;
            case U_RIGHT_TO_LEFT_ARABIC: // AL
                lastStrongWasAL = TRUE;
                break;
            case U_EUROPEAN_NUMBER: // EN
                if ((lastStrongWasAL) && ((uchar - 0x30) >= 0) && ((uchar - 0x30) < 10)) {
                    utext_replace32(ut, nativeStart, &nativeLimit, digitBase + uchar, FALSE, pErrorCode);
                }
                break;
            default:
                break;
            }
        }

        return (int32_t)utext_nativeLength(ut);
    }
}

/*
 ****************************************
 * u_shapeArabic
 ****************************************
 */

U_STABLE int32_t U_EXPORT2
u_shapeUText(UText *srcUt, UText *dstUt,
    uint32_t options,
    UErrorCode *pErrorCode) {

    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    if (srcUt == NULL || dstUt == NULL) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if (!utext_isWritable(dstUt)) {
        *pErrorCode = U_NO_WRITE_PERMISSION;
        return 0;
    }

    // Do input and output overlap?
    if (utext_equals(srcUt, dstUt)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    // Make sure that no reserved options values are used.
    if ((((options & U_SHAPE_TASHKEEL_MASK) > 0) &&
        ((options & U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED) == U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED)) ||
        (((options & U_SHAPE_TASHKEEL_MASK) > 0) &&
        ((options & U_SHAPE_LETTERS_MASK) == U_SHAPE_LETTERS_UNSHAPE)) ||
            (options & U_SHAPE_DIGIT_TYPE_RESERVED) == U_SHAPE_DIGIT_TYPE_RESERVED ||
        (options & U_SHAPE_DIGITS_MASK) == U_SHAPE_DIGITS_RESERVED ||
        ((options & U_SHAPE_LAMALEF_MASK) != U_SHAPE_LAMALEF_RESIZE &&
        (options & U_SHAPE_AGGREGATE_TASHKEEL_MASK) != 0) ||
            ((options & U_SHAPE_AGGREGATE_TASHKEEL_MASK) == U_SHAPE_AGGREGATE_TASHKEEL &&
        (options & U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED) != U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED)
        )
    {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    // Validate lamalef options.
    if (((options & U_SHAPE_LAMALEF_MASK) > 0) &&
        !(((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_BEGIN) ||
        ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_END) ||
            ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_RESIZE) ||
            ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_AUTO) ||
            ((options & U_SHAPE_LAMALEF_MASK) == U_SHAPE_LAMALEF_NEAR)))
    {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    // Validate Tashkeel options.
    if (((options & U_SHAPE_TASHKEEL_MASK) > 0) &&
        !(((options & U_SHAPE_TASHKEEL_MASK) == U_SHAPE_TASHKEEL_BEGIN) ||
        ((options & U_SHAPE_TASHKEEL_MASK) == U_SHAPE_TASHKEEL_END)
            || ((options & U_SHAPE_TASHKEEL_MASK) == U_SHAPE_TASHKEEL_RESIZE) ||
            ((options & U_SHAPE_TASHKEEL_MASK) == U_SHAPE_TASHKEEL_REPLACE_BY_TATWEEL)))
    {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    struct uShapeVariables shapeVars = { OLD_TAIL_CHAR,U_SHAPE_LAMALEF_BEGIN,U_SHAPE_LAMALEF_END,U_SHAPE_TASHKEEL_BEGIN,U_SHAPE_TASHKEEL_END,0 };
    int32_t dstNativeLength = 0;

    // Does Options contain the new Seen Tail Unicode code point option
    if ((options & U_SHAPE_TAIL_TYPE_MASK) == U_SHAPE_TAIL_NEW_UNICODE) {
        shapeVars.tailChar = NEW_TAIL_CHAR;
    }
    else {
        shapeVars.tailChar = OLD_TAIL_CHAR;
    }

    dstNativeLength = (int32_t)utext_copyUText(dstUt, srcUt, pErrorCode);
    if (!U_FAILURE(*pErrorCode))
    {
        // Perform letter shaping.
        if ((options & U_SHAPE_LETTERS_MASK) != U_SHAPE_LETTERS_NOOP) {
            int32_t nativeSpacesLeft = 0;
            int32_t nativeSpacesRight = 0;

            if ((options & U_SHAPE_AGGREGATE_TASHKEEL_MASK) > 0) {
                dstNativeLength = handleAggregateTashkeel(dstUt, options, pErrorCode);
            }

            // Start of Arabic letter shaping part.

            if ((options & U_SHAPE_TEXT_DIRECTION_MASK) == U_SHAPE_TEXT_DIRECTION_LOGICAL) {
                countSpaces(dstUt, options, &nativeSpacesLeft, &nativeSpacesRight);
                invertBuffer(dstUt, options, nativeSpacesLeft, (int32_t)utext_nativeLength(dstUt) - nativeSpacesRight, pErrorCode);
            }

            if ((options & U_SHAPE_TEXT_DIRECTION_MASK) == U_SHAPE_TEXT_DIRECTION_VISUAL_LTR) {
                if ((options & U_SHAPE_SPACES_RELATIVE_TO_TEXT_MASK) == U_SHAPE_SPACES_RELATIVE_TO_TEXT_BEGIN_END) {
                    shapeVars.spacesRelativeToTextBeginEnd = 1;
                    shapeVars.uShapeLamalefBegin = U_SHAPE_LAMALEF_END;
                    shapeVars.uShapeLamalefEnd = U_SHAPE_LAMALEF_BEGIN;
                    shapeVars.uShapeTashkeelBegin = U_SHAPE_TASHKEEL_END;
                    shapeVars.uShapeTashkeelEnd = U_SHAPE_TASHKEEL_BEGIN;
                }
            }

            switch (options & U_SHAPE_LETTERS_MASK) {
            case U_SHAPE_LETTERS_SHAPE:
                if ((options & U_SHAPE_TASHKEEL_MASK) > 0
                    && ((options & U_SHAPE_TASHKEEL_MASK) != U_SHAPE_TASHKEEL_REPLACE_BY_TATWEEL)) {
                    // Call the shaping function with tashkeel flag == 2 for removal of tashkeel
                    dstNativeLength = shapeUnicode(dstUt, options, pErrorCode, 2, shapeVars);
                }
                else {
                    // Default Call the shaping function with tashkeel flag == 1
                    dstNativeLength = shapeUnicode(dstUt, options, pErrorCode, 1, shapeVars);

                    // After shaping text check if user wants to remove tashkeel and replace it with tatweel
                    if ((options & U_SHAPE_TASHKEEL_MASK) == U_SHAPE_TASHKEEL_REPLACE_BY_TATWEEL) {
                        dstNativeLength = handleTashkeelWithTatweel(dstUt, options, pErrorCode);
                    }
                }
                break;
            case U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED:
                // Call the shaping function with tashkeel flag == 0
                dstNativeLength = shapeUnicode(dstUt, options, pErrorCode, 0, shapeVars);
                break;

            case U_SHAPE_LETTERS_UNSHAPE:
                // Call the deshaping function
                dstNativeLength = deShapeUnicode(dstUt, options, pErrorCode, shapeVars);
                break;
            default:
                // Will never occur because of validity checks above
                break;
            }

            if (*pErrorCode == U_NO_SPACE_AVAILABLE)
                *pErrorCode = U_ZERO_ERROR;

            if ((options & U_SHAPE_TEXT_DIRECTION_MASK) == U_SHAPE_TEXT_DIRECTION_LOGICAL) {
                countSpaces(dstUt, options, &nativeSpacesLeft, &nativeSpacesRight);
                invertBuffer(dstUt, options, nativeSpacesLeft, (int32_t)utext_nativeLength(dstUt) - nativeSpacesRight, pErrorCode);
            }

            // End of Arabic letter shaping part.
        }
    }

    if ((!U_FAILURE(*pErrorCode)) || (*pErrorCode == U_BUFFER_OVERFLOW_ERROR))
    {
        // Perform number shaping.
        if ((options & U_SHAPE_DIGITS_MASK) != U_SHAPE_DIGITS_NOOP) {
            UChar digitBase;

            // Select the requested digit group
            switch (options & U_SHAPE_DIGIT_TYPE_MASK) {
            case U_SHAPE_DIGIT_TYPE_AN:
                digitBase = 0x660; // Unicode: "Arabic-Indic digits"
                break;
            case U_SHAPE_DIGIT_TYPE_AN_EXTENDED:
                digitBase = 0x6f0; // Unicode: "Eastern Arabic-Indic digits (Persian and Urdu)"
                break;
            default:
                // Will never occur because of validity checks above
                digitBase = 0;
                break;
            }

            // Perform the requested operation
            switch (options & U_SHAPE_DIGITS_MASK) {
            case U_SHAPE_DIGITS_EN2AN:
            case U_SHAPE_DIGITS_AN2EN:
                dstNativeLength = shapeToArabicDigits(*pErrorCode == U_BUFFER_OVERFLOW_ERROR ? srcUt : dstUt, digitBase, options, pErrorCode);
                break;
            case U_SHAPE_DIGITS_ALEN2AN_INIT_LR:
                dstNativeLength = shapeToArabicDigitsWithContext(*pErrorCode == U_BUFFER_OVERFLOW_ERROR ? srcUt : dstUt,
                    digitBase,
                    (UBool)((options & U_SHAPE_TEXT_DIRECTION_MASK) == U_SHAPE_TEXT_DIRECTION_LOGICAL),
                    FALSE,
                    pErrorCode);
                break;
            case U_SHAPE_DIGITS_ALEN2AN_INIT_AL:
                dstNativeLength = shapeToArabicDigitsWithContext(*pErrorCode == U_BUFFER_OVERFLOW_ERROR ? srcUt : dstUt,
                    digitBase,
                    (UBool)((options & U_SHAPE_TEXT_DIRECTION_MASK) == U_SHAPE_TEXT_DIRECTION_LOGICAL),
                    TRUE,
                    pErrorCode);
                break;
            default:
                // Will never occur because of validity checks above
                break;
            }
        }
    }

    return dstNativeLength;
}

U_CAPI int32_t U_EXPORT2
u_shapeArabic(const UChar *src, int32_t srcLength,
    UChar *dest, int32_t destSize,
    uint32_t options,
    UErrorCode *pErrorCode) {

    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    if ((src == NULL) || (srcLength < -1)
        || (destSize < 0) || ((destSize > 0) && (dest == NULL))
        ) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    // Do input and output overlap?
    if ((dest != NULL) && ((src >= dest && src < dest + destSize)
        || (dest >= src && dest < src + srcLength))) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    UText srcUt = UTEXT_INITIALIZER;
    utext_openUChars(&srcUt, src, srcLength, pErrorCode);
    if (U_FAILURE(*pErrorCode))
        return 0;

    UText dstUt = UTEXT_INITIALIZER;
    UChar preFlight[1];
    if (dest == NULL) {
        utext_openU16(&dstUt, preFlight, 0, 0, pErrorCode);
        if (U_FAILURE(*pErrorCode))
            return 0;
    }
    else {
        utext_openU16(&dstUt, dest, 0, destSize, pErrorCode);
        if (U_FAILURE(*pErrorCode))
            return 0;
    }

    // A stack allocated UText wrapping a UChar * string
    // can be dumped without explicitly closing it.
    int32_t length = u_shapeUText(&srcUt, &dstUt, options, pErrorCode);

    utext_close(&srcUt);
    utext_close(&dstUt);

    return length;
}
