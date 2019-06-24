/********************************************************************
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 ********************************************************************/
/*   file name:  cbiditransformtst.c
 *   encoding:   UTF-8
 *   tab size:   8 (not used)
 *   indentation:4
 *
 *   created on: 2016aug21
 *   created by: Lina Kemmel
*
*   Contributions:
*   UText and UBiDi enhancements by Paul Werbicki
*/

#include "cintltst.h"
#include "unicode/ubidi.h"
#include "unicode/ubiditransform.h"
#include "unicode/ushape.h"
#include "unicode/ustring.h"
#include "unicode/utf16.h"
#include "unicode/ucnv.h"
#include "cbiditst.h"
#include "cstring.h"
#include "memory.h"

#define MAXLEN MAX_STRING_LENGTH * 4 // To accomodate u8

/*
 * Structures ----------------------------------------------------
 *
 */

enum UEncoding {
    UEncoding_U16 = 0,
    UEncoding_U8 = 1,
    UEncoding_U32 = 2,
};

#define MAKE_ITEMS(val) val, #val

typedef struct {
    int value;
    const char* description;
} Encoding;

static const Encoding Encodings[] = {
    { MAKE_ITEMS(UEncoding_U16) },
    { MAKE_ITEMS(UEncoding_U8) },
    { MAKE_ITEMS(UEncoding_U32) },
};
static const int32_t EncodingsCount = sizeof(Encodings) / sizeof(Encodings[0]);

typedef struct {
    const char *source;
    int32_t sourceLength;
    UChar *dest;
    int32_t destSize;
    UBiDiLevel inParaLevel;
    UBiDiOrder inOrder;
    UBiDiLevel outParaLevel;
    UBiDiOrder outOrder;
    const char *pReorderNoMirror;
    const char *pReorderAndMirror;
    const char *pContextShapes;
    const char *pMessage;
} UBidiTransformTestCases;

/*
 * Prototypes ---------------------------------------------------
 *
 */

void addBidiTransformTest(TestNode** root);

// complex/bidi-transform/TestAutoDirection

static void testAutoDirection(void);

static int32_t
doBiDiTransform(UBiDiTransform *pBiDiTransform,
    const UBidiTransformTestCases *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber,
    UBiDiMirroring doMirroring, uint32_t shapingOptions,
    const char *expectedChars,
    uint32_t digits,
    uint32_t letters
);

// complex/bidi-transform/TestAllTransformOptions

static void testAllTransformOptions(void);

/*
 * Helpers -----------------------------------------------------
 *
 */

static char* pseudoScript(const UChar *str);

static void shapeDigits(UChar *str, UChar srcZero, UChar destZero);

static void shapeLetters(UChar *str, const UChar *from, const UChar *to);

static void logResultsForDir(const UChar *srcText, 
    const UChar *destTxt,
    const UChar *expectedTxt, 
    UBiDiLevel inLevel, 
    UBiDiLevel outLevel);

/*
 * Regression Tests -------------------------------------------
 *
 */

void
addBidiTransformTest(TestNode** root)
{
    addTest(root, testAutoDirection, "complex/bidi-transform/TestAutoDirection");
    addTest(root, testAllTransformOptions, "complex/bidi-transform/TestAllTransformOptions");
}

/*
 * BiDi Transform --------------------------------------------
 *
 */

#define     LATN_ZERO           0x0030
#define     ARAB_ZERO           0x0660
#define     MIN_HEB_LETTER      0x05D0
#define     MIN_ARAB_LETTER     0x0630 /* relevant to this test only */
#define     MIN_SHAPED_LETTER   0xFEAB /* relevant to this test only */

#define     NUM_LETTERS         5       /* Used for arrays hereafter */
static const UChar unshapedLetters[NUM_LETTERS + 1] = { 0x0630, 0, 0x0631, 0, 0x0632, 2 };
static const UChar shapedLetters[NUM_LETTERS + 1] = { 0xfeab, 0, 0xfead, 0, 0xfeaf, 1 };

#if 0
static void substituteByPseudoChar(const UChar *src, char *dest,
            const UChar baseReal, const char basePseudo, const char max);


/* TODO: This code assumes the codepage is ASCII based. */

/*
 * Using the following conventions:
 * AL unshaped: A-E
 * AL shaped: F-J
 * R:  K-Z
 * EN: 0-4
 * AN: 5-9
*/
static void
substituteByPseudoChar(const UChar *src, char *dest, const UChar baseReal,
           const char basePseudo, const char max) {
    *dest = basePseudo + (*src - baseReal); /* (range math won't work on EBCDIC) */
    if (*dest > max) {
        *dest = max;
    }
}

static char*
pseudoScript(const UChar *str) {
    char *p = pseudo;
    if (str) {
        for (; *str; str++, p++) {
            switch (u_charDirection(*str)) {
                case U_RIGHT_TO_LEFT:
                    substituteByPseudoChar(str, p, MIN_HEB_LETTER, 'K', 'Z');
                    break;
                case U_RIGHT_TO_LEFT_ARABIC:
                    if (*str > 0xFE00) {
                        substituteByPseudoChar(str, p, MIN_SHAPED_LETTER, 'F', 'J');
                    } else {
                        substituteByPseudoChar(str, p, MIN_ARAB_LETTER, 'A', 'E');
                    }
                    break;
                case U_ARABIC_NUMBER:
                    substituteByPseudoChar(str, p, ARAB_ZERO, '5', '9');
                    break;
                default:
                    *p = (char)*str;
                    break;
            }
        }
    }
    *p = '\0';
    return pseudo;
}
#else
static char*
pseudoScript(const UChar *str) {
    return aescstrdup(str, -1);
}
#endif

static void
logResultsForDir(const UChar *srcTxt, const UChar *destTxt, const UChar *expectedTxt,
            UBiDiLevel inLevel, UBiDiLevel outLevel)
{
    if (u_strcmp(expectedTxt, destTxt)) {
        log_err("Unexpected transform Dest: inLevel: 0x%02x; outLevel: 0x%02x;\ninText: %s; outText: %s; expected: %s\n",
                inLevel, outLevel, pseudoScript(srcTxt), pseudoScript(destTxt), pseudoScript(expectedTxt));
    }
}

/**
 * Tests various combinations of base directions, with the input either
 * <code>UBIDI_DEFAULT_LTR</code> or <code>UBIDI_DEFAULT_RTL</code>, and the
 * output either <code>UBIDI_LTR</code> or <code>UBIDI_RTL</code>. Order is
 * always <code>UBIDI_LOGICAL</code> for the input and <code>UBIDI_VISUAL</code>
 * for the output.
 */
static void
testAutoDirection(void)
{
    static const UBiDiLevel inLevels[] = {
        UBIDI_DEFAULT_LTR, UBIDI_DEFAULT_RTL
    };
    static const UBiDiLevel outLevels[] = {
        UBIDI_LTR, UBIDI_RTL
    };
    static const char *srcTexts[] = {
        "abc \\u05d0\\u05d1",
        "... abc \\u05d0\\u05d1",
        "\\u05d0\\u05d1 abc",
        "... \\u05d0\\u05d1 abc",
        ".*:"
    };
    uint32_t nTexts = sizeof(srcTexts) / sizeof(srcTexts[0]);
    uint32_t i, nInLevels = sizeof(inLevels) / sizeof(inLevels[0]);
    uint32_t j, nOutLevels = sizeof(outLevels) / sizeof(outLevels[0]);

    UChar src[MAXLEN] = { 0 };
    UChar dest[MAXLEN] = { 0 };
    UChar expected[MAXLEN] = { 0 };
    UChar temp[MAXLEN * 2] = { 0 };

    UErrorCode errorCode = U_ZERO_ERROR;
    UBiDiTransform *pTransform = ubiditransform_open(&errorCode);

    UBiDi *pBidi = ubidi_open();

    while (nTexts-- > 0) {
        uint32_t srcLen;
        u_unescape(srcTexts[nTexts], src, MAXLEN);
        srcLen = u_strlen(src);
        for (i = 0; i < nInLevels; i++) {
            for (j = 0; j < nOutLevels; j++) {
                errorCode = U_ZERO_ERROR;
                ubiditransform_transform(pTransform, src, -1, dest, MAXLEN - 1,
                        inLevels[i], UBIDI_LOGICAL, outLevels[j], UBIDI_VISUAL,
                        UBIDI_MIRRORING_OFF, 0, &errorCode);
                /* Use UBiDi as a model we compare to */
                ubidi_setPara(pBidi, src, srcLen, inLevels[i], NULL, &errorCode);
                ubidi_writeReordered(pBidi, expected, MAXLEN, UBIDI_REORDER_DEFAULT, &errorCode);
                if (outLevels[j] == UBIDI_RTL) {
                    ubidi_writeReverse(expected, u_strlen(expected), temp, MAXLEN,
                            UBIDI_OUTPUT_REVERSE, &errorCode);
                    logResultsForDir(src, dest, temp, inLevels[i], outLevels[j]);
                } else {
                    logResultsForDir(src, dest, expected, inLevels[i], outLevels[j]);
                }
            }
        }
    }
    ubidi_close(pBidi);
    ubiditransform_close(pTransform);
}

static void
shapeDigits(UChar *str, UChar srcZero, UChar destZero)
{
    UChar32 c = 0;
    uint32_t i = 0, j, length = u_strlen(str);
    while (i < length) {
        j = i;
        U16_NEXT(str, i, length, c); 
        if (c >= srcZero && c <= srcZero + 9) {
            /* length of c here is always a single UChar16 */
            str[j] = (UChar)(c + (destZero - srcZero));
        }
    }
}

static void
shapeLetters(UChar *str, const UChar *from, const UChar *to)
{
    uint32_t i = 0, j, length = u_strlen(str), index;
    UChar32 c = 0;
    while (i < length) {
        j = i;
        U16_NEXT(str, i, length, c); 
        index = c - from[0];
        if (index < NUM_LETTERS && from[index * from[NUM_LETTERS]] != 0) {
            /* The length of old and new values is always a single UChar16,
               so can just assign a new value to str[j] */
            str[j] = to[index * from[NUM_LETTERS]];
        }
    }
}

static int32_t
doBiDiTransform(UBiDiTransform *pBiDiTransform,
    const UBidiTransformTestCases *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber,
    UBiDiMirroring doMirroring, uint32_t shapingOptions,
    const char *expectedChars,
    uint32_t digits,
    uint32_t letters)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    UChar u16Buf[MAXLEN];
    int32_t u16Len = 0;

    UChar u16BufSrc[MAXLEN];
    int32_t u16LenSrc = 0;
    memset(u16BufSrc, 0, sizeof(u16BufSrc));

    {
        const char *srcChars = testCases[testNumber].source;
        int32_t srcLen = testCases[testNumber].sourceLength;
        if (srcLen == (-1))
            srcLen = (int32_t)strlen(srcChars);
        u16LenSrc = u_unescape(srcChars, u16BufSrc, srcLen);
    }

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        errorCode = U_ZERO_ERROR;
        memset(u16Buf, 0, sizeof(u16Buf));

        if (encoding == UEncoding_U16) {
            u16Len = ubiditransform_transform(pBiDiTransform,
                u16BufSrc, u16LenSrc,
                testCases[testNumber].dest, testCases[testNumber].destSize,
                testCases[testNumber].inParaLevel, testCases[testNumber].inOrder,
                testCases[testNumber].outParaLevel, testCases[testNumber].outOrder,
                doMirroring, shapingOptions, 
                &errorCode);

            if (testCases[testNumber].dest)
                u_strcpy(u16Buf, testCases[testNumber].dest);
        }
        else if (encoding == UEncoding_U8) {
            UConverter *u8Convertor = ucnv_open("UTF8", &errorCode);

            if ((U_FAILURE(errorCode)) || (u8Convertor == NULL)) {
                return 0;
            }

            uint8_t u8BufDst[MAXLEN];

            memset(u8BufDst, 0, sizeof(u8BufDst) / sizeof(char));

            UText* srcUt16 = NULL;
            if (testCases[testNumber].source)
                srcUt16 = utext_openUChars(NULL, u16BufSrc, u16LenSrc, &errorCode);

            UText* dstUt8 = NULL;
            if (testCases[testNumber].dest)
                dstUt8 = utext_openU8(NULL, u8BufDst, 0, (!testCases[testNumber].dest) ? 0 : sizeof(u8BufDst), &errorCode);

            int32_t u8LenDst = ubiditransform_transformUText(pBiDiTransform,
                srcUt16, dstUt8,
                testCases[testNumber].inParaLevel, testCases[testNumber].inOrder,
                testCases[testNumber].outParaLevel, testCases[testNumber].outOrder,
                doMirroring, shapingOptions,
                &errorCode);

            u16Len = u8LenDst;
            if (testCases[testNumber].dest) {
                u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
                u16Len = ucnv_toUChars(u8Convertor, u16Buf, sizeof(u16Buf), (const char *)u8BufDst, u8LenDst * sizeof(char), &errorCode);
            }

            utext_close(srcUt16);
            utext_close(dstUt8);
            ucnv_close(u8Convertor);
        }
        else if (encoding == UEncoding_U32) {
            UConverter *u32Convertor = ucnv_open("UTF32", &errorCode);

            if ((U_FAILURE(errorCode)) || (u32Convertor == NULL)) {
                return 0;
            }

            UChar32 u32BufDst[MAXLEN];

            memset(u32BufDst, 0, sizeof(u32BufDst) / sizeof(UChar32));
            u32BufDst[0] = 0x0000FEFF;

            UText* srcUt16 = NULL;
            if (testCases[testNumber].source)
                srcUt16 = utext_openUChars(NULL, u16BufSrc, u16LenSrc, &errorCode);

            UText* dstUt32 = NULL;
            if (testCases[testNumber].dest)
                dstUt32 = utext_openU32(NULL, &u32BufDst[1], 0, (!testCases[testNumber].dest) ? 0 : sizeof(u32BufDst), &errorCode);

            int32_t u32LenDst = ubiditransform_transformUText(pBiDiTransform,
                srcUt16, dstUt32,
                testCases[testNumber].inParaLevel, testCases[testNumber].inOrder,
                testCases[testNumber].outParaLevel, testCases[testNumber].outOrder,
                doMirroring, shapingOptions,
                &errorCode);

            u16Len = u32LenDst;
            if (testCases[testNumber].dest) {
                u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
                u16Len = ucnv_toUChars(u32Convertor, u16Buf, sizeof(u16Buf), (const char *)u32BufDst, (u32LenDst + 1) * sizeof(UChar32), &errorCode);
            }

            utext_close(srcUt16);
            utext_close(dstUt32);
            ucnv_close(u32Convertor);
        }

        if (!U_SUCCESS(errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                fileName, lineNumber, "ubiditransform_transform", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
        }
        else {
            UChar expected[MAXLEN] = { 0 };
            int32_t u16ExpectedLength = 0;

            u16ExpectedLength = u_unescape(expectedChars, expected, MAXLEN);

            switch (digits) {
            case U_SHAPE_DIGITS_EN2AN:
                shapeDigits(expected, LATN_ZERO, ARAB_ZERO);
                break;
            case U_SHAPE_DIGITS_AN2EN:
                shapeDigits(expected, ARAB_ZERO, LATN_ZERO);
                break;
            default:
                break;
            }
            switch (letters) {
            case U_SHAPE_LETTERS_SHAPE:
                shapeLetters(expected, unshapedLetters, shapedLetters);
                break;
            case U_SHAPE_LETTERS_UNSHAPE:
                shapeLetters(expected, shapedLetters, unshapedLetters);
                break;
            }

            if ((U_SUCCESS(errorCode)) && (expectedChars) && (u16Len != u16ExpectedLength)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                    fileName, lineNumber, "ubiditransform_transform", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                    u16Len, u16ExpectedLength);
            }
            else if (memcmp(expected, u16Buf, u16Len)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): Unexpected transform; Digits: 0x%08x; Letters: 0x%08x\nInput\t: %s\nGot\t: %s\nExpected: %s\n",
                    fileName, lineNumber, "ubiditransform_transform", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                    digits, letters, pseudoScript(u16BufSrc), pseudoScript(u16Buf), pseudoScript(expected));
            }
        }
    }

    return u16Len;
}

/**
 * This function covers:
 * <ul>
 * <li>all possible combinations of ordering schemes and <strong>explicit</strong>
 * base directions, applied to both input and output,</li>
 * <li>selected tests for auto direction (systematically, auto direction is
 * covered in a dedicated test) applied on both input and output,</li>
 * <li>all possible combinations of mirroring, digits and letters applied
 * to output only.</li>
 * </ul>
 */
static void
testAllTransformOptions(void)
{
    static const char inText[] =
        "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662";

    static UChar dest[MAXLEN] = { 0 };

    static const UBidiTransformTestCases testCases[] = {
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_LTR,
            UBIDI_LOGICAL, 
            UBIDI_LTR, 
            UBIDI_LOGICAL,
            "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662", // reordering no mirroring
            "a[b]c \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662", // mirroring
            "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u0662\\u0663\\u0660 e\\u0631\\u0664 f \\ufeaf \\u0661\\u0662", // context numeric shaping
            "1: Logical LTR ==> Logical LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_LTR,
            UBIDI_LOGICAL, 
            UBIDI_LTR, 
            UBIDI_VISUAL,
            "a[b]c 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "a[b]c 1 \\u05d4(\\u05d3 \\u05d2\\u05d1)\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "a[b]c 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 d \\u0662\\u0663\\u0660 \\u0630 e\\u0664\\u0631 f \\u0661\\u0662 \\ufeaf",
            "2: Logical LTR ==> Visual LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_LTR,
            UBIDI_LOGICAL, 
            UBIDI_RTL, 
            UBIDI_LOGICAL,
            "\\ufeaf \\u0661\\u0662 f \\u0631e4 \\u0630 23\\u0660 d \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 a[b]c",
            "\\ufeaf \\u0661\\u0662 f \\u0631e4 \\u0630 23\\u0660 d \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 a[b]c",
            "\\ufeaf \\u0661\\u0662 f \\u0631e\\u0664 \\u0630 \\u0662\\u0663\\u0660 d \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 a[b]c",
            "3: Logical LTR ==> Logical RTL" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_LTR,
            UBIDI_LOGICAL, 
            UBIDI_RTL, 
            UBIDI_VISUAL,
            "\\ufeaf \\u0662\\u0661 f \\u06314e \\u0630 \\u066032 d \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 c]b[a",
            "\\ufeaf \\u0662\\u0661 f \\u06314e \\u0630 \\u066032 d \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 c]b[a",
            "\\ufeaf \\u0662\\u0661 f \\u0631\\u0664e \\u0630 \\u0660\\u0663\\u0662 d \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 c]b[a",
            "4: Logical LTR ==> Visual RTL" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_LOGICAL, 
            UBIDI_RTL, 
            UBIDI_LOGICAL,
            "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662",
            "a[b]c \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662", // mirroring
            "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662",
            "5: Logical RTL ==> Logical RTL" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_LOGICAL, 
            UBIDI_RTL, 
            UBIDI_VISUAL,
            "c]b[a \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "c]b[a \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "c]b[a \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "6: Logical RTL ==> Visual RTL" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_LOGICAL, 
            UBIDI_LTR, 
            UBIDI_LOGICAL,
            "\\ufeaf \\u0661\\u0662 f 4\\u0631e 23\\u0630 \\u0660 d 1 \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 a[b]c",
            "\\ufeaf \\u0661\\u0662 f 4\\u0631e 23\\u0630 \\u0660 d 1 \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 a[b]c",
            "\\ufeaf \\u0661\\u0662 f 4\\u0631e 23\\u0630 \\u0660 d 1 \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 a[b]c",
            "7: Logical RTL ==> Logical LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_LOGICAL, 
            UBIDI_LTR, 
            UBIDI_VISUAL,
            "\\u0661\\u0662 \\ufeaf f 4\\u0631e 23\\u0660 \\u0630 d 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 a[b]c",
            "\\u0661\\u0662 \\ufeaf f 4\\u0631e 23\\u0660 \\u0630 d 1 \\u05d4(\\u05d3 \\u05d2\\u05d1)\\u05d0 a[b]c",
            "\\u0661\\u0662 \\ufeaf f 4\\u0631e 23\\u0660 \\u0630 d 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 a[b]c",
            "8: Logical RTL ==> Visual LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_LTR,
            UBIDI_VISUAL, 
            UBIDI_LTR, 
            UBIDI_VISUAL,
            "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662",
            "a[b]c \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662", // mirroring
            "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u0662\\u0663\\u0660 e\\u0631\\u0664 f \\ufeaf \\u0661\\u0662",
            "9: Visual LTR ==> Visual LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_LTR,
            UBIDI_VISUAL, 
            UBIDI_LTR, 
            UBIDI_LOGICAL,
            "a[b]c 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "a[b]c 1 \\u05d4(\\u05d3 \\u05d2\\u05d1)\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "a[b]c 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "10: Visual LTR ==> Logical LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_LTR,
            UBIDI_VISUAL, 
            UBIDI_RTL, 
            UBIDI_VISUAL,
            "\\u0662\\u0661 \\ufeaf f 4\\u0631e \\u066032 \\u0630 d 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 c]b[a",
            "\\u0662\\u0661 \\ufeaf f 4\\u0631e \\u066032 \\u0630 d 1 \\u05d4(\\u05d3 \\u05d2\\u05d1)\\u05d0 c]b[a",
            "\\u0662\\u0661 \\ufeaf f \\u0664\\u0631e \\u0660\\u0663\\u0662 \\u0630 d 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 c]b[a",
            "11: Visual LTR ==> Visual RTL" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_LTR,
            UBIDI_VISUAL, 
            UBIDI_RTL, 
            UBIDI_LOGICAL,
            "\\u0661\\u0662 \\ufeaf f 4\\u0631e 23\\u0660 \\u0630 d 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 a[b]c",
            "\\u0661\\u0662 \\ufeaf f 4\\u0631e 23\\u0660 \\u0630 d 1 \\u05d4(\\u05d3 \\u05d2\\u05d1)\\u05d0 a[b]c",
            "\\u0661\\u0662 \\ufeaf f \\u0664\\u0631e \\u0662\\u0663\\u0660 \\u0630 d 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 a[b]c",
            "12: Visual LTR ==> Logical RTL" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_VISUAL, 
            UBIDI_RTL, 
            UBIDI_VISUAL,
            "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662",
            "a[b]c \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662",
            "a[b]c \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 23\\u0660 e\\u06314 f \\ufeaf \\u0661\\u0662",
            "13: Visual RTL ==> Visual RTL" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_VISUAL, 
            UBIDI_RTL, 
            UBIDI_LOGICAL,
            "c]b[a \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "c]b[a \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "c]b[a \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "14: Visual RTL ==> Logical RTL" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_VISUAL, 
            UBIDI_LTR, 
            UBIDI_VISUAL,
            "\\u0662\\u0661 \\ufeaf f 4\\u0631e \\u066032 \\u0630 d 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 c]b[a",
            "\\u0662\\u0661 \\ufeaf f 4\\u0631e \\u066032 \\u0630 d 1 \\u05d4(\\u05d3 \\u05d2\\u05d1)\\u05d0 c]b[a",
            "\\u0662\\u0661 \\ufeaf f 4\\u0631e \\u066032 \\u0630 d 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 c]b[a",
            "15: Visual RTL ==> Visual LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_VISUAL, 
            UBIDI_LTR, 
            UBIDI_LOGICAL,
            "\\ufeaf \\u0662\\u0661 f 4\\u0631e \\u066032 \\u0630 d 1 \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 c]b[a",
            "\\ufeaf \\u0662\\u0661 f 4\\u0631e \\u066032 \\u0630 d 1 \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 c]b[a",
            "\\ufeaf \\u0662\\u0661 f 4\\u0631e \\u066032 \\u0630 d 1 \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 c]b[a",
            "16: Visual RTL ==> Logical LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_DEFAULT_RTL,
            UBIDI_LOGICAL, 
            UBIDI_LTR, 
            UBIDI_VISUAL,
            "a[b]c 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "a[b]c 1 \\u05d4(\\u05d3 \\u05d2\\u05d1)\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "a[b]c 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 d \\u0662\\u0663\\u0660 \\u0630 e\\u0664\\u0631 f \\u0661\\u0662 \\ufeaf",
            "17: Logical DEFAULT_RTL ==> Visual LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_LOGICAL, 
            UBIDI_DEFAULT_LTR, 
            UBIDI_VISUAL,
            "c]b[a \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "c]b[a \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "c]b[a \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "18: Logical RTL ==> Visual DEFAULT_LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_DEFAULT_LTR,
            UBIDI_LOGICAL, 
            UBIDI_LTR, 
            UBIDI_VISUAL,
            "a[b]c 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "a[b]c 1 \\u05d4(\\u05d3 \\u05d2\\u05d1)\\u05d0 d 23\\u0660 \\u0630 e4\\u0631 f \\u0661\\u0662 \\ufeaf",
            "a[b]c 1 \\u05d4)\\u05d3 \\u05d2\\u05d1(\\u05d0 d \\u0662\\u0663\\u0660 \\u0630 e\\u0664\\u0631 f \\u0661\\u0662 \\ufeaf",
            "19: Logical DEFAULT_LTR ==> Visual LTR" },
        { inText,
            -1,
            dest,
            MAXLEN,
            UBIDI_RTL,
            UBIDI_LOGICAL, 
            UBIDI_DEFAULT_RTL, 
            UBIDI_VISUAL,
            "c]b[a \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "c]b[a \\u05d0)\\u05d1\\u05d2 \\u05d3(\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "c]b[a \\u05d0(\\u05d1\\u05d2 \\u05d3)\\u05d4 1 d \\u0630 \\u066032 e\\u06314 f \\ufeaf \\u0662\\u0661",
            "20: Logical RTL ==> Visual DEFAULT_RTL" }
    };
    static const uint32_t digits[] = {
        U_SHAPE_DIGITS_NOOP,
        U_SHAPE_DIGITS_AN2EN,
        U_SHAPE_DIGITS_EN2AN,
        U_SHAPE_DIGITS_ALEN2AN_INIT_LR
    };
    static const uint32_t letters[] = {
        U_SHAPE_LETTERS_UNSHAPE,
        U_SHAPE_LETTERS_SHAPE
    };
    const char *expectedStr;
    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);
    uint32_t j, nDigits = sizeof(digits) / sizeof(digits[0]);
    uint32_t k, nLetters = sizeof(letters) / sizeof(letters[0]);

    UErrorCode errorCode = U_ZERO_ERROR;

    UBiDiTransform *pTransform = ubiditransform_open(&errorCode);
    if ((pTransform == NULL) || (!U_SUCCESS(errorCode))) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory, error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubiditransform_open", Encodings[UEncoding_U16].description, 0, "testAllTransformOptions",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }

    // Test various combinations of base levels, orders, mirroring, digits and letters
    for (i = 0; i < nTestCases; i++) {
        expectedStr = testCases[i].pReorderAndMirror;

        doBiDiTransform(pTransform, testCases, i, __FILE__, __LINE__, UBIDI_MIRRORING_ON, 0, expectedStr, U_SHAPE_DIGITS_NOOP, U_SHAPE_LETTERS_NOOP);

        for (j = 0; j < nDigits; j++) {
            expectedStr = digits[j] == U_SHAPE_DIGITS_ALEN2AN_INIT_LR ? testCases[i].pContextShapes : testCases[i].pReorderNoMirror;

            for (k = 0; k < nLetters; k++) {
                // Here use NULL for pTransform
                doBiDiTransform(NULL, testCases, i, __FILE__, __LINE__, UBIDI_MIRRORING_OFF, digits[j] | letters[k], expectedStr, digits[j], letters[k]);
            }
        }
    }

    ubiditransform_close(pTransform);
}
