// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
*
*   Copyright (c) 1997-2014, International Business Machines Corporation and
*   others. All Rights Reserved.
*
*********************************************************************
*   file name:  cbiditst.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999sep27
*   created by: Markus W. Scherer
*
*   Contributions:
*   Updated by Matitiahu Allouche
*   UText and UBiDi enhancements by Paul Werbicki
*/

#include "cintltst.h"
#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/ustring.h"
#include "unicode/ubidi.h"
#include "unicode/ushape.h"
#include "unicode/ucnv.h"
#include "cbiditst.h"
#include "cstring.h"
// The following include is needed for sprintf
#include <stdio.h>

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
    int32_t value;
    const char* description;
} Encoding;

static const Encoding Encodings[] = {
    { MAKE_ITEMS(UEncoding_U16) },
    { MAKE_ITEMS(UEncoding_U8) },
    { MAKE_ITEMS(UEncoding_U32) },
};
static const int32_t EncodingsCount = sizeof(Encodings) / sizeof(Encodings[0]);

enum UInputType {
    UInputType_char = 0,
    UInputType_UChar = 1,
    UInputType_pseudo16_char = 2,
    UInputType_pseudo16_UChar = 3,
    UInputType_unescape_char = 4,
    UInputType_unescape_UChar = 5,
};

typedef struct {
    const char* source;
    int32_t sourceLength;
    UBiDiLevel paraLevel;
    UBiDiLevel *embeddingLevels;
    UChar *dest;
    int32_t destSize;
    uint16_t options;
    UErrorCode errorCode;
    const char* expectedChars;
    enum UInputType inputType;
    int32_t expectedU16Length;
    const char* pMessage;
    UBool skipSetPara;
    UBool skipCheckWhatYouCan;
} UBidiWriteReorderedTestCases;

typedef struct {
    const UChar* source;
    int32_t sourceLength;
    UChar *dest;
    int32_t destSize;
    uint16_t options;
    UErrorCode errorCode;
    const UChar* expectedChars;
    int32_t expectedLength;
    const char* pMessage;
} UBidiWriteReverseTestCases;

typedef struct {
    const UChar* source;
    int32_t sourceLength;
    UBiDiDirection expectedDir;
    const char* pMessage;
} UBidiGetBaseDirectionTestCase;

typedef struct {
    const char* prologue;
    const char* source;
    const char* epilogue;
    const char* expected;
    UBiDiLevel paraLevel;
    int32_t proLength;
    int32_t sourceLength;
    int32_t epiLength;
} UBidiSetContextTestCase;

typedef struct {
    const UChar* source;
    int32_t sourceLength;
    UChar* dest;
    int32_t destSize;
    uint32_t options;
    UErrorCode errorCode;
    const UChar* expectedChars;
    int32_t expectedLength;
    const char *pMessage;
} UBidiShapeTestCase;

/*
 * Prototypes ---------------------------------------------------
 *
 */

void addComplexTest(TestNode** root);

// BiDi

// complex/bidi/TestCharFromDirProp

static void testCharFromDirProp(void);

// complex/bidi/TestFailureRecovery

static void testFailureRecovery(void);

// complex/bidi/TestBidi

static void testBidi(void);

static void _testBidiDoTests(UBiDi *pBiDi, UBiDi *pLine, UBool countRunsFirst);

static UBool doSetPara(UBiDi *pBiDi, 
    const UChar *src,
    int32_t srcLength,
    UBiDiLevel paraLevel, 
    UBiDiLevel *embeddingLevels,
    int32_t embeddingLevelsLen,
    UErrorCode expectedError,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber,
    const Encoding encoding, UText **srcUt, uint8_t *srcBuffer, int32_t srcSize);

static void _testBidiSetParaAndLine(UBiDi *pBiDi, 
    UBiDi *pLine,
    const UChar *src, int32_t srcLength,
    UBiDiLevel paraLevel,
    UBool countRunsFirst,
    const BiDiTestData *testCases,
    int32_t testNumber,
    int32_t encoding);

static void _testBidiDoTest(UBiDi *pBiDi,
    int32_t lineStart,
    UBool countRunsFirst,
    const BiDiTestData *testCases,
    int testNumber,
    int32_t encoding);

static void _testBidiDoTestReordering(UBiDi *pBiDi, 
    int testNumber,
    int32_t encoding);

static void _testBidiDoMiscTest(void);

// complex/bidi/TestInverse

static void testInverse(void);

static void _testInverseBidiManyTest(UBiDi *pBiDi, UBiDiLevel direction);

static void _testInverseBidi(UBiDi *pBiDi, const UChar *src, int32_t srcLength,
                             UBiDiLevel direction);

static void _testInverseBidiReverseTest(void);

static void _testInverseBidiManyAddedPointsTest(void);

static int32_t doWriteReverse(const UBidiWriteReverseTestCases *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber);

static void _testInverseBidiMisc(void);

// complex/bidi/TestReorder

static void testReorder(void);

static int32_t doWriteReordered(UBiDi *pBiDi,
    const UBidiWriteReorderedTestCases *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber);

// complex/bidi/bug-9024

static void testReorderArabicMathSymbols(void);

// complex/bidi/TestReorderingMode

static void testReorderingMode(void);

static const char* _testReorderingModeInverseBasic(UBiDi *pBiDi, const char *src, int32_t srcLen,
                                uint32_t option, UBiDiLevel level, char *result);

static UBool _testReorderingModeAssertRoundTrip(UBiDi *pBiDi, int32_t tc, int32_t outIndex,
                             const char *srcChars, const char *destChars,
                             const UChar *dest, int32_t destLen, int mode,
                             int option, UBiDiLevel level);

static UBool _testReorderingModeCheckResultLength(UBiDi *pBiDi, const char *srcChars,
                               const char *destChars,
                               int32_t destLen, const char *mode,
                               const char *option, UBiDiLevel level);

static UBool _testReorderingModeCheckMaps(UBiDi *pBiDi, int32_t stringIndex, const char *src,
                       const char *dest, const char *mode, const char* option,
                       UBiDiLevel level, UBool forward);

// complex/bidi/TestReorderRunsOnly

static void testReorderRunsOnly(void);

// complex/bidi/TestMultipleParagraphs

static void testMultipleParagraphs(void);

// complex/bidi/TestStreaming

static void testStreaming(void);

// complex/bidi/TestClassOverride

static void testClassOverride(void);

static void _testClassOverrideVerifyCallbackParams(UBiDiClassCallback* fn, 
    const void* context,
    UBiDiClassCallback* expectedFn,
    const void* expectedContext,
    int32_t sizeOfContext);

// complex/bidi/testGetBaseDirection

static void testGetBaseDirection(void);

static UBiDiDirection doGetBaseDirection(const UBidiGetBaseDirectionTestCase *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber);

// complex/bidi/testContext

static void testContext(void);

static int32_t doSetContext(UBiDi* pBiDi, 
    const UBidiSetContextTestCase *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber);

// complex/bidi/TestBracketOverflow

static void testBracketOverflow(void);

// complex/bidi/TestExplicitLevel0

static void testExplicitLevel0(void);

// complex/bidi/TestVisualText

static void testVisualText(void);

// Arabic Shaping

// complex/arabic-shaping/ArabicShapingTest

static void testArabicShaping(void);

static int32_t doShapeArabic(const UBidiShapeTestCase *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber);

static void testArabicShapingLamAlefSpecialVLTR(void);

static void testArabicShapingTashkeelSpecialVLTR(void);

static void testArabicDeShapingLOGICAL(void);

static void testArabicShapingTailTest(void);

static void testArabicShapingForBug5421(void);

static void testArabicShapingForBug8703(void);

static void testArabicShapingForBug9024(void);

static void testArabicShapingForNewCharacters(void);

static void _testPresentationForms(const UChar *in);

/*
 * Helpers -----------------------------------------------------
 *
 */

static const char *levelString="...............................................................";

static void initCharFromDirProps(void);

static UChar *
getStringFromDirProps(const uint8_t *dirProps, int32_t length, UChar *buffer);

static void printUnicode(const UChar *s, int32_t length, const UBiDiLevel *levels);

static int pseudoToU16(const int length, const char * input, UChar * output);
static int u16ToPseudo(const int length, const UChar * input, char * output);

static char * formatLevels(UBiDi *pBiDi, char *buffer);

static UBool checkWhatYouCan(UBiDi *pBiDi,
    const char *srcChars,
    const char *dstChars,
    const char *fileName,
    int32_t lineNumber,
    int32_t encoding);

static UBool
assertIllegalArgument(const char* message, UErrorCode* rc);

/*
 * Regression Tests -------------------------------------------
 *
 */

void
addComplexTest(TestNode** root) {

    // BiDi
    addTest(root, testCharFromDirProp, "complex/bidi/TestCharFromDirProp");
    addTest(root, testFailureRecovery, "complex/bidi/TestFailureRecovery");
    addTest(root, testBidi, "complex/bidi/TestBidi"); // u8
    addTest(root, testInverse, "complex/bidi/TestInverse");
    addTest(root, testReorder, "complex/bidi/TestReorder"); // u8
    addTest(root, testReorderArabicMathSymbols, "complex/bidi/bug-9024");
    addTest(root, testReorderingMode, "complex/bidi/TestReorderingMode");
    addTest(root, testReorderRunsOnly, "complex/bidi/TestReorderRunsOnly"); // u8
    addTest(root, testMultipleParagraphs, "complex/bidi/TestMultipleParagraphs"); // u8
    addTest(root, testStreaming, "complex/bidi/TestStreaming");
    addTest(root, testContext, "complex/bidi/TestContext"); // u8, u16, u32 - index off with context in place
    addTest(root, testClassOverride, "complex/bidi/TestClassOverride");
    addTest(root, testGetBaseDirection, "complex/bidi/testGetBaseDirection");
    addTest(root, testBracketOverflow, "complex/bidi/TestBracketOverflow");
    addTest(root, testExplicitLevel0, "complex/bidi/TestExplicitLevel0");
    addTest(root, testVisualText, "complex/bidi/TestVisualText");

    // Arabic Shaping
    addTest(root, testArabicShaping, "complex/arabic-shaping/ArabicShapingTest");
    addTest(root, testArabicShapingLamAlefSpecialVLTR, "complex/arabic-shaping/lamalef");
    addTest(root, testArabicShapingTashkeelSpecialVLTR, "complex/arabic-shaping/tashkeel");
    addTest(root, testArabicDeShapingLOGICAL, "complex/arabic-shaping/unshaping");
    addTest(root, testArabicShapingTailTest, "complex/arabic-shaping/tailtest");
    addTest(root, testArabicShapingForBug5421, "complex/arabic-shaping/bug-5421");
    addTest(root, testArabicShapingForBug8703, "complex/arabic-shaping/bug-8703");
    addTest(root, testArabicShapingForBug9024, "complex/arabic-shaping/bug-9024");
    addTest(root, testArabicShapingForNewCharacters, "complex/arabic-shaping/shaping2");
}

/*
 * BiDi -----------------------------------------------------------
 *
 */

#define RETURN_IF_BAD_ERRCODE(x)    \
    if (!U_SUCCESS(errorCode)) {      \
        log_err("\nbad errorCode %d at %s\n", errorCode, (x));  \
        return;     \
    }               \

static const char columns[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// complex/bidi/TestCharFromDirProp

static void
testCharFromDirProp(void) {
    /* verify that the exemplar characters have the expected bidi classes */
    int32_t i;

    log_verbose("testCharFromDirProp(): Started, %u test cases:\n", U_CHAR_DIRECTION_COUNT);

    initCharFromDirProps();

    for (i = 0; i < U_CHAR_DIRECTION_COUNT; ++i) {
        if (u_charDirection(charFromDirProp[i]) != (UCharDirection)i) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): (charFromDirProp[%d]=U+%04x)==%d!=%d\n",
                __FILE__, __LINE__, "u_charDirection", Encodings[UEncoding_U16].description, 0, "testCharFromDirProp",
                i, charFromDirProp[i], u_charDirection(charFromDirProp[i]), i);
        }
    }

    log_verbose("testCharFromDirProp(): Finished\n");
}

// complex/bidi/TestFailureRecovery

static void
testFailureRecovery(void) {
    UErrorCode errorCode = U_ZERO_ERROR;
    uint8_t u8BufSrc[MAXLEN];
    UText* ut = 0;

    UBiDi *pBiDi, *pLine;
    UChar src[MAXLEN];
    int32_t srcLen;
    UBiDiLevel level;
    UBiDiReorderingMode rm;
    static UBiDiLevel myLevels[3] = {6,5,4};
    int32_t encoding = 0;

    log_verbose("\nEntering TestFailureRecovery\n\n");

    pBiDi = ubidi_open();

    srcLen = u_unescape("abc", src, MAXLEN);
    errorCode = U_ZERO_ERROR;
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_DEFAULT_LTR - 1, NULL, &errorCode);
    if (U_SUCCESS(errorCode)) {
        log_err("\nubidi_setPara did not fail when passed too big para level\n");
    }

    pLine = ubidi_open();
    errorCode = U_ZERO_ERROR;
    ubidi_setLine(pBiDi, 0, 6, pLine, &errorCode);
    if (U_SUCCESS(errorCode)) {
        log_err("\nubidi_setLine did not fail when called before valid setPara()\n");
    }
    errorCode = U_ZERO_ERROR;
    srcLen = u_unescape("abc", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_LTR + 4, NULL, &errorCode);
    level = ubidi_getLevelAt(pBiDi, 3);
    if (level != 0) {
        log_err("\nubidi_getLevelAt did not fail when called with bad argument\n");
    }
    errorCode = U_ZERO_ERROR;
    ubidi_close(pBiDi);
    pBiDi = ubidi_openSized(-1, 0, &errorCode);
    if (U_SUCCESS(errorCode)) {
        log_err("\nubidi_openSized did not fail when called with bad argument\n");
    }
    ubidi_close(pBiDi);
    pBiDi = ubidi_openSized(2, 1, &errorCode);
    errorCode = U_ZERO_ERROR;
    srcLen = u_unescape("abc", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_LTR, NULL, &errorCode);
    if (U_SUCCESS(errorCode)) {
        log_err("\nsetPara did not fail when called with text too long\n");
    }
    errorCode = U_ZERO_ERROR;
    srcLen = u_unescape("=2", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    ubidi_countRuns(pBiDi, &errorCode);
    if (U_SUCCESS(errorCode)) {
        log_err("\nsetPara did not fail when called for too many runs\n");
    }
    ubidi_close(pBiDi);
    pBiDi = ubidi_open();
    rm = ubidi_getReorderingMode(pBiDi);
    ubidi_setReorderingMode(pBiDi, UBIDI_REORDER_DEFAULT - 1);
    if (rm != ubidi_getReorderingMode(pBiDi)) {
        log_err("\nsetReorderingMode with bad argument #1 should have no effect\n");
    }
    ubidi_setReorderingMode(pBiDi, 9999);
    if (rm != ubidi_getReorderingMode(pBiDi)) {
        log_err("\nsetReorderingMode with bad argument #2 should have no effect\n");
    }

    /* Try a surrogate char */
    errorCode = U_ZERO_ERROR;
    srcLen = u_unescape("\\uD800\\uDC00", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    if (ubidi_getDirection(pBiDi) != UBIDI_MIXED) {
        log_err("\ngetDirection for 1st surrogate char should be MIXED\n");
    }

    errorCode = U_ZERO_ERROR;
    srcLen = u_unescape("abc", src, MAXLEN);
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (!doSetPara(pBiDi, src, srcLen, 5, myLevels, sizeof(myLevels) / sizeof(UBiDiLevel), U_ILLEGAL_ARGUMENT_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            log_err("%s(%d): %s(%s): did not fail when called with bad levels\n",
                __FILE__, __LINE__, "ubidi_setPara", Encodings[encoding].description);
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, srcLen,
                UBIDI_DEFAULT_LTR, NULL,
                &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }
/*
    ubidi_setPara(pBiDi, src, srcLen, 5, myLevels, &errorCode);
    if (U_SUCCESS(errorCode)) {
        log_err("\nsetPara did not fail when called with bad levels\n");
    }
*/

    // ubidi_setContext

    // test proLength < -1
    errorCode = U_ZERO_ERROR;
    ubidi_setContext(pBiDi, NULL, -2, NULL, 0, &errorCode);
    assertIllegalArgument("Error when proLength < -1", &errorCode);

    // test epiLength < -1
    errorCode = U_ZERO_ERROR;
    ubidi_setContext(pBiDi, NULL, 0, NULL, -2, &errorCode);
    assertIllegalArgument("Error when epiLength < -1", &errorCode);

    // test prologue == NULL
    errorCode = U_ZERO_ERROR;
    ubidi_setContext(pBiDi, NULL, 3, NULL, 0, &errorCode);
    assertIllegalArgument("Prologue is NULL", &errorCode);

    // test epilogue == NULL
    errorCode = U_ZERO_ERROR;
    ubidi_setContext(pBiDi, NULL, 0, NULL, 4, &errorCode);
    assertIllegalArgument("Epilogue is NULL", &errorCode);

    ubidi_close(pBiDi);
    ubidi_close(pLine);

    //ubidi_writeReordered

    errorCode = U_FILE_ACCESS_ERROR;
    if (ubidi_writeReordered(NULL, NULL, 0, 0, &errorCode) != 0) {
        log_err("ubidi_writeReordered did not return 0 when passed a failing UErrorCode\n");
    }

    if (ubidi_writeUReordered(NULL, NULL, 0, &errorCode) != 0) {
        log_err("ubidi_writeUReordered did not return 0 when passed a failing UErrorCode\n");
    }

    if (ubidi_writeReverse(NULL, 0, NULL, 0, 0, &errorCode) != 0) {
        log_err("ubidi_writeReverse did not return 0 when passed a failing UErrorCode\n");
    }

    if (ubidi_writeUReverse(NULL, NULL, 0, &errorCode) != 0) {
        log_err("ubidi_writeUReverse did not return 0 when passed a failing UErrorCode\n");
    }

    errorCode = U_ZERO_ERROR;
    if (ubidi_writeReordered(NULL, NULL, 0, 0, &errorCode) != 0 || errorCode != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("ubidi_writeReordered did not fail as expected\n");
    }

    if (ubidi_writeUReordered(NULL, NULL, 0, &errorCode) != 0 || errorCode != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("ubidi_writeUReordered did not fail as expected\n");
    }

    // ubidi_writeReverse

    errorCode = U_ZERO_ERROR;
    if (ubidi_writeReverse(NULL, 0, NULL, 0, 0, &errorCode) != 0 || errorCode != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("ubidi_writeReverse did not fail as expected\n");
    }
    if (ubidi_writeUReverse(NULL, NULL, 0, &errorCode) != 0 || errorCode != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("ubidi_writeUReverse did not fail as expected\n");
    }

    log_verbose("\nExiting TestFailureRecovery\n\n");
}

// complex/bidi/TestBidi

static void
testBidi(void) {
    UErrorCode errorCode = U_ZERO_ERROR;

    log_verbose("testBidi(): Started\n");

    UBiDi *pBiDi = ubidi_openSized(MAXLEN, 0, &errorCode);
    if ((pBiDi == NULL) || (!U_SUCCESS(errorCode))) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory, error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testBidi",
        u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }
    else {
        UBiDi *pLine = ubidi_open();
        if (pLine == NULL) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
                __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "testBidi");

            return;
        }
        else {
            _testBidiDoTests(pBiDi, pLine, FALSE);
            _testBidiDoTests(pBiDi, pLine, TRUE);

            ubidi_close(pLine);
        }

        ubidi_close(pBiDi);
    }

    _testBidiDoMiscTest();

    log_verbose("testBidi(): Finished\n");
}

static void
_testBidiDoTests(UBiDi *pBiDi, UBiDi *pLine, UBool countRunsFirst) {
    UChar string[MAXLEN];
    UBiDiLevel paraLevel;
    int testNumber;

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        for (testNumber = 0; testNumber < bidiTestCount; ++testNumber) {
            getStringFromDirProps(tests[encoding][testNumber].text, tests[encoding][testNumber].length, string);
            paraLevel = tests[encoding][testNumber].paraLevel;

            _testBidiSetParaAndLine(pBiDi, pLine, string, -1, paraLevel, countRunsFirst, tests[encoding], testNumber, encoding);
        }
    }
}

static UBool
doSetPara(UBiDi *pBiDi,
    const UChar *src,
    int32_t srcLength,
    UBiDiLevel paraLevel,
    UBiDiLevel *embeddingLevels,
    int32_t embeddingLevelsLen,
    UErrorCode expectedError,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber,
    const Encoding encoding, UText **srcUt, uint8_t *srcBuffer, int32_t srcSize)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    if (srcUt)
        *srcUt = NULL;

    if (encoding.value == UEncoding_U16) {
        ubidi_setPara(pBiDi, src, srcLength,
            paraLevel, embeddingLevels,
            &errorCode);
    }
    else if (encoding.value == UEncoding_U8) {
        errorCode = U_ZERO_ERROR;
        UConverter *u8Convertor = ucnv_open("UTF8", &errorCode);

        if ((!U_SUCCESS(errorCode)) || (u8Convertor == NULL)) {
            return FALSE;
        }

        uint8_t* u8BufSrc = (uint8_t*)srcBuffer;

        memset(u8BufSrc, 0, srcSize);
        int32_t u16Len = srcLength;
        if (u16Len == (-1))
            u16Len = u_strlen(src);
        int32_t u8LenSrc = ucnv_fromUChars(u8Convertor, (char *)u8BufSrc, srcSize, (const UChar *)src, u16Len, &errorCode);

        ucnv_close(u8Convertor);

        *srcUt = utext_openConstU8(NULL, u8BufSrc, u8LenSrc, srcSize, &errorCode);

        ubidi_setUPara(pBiDi, *srcUt,
            paraLevel, embeddingLevels, embeddingLevelsLen,
            &errorCode);
    }
    else if (encoding.value == UEncoding_U32) {
        errorCode = U_ZERO_ERROR;
        UConverter *u32Convertor = ucnv_open("UTF32", &errorCode);

        if ((!U_SUCCESS(errorCode)) || (u32Convertor == NULL)) {
            return FALSE;
        }

        UChar32* u32BufSrc = (UChar32*)srcBuffer;

        memset(u32BufSrc, 0, srcSize);
        int32_t u16Len = srcLength;
        if (u16Len == (-1))
            u16Len = u_strlen(src);
        int32_t u32Len = (ucnv_fromUChars(u32Convertor, (char *)u32BufSrc, srcSize, (const UChar *)src, u16Len, &errorCode) / sizeof(UChar32)) - 1;

        ucnv_close(u32Convertor);

        *srcUt = utext_openConstU32(NULL, &u32BufSrc[1], u32Len, srcSize / sizeof(UChar32), &errorCode);

        ubidi_setUPara(pBiDi, *srcUt,
            paraLevel, embeddingLevels, embeddingLevelsLen,
            &errorCode);
    }

    if ((errorCode != U_STRING_NOT_TERMINATED_WARNING) && (errorCode != expectedError)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
            fileName, lineNumber, "ubidi_setPara", encoding.description, testNumber, "doSetPara",
            u_errorName(errorCode), u_errorName(expectedError));

        return FALSE;
    }

    return TRUE;
}

static void
_testBidiSetParaAndLine(UBiDi *pBiDi,
    UBiDi *pLine,
    const UChar *src, int32_t srcLength,
    UBiDiLevel paraLevel,
    UBool countRunsFirst,
    const BiDiTestData *testCases,
    int32_t testNumber,
    int32_t encoding)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    uint8_t u8BufSrc[MAXLEN];
    UText* ut = 0;

    if (doSetPara(pBiDi, src, srcLength, paraLevel, NULL, 0, U_ZERO_ERROR, testNumber, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN))
    {
        log_verbose("ubidi_setPara(%s, tests[%d], paraLevel %d) ok, direction %d paraLevel=%d\n", 
            Encodings[encoding].description, testNumber, paraLevel,
            ubidi_getDirection(pBiDi), paraLevel);

        errorCode = U_ZERO_ERROR;
        int32_t lineStart = testCases[testNumber].lineStart;

        if (lineStart == -1) {
            _testBidiDoTest(pBiDi, 0, countRunsFirst, &testCases[testNumber], testNumber, encoding);
        }
        else {
            ubidi_setLine(pBiDi, lineStart, testCases[testNumber].lineLimit, pLine, &errorCode);

            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_setLine", Encodings[encoding].description, testNumber, "_testBidiSetParaAndLine",
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }
            else {
                log_verbose("ubidi_setLine(%s, tests[%d], %d) ok, direction %d paraLevel=%d\n",
                    Encodings[encoding].description, lineStart, testCases[testNumber].lineLimit,
                    ubidi_getDirection(pLine), ubidi_getParaLevel(pLine));

                _testBidiDoTest(pLine, lineStart, countRunsFirst, &testCases[testNumber], testNumber, encoding);
            }
        }
    }

    if (utext_isValid(ut)) {
        // Reset without a UText before closing since we can't call ubidi_close().

        ubidi_setPara(pBiDi, src, srcLength,
            paraLevel, NULL,
            &errorCode);

        ubidi_setLine(pBiDi, 0, 0, pLine, &errorCode);

        utext_close(ut);
        ut = 0;
    }
}

static void
_testBidiDoTest(UBiDi *pBiDi, 
    int32_t lineStart,
    UBool countRunsFirst,
    const BiDiTestData *test,
    int testNumber,
    int32_t encoding)
{
    const uint8_t *dirProps=test->text+lineStart;
    const UBiDiLevel *levels=test->levels;
    const uint8_t *visualMap=test->visualMap;
    int32_t i, len=ubidi_getLength(pBiDi), logicalIndex, runCount = 0;
    UErrorCode errorCode=U_ZERO_ERROR;
    UBiDiLevel level, level2;

    if (countRunsFirst) {
        log_verbose("Calling ubidi_countRuns() first.\n");

        runCount = ubidi_countRuns(pBiDi, &errorCode);
        if(!U_SUCCESS(errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                __FILE__, __LINE__, "ubidi_countRuns", Encodings[encoding].description, testNumber, "_testBidiDoTest",
                u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

            return;
        }
    } else {
        log_verbose("Calling ubidi_getLogicalMap() first.\n");
    }

    _testBidiDoTestReordering(pBiDi, testNumber, encoding);

    for(i=0; i<len; ++i) {
        log_verbose("%3d %3d %.*s%-3s @%d\n",
                i, ubidi_getLevelAt(pBiDi, i), ubidi_getLevelAt(pBiDi, i), levelString,
                dirPropNames[dirProps[i]],
                ubidi_getVisualIndex(pBiDi, i, &errorCode));
    }

    log_verbose("\n-----levels:");
    for(i=0; i<len; ++i) {
        if(i>0) {
            log_verbose(",");
        }
        log_verbose(" %d", ubidi_getLevelAt(pBiDi, i));
    }

    log_verbose("\n--reordered:");
    for(i=0; i<len; ++i) {
        if(i>0) {
            log_verbose(",");
        }
        log_verbose(" %d", ubidi_getVisualIndex(pBiDi, i, &errorCode));
    }
    log_verbose("\n");

    if(test->direction != ubidi_getDirection(pBiDi)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): wrong direction %d (expected %d)\n",
            __FILE__, __LINE__, "ubidi_getDirection", Encodings[encoding].description, testNumber, "_testBidiDoTest",
            ubidi_getDirection(pBiDi), test->direction);
    }

    if(test->resultLevel != ubidi_getParaLevel(pBiDi)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): wrong paragraph level %d (expected %d)\n",
            __FILE__, __LINE__, "ubidi_getParaLevel", Encodings[encoding].description, testNumber, "_testBidiDoTest",
            ubidi_getParaLevel(pBiDi), test->resultLevel);
    }

    for(i=0; i<len; ++i) {
        if(levels[i]!=ubidi_getLevelAt(pBiDi, i)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): index %d, wrong level %d (expected %d)\n",
                __FILE__, __LINE__, "ubidi_getLevelAt", Encodings[encoding].description, testNumber, "_testBidiDoTest",
                i, ubidi_getLevelAt(pBiDi, i), levels[i]);

            return;
        }
    }

    for(i=0; i<len; ++i) {
        logicalIndex=ubidi_getVisualIndex(pBiDi, i, &errorCode);

        if(!U_SUCCESS(errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): index %d, error code %s (expected %s)\n"
                __FILE__, __LINE__, "ubidi_getVisualIndex", Encodings[encoding].description, testNumber, "_testBidiDoTest",
                i, ubidi_getLevelAt(pBiDi, i), levels[i]);

            return;
        }

        if(visualMap[i]!=logicalIndex) {
            int32_t v[MAXLEN];
            memset(v, 0, MAXLEN * sizeof(int32_t));
            ubidi_getLogicalMap(pBiDi, &v, &errorCode);

            char display[MAXLEN];
            memset(display, 0, MAXLEN);

            strcat(display, "{ ");
            for (int32_t j = 0; j < len; j++) {
                if (j > 0)
                    strcat(display, ", ");
                sprintf(&display[strlen(display)], "%d", v[j]);
            }
            strcat(display, " }\n");
            log_err("\n%s\n", display);

            log_err("%s(%d): %s(%s, tests[%d]: %s): index %d, wrong index %d (expected %d)\n",
                __FILE__, __LINE__, "ubidi_getVisualIndex", Encodings[encoding].description, testNumber, "_testBidiDoTest",
                i, logicalIndex, visualMap[i]);

            return;
        }
    }

    if (! countRunsFirst) {
        runCount=ubidi_countRuns(pBiDi, &errorCode);
        if(!U_SUCCESS(errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                __FILE__, __LINE__, "ubidi_countRuns", Encodings[encoding].description, testNumber, "_testBidiDoTest",
                u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

            return;
        }
    }

    for(logicalIndex=0; logicalIndex<len;) {
        level=ubidi_getLevelAt(pBiDi, logicalIndex);
        ubidi_getLogicalRun(pBiDi, logicalIndex, &logicalIndex, &level2);

        if(level!=level2) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): run ending at index %d, wrong level %d (expected %d)\n",
                __FILE__, __LINE__, "ubidi_getLogicalRun", Encodings[encoding].description, testNumber, "_testBidiDoTest",
                logicalIndex, level, level2);
        }

        if(--runCount<0) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): wrong number of runs compared to %d=ubidi_countRuns()\n",
                __FILE__, __LINE__, "ubidi_getLogicalRun", Encodings[encoding].description, testNumber, "_testBidiDoTest",
                ubidi_countRuns(pBiDi, &errorCode));

            return;
        }
    }

    if(runCount!=0) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): wrong number of runs compared to %d=ubidi_countRuns()\n",
            __FILE__, __LINE__, "ubidi_getLogicalRun", Encodings[encoding].description, testNumber, "_testBidiDoTest",
            ubidi_countRuns(pBiDi, &errorCode));

        return;
    }

    log_verbose("\n\n");
}

static void
_testBidiDoTestReordering(UBiDi *pBiDi, 
    int testNumber,
    int32_t encoding)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t
        logicalMap1[MAXLEN], logicalMap2[MAXLEN], logicalMap3[MAXLEN],
        visualMap1[MAXLEN], visualMap2[MAXLEN], visualMap3[MAXLEN], visualMap4[MAXLEN];
    const UBiDiLevel *levels;
    int32_t i, length=ubidi_getLength(pBiDi),
               destLength=ubidi_getResultLength(pBiDi);
    int32_t runCount, visualIndex, logicalStart, runLength;
    UBool odd;

    if(length<=0) {
        return;
    }

    /* get the logical and visual maps from the object */
    ubidi_getLogicalMap(pBiDi, logicalMap1, &errorCode);
    if(!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_getLogicalMap", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }

    ubidi_getVisualMap(pBiDi, visualMap1, &errorCode);
    if(!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_getVisualMap", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }

    /* invert them both */
    ubidi_invertMap(logicalMap1, visualMap2, length);
    ubidi_invertMap(visualMap1, logicalMap2, destLength);

    /* get them from the levels array, too */
    levels=ubidi_getLevels(pBiDi, &errorCode);
    if(!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }

    ubidi_reorderLogical(levels, length, logicalMap3);
    ubidi_reorderVisual(levels, length, visualMap3);

    /* get the visual map from the runs, too */
    runCount=ubidi_countRuns(pBiDi, &errorCode);
    if(!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_countRuns", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }
    log_verbose("\n----%2d runs:", runCount);

    visualIndex=0;
    for(i=0; i<runCount; ++i) {
        odd=(UBool)ubidi_getVisualRun(pBiDi, i, &logicalStart, &runLength);
        log_verbose(" (%c @%d[%d])", odd ? 'R' : 'L', logicalStart, runLength);
        if(UBIDI_LTR==odd) {
            do { /* LTR */
                visualMap4[visualIndex++]=logicalStart++;
            } while(--runLength>0);
        } else {
            logicalStart+=runLength;   /* logicalLimit */
            do { /* RTL */
                visualMap4[visualIndex++]=--logicalStart;
            } while(--runLength>0);
        }
    }
    log_verbose("\n");

    /* print all the maps */
    log_verbose("logical maps:\n");
    for(i=0; i<length; ++i) {
        log_verbose("%4d", logicalMap1[i]);
    }
    log_verbose("\n");
    for(i=0; i<length; ++i) {
        log_verbose("%4d", logicalMap2[i]);
    }
    log_verbose("\n");
    for(i=0; i<length; ++i) {
        log_verbose("%4d", logicalMap3[i]);
    }

    log_verbose("\nvisual maps:\n");
    for(i=0; i<destLength; ++i) {
        log_verbose("%4d", visualMap1[i]);
    }
    log_verbose("\n");
    for(i=0; i<destLength; ++i) {
        log_verbose("%4d", visualMap2[i]);
    }
    log_verbose("\n");
    for(i=0; i<length; ++i) {
        log_verbose("%4d", visualMap3[i]);
    }
    log_verbose("\n");
    for(i=0; i<length; ++i) {
        log_verbose("%4d", visualMap4[i]);
    }
    log_verbose("\n");

    /* check that the indexes are the same between these and ubidi_getLogical/VisualIndex() */
    for(i=0; i<length; ++i) {
        if(logicalMap1[i]!=logicalMap2[i]) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): bidi reordering error, logicalMap1[i]!=logicalMap2[i] at i=%d\n",
                __FILE__, __LINE__, "ubidi_getLogicalMap", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                i);

            break;
        }

        if(logicalMap1[i]!=logicalMap3[i]) {
            if (Encodings[encoding].value != UEncoding_U16)
                log_knownIssue("99999", "ICU-99999 ubidi_reorderLogical() only supports UTF16");
            else {
                log_err("%s(%d): %s(%s, tests[%d]: %s): bidi reordering error, logicalMap1[i]!=logicalMap3[i] at i=%d\n",
                    __FILE__, __LINE__, "ubidi_getLogicalMap", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                    i);
            }

            break;
        }

        if(visualMap1[i]!=visualMap2[i]) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): bidi reordering error, visualMap1[i]!=visualMap2[i] at i=%d\n",
                __FILE__, __LINE__, "ubidi_getVisuallMap", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                i);

            break;
        }

        if(visualMap1[i]!=visualMap3[i]) {
            if (Encodings[encoding].value != UEncoding_U16)
                log_knownIssue("99999", "ICU-99999 ubidi_reorderVisual() only supports UTF16");
            else {
                log_err("%s(%d): %s(%s, tests[%d]: %s): bidi reordering error, visualMap1[i]!=visualMap3[i] at i=%d\n",
                    __FILE__, __LINE__, "ubidi_getVisuallMap", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                    i);
            }
            break;
        }

        if(visualMap1[i]!=visualMap4[i]) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): bidi reordering error, visualMap1[i]!=visualMap4[i] at i=%d\n",
                __FILE__, __LINE__, "ubidi_getVisuallMap", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                i);

            break;
        }

        if(logicalMap1[i]!=ubidi_getVisualIndex(pBiDi, i, &errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): bidi reordering error, logicalMap1[i]!=ubidi_getVisualIndex(i) at i=%d\n",
                __FILE__, __LINE__, "ubidi_getVisualIndex", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                i);

            break;
        }

        if(!U_SUCCESS(errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                __FILE__, __LINE__, "ubidi_getVisualIndex", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

            break;
        }

        if(visualMap1[i]!=ubidi_getLogicalIndex(pBiDi, i, &errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): bidi reordering error, visualMap1[i]!=ubidi_getLogicalIndex(i) at i=%d\n",
                __FILE__, __LINE__, "ubidi_getLogicalIndex", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                i);

            break;
        }

        if(!U_SUCCESS(errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                __FILE__, __LINE__, "ubidi_getLogicalIndex", Encodings[encoding].description, testNumber, "_testBidiDoTestReordering",
                u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

            break;
        }
    }
}

/* Miscellaneous tests to exercize less popular code paths */
static void _testBidiDoMiscTest(void)
{
    int32_t srcLen, destLen, runCount, i;
    UBiDiLevel level;
    UBiDiDirection dir;
    int32_t map[MAXLEN];
    UErrorCode errorCode=U_ZERO_ERROR;
    static const int32_t srcMap[6] = {0,1,-1,5,4};
    static const int32_t dstMap[6] = {0,1,-1,-1,4,3};

    UBiDi *pBiDi = ubidi_openSized(120, 66, &errorCode);
    if ((pBiDi == NULL) || (!U_SUCCESS(errorCode))) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory, error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "_testBidiDoMiscTest",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }

    UBiDi *pLine = ubidi_open();
    if (pLine == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "_testBidiDoMiscTest");

        return;
    }

    static UChar src[MAXLEN];
    static UChar dest[MAXLEN];

    {
        UBidiWriteReverseTestCases testCases[] = {
            { src,
                0,
                dest,
                MAXLEN,
                0,
                U_ZERO_ERROR,
                0,
                0,
                "srcLength==0" },
        };

        destLen = doWriteReverse(testCases, 0, __FILE__, __LINE__);

        if (destLen != 0) {
            log_err("\nwriteReverse should return zero length, ",
                    "returned %d instead\n", destLen);
        }
    }
    RETURN_IF_BAD_ERRCODE("#1#");

    ubidi_setPara(pBiDi, src, 0, UBIDI_LTR, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    if (destLen != 0) {
        log_err("\nwriteReordered should return zero length, ",
                "returned %d instead\n", destLen);
    }
    RETURN_IF_BAD_ERRCODE("#2#");

    srcLen = u_unescape("abc       ", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    ubidi_setLine(pBiDi, 0, 6, pLine, &errorCode);
    for (i = 3; i < 6; i++) {
        level = ubidi_getLevelAt(pLine, i);
        if (level != UBIDI_RTL) {
            log_err("\nTrailing space at index %d should get paragraph level"
                    "%d, got %d instead\n", i, UBIDI_RTL, level);
        }
    }
    RETURN_IF_BAD_ERRCODE("#3#");

    srcLen = u_unescape("abc       def", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    ubidi_setLine(pBiDi, 0, 6, pLine, &errorCode);
    for (i = 3; i < 6; i++) {
        level = ubidi_getLevelAt(pLine, i);
        if (level != UBIDI_RTL) {
            log_err("\nTrailing space at index %d should get paragraph level"
                    "%d, got %d instead\n", i, UBIDI_RTL, level);
        }
    }
    RETURN_IF_BAD_ERRCODE("#4#");

    srcLen = u_unescape("abcdefghi    ", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    ubidi_setLine(pBiDi, 0, 6, pLine, &errorCode);
    for (i = 3; i < 6; i++) {
        level = ubidi_getLevelAt(pLine, i);
        if (level != 2) {
            log_err("\nTrailing char at index %d should get level 2, "
                    "got %d instead\n", i, level);
        }
    }
    RETURN_IF_BAD_ERRCODE("#5#");

    ubidi_setReorderingOptions(pBiDi, UBIDI_OPTION_REMOVE_CONTROLS);
    srcLen = u_unescape("\\u200eabc       def", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    ubidi_setLine(pBiDi, 0, 6, pLine, &errorCode);
    destLen = ubidi_getResultLength(pLine);
    if (destLen != 5) {
        log_err("\nWrong result length, should be 5, got %d\n", destLen);
    }
    RETURN_IF_BAD_ERRCODE("#6#");

    srcLen = u_unescape("abcdefghi", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_LTR, NULL, &errorCode);
    ubidi_setLine(pBiDi, 0, 6, pLine, &errorCode);
    dir = ubidi_getDirection(pLine);
    if (dir != UBIDI_LTR) {
        log_err("\nWrong direction #1, should be %d, got %d\n",
                UBIDI_LTR, dir);
    }
    RETURN_IF_BAD_ERRCODE("#7#");

    ubidi_setPara(pBiDi, src, 0, UBIDI_LTR, NULL, &errorCode);
    runCount = ubidi_countRuns(pBiDi, &errorCode);
    if (runCount != 0) {
        log_err("\nWrong number of runs #1, should be 0, got %d\n", runCount);
    }
    RETURN_IF_BAD_ERRCODE("#8#");

    srcLen = u_unescape("          ", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    ubidi_setLine(pBiDi, 0, 6, pLine, &errorCode);
    runCount = ubidi_countRuns(pLine, &errorCode);
    if (runCount != 1) {
        log_err("\nWrong number of runs #2, should be 1, got %d\n", runCount);
    }
    RETURN_IF_BAD_ERRCODE("#9#");

    srcLen = u_unescape("a\\u05d0        bc", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    ubidi_setLine(pBiDi, 0, 6, pLine, &errorCode);
    dir = ubidi_getDirection(pBiDi);
    if (dir != UBIDI_MIXED) {
        log_err("\nWrong direction #2, should be %d, got %d\n",
                UBIDI_MIXED, dir);
    }
    dir = ubidi_getDirection(pLine);
    if (dir != UBIDI_MIXED) {
        log_err("\nWrong direction #3, should be %d, got %d\n",
                UBIDI_MIXED, dir);
    }
    runCount = ubidi_countRuns(pLine, &errorCode);
    if (runCount != 2) {
        log_err("\nWrong number of runs #3, should be 2, got %d\n", runCount);
    }
    RETURN_IF_BAD_ERRCODE("#10#");

    ubidi_invertMap(srcMap, map, 5);
    if (memcmp(dstMap, map, sizeof(dstMap))) {
        log_err("\nUnexpected inverted Map, got ");
        for (i = 0; i < 6; i++) {
            log_err("%d ", map[i]);
        }
        log_err("\n");
    }

    /* test REMOVE_BIDI_CONTROLS together with DO_MIRRORING */
    srcLen = u_unescape("abc\\u200e", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_LTR, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN,
              UBIDI_REMOVE_BIDI_CONTROLS | UBIDI_DO_MIRRORING, &errorCode);
    if (destLen != 3 || memcmp(dest, src, 3 * sizeof(UChar))) {
        log_err("\nWrong result #1, should be 'abc', got '%s'\n",
                aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#11#");

    /* test inverse Bidi with marks and contextual orientation */
    ubidi_setReorderingMode(pBiDi, UBIDI_REORDER_INVERSE_LIKE_DIRECT);
    ubidi_setReorderingOptions(pBiDi, UBIDI_OPTION_INSERT_MARKS);
    ubidi_setPara(pBiDi, src, 0, UBIDI_DEFAULT_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    if (destLen != 0) {
        log_err("\nWrong result #2, length should be 0, got %d\n", destLen);
    }
    RETURN_IF_BAD_ERRCODE("#12#");
    srcLen = u_unescape("   ", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_DEFAULT_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    if (destLen != 3 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #3, should be '   ', got '%s'\n",
                aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#13#");
    srcLen = u_unescape("abc", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_DEFAULT_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    if (destLen != 3 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #4, should be 'abc', got '%s'\n",
                aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#14#");
    srcLen = u_unescape("\\u05d0\\u05d1", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_DEFAULT_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\\u05d1\\u05d0", src, MAXLEN);
    if (destLen != 2 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #5, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#15#");
    srcLen = u_unescape("abc \\u05d0\\u05d1", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_DEFAULT_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\\u05d1\\u05d0 abc", src, MAXLEN);
    if (destLen != 6 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #6, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#16#");
    srcLen = u_unescape("\\u05d0\\u05d1 abc", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_DEFAULT_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\\u200fabc \\u05d1\\u05d0", src, MAXLEN);
    if (destLen != 7 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #7, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#17#");
    srcLen = u_unescape("\\u05d0\\u05d1 abc .-=", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_DEFAULT_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\\u200f=-. abc \\u05d1\\u05d0", src, MAXLEN);
    if (destLen != 11 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #8, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#18#");
    ubidi_orderParagraphsLTR(pBiDi, TRUE);
    srcLen = u_unescape("\n\r   \n\rabc\n\\u05d0\\u05d1\rabc \\u05d2\\u05d3\n\r"
                        "\\u05d4\\u05d5 abc\n\\u05d6\\u05d7 abc .-=\r\n"
                        "-* \\u05d8\\u05d9 abc .-=", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_DEFAULT_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\n\r   \n\rabc\n\\u05d1\\u05d0\r\\u05d3\\u05d2 abc\n\r"
                        "\\u200fabc \\u05d5\\u05d4\n\\u200f=-. abc \\u05d7\\u05d6\r\n"
                        "\\u200f=-. abc \\u05d9\\u05d8 *-", src, MAXLEN);
    if (destLen != 57 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #9, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#19#");
    srcLen = u_unescape("\\u05d0 \t", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_LTR, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\\u05D0\\u200e \t", src, MAXLEN);
    if (destLen != 4 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #10, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#20#");
    srcLen = u_unescape("\\u05d0 123 \t\\u05d1 123 \\u05d2", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_LTR, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\\u05d0 \\u200e123\\u200e \t\\u05d2 123 \\u05d1", src, MAXLEN);
    if (destLen != 16 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #11, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#21#");
    srcLen = u_unescape("\\u05d0 123 \\u0660\\u0661 ab", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_LTR, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\\u05d0 \\u200e123 \\u200e\\u0660\\u0661 ab", src, MAXLEN);
    if (destLen != 13 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #12, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#22#");
    srcLen = u_unescape("ab \t", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_RTL, NULL, &errorCode);
    destLen = ubidi_writeReordered(pBiDi, dest, MAXLEN, 0, &errorCode);
    srcLen = u_unescape("\\u200f\t ab", src, MAXLEN);
    if (destLen != 5 || memcmp(dest, src, destLen * sizeof(UChar))) {
        log_err("\nWrong result #13, should be '%s', got '%s'\n",
                aescstrdup(src, srcLen), aescstrdup(dest, destLen));
    }
    RETURN_IF_BAD_ERRCODE("#23#");

    /* check exceeding para level */
    ubidi_close(pBiDi);
    pBiDi = ubidi_open();
    srcLen = u_unescape("A\\u202a\\u05d0\\u202aC\\u202c\\u05d1\\u202cE", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_MAX_EXPLICIT_LEVEL - 1, NULL, &errorCode);
    level = ubidi_getLevelAt(pBiDi, 2);
    if (level != UBIDI_MAX_EXPLICIT_LEVEL) {
        log_err("\nWrong level at index 2\n, should be %d, got %d\n", UBIDI_MAX_EXPLICIT_LEVEL, level);
    }
    RETURN_IF_BAD_ERRCODE("#24#");

    /* check 1-char runs with RUNS_ONLY */
    ubidi_setReorderingMode(pBiDi, UBIDI_REORDER_RUNS_ONLY);
    srcLen = u_unescape("a \\u05d0 b \\u05d1 c \\u05d2 d ", src, MAXLEN);
    ubidi_setPara(pBiDi, src, srcLen, UBIDI_LTR, NULL, &errorCode);
    runCount = ubidi_countRuns(pBiDi, &errorCode);
    if (runCount != 14) {
        log_err("\nWrong number of runs #3, should be 14, got %d\n", runCount);
    }
    RETURN_IF_BAD_ERRCODE("#25#");

    ubidi_close(pBiDi);
    ubidi_close(pLine);
}

// complex/bidi/TestInverse

static int countRoundtrips=0, countNonRoundtrips=0;

#define STRING_TEST_CASE(s) { (s), UPRV_LENGTHOF(s) }

static void
testInverse(void) {
    static const UChar
        string0[]={ 0x6c, 0x61, 0x28, 0x74, 0x69, 0x6e, 0x20, 0x5d0, 0x5d1, 0x29, 0x5d2, 0x5d3 },
        string1[]={ 0x6c, 0x61, 0x74, 0x20, 0x5d0, 0x5d1, 0x5d2, 0x20, 0x31, 0x32, 0x33 },
        string2[]={ 0x6c, 0x61, 0x74, 0x20, 0x5d0, 0x28, 0x5d1, 0x5d2, 0x20, 0x31, 0x29, 0x32, 0x33 },
        string3[]={ 0x31, 0x32, 0x33, 0x20, 0x5d0, 0x5d1, 0x5d2, 0x20, 0x34, 0x35, 0x36 },
        string4[]={ 0x61, 0x62, 0x20, 0x61, 0x62, 0x20, 0x661, 0x662 };

    static const struct {
        const UChar *s;
        int32_t length;
    } testCases[]={
        STRING_TEST_CASE(string0),
        STRING_TEST_CASE(string1),
        STRING_TEST_CASE(string2),
        STRING_TEST_CASE(string3),
        STRING_TEST_CASE(string4)
    };

    UErrorCode errorCode;
    int i;

    log_verbose("testInverse(): Started\n");

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testInverse");

        return;
    }

    log_verbose("  testInverse(L): %u test cases:\n", UPRV_LENGTHOF(testCases));
     for(i=0; i<UPRV_LENGTHOF(testCases); ++i) {
        log_verbose("  Testing case %d\n", i);
        errorCode=U_ZERO_ERROR;
        _testInverseBidi(pBiDi, testCases[i].s, testCases[i].length, 0);
    }

     log_verbose("  testInverse(R): %u test cases:\n", UPRV_LENGTHOF(testCases));
    for(i=0; i<UPRV_LENGTHOF(testCases); ++i) {
        log_verbose("  Testing case %d\n", i);
        errorCode=U_ZERO_ERROR;
        _testInverseBidi(pBiDi, testCases[i].s, testCases[i].length, 1);
    }

    _testInverseBidiManyTest(pBiDi, 0);
    _testInverseBidiManyTest(pBiDi, 1);

    ubidi_close(pBiDi);

    log_verbose("  inverse Bidi: rountrips: %5u\nnon-roundtrips: %5u\n", countRoundtrips, countNonRoundtrips);

    _testInverseBidiReverseTest();

    _testInverseBidiManyAddedPointsTest();

    _testInverseBidiMisc();

    log_verbose("testInverse(L): Finished\n");
}

#define COUNT_REPEAT_SEGMENTS 6

static const UChar repeatSegments[COUNT_REPEAT_SEGMENTS][2]={
    { 0x61, 0x62 },     /* L */
    { 0x5d0, 0x5d1 },   /* R */
    { 0x627, 0x628 },   /* AL */
    { 0x31, 0x32 },     /* EN */
    { 0x661, 0x662 },   /* AN */
    { 0x20, 0x20 }      /* WS (N) */
};

static void
_testInverseBidiManyTest(UBiDi *pBiDi, UBiDiLevel direction) {
    UChar text[8]={ 0, 0, 0x20, 0, 0, 0x20, 0, 0 };
    int i, j, k;

    log_verbose("testManyInverseBidi(%c): Test permutations of text snippets ---\n", direction==0 ? 'L' : 'R');

    for(i=0; i<COUNT_REPEAT_SEGMENTS; ++i) {
        text[0]=repeatSegments[i][0];
        text[1]=repeatSegments[i][1];
        for(j=0; j<COUNT_REPEAT_SEGMENTS; ++j) {
            text[3]=repeatSegments[j][0];
            text[4]=repeatSegments[j][1];
            for(k=0; k<COUNT_REPEAT_SEGMENTS; ++k) {
                text[6]=repeatSegments[k][0];
                text[7]=repeatSegments[k][1];

                log_verbose("inverse Bidi: testManyInverseBidi()[%u %u %u]\n", i, j, k);
                _testInverseBidi(pBiDi, text, 8, direction);
            }
        }
    }
}

static void
_testInverseBidi(UBiDi *pBiDi, 
    const UChar *srcText, int32_t srcLength,
    UBiDiLevel direction)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t ltrLength = 0, logicalLength = 0, visualLength = 0;

    static UChar src[MAXLEN];
    static UChar visualLTR[MAXLEN];
    static UChar logicalDest[MAXLEN];
    static UChar visualDest[MAXLEN];

    u_memset(src, 0, MAXLEN);
    u_memset(visualLTR, 0, MAXLEN);
    u_memset(logicalDest, 0, MAXLEN);
    u_memset(visualDest, 0, MAXLEN);

    u_strncpy(src, srcText, srcLength);

    if(direction==0) {
        log_verbose("inverse Bidi: testInverse(L)\n");

        /* convert visual to logical */
        ubidi_setInverse(pBiDi, TRUE);
        if (!ubidi_isInverse(pBiDi)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): Error while doing ubidi_setInverse(TRUE)\n",
                __FILE__, __LINE__, "ubidi_getLogicalIndex", Encodings[UEncoding_U16].description, 0, "_testInverseBidi");
        }

        {
            UBidiWriteReorderedTestCases testCases[] = {
                { (const char*)src,
                    0,
                    0,
                    NULL,
                    logicalDest,
                    UPRV_LENGTHOF(logicalDest),
                    UBIDI_DO_MIRRORING | UBIDI_INSERT_LRM_FOR_NUMERIC,
                    U_ZERO_ERROR,
                    0,
                    UInputType_UChar,
                    -1,
                    "InverseBidi UBIDI_DO_MIRRORING | UBIDI_INSERT_LRM_FOR_NUMERIC",
                    FALSE },
                };
            testCases[0].sourceLength = srcLength;

            logicalLength = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);
        }

        log_verbose("  v ");
        printUnicode(src, srcLength, ubidi_getLevels(pBiDi, &errorCode));
        log_verbose("\n");

        // Convert back to visual LTR
        ubidi_setInverse(pBiDi, FALSE);
        if (ubidi_isInverse(pBiDi)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): Error while doing ubidi_setInverse(FALSE)\n",
                __FILE__, __LINE__, "ubidi_getLogicalIndex", Encodings[UEncoding_U16].description, 0, "_testInverseBidi");
        }

        {
            UBidiWriteReorderedTestCases testCases[] = {
                { (const char*)logicalDest,
                    0,
                    0,
                    NULL,
                    visualDest,
                    UPRV_LENGTHOF(visualDest),
                    UBIDI_DO_MIRRORING | UBIDI_REMOVE_BIDI_CONTROLS,
                    U_ZERO_ERROR,
                    0,
                    UInputType_UChar,
                    -1,
                    "InverseBidi UBIDI_DO_MIRRORING | UBIDI_REMOVE_BIDI_CONTROLS",
                    FALSE },
                };
            testCases[0].sourceLength = logicalLength;

            visualLength = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);
        }
    } else {
        log_verbose("inverse Bidi: testInverse(R)\n");

        {
            // Reverse visual from RTL to LTR
            UBidiWriteReverseTestCases testCases[] = {
                { src,
                    0,
                    visualLTR,
                    UPRV_LENGTHOF(visualLTR),
                    0,
                    U_ZERO_ERROR,
                    0,
                    0,
                    "inverse Bidi" },
            };
            testCases[0].sourceLength = srcLength;
            testCases[0].expectedLength = srcLength;

            ltrLength = doWriteReverse(testCases, 0, __FILE__, __LINE__);
        }

        log_verbose("  vr");
        printUnicode(src, srcLength, NULL);
        log_verbose("\n");

        // Convert visual RTL to logical
        ubidi_setInverse(pBiDi, TRUE);
        if (!ubidi_isInverse(pBiDi)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): Error while doing ubidi_setInverse(TRUE)\n",
                __FILE__, __LINE__, "ubidi_getLogicalIndex", Encodings[UEncoding_U16].description, 0, "_testInverseBidi");
        }

        {
            UBidiWriteReorderedTestCases testCases[] = {
                { (const char*)visualLTR,
                    0,
                    0,
                    NULL,
                    logicalDest,
                    UPRV_LENGTHOF(logicalDest),
                    UBIDI_DO_MIRRORING | UBIDI_INSERT_LRM_FOR_NUMERIC,
                    U_ZERO_ERROR,
                    0,
                    UInputType_UChar,
                    -1,
                    "InverseBidi UBIDI_DO_MIRRORING | UBIDI_INSERT_LRM_FOR_NUMERIC",
                    FALSE },
                };
            testCases[0].sourceLength = ltrLength;

            logicalLength = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);
        }

        log_verbose("  vl");
        printUnicode(visualLTR, ltrLength, ubidi_getLevels(pBiDi, &errorCode));
        log_verbose("\n");

        // Convert back to visual RTL
        ubidi_setInverse(pBiDi, FALSE);
        if (ubidi_isInverse(pBiDi)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): Error while doing ubidi_setInverse(FALSE)\n",
                __FILE__, __LINE__, "ubidi_getLogicalIndex", Encodings[UEncoding_U16].description, 0, "_testInverseBidi");
        }

        {
            UBidiWriteReorderedTestCases testCases[] = {
                { (const char*)logicalDest,
                    0,
                    0,
                    NULL,
                    visualDest,
                    UPRV_LENGTHOF(visualDest),
                    UBIDI_DO_MIRRORING | UBIDI_REMOVE_BIDI_CONTROLS | UBIDI_OUTPUT_REVERSE,
                    U_ZERO_ERROR,
                    0,
                    UInputType_UChar,
                    -1,
                    "InverseBidi UBIDI_DO_MIRRORING | UBIDI_REMOVE_BIDI_CONTROLS | UBIDI_OUTPUT_REVERSE",
                    FALSE },
                };
            testCases[0].sourceLength = logicalLength;

            visualLength = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);
        }
    }

    log_verbose("  l ");
    printUnicode(logicalDest, logicalLength, ubidi_getLevels(pBiDi, &errorCode));
    log_verbose("\n");
    log_verbose("  v ");
    printUnicode(visualDest, visualLength, NULL);
    log_verbose("\n");

    if(srcLength==visualLength && memcmp(src, visualDest, srcLength*U_SIZEOF_UCHAR)==0) {
        ++countRoundtrips;
        log_verbose(" + roundtripped\n");
    } else {
        ++countNonRoundtrips;
        log_verbose(" * did not roundtrip\n");
        log_err("inverse BiDi: transformation visual->logical->visual did not roundtrip the text;\n"
                "                 turn on verbose mode to see details\n");
    }
}

static void
_testInverseBidiReverseTest(void) {
    /* U+064e and U+0650 are combining marks (Mn) */
    static const UChar forward[]={
        0x200f, 0x627, 0x64e, 0x650, 0x20, 0x28, 0x31, 0x29
    }, reverseKeepCombining[]={
        0x29, 0x31, 0x28, 0x20, 0x627, 0x64e, 0x650, 0x200f
    }, reverseRemoveControlsKeepCombiningDoMirror[]={
        0x28, 0x31, 0x29, 0x20, 0x627, 0x64e, 0x650
    };

    static UChar reverse[MAXLEN];

    static UBidiWriteReverseTestCases testCases[] = {
            { forward,
                UPRV_LENGTHOF(forward),
                reverse,
                UPRV_LENGTHOF(reverse),
                UBIDI_KEEP_BASE_COMBINING,
                U_ZERO_ERROR,
                reverseKeepCombining,
                UPRV_LENGTHOF(reverseKeepCombining),
                "UBIDI_KEEP_BASE_COMBINING" },
            { forward,
                UPRV_LENGTHOF(forward),
                reverse,
                UPRV_LENGTHOF(reverse),
                UBIDI_REMOVE_BIDI_CONTROLS | UBIDI_DO_MIRRORING | UBIDI_KEEP_BASE_COMBINING,
                U_ZERO_ERROR,
                reverseRemoveControlsKeepCombiningDoMirror,
                UPRV_LENGTHOF(reverseRemoveControlsKeepCombiningDoMirror),
                "UBIDI_REMOVE_BIDI_CONTROLS|UBIDI_DO_MIRRORING|UBIDI_KEEP_BASE_COMBINING" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);
    UErrorCode errorCode;

    // test ubidi_writeReverse() with "interesting" options

    for (i = 0; i < nTestCases; i++) {
        errorCode = U_ZERO_ERROR;

        if (i == 1)
            memset(reverse, 0xa5, UPRV_LENGTHOF(reverse)*U_SIZEOF_UCHAR);

        doWriteReverse(testCases, i, __FILE__, __LINE__);
    }
}

static void _testInverseBidiManyAddedPointsTest(void) {
    static UChar text[90];
    static UChar dest[MAXLEN];
    static UChar expected[120];

    int destLen, i;
    for (i = 0; i < UPRV_LENGTHOF(text); i+=3) {
        text[i] = 0x0061; /* 'a' */
        text[i+1] = 0x05d0;
        text[i+2] = 0x0033; /* '3' */
    }

    for (i = 0; i < UPRV_LENGTHOF(expected); i+=4) {
        expected[i] = 0x0061; /* 'a' */
        expected[i+1] = 0x05d0;
        expected[i+2] = 0x200e;
        expected[i+3] = 0x0033; /* '3' */
    }

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "_testInverseBidiManyAddedPointsTest");

        return;
    }

    ubidi_setReorderingMode(pBiDi, UBIDI_REORDER_INVERSE_LIKE_DIRECT);
    ubidi_setReorderingOptions(pBiDi, UBIDI_OPTION_INSERT_MARKS);

    static UBidiWriteReorderedTestCases testCases[] = {
        { (const char*)text, UPRV_LENGTHOF(text), UBIDI_LTR, NULL, dest, MAXLEN,
            0, U_ZERO_ERROR, (const char*)expected, UInputType_UChar, UPRV_LENGTHOF(expected),
            "InverseBidi Many Added Points Test", FALSE },
        };

    destLen = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);

    if (memcmp(dest, expected, destLen * sizeof(UChar))) {
        log_err("\nInvalid output with many added points, "
                "expected '%s', got '%s'\n",
                aescstrdup(expected, UPRV_LENGTHOF(expected)),
                aescstrdup(dest, destLen));
    }
    ubidi_close(pBiDi);
}

static int32_t
doWriteReverse(const UBidiWriteReverseTestCases *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    UChar u16Buf[MAXLEN];
    int32_t u16Len = 0;

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        errorCode = U_ZERO_ERROR;

        if (encoding == UEncoding_U16) {
            u16Len = ubidi_writeReverse(testCases[testNumber].source, testCases[testNumber].sourceLength,
                testCases[testNumber].dest, testCases[testNumber].destSize,
                testCases[testNumber].options,
                &errorCode);

            u_strncpy(u16Buf, testCases[testNumber].dest, u16Len);
        }
        else if (encoding == UEncoding_U8) {
            UConverter *u8Convertor = ucnv_open("UTF8", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u8Convertor == NULL)) {
                return 0;
            }

            uint8_t u8BufDst[MAXLEN];

            memset(u8BufDst, 0, sizeof(u8BufDst) / sizeof(char));

            int32_t u8LenDst = testCases[testNumber].destSize;
            if (!testCases[testNumber].dest)
                u8LenDst = 0;
            else if (testCases[testNumber].destSize < 0)
                u8LenDst = sizeof(u8BufDst);

            UText* srcUt16 = NULL;
            if (testCases[testNumber].source)
                srcUt16 = utext_openUChars(NULL, testCases[testNumber].source, testCases[testNumber].sourceLength, &errorCode);

            UText* dstUt8 = NULL;
            if (testCases[testNumber].dest)
                dstUt8 = utext_openU8(NULL, u8BufDst, 0, u8LenDst, &errorCode);

            u8LenDst = ubidi_writeUReverse(srcUt16,
                dstUt8,
                testCases[testNumber].options,
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

            if ((!U_SUCCESS(errorCode)) || (u32Convertor == NULL)) {
                return 0;
            }

            UChar32 u32BufDst[MAXLEN];

            memset(u32BufDst, 0, sizeof(u32BufDst) / sizeof(UChar32));
            u32BufDst[0] = 0x0000FEFF;

            int32_t u32LenDst = testCases[testNumber].destSize;
            if (!testCases[testNumber].dest)
                u32LenDst = 0;
            else if (testCases[testNumber].destSize < 0)
                u32LenDst = sizeof(u32LenDst);

            UText* srcUt16 = NULL;
            if (testCases[testNumber].source)
                srcUt16 = utext_openUChars(NULL, testCases[testNumber].source, testCases[testNumber].sourceLength, &errorCode);

            UText* dstUt32 = NULL;
            if (testCases[testNumber].source)
                dstUt32 = utext_openU32(NULL, &u32BufDst[1], 0, u32LenDst, &errorCode);

            u32LenDst = ubidi_writeUReverse(srcUt16,
                dstUt32,
                testCases[testNumber].options,
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

        if ((errorCode != U_STRING_NOT_TERMINATED_WARNING) && (errorCode != testCases[testNumber].errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                fileName, lineNumber, "ubidi_writeReverse", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                u_errorName(errorCode), u_errorName(testCases[testNumber].errorCode));
        }
        else if ((testCases[testNumber].expectedLength >= 0) && (u16Len != testCases[testNumber].expectedLength)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                fileName, lineNumber, "ubidi_writeReverse", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                u16Len, testCases[testNumber].expectedLength);
        }
        else if ((testCases[testNumber].expectedChars) && (memcmp(u16Buf, testCases[testNumber].expectedChars, u16Len * sizeof(UChar)) != 0)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): result mismatch\n"
                "Input\t: %s\nGot\t: %s\nExpected: %s\n",
                fileName, lineNumber, "ubidi_writeReverse", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                testCases[testNumber].source, u16Buf, testCases[testNumber].expectedChars);
        }

        // Do input and output overlap?
        if ((testCases[testNumber].dest != NULL) && ((testCases[testNumber].source >= testCases[testNumber].dest && testCases[testNumber].source < testCases[testNumber].dest + testCases[testNumber].destSize)
            || (testCases[testNumber].dest >= testCases[testNumber].source && testCases[testNumber].dest < testCases[testNumber].source + testCases[testNumber].sourceLength))) {
            return u16Len;
        }
    }

    return u16Len;
}

static void _testInverseBidiMisc(void) {
    static UChar src[3];
    static UChar dest[MAXLEN];
    static UChar expected[5];
    int destLen;
    src[0] = src[1] = src[2] = 0x0020;

    u_unescape("\\u200f   \\u200f", expected, 5);

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "_testInverseBidiMisc");

        return;
    }

    ubidi_setInverse(pBiDi, TRUE);

    static UBidiWriteReorderedTestCases testCases[] = {
        { (const char*)src,
            UPRV_LENGTHOF(src),
            UBIDI_RTL,
            NULL,
            dest,
            MAXLEN,
            UBIDI_OUTPUT_REVERSE | UBIDI_INSERT_LRM_FOR_NUMERIC,
            U_ZERO_ERROR,
            (const char*)expected,
            UInputType_UChar,
            UPRV_LENGTHOF(expected),
            "InverseBidi Misc - invalid output with RLM at both sides",
            FALSE },
        };

    destLen = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);

    ubidi_close(pBiDi);
}

// complex/bidi/TestReorder

static const char
logicalOrder1[] = { "del(KC)add(K.C.&)" },
logicalOrder2[] = { "del(QDVT) add(BVDL)" },
logicalOrder3[] = { "del(PQ)add(R.S.)T)U.&" },
logicalOrder4[] = { "del(LV)add(L.V.) L.V.&" },
logicalOrder5[] = { "day  0  R  DPDHRVR dayabbr" },
logicalOrder6[] = { "day  1  H  DPHPDHDA dayabbr" },
logicalOrder7[] = { "day  2   L  DPBLENDA dayabbr" },
logicalOrder8[] = { "day  3  J  DPJQVM  dayabbr" },
logicalOrder9[] = { "day  4   I  DPIQNF    dayabbr" },
logicalOrder10[] = { "day  5  M  DPMEG  dayabbr" },
logicalOrder11[] = { "helloDPMEG" },
logicalOrder12[] = { "hello WXY" };

static const char
visualOrderMirror1[] = { "del(CK)add(&.C.K)" },
visualOrderMirror2[] = { "del(TVDQ) add(LDVB)" },
visualOrderMirror3[] = { "del(QP)add(S.R.)&.U(T" },  // Updated for Unicode 6.3 matching brackets
visualOrderMirror4[] = { "del(VL)add(V.L.) &.V.L" }, // Updated for Unicode 6.3 matching brackets
visualOrderMirror5[] = { "day  0  RVRHDPD  R dayabbr" },
visualOrderMirror6[] = { "day  1  ADHDPHPD  H dayabbr" },
visualOrderMirror7[] = { "day  2   ADNELBPD  L dayabbr" },
visualOrderMirror8[] = { "day  3  MVQJPD  J  dayabbr" },
visualOrderMirror9[] = { "day  4   FNQIPD  I    dayabbr" },
visualOrderMirror10[] = { "day  5  GEMPD  M  dayabbr" },
visualOrderMirror11[] = { "helloGEMPD" },
visualOrderMirror12[] = { "hello YXW" };

static const char
visualOrderMirrorReverse1[] = { ")K.C.&(dda)KC(led" },
visualOrderMirrorReverse2[] = { ")BVDL(dda )QDVT(led" },
visualOrderMirrorReverse3[] = { "T(U.&).R.S(dda)PQ(led" },  // Updated for Unicode 6.3 matching brackets
visualOrderMirrorReverse4[] = { "L.V.& ).L.V(dda)LV(led" }, // Updated for Unicode 6.3 matching brackets
visualOrderMirrorReverse5[] = { "rbbayad R  DPDHRVR  0  yad" },
visualOrderMirrorReverse6[] = { "rbbayad H  DPHPDHDA  1  yad" },
visualOrderMirrorReverse7[] = { "rbbayad L  DPBLENDA   2  yad" },
visualOrderMirrorReverse8[] = { "rbbayad  J  DPJQVM  3  yad" },
visualOrderMirrorReverse9[] = { "rbbayad    I  DPIQNF   4  yad" },
visualOrderMirrorReverse10[] = { "rbbayad  M  DPMEG  5  yad" },
visualOrderMirrorReverse11[] = { "DPMEGolleh" },
visualOrderMirrorReverse12[] = { "WXY olleh" };

static const char
visualOrderInsertLrmReverse1[] = { "@)@K.C.&@(dda)@KC@(led" },
visualOrderInsertLrmReverse2[] = { "@)@BVDL@(dda )@QDVT@(led" },
visualOrderInsertLrmReverse3[] = { "R.S.)T)U.&@(dda)@PQ@(led" },
visualOrderInsertLrmReverse4[] = { "L.V.) L.V.&@(dda)@LV@(led" },
visualOrderInsertLrmReverse5[] = { "rbbayad @R  DPDHRVR@  0  yad" },
visualOrderInsertLrmReverse6[] = { "rbbayad @H  DPHPDHDA@  1  yad" },
visualOrderInsertLrmReverse7[] = { "rbbayad @L  DPBLENDA@   2  yad" },
visualOrderInsertLrmReverse8[] = { "rbbayad  @J  DPJQVM@  3  yad" },
visualOrderInsertLrmReverse9[] = { "rbbayad    @I  DPIQNF@   4  yad" },
visualOrderInsertLrmReverse10[] = { "rbbayad  @M  DPMEG@  5  yad" },
visualOrderInsertLrmReverse11[] = { "DPMEGolleh" },
visualOrderInsertLrmReverse12[] = { "WXY@ olleh" };

static UBiDiLevel visualOrderExplicitLvls[UBIDI_MAX_EXPLICIT_LEVEL] = { 1,2,3,4,5,6,7,8,9,10 };

static const char
visualOrderExplcitLvlReverse1[] = { ")K.C.&(KC)dda(led" },
visualOrderExplcitLvlReverse2[] = { ")BVDL(ddaQDVT) (led" },
visualOrderExplcitLvlReverse3[] = { "R.S.)T)U.&(PQ)dda(led" },
visualOrderExplcitLvlReverse4[] = { "L.V.) L.V.&(LV)dda(led" },
visualOrderExplcitLvlReverse5[] = { "rbbayad DPDHRVR   R  0 yad" },
visualOrderExplcitLvlReverse6[] = { "rbbayad DPHPDHDA   H  1 yad" },
visualOrderExplcitLvlReverse7[] = { "rbbayad DPBLENDA     L 2 yad" },
visualOrderExplcitLvlReverse8[] = { "rbbayad  DPJQVM   J  3 yad" },
visualOrderExplcitLvlReverse9[] = { "rbbayad    DPIQNF     I 4 yad" },
visualOrderExplcitLvlReverse10[] = { "rbbayad  DPMEG   M  5 yad" },
visualOrderExplcitLvlReverse11[] = { "DPMEGolleh" },
visualOrderExplcitLvlReverse12[] = { "WXY olleh" };

static const char
visualOrderExplcitLvlRemoveCtrls1[] = { "del(add(CK(.C.K)" },
visualOrderExplcitLvlRemoveCtrls2[] = { "del( (TVDQadd(LDVB)" },
visualOrderExplcitLvlRemoveCtrls3[] = { "del(add(QP(.U(T(.S.R" },
visualOrderExplcitLvlRemoveCtrls4[] = { "del(add(VL(.V.L (.V.L" },
visualOrderExplcitLvlRemoveCtrls5[] = { "day 0  R   RVRHDPD dayabbr" },
visualOrderExplcitLvlRemoveCtrls6[] = { "day 1  H   ADHDPHPD dayabbr" },
visualOrderExplcitLvlRemoveCtrls7[] = { "day 2 L     ADNELBPD dayabbr" },
visualOrderExplcitLvlRemoveCtrls8[] = { "day 3  J   MVQJPD  dayabbr" },
visualOrderExplcitLvlRemoveCtrls9[] = { "day 4 I     FNQIPD    dayabbr" },
visualOrderExplcitLvlRemoveCtrls10[] = { "day 5  M   GEMPD  dayabbr" },
visualOrderExplcitLvlRemoveCtrls11[] = { "helloGEMPD" },
visualOrderExplcitLvlRemoveCtrls12[] = { "hello YXW" };

static void
testReorder(void) {
    static UChar dest[MAXLEN];

    static const UBidiWriteReorderedTestCases testCases[] = {
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0, 
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror1, UInputType_pseudo16_char, -1, 
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN, 
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror1, UInputType_pseudo16_char, -1, 
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse1, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN, 
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse1, UInputType_pseudo16_char, -1, 
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse1, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN, 
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse1, UInputType_pseudo16_char, -2, 
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse1, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN, 
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse1, UInputType_pseudo16_char, -1, 
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls1, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder1, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN, 
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls1, UInputType_pseudo16_char, -2, 
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0, 
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror2, UInputType_pseudo16_char, -1, 
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN, 
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror2, UInputType_pseudo16_char, -1, 
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse2, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN, 
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse2, UInputType_pseudo16_char, -1, 
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse2, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN, 
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse2, UInputType_pseudo16_char, -2, 
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse2, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN, 
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse2, UInputType_pseudo16_char, -1, 
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls2, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder2, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN, 
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls2, UInputType_pseudo16_char, -2, 
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror3, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror3, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse3, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse3, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse3, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse3, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse3, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse3, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls3, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder3, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls3, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror4, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror4, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse4, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse4, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse4, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse4, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse4, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse4, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls4, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder4, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls4, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror5, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror5, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse5, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse5, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse5, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse5, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse5, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse5, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls5, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder5, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls5, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror6, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror6, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse6, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse6, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse6, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse6, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse6, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse6, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls6, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder6, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls6, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror7, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror7, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse7, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse7, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse7, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse7, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse7, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse7, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls7, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder7, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls7, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror8, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror8, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse8, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse8, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse8, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse8, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse8, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse8, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls8, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder8, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls8, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror9, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror9, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse9, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse9, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse9, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse9, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse9, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse9, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls9, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder9, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls9, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror10, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror10, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse10, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse10, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse10, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse10, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse10, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse10, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls10, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder10, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls10, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror11, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror11, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse11, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse11, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse11, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse11, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse11, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse11, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls11, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder11, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls11, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },

        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, visualOrderMirror12, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, visualOrderMirror12, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderMirrorReverse12, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderMirrorReverse12, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderInsertLrmReverse12, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderInsertLrmReverse12, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_OUTPUT_REVERSE, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlReverse12, UInputType_pseudo16_char, -1,
            "Reordering (pre-flight) UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_OUTPUT_REVERSE, U_ZERO_ERROR, visualOrderExplcitLvlReverse12, UInputType_pseudo16_char, -1,
            "Reordering UBIDI_OUTPUT_REVERSE", FALSE, TRUE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, 0,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_BUFFER_OVERFLOW_ERROR, visualOrderExplcitLvlRemoveCtrls12, UInputType_pseudo16_char, -2,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
        { logicalOrder12, -1, UBIDI_DEFAULT_LTR, visualOrderExplicitLvls, dest, MAXLEN,
            UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS, U_ZERO_ERROR, visualOrderExplcitLvlRemoveCtrls12, UInputType_pseudo16_char, -2,
            "Reordering UBIDI_DO_MIRRORING + UBIDI_REMOVE_BIDI_CONTROLS", FALSE, TRUE },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testReorder(): Started, %u test cases:\n", nTestCases);

    UBiDi* pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "testReorderArabicMathSymbols");

        return;
    }

    for (i = 0; i < nTestCases; i++) {
        if (testCases[i].options == UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE) {
            ubidi_setInverse(pBiDi, TRUE);
        }

        int32_t destLen = doWriteReordered(pBiDi, testCases, i, __FILE__, __LINE__);

        if ((testCases[i].options == UBIDI_DO_MIRRORING) &&
            (testCases[i].destSize > 0)) {
            char destChars[MAXLEN];
            u16ToPseudo(destLen, dest, destChars);

            checkWhatYouCan(pBiDi, testCases[i].source, destChars, __FILE__, __LINE__, UEncoding_U16);
        }
        else if (testCases[i].options == UBIDI_INSERT_LRM_FOR_NUMERIC + UBIDI_OUTPUT_REVERSE) {
            ubidi_setInverse(pBiDi, FALSE);
        }
    }

    ubidi_close(pBiDi);

    log_verbose("testReorder(): Finished\n");
}

static int32_t
doWriteReordered(UBiDi *pBiDi,
    const UBidiWriteReorderedTestCases *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    UChar u16Buf[MAXLEN];
    int32_t u16Len = 0;

    UChar u16BufSrc[MAXLEN];
    int32_t u16LenSrc = 0;
    memset(u16BufSrc, 0, sizeof(u16BufSrc));

    UText* paraUt = 0;
    uint8_t u8BufSrc[MAXLEN];

    UBiDiLevel explicitLevels[UBIDI_MAX_EXPLICIT_LEVEL];
    UBiDiLevel* levels = NULL;

    if ((!testCases[testNumber].skipSetPara) && (testCases[testNumber].source)) {
        if (testCases[testNumber].inputType == UInputType_char) {
            errorCode = U_ZERO_ERROR;
            UConverter *u8Convertor = ucnv_open("UTF8", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u8Convertor == NULL)) {
                return 0;
            }

            const char *srcChars = (const char *)testCases[testNumber].source;
            int32_t srcLen = testCases[testNumber].sourceLength;
            if (srcLen <= (-1))
                srcLen = (int32_t)strlen(srcChars);

            u16LenSrc = ucnv_toUChars(u8Convertor, u16BufSrc, MAXLEN, srcChars, srcLen, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                return 0;
            }

            ucnv_close(u8Convertor);
        }
        else if (testCases[testNumber].inputType == UInputType_UChar) {
            const UChar *srcUChars = (const UChar *)testCases[testNumber].source;
            int32_t srcLen = testCases[testNumber].sourceLength;
            if (srcLen <= (-1))
                srcLen = u_strlen(srcUChars);
            u_strncpy(u16BufSrc, srcUChars, srcLen);
            u16LenSrc = srcLen;
        }
        else if (testCases[testNumber].inputType == UInputType_pseudo16_char) {
            const char *srcChars = (const char *)testCases[testNumber].source;
            int32_t srcLen = testCases[testNumber].sourceLength;
            if (srcLen <= (-1))
                srcLen = (int32_t)strlen(srcChars);
            u16LenSrc = pseudoToU16(srcLen, srcChars, u16BufSrc);
        }
        else if (testCases[testNumber].inputType == UInputType_pseudo16_UChar) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): inputType not supported\n",
                fileName, lineNumber, "ubidi_writeReordered", Encodings[UEncoding_U16].description, testNumber, testCases[testNumber].pMessage);

            return 0;
        }
        else if (testCases[testNumber].inputType == UInputType_unescape_char) {
            const char *srcChars = (const char *)testCases[testNumber].source;
            int32_t srcLen = testCases[testNumber].sourceLength;
            if (srcLen <= (-1))
                srcLen = (int32_t)strlen(srcChars);
            u16LenSrc = u_unescape(srcChars, u16BufSrc, srcLen);
        }
        else if (testCases[testNumber].inputType == UInputType_unescape_UChar) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): inputType not supported\n",
                fileName, lineNumber, "ubidi_writeReordered", Encodings[UEncoding_U16].description, testNumber, testCases[testNumber].pMessage);

            return 0;
        }
    }

    if (testCases[testNumber].embeddingLevels) {
        levels = explicitLevels;
    }

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        errorCode = U_ZERO_ERROR;
        int32_t nativeLenDst = 0;
        memset(u16Buf, 0, sizeof(u16Buf));
        memset(u8BufSrc, 0, sizeof(u8BufSrc));

        if (testCases[testNumber].embeddingLevels) {
            memcpy(explicitLevels, testCases[testNumber].embeddingLevels, sizeof(UBiDiLevel) * UBIDI_MAX_EXPLICIT_LEVEL);
        }

        if (!testCases[testNumber].skipSetPara) {
            if (!doSetPara(pBiDi, testCases[testNumber].source ? u16BufSrc : NULL, 
                testCases[testNumber].sourceLength >= 0 ? u16LenSrc : -1, testCases[testNumber].paraLevel, 
                levels, (levels ? UBIDI_MAX_EXPLICIT_LEVEL : 0), U_ZERO_ERROR, 0, fileName, lineNumber, Encodings[encoding], &paraUt, u8BufSrc, MAXLEN)) {
                return FALSE;
            }
        }

        if (encoding == UEncoding_U16) {
            u16Len = ubidi_writeReordered(pBiDi,
                testCases[testNumber].dest, testCases[testNumber].destSize,
                testCases[testNumber].options,
                &errorCode);

            u_strncpy(u16Buf, testCases[testNumber].dest, u16Len);
        }
        else if (encoding == UEncoding_U8) {
            UConverter *u8Convertor = ucnv_open("UTF8", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u8Convertor == NULL)) {
                return 0;
            }

            uint8_t u8BufDst[MAXLEN];

            memset(u8BufDst, 0, sizeof(u8BufDst) / sizeof(char));

            int32_t u8LenDst = testCases[testNumber].destSize;
            if (!testCases[testNumber].dest)
                u8LenDst = 0;
            else if (testCases[testNumber].destSize < 0)
                u8LenDst = sizeof(u8BufDst);

            UText* dstUt8 = NULL;
            if (testCases[testNumber].dest)
                dstUt8 = utext_openU8(NULL, u8BufDst, 0, u8LenDst, &errorCode);

            nativeLenDst = ubidi_writeUReordered(pBiDi,
                dstUt8,
                testCases[testNumber].options,
                &errorCode);

            u16Len = nativeLenDst;
            if (testCases[testNumber].dest) {
                u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
                u16Len = ucnv_toUChars(u8Convertor, u16Buf, sizeof(u16Buf), (const char *)u8BufDst, nativeLenDst * sizeof(char), &errorCode);
            }

            utext_close(dstUt8);
            ucnv_close(u8Convertor);
        }
        else if (encoding == UEncoding_U32) {
            UConverter *u32Convertor = ucnv_open("UTF32", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u32Convertor == NULL)) {
                return 0;
            }

            UChar32 u32BufDst[MAXLEN];

            memset(u32BufDst, 0, sizeof(u32BufDst) / sizeof(UChar32));
            u32BufDst[0] = 0x0000FEFF;

            int32_t u32LenDst = testCases[testNumber].destSize;
            if (!testCases[testNumber].dest)
                u32LenDst = 0;
            else if (testCases[testNumber].destSize < 0)
                u32LenDst = sizeof(u32LenDst);

            UText* dstUt32 = NULL;
            if (testCases[testNumber].dest)
                dstUt32 = utext_openU32(NULL, &u32BufDst[1], 0, u32LenDst, &errorCode);

            nativeLenDst = ubidi_writeUReordered(pBiDi,
                dstUt32,
                testCases[testNumber].options,
                &errorCode);

            u16Len = nativeLenDst;
            if (testCases[testNumber].dest) {
                u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
                u16Len = ucnv_toUChars(u32Convertor, u16Buf, sizeof(u16Buf), (const char *)u32BufDst, (nativeLenDst + 1) * sizeof(UChar32), &errorCode);
            }

            utext_close(dstUt32);
            ucnv_close(u32Convertor);
        }

        if ((errorCode != U_STRING_NOT_TERMINATED_WARNING) && (errorCode != testCases[testNumber].errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                fileName, lineNumber, "ubidi_writeReordered", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                u_errorName(errorCode), u_errorName(testCases[testNumber].errorCode));
        }
        else if (testCases[testNumber].inputType == UInputType_char) {
        }
        else if (testCases[testNumber].inputType == UInputType_UChar) {
        }
        else if (testCases[testNumber].inputType == UInputType_pseudo16_char) {
            UChar u16BufExpected[MAXLEN];
            int32_t u16ExpectedLength = testCases[testNumber].expectedU16Length;
            char u8Pseudo[MAXLEN];

            memset(u16BufExpected, 0, sizeof(u16BufExpected));

            if (testCases[testNumber].expectedChars) {
                if (testCases[testNumber].expectedU16Length <= (-1))
                    u16ExpectedLength = (int32_t)strlen(testCases[testNumber].expectedChars);

                pseudoToU16(u16ExpectedLength, testCases[testNumber].expectedChars, u16BufExpected);
            }

            if (testCases[testNumber].expectedU16Length <= (-1))
                u16ExpectedLength = u_strlen(u16BufExpected);

            memset(u8Pseudo, 0, sizeof(u8Pseudo));
            u16ToPseudo(u16Len, u16Buf, u8Pseudo);

            if ((testCases[testNumber].expectedChars) && (testCases[testNumber].expectedU16Length >= (-1)) &&
                (!testCases[testNumber].skipSetPara) &&
                (errorCode == U_BUFFER_OVERFLOW_ERROR) && (encoding != UEncoding_U16)) {
                if (utext_nativeLength(paraUt) != nativeLenDst) {
                    log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                        fileName, lineNumber, "ubidi_writeReordered", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                        nativeLenDst, utext_nativeLength(paraUt));
                }
            }
            else if ((testCases[testNumber].expectedChars) && (testCases[testNumber].expectedU16Length >= (-1)) &&
                (u16Len != u16ExpectedLength)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                    fileName, lineNumber, "ubidi_writeReordered", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                    u16Len, u16ExpectedLength);
            }
            else if ((errorCode != U_BUFFER_OVERFLOW_ERROR) &&
                (testCases[testNumber].expectedChars) && (memcmp(u16Buf, u16BufExpected, u16Len) != 0)) {
                char formatChars[MAXLEN];

                memset(formatChars, 0, sizeof(formatChars));

                log_err("%s(%d): %s(%s, tests[%d]: %s): result mismatch\n"
                    "Input\t: %s\nGot\t: %s\nExpected: %s\nLevels\t: %s\n",
                    fileName, lineNumber, "ubidi_writeReordered", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                    testCases[testNumber].source ? testCases[testNumber].source : "", u8Pseudo, testCases[testNumber].expectedChars ? testCases[testNumber].expectedChars : "", formatLevels(pBiDi, formatChars));
            }
        }
        else if (testCases[testNumber].inputType == UInputType_unescape_char) {
            UChar u16BufExpected[MAXLEN];
            char formatChars[MAXLEN];
            int32_t u16ExpectedLength = testCases[testNumber].expectedU16Length;
            int32_t u16UnescapeLength = 0;

            memset(u16BufExpected, 0, sizeof(u16BufExpected));

            if (testCases[testNumber].expectedChars) {
                if (testCases[testNumber].expectedU16Length <= (-1))
                    u16ExpectedLength = (int32_t)strlen(testCases[testNumber].expectedChars);

                u16UnescapeLength = u_unescape(testCases[testNumber].expectedChars, u16BufExpected, u16ExpectedLength);
            }

            if (testCases[testNumber].expectedU16Length <= (-1))
                u16ExpectedLength = u16UnescapeLength;

            if ((testCases[testNumber].expectedChars) && (testCases[testNumber].expectedU16Length >= (-1)) &&
                (!testCases[testNumber].skipSetPara) &&
                (errorCode == U_BUFFER_OVERFLOW_ERROR) && (encoding != UEncoding_U16)) {
                if (utext_nativeLength(paraUt) != nativeLenDst) {
                    log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                        fileName, lineNumber, "ubidi_writeReordered", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                        nativeLenDst, utext_nativeLength(paraUt));
                }
            }
            else if ((testCases[testNumber].expectedChars) && (testCases[testNumber].expectedU16Length >= (-1)) &&
                (u16Len != u16ExpectedLength)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                    fileName, lineNumber, "ubidi_writeReordered", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                    u16Len, u16ExpectedLength);
            }
            else if ((errorCode != U_BUFFER_OVERFLOW_ERROR) &&
                (testCases[testNumber].expectedChars) && (memcmp(u16Buf, u16BufExpected, u16Len) != 0)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): result mismatch\n"
                    "Input\t: %s\nGot\t: %s\nExpected: %s\nLevels\t: %s\n",
                    fileName, lineNumber, "ubidi_writeReordered", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                    testCases[testNumber].source, aescstrdup(u16Buf, u16Len), testCases[testNumber].expectedChars ? testCases[testNumber].expectedChars : "", formatLevels(pBiDi, formatChars));
            }
        }

        if (utext_isValid(paraUt)) {
            ubidi_setPara(pBiDi, 0, -1, UBIDI_DEFAULT_LTR, NULL, &errorCode);

            utext_close(paraUt);
            paraUt = 0;
        }
    }

    return u16Len;
}

// complex/bidi/bug-9024

static const UChar
/* Arabic mathematical Symbols 0x1EE00 - 0x1EE1B */
logicalOrderArabicMath1[] = { 
    0xD83B, 0xDE00, 0xD83B, 0xDE01, 0xD83B, 0xDE02, 0xD83B, 0xDE03, 0x20,
    0xD83B, 0xDE24, 0xD83B, 0xDE05, 0xD83B, 0xDE06, 0x20,
    0xD83B, 0xDE07, 0xD83B, 0xDE08, 0xD83B, 0xDE09, 0x20,
    0xD83B, 0xDE0A, 0xD83B, 0xDE0B, 0xD83B, 0xDE0C, 0xD83B, 0xDE0D, 0x20,
    0xD83B, 0xDE0E, 0xD83B, 0xDE0F, 0xD83B, 0xDE10, 0xD83B, 0xDE11, 0x20,
    0xD83B, 0xDE12, 0xD83B, 0xDE13, 0xD83B, 0xDE14, 0xD83B, 0xDE15, 0x20,
    0xD83B, 0xDE16, 0xD83B, 0xDE17, 0xD83B, 0xDE18, 0x20,
    0xD83B, 0xDE19, 0xD83B, 0xDE1A, 0xD83B, 0xDE1B },
/* Arabic mathematical Symbols - Looped Symbols, 0x1EE80 - 0x1EE9B */
logicalOrderArabicMath2[] = {
    0xD83B, 0xDE80, 0xD83B, 0xDE81, 0xD83B, 0xDE82, 0xD83B, 0xDE83, 0x20,
    0xD83B, 0xDE84, 0xD83B, 0xDE85, 0xD83B, 0xDE86, 0x20,
    0xD83B, 0xDE87, 0xD83B, 0xDE88, 0xD83B, 0xDE89, 0x20,
    0xD83B, 0xDE8B, 0xD83B, 0xDE8C, 0xD83B, 0xDE8D, 0x20,
    0xD83B, 0xDE8E, 0xD83B, 0xDE8F, 0xD83B, 0xDE90, 0xD83B, 0xDE91, 0x20,
    0xD83B, 0xDE92, 0xD83B, 0xDE93, 0xD83B, 0xDE94, 0xD83B, 0xDE95, 0x20,
    0xD83B, 0xDE96, 0xD83B, 0xDE97, 0xD83B, 0xDE98, 0x20,
    0xD83B, 0xDE99, 0xD83B, 0xDE9A, 0xD83B, 0xDE9B },
/* Arabic mathematical Symbols - Double-struck Symbols, 0x1EEA1 - 0x1EEBB */
logicalOrderArabicMath3[] = {
    0xD83B, 0xDEA1, 0xD83B, 0xDEA2, 0xD83B, 0xDEA3, 0x20,
    0xD83B, 0xDEA5, 0xD83B, 0xDEA6, 0x20,
    0xD83B, 0xDEA7, 0xD83B, 0xDEA8, 0xD83B, 0xDEA9, 0x20,
    0xD83B, 0xDEAB, 0xD83B, 0xDEAC, 0xD83B, 0xDEAD, 0x20,
    0xD83B, 0xDEAE, 0xD83B, 0xDEAF, 0xD83B, 0xDEB0, 0xD83B, 0xDEB1, 0x20,
    0xD83B, 0xDEB2, 0xD83B, 0xDEB3, 0xD83B, 0xDEB4, 0xD83B, 0xDEB5, 0x20,
    0xD83B, 0xDEB6, 0xD83B, 0xDEB7, 0xD83B, 0xDEB8, 0x20,
    0xD83B, 0xDEB9, 0xD83B, 0xDEBA, 0xD83B, 0xDEBB },
/* Arabic mathematical Symbols - Initial Symbols, 0x1EE21 - 0x1EE3B */
logicalOrderArabicMath4[] = { 
    0xD83B, 0xDE21, 0xD83B, 0xDE22, 0x20,
    0xD83B, 0xDE27, 0xD83B, 0xDE29, 0x20,
    0xD83B, 0xDE2A, 0xD83B, 0xDE2B, 0xD83B, 0xDE2C, 0xD83B, 0xDE2D, 0x20,
    0xD83B, 0xDE2E, 0xD83B, 0xDE2F, 0xD83B, 0xDE30, 0xD83B, 0xDE31, 0x20,
    0xD83B, 0xDE32, 0xD83B, 0xDE34, 0xD83B, 0xDE35, 0x20,
    0xD83B, 0xDE36, 0xD83B, 0xDE37, 0x20,
    0xD83B, 0xDE39, 0xD83B, 0xDE3B },
/* Arabic mathematical Symbols - Tailed Symbols */
logicalOrderArabicMath5[] = { 
    0xD83B, 0xDE42, 0xD83B, 0xDE47, 0xD83B, 0xDE49, 0xD83B, 0xDE4B, 0x20,
    0xD83B, 0xDE4D, 0xD83B, 0xDE4E, 0xD83B, 0xDE4F, 0x20,
    0xD83B, 0xDE51, 0xD83B, 0xDE52, 0xD83B, 0xDE54, 0xD83B, 0xDE57, 0x20,
    0xD83B, 0xDE59, 0xD83B, 0xDE5B, 0xD83B, 0xDE5D, 0xD83B, 0xDE5F };

static const UChar
/* Arabic mathematical Symbols 0x1EE00 - 0x1EE1B */
visualOrderArabicMath1[] = {
    0xD83B, 0xDE1B, 0xD83B, 0xDE1A, 0xD83B, 0xDE19, 0x20,
    0xD83B, 0xDE18, 0xD83B, 0xDE17, 0xD83B, 0xDE16, 0x20,
    0xD83B, 0xDE15, 0xD83B, 0xDE14, 0xD83B, 0xDE13, 0xD83B, 0xDE12, 0x20,
    0xD83B, 0xDE11, 0xD83B, 0xDE10, 0xD83B, 0xDE0F, 0xD83B, 0xDE0E, 0x20,
    0xD83B, 0xDE0D, 0xD83B, 0xDE0C, 0xD83B, 0xDE0B, 0xD83B, 0xDE0A, 0x20,
    0xD83B, 0xDE09, 0xD83B, 0xDE08, 0xD83B, 0xDE07, 0x20,
    0xD83B, 0xDE06, 0xD83B, 0xDE05, 0xD83B, 0xDE24, 0x20,
    0xD83B, 0xDE03, 0xD83B, 0xDE02, 0xD83B, 0xDE01, 0xD83B, 0xDE00 },
/* Arabic mathematical Symbols - Looped Symbols, 0x1EE80 - 0x1EE9B */
visualOrderArabicMath2[] = { 
    0xD83B, 0xDE9B, 0xD83B, 0xDE9A, 0xD83B, 0xDE99, 0x20,
    0xD83B, 0xDE98, 0xD83B, 0xDE97, 0xD83B, 0xDE96, 0x20,
    0xD83B, 0xDE95, 0xD83B, 0xDE94, 0xD83B, 0xDE93, 0xD83B, 0xDE92, 0x20,
    0xD83B, 0xDE91, 0xD83B, 0xDE90, 0xD83B, 0xDE8F, 0xD83B, 0xDE8E, 0x20,
    0xD83B, 0xDE8D, 0xD83B, 0xDE8C, 0xD83B, 0xDE8B, 0x20,
    0xD83B, 0xDE89, 0xD83B, 0xDE88, 0xD83B, 0xDE87, 0x20,
    0xD83B, 0xDE86, 0xD83B, 0xDE85, 0xD83B, 0xDE84, 0x20,
    0xD83B, 0xDE83, 0xD83B, 0xDE82, 0xD83B, 0xDE81, 0xD83B, 0xDE80 },
/* Arabic mathematical Symbols - Double-struck Symbols, 0x1EEA1 - 0x1EEBB */
visualOrderArabicMath3[] = {
    0xD83B, 0xDEBB, 0xD83B, 0xDEBA, 0xD83B, 0xDEB9, 0x20,
    0xD83B, 0xDEB8, 0xD83B, 0xDEB7, 0xD83B, 0xDEB6, 0x20,
    0xD83B, 0xDEB5, 0xD83B, 0xDEB4, 0xD83B, 0xDEB3, 0xD83B, 0xDEB2, 0x20,
    0xD83B, 0xDEB1, 0xD83B, 0xDEB0, 0xD83B, 0xDEAF, 0xD83B, 0xDEAE, 0x20,
    0xD83B, 0xDEAD, 0xD83B, 0xDEAC, 0xD83B, 0xDEAB, 0x20,
    0xD83B, 0xDEA9, 0xD83B, 0xDEA8, 0xD83B, 0xDEA7, 0x20,
    0xD83B, 0xDEA6, 0xD83B, 0xDEA5, 0x20,
    0xD83B, 0xDEA3, 0xD83B, 0xDEA2, 0xD83B, 0xDEA1 },
/* Arabic mathematical Symbols - Initial Symbols, 0x1EE21 - 0x1EE3B */
visualOrderArabicMath4[] = {
    0xD83B, 0xDE3B, 0xD83B, 0xDE39, 0x20,
    0xD83B, 0xDE37, 0xD83B, 0xDE36, 0x20,
    0xD83B, 0xDE35, 0xD83B, 0xDE34, 0xD83B, 0xDE32, 0x20,
    0xD83B, 0xDE31, 0xD83B, 0xDE30, 0xD83B, 0xDE2F, 0xD83B, 0xDE2E, 0x20,
    0xD83B, 0xDE2D, 0xD83B, 0xDE2C, 0xD83B, 0xDE2B, 0xD83B, 0xDE2A, 0x20,
    0xD83B, 0xDE29, 0xD83B, 0xDE27, 0x20,
    0xD83B, 0xDE22, 0xD83B, 0xDE21 },
/* Arabic mathematical Symbols - Tailed Symbols */
visualOrderArabicMath5[] = {
    0xD83B, 0xDE5F, 0xD83B, 0xDE5D, 0xD83B, 0xDE5B, 0xD83B, 0xDE59, 0x20,
    0xD83B, 0xDE57, 0xD83B, 0xDE54, 0xD83B, 0xDE52, 0xD83B, 0xDE51, 0x20,
    0xD83B, 0xDE4F, 0xD83B, 0xDE4E, 0xD83B, 0xDE4D, 0x20,
    0xD83B, 0xDE4B, 0xD83B, 0xDE49, 0xD83B, 0xDE47, 0xD83B, 0xDE42 };

static void
testReorderArabicMathSymbols(void) {
    static UChar dest[MAXLEN];

    static UBidiWriteReorderedTestCases testCases[] = {
        { (const char*)logicalOrderArabicMath1, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, 0, UInputType_UChar, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath1, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, (const char*)visualOrderArabicMath1, UInputType_UChar, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath2, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, 0, UInputType_UChar, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath2, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, (const char*)visualOrderArabicMath2, UInputType_UChar, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath3, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, 0, UInputType_UChar, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath3, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, (const char*)visualOrderArabicMath3, UInputType_UChar, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath4, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, 0, UInputType_UChar, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath4, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, (const char*)visualOrderArabicMath4, UInputType_UChar, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath5, -1, UBIDI_DEFAULT_LTR, NULL, dest, 0,
            UBIDI_DO_MIRRORING, U_BUFFER_OVERFLOW_ERROR, 0, UInputType_UChar, -1,
            "Reordering (pre-flight) UBIDI_DO_MIRRORING", FALSE },
        { (const char*)logicalOrderArabicMath5, -1, UBIDI_DEFAULT_LTR, NULL, dest, MAXLEN,
            UBIDI_DO_MIRRORING, U_ZERO_ERROR, (const char*)visualOrderArabicMath5, UInputType_UChar, -1,
            "Reordering UBIDI_DO_MIRRORING", FALSE },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testReorderArabicMathSymbols(): Started, %u test cases:\n", nTestCases);

    UBiDi* pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "testReorderArabicMathSymbols");

        return;
    }

    for (i = 0; i < nTestCases; i++) {
        doWriteReordered(pBiDi, testCases, i, __FILE__, __LINE__);
    }

    ubidi_close(pBiDi);

    log_verbose("testReorderArabicMathSymbols(): Finished\n");
}

// complex/bidi/TestReorderingMode

// new BIDI API
// Reordering Mode BiDi --------------------------------------------------------- 

static UBool
assertSuccessful(const char* message, UErrorCode* rc) {
    if (rc != NULL && !U_SUCCESS(*rc)) {
        log_err("%s() failed with error %s.\n", message, u_errorName(*rc));
        return FALSE;
    }
    return TRUE;
}

static UBool
assertStringsEqual(const char* expected, const char* actual, const char* src,
                   const char* mode, const char* option, UBiDi* pBiDi) {
    if (uprv_strcmp(expected, actual)) {
        char formatChars[MAXLEN];
        log_err("\nActual and expected output mismatch.\n"
            "%20s %s\n%20s %s\n%20s %s\n%20s %s\n%20s %d %s\n%20s %u\n%20s %d %s\n",
            "Input:", src,
            "Actual output:", actual,
            "Expected output:", expected,
            "Levels:", formatLevels(pBiDi, formatChars),
            "Reordering mode:", ubidi_getReorderingMode(pBiDi), mode,
            "Paragraph level:", ubidi_getParaLevel(pBiDi),
            "Reordering option:", ubidi_getReorderingOptions(pBiDi), option);
        return FALSE;
    }
    return TRUE;
}

static const struct {
    UBiDiReorderingMode value;
    const char* description;
}
testReorderingMode_modes[] = {
    { MAKE_ITEMS(UBIDI_REORDER_GROUP_NUMBERS_WITH_R) },
    { MAKE_ITEMS(UBIDI_REORDER_INVERSE_LIKE_DIRECT) },
    { MAKE_ITEMS(UBIDI_REORDER_NUMBERS_SPECIAL) },
    { MAKE_ITEMS(UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL) },
    { MAKE_ITEMS(UBIDI_REORDER_INVERSE_NUMBERS_AS_L) }
};
#define MODES_COUNT UPRV_LENGTHOF(testReorderingMode_modes)

static const struct {
    uint32_t value;
    const char* description;
}
testReorderingMode_options[] = {
    { MAKE_ITEMS(UBIDI_OPTION_INSERT_MARKS) },
    { MAKE_ITEMS(0) }
};
#define OPTIONS_COUNT UPRV_LENGTHOF(testReorderingMode_options)

static const UBiDiLevel 
testReorderingMode_paraLevels[] = { UBIDI_LTR, UBIDI_RTL };
#define LEVELS_COUNT UPRV_LENGTHOF(testReorderingMode_paraLevels)

static const char* const testReorderingMode_textIn[] = {
/* (0) 123 */
    "123",
/* (1) .123->4.5 */
    ".123->4.5",
/* (2) 678 */
    "678",
/* (3) .678->8.9 */
    ".678->8.9",
/* (4) JIH1.2,3MLK */
    "JIH1.2,3MLK",
/* (5) FE.>12-> */
    "FE.>12->",
/* (6) JIH.>12->a */
    "JIH.>12->a",
/* (7) CBA.>67->89=a */
    "CBA.>67->89=a",
/* (8) CBA.123->xyz */
    "CBA.123->xyz",
/* (9) .>12->xyz */
    ".>12->xyz",
/* (10) a.>67->xyz */
    "a.>67->xyz",
/* (11) 123JIH */
    "123JIH",
/* (12) 123 JIH */
    "123 JIH"
};
#define TC_COUNT UPRV_LENGTHOF(testReorderingMode_textIn)

static const char* const testReorderingMode_textOut[] = {
/* TC 0: 123 */
    "123",                                                              /* (0) */
/* TC 1: .123->4.5 */
    ".123->4.5",                                                        /* (1) */
    "4.5<-123.",                                                        /* (2) */
/* TC 2: 678 */
    "678",                                                              /* (3) */
/* TC 3: .678->8.9 */
    ".8.9<-678",                                                        /* (4) */
    "8.9<-678.",                                                        /* (5) */
    ".678->8.9",                                                        /* (6) */
/* TC 4: MLK1.2,3JIH */
    "KLM1.2,3HIJ",                                                      /* (7) */
/* TC 5: FE.>12-> */
    "12<.EF->",                                                         /* (8) */
    "<-12<.EF",                                                         /* (9) */
    "EF.>@12->",                                                        /* (10) */
/* TC 6: JIH.>12->a */
    "12<.HIJ->a",                                                       /* (11) */
    "a<-12<.HIJ",                                                       /* (12) */
    "HIJ.>@12->a",                                                      /* (13) */
    "a&<-12<.HIJ",                                                      /* (14) */
/* TC 7: CBA.>67->89=a */
    "ABC.>@67->89=a",                                                   /* (15) */
    "a=89<-67<.ABC",                                                    /* (16) */
    "a&=89<-67<.ABC",                                                   /* (17) */
    "89<-67<.ABC=a",                                                    /* (18) */
/* TC 8: CBA.123->xyz */
    "123.ABC->xyz",                                                     /* (19) */
    "xyz<-123.ABC",                                                     /* (20) */
    "ABC.@123->xyz",                                                    /* (21) */
    "xyz&<-123.ABC",                                                    /* (22) */
/* TC 9: .>12->xyz */
    ".>12->xyz",                                                        /* (23) */
    "xyz<-12<.",                                                        /* (24) */
    "xyz&<-12<.",                                                       /* (25) */
/* TC 10: a.>67->xyz */
    "a.>67->xyz",                                                       /* (26) */
    "a.>@67@->xyz",                                                     /* (27) */
    "xyz<-67<.a",                                                       /* (28) */
/* TC 11: 123JIH */
    "123HIJ",                                                           /* (29) */
    "HIJ123",                                                           /* (30) */
/* TC 12: 123 JIH */
    "123 HIJ",                                                          /* (31) */
    "HIJ 123",                                                          /* (32) */
};

#define NO                  UBIDI_MAP_NOWHERE
#define MAX_MAP_LENGTH      20

static const int32_t testReorderingMode_forwardMap[][MAX_MAP_LENGTH] = {
/* TC 0: 123 */
    { 0, 1, 2 },                                                        /* (0) */
/* TC 1: .123->4.5 */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 },                                      /* (1) */
    { 8, 5, 6, 7, 4, 3, 0, 1, 2 },                                      /* (2) */
/* TC 2: 678 */
    { 0, 1, 2 },                                                        /* (3) */
/* TC 3: .678->8.9 */
    { 0, 6, 7, 8, 5, 4, 1, 2, 3 },                                      /* (4) */
    { 8, 5, 6, 7, 4, 3, 0, 1, 2 },                                      /* (5) */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 },                                      /* (6) */
/* TC 4: MLK1.2,3JIH */
    { 10, 9, 8, 3, 4, 5, 6, 7, 2, 1, 0 },                               /* (7) */
/* TC 5: FE.>12-> */
    { 5, 4, 3, 2, 0, 1, 6, 7 },                                         /* (8) */
    { 7, 6, 5, 4, 2, 3, 1, 0 },                                         /* (9) */
    { 1, 0, 2, 3, 5, 6, 7, 8 },                                         /* (10) */
/* TC 6: JIH.>12->a */
    { 6, 5, 4, 3, 2, 0, 1, 7, 8, 9 },                                   /* (11) */
    { 9, 8, 7, 6, 5, 3, 4, 2, 1, 0 },                                   /* (12) */
    { 2, 1, 0, 3, 4, 6, 7, 8, 9, 10 },                                  /* (13) */
    { 10, 9, 8, 7, 6, 4, 5, 3, 2, 0 },                                  /* (14) */
/* TC 7: CBA.>67->89=a */
    { 2, 1, 0, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13 },                      /* (15) */
    { 12, 11, 10, 9, 8, 6, 7, 5, 4, 2, 3, 1, 0 },                       /* (16) */
    { 13, 12, 11, 10, 9, 7, 8, 6, 5, 3, 4, 2, 0 },                      /* (17) */
    { 10, 9, 8, 7, 6, 4, 5, 3, 2, 0, 1, 11, 12 },                       /* (18) */
/* TC 8: CBA.123->xyz */
    { 6, 5, 4, 3, 0, 1, 2, 7, 8, 9, 10, 11 },                           /* (19) */
    { 11, 10, 9, 8, 5, 6, 7, 4, 3, 0, 1, 2 },                           /* (20) */
    { 2, 1, 0, 3, 5, 6, 7, 8, 9, 10, 11, 12 },                          /* (21) */
    { 12, 11, 10, 9, 6, 7, 8, 5, 4, 0, 1, 2 },                          /* (22) */
/* TC 9: .>12->xyz */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 },                                      /* (23) */
    { 8, 7, 5, 6, 4, 3, 0, 1, 2 },                                      /* (24) */
    { 9, 8, 6, 7, 5, 4, 0, 1, 2 },                                      /* (25) */
/* TC 10: a.>67->xyz */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },                                   /* (26) */
    { 0, 1, 2, 4, 5, 7, 8, 9, 10, 11 },                                 /* (27) */
    { 9, 8, 7, 5, 6, 4, 3, 0, 1, 2 },                                   /* (28) */
/* TC 11: 123JIH */
    { 0, 1, 2, 5, 4, 3 },                                               /* (29) */
    { 3, 4, 5, 2, 1, 0 },                                               /* (30) */
/* TC 12: 123 JIH */
    { 0, 1, 2, 3, 6, 5, 4 },                                            /* (31) */
    { 4, 5, 6, 3, 2, 1, 0 },                                            /* (32) */
};

static const int32_t testReorderingMode_inverseMap[][MAX_MAP_LENGTH] = {
/* TC 0: 123 */
    { 0, 1, 2 },                                                        /* (0) */
/* TC 1: .123->4.5 */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 },                                      /* (1) */
    { 6, 7, 8, 5, 4, 1, 2, 3, 0 },                                      /* (2) */
/* TC 2: 678 */
    { 0, 1, 2 },                                                        /* (3) */
/* TC 3: .678->8.9 */
    { 0, 6, 7, 8, 5, 4, 1, 2, 3 },                                      /* (4) */
    { 6, 7, 8, 5, 4, 1, 2, 3, 0 },                                      /* (5) */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 },                                      /* (6) */
/* TC 4: MLK1.2,3JIH */
    { 10, 9, 8, 3, 4, 5, 6, 7, 2, 1, 0 },                               /* (7) */
/* TC 5: FE.>12-> */
    { 4, 5, 3, 2, 1, 0, 6, 7 },                                         /* (8) */
    { 7, 6, 4, 5, 3, 2, 1, 0 },                                         /* (9) */
    { 1, 0, 2, 3, NO, 4, 5, 6, 7 },                                     /* (10) */
/* TC 6: JIH.>12->a */
    { 5, 6, 4, 3, 2, 1, 0, 7, 8, 9 },                                   /* (11) */
    { 9, 8, 7, 5, 6, 4, 3, 2, 1, 0 },                                   /* (12) */
    { 2, 1, 0, 3, 4, NO, 5, 6, 7, 8, 9 },                               /* (13) */
    { 9, NO, 8, 7, 5, 6, 4, 3, 2, 1, 0 },                               /* (14) */
/* TC 7: CBA.>67->89=a */
    { 2, 1, 0, 3, 4, NO, 5, 6, 7, 8, 9, 10, 11, 12 },                   /* (15) */
    { 12, 11, 9, 10, 8, 7, 5, 6, 4, 3, 2, 1, 0 },                       /* (16) */
    { 12, NO, 11, 9, 10, 8, 7, 5, 6, 4, 3, 2, 1, 0 },                   /* (17) */
    { 9, 10, 8, 7, 5, 6, 4, 3, 2, 1, 0, 11, 12 },                       /* (18) */
/* TC 8: CBA.123->xyz */
    { 4, 5, 6, 3, 2, 1, 0, 7, 8, 9, 10, 11 },                           /* (19) */
    { 9, 10, 11, 8, 7, 4, 5, 6, 3, 2, 1, 0 },                           /* (20) */
    { 2, 1, 0, 3, NO, 4, 5, 6, 7, 8, 9, 10, 11 },                       /* (21) */
    { 9, 10, 11, NO, 8, 7, 4, 5, 6, 3, 2, 1, 0 },                       /* (22) */
/* TC 9: .>12->xyz */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8 },                                      /* (23) */
    { 6, 7, 8, 5, 4, 2, 3, 1, 0 },                                      /* (24) */
    { 6, 7, 8, NO, 5, 4, 2, 3, 1, 0 },                                  /* (25) */
/* TC 10: a.>67->xyz */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 },                                   /* (26) */
    { 0, 1, 2, NO, 3, 4, NO, 5, 6, 7, 8, 9 },                           /* (27) */
    { 7, 8, 9, 6, 5, 3, 4, 2, 1, 0 },                                   /* (28) */
/* TC 11: 123JIH */
    { 0, 1, 2, 5, 4, 3 },                                               /* (29) */
    { 5, 4, 3, 0, 1, 2 },                                               /* (30) */
/* TC 12: 123 JIH */
    { 0, 1, 2, 3, 6, 5, 4 },                                            /* (31) */
    { 6, 5, 4, 3, 0, 1, 2 },                                            /* (32) */
};

static const char testReorderingMode_outIndices[TC_COUNT][MODES_COUNT - 1][OPTIONS_COUNT][LEVELS_COUNT] = {
    { /* TC 0: 123 */
        {{ 0,  0}, { 0,  0}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{ 0,  0}, { 0,  0}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{ 0,  0}, { 0,  0}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{ 0,  0}, { 0,  0}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 1: .123->4.5 */
        {{ 1,  2}, { 1,  2}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{ 1,  2}, { 1,  2}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{ 1,  2}, { 1,  2}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{ 1,  2}, { 1,  2}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 2: 678 */
        {{ 3,  3}, { 3,  3}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{ 3,  3}, { 3,  3}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{ 3,  3}, { 3,  3}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{ 3,  3}, { 3,  3}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 3: .678->8.9 */
        {{ 6,  5}, { 6,  5}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{ 4,  5}, { 4,  5}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{ 6,  5}, { 6,  5}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{ 6,  5}, { 6,  5}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 4: MLK1.2,3JIH */
        {{ 7,  7}, { 7,  7}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{ 7,  7}, { 7,  7}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{ 7,  7}, { 7,  7}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{ 7,  7}, { 7,  7}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 5: FE.>12-> */
        {{ 8,  9}, { 8,  9}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{10,  9}, { 8,  9}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{ 8,  9}, { 8,  9}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{10,  9}, { 8,  9}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 6: JIH.>12->a */
        {{11, 12}, {11, 12}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{13, 14}, {11, 12}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{11, 12}, {11, 12}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{13, 14}, {11, 12}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 7: CBA.>67->89=a */
        {{18, 16}, {18, 16}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{18, 17}, {18, 16}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{18, 16}, {18, 16}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{15, 17}, {18, 16}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 8: CBA.>124->xyz */
        {{19, 20}, {19, 20}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{21, 22}, {19, 20}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{19, 20}, {19, 20}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{21, 22}, {19, 20}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 9: .>12->xyz */
        {{23, 24}, {23, 24}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{23, 25}, {23, 24}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{23, 24}, {23, 24}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{23, 25}, {23, 24}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 10: a.>67->xyz */
        {{26, 26}, {26, 26}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{26, 27}, {26, 28}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{26, 28}, {26, 28}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{26, 27}, {26, 28}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 11: 124JIH */
        {{30, 30}, {30, 30}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{29, 30}, {29, 30}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{30, 30}, {30, 30}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{30, 30}, {30, 30}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    },
    { /* TC 12: 124 JIH */
        {{32, 32}, {32, 32}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
        {{31, 32}, {31, 32}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
        {{31, 32}, {31, 32}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
        {{31, 32}, {31, 32}}  /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
    }
};

static void
testReorderingMode(void) {

    static UChar dest[MAXLEN];
    char destChars[MAXLEN];
    const char *expectedChars;
    int32_t destLen = 0;
    int32_t destCharsLen = 0;
    uint32_t optionValue, optionBack;
    UBiDiReorderingMode modeValue, modeBack;
    UErrorCode errorCode = U_ZERO_ERROR;

    int tc, mode, option, level, idx;
    UBool testOK = TRUE;

    log_verbose("\nEntering TestReorderingMode\n\n");

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testInverse");

        return;
    }

    UBiDi *pBiDi2 = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testInverse");

        ubidi_close(pBiDi);
        return;
    }

    UBiDi *pBiDi3 = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testInverse");

        ubidi_close(pBiDi);
        ubidi_close(pBiDi2);
        return;
    }

    ubidi_setInverse(pBiDi2, TRUE);

    for (tc = 0; tc < TC_COUNT; tc++) {
        for (mode = 0; mode < MODES_COUNT; mode++) {
            modeValue = testReorderingMode_modes[mode].value;
            ubidi_setReorderingMode(pBiDi, modeValue);
            modeBack = ubidi_getReorderingMode(pBiDi);
            if (modeValue != modeBack) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): Error while setting reordering mode to %d, returned %d\n",
                    __FILE__, __LINE__, "ubidi_getReorderingMode", Encodings[UEncoding_U16].description, tc, "testReorderingMode",
                    modeValue, modeBack);
            }

            for (option = 0; option < OPTIONS_COUNT; option++) {
                optionValue = testReorderingMode_options[option].value;
                ubidi_setReorderingOptions(pBiDi, optionValue);
                optionBack = ubidi_getReorderingOptions(pBiDi);
                if (optionValue != optionBack) {
                    log_err("%s(%d): %s(%s, tests[%d]: %s): Error while setting reordering option to %d, returned %d\n",
                        __FILE__, __LINE__, "ubidi_getReorderingOptions", Encodings[UEncoding_U16].description, tc, "testReorderingMode",
                        optionValue, optionBack);
                }

                for (level = 0; level < LEVELS_COUNT; level++) {
                    log_verbose("starting test %d mode=%d option=%d level=%d\n",
                                tc, testReorderingMode_modes[mode].value, testReorderingMode_options[option].value, level);
                    errorCode = U_ZERO_ERROR;

                    UBidiWriteReorderedTestCases testCases[] = {
                        { NULL,
                            -1,
                            0,
                            NULL,
                            dest,
                            MAXLEN,
                            UBIDI_DO_MIRRORING,
                            U_ZERO_ERROR,
                            0, // Don't compare with expected
                            UInputType_pseudo16_char,
                            -1,
                            "Reordering UBIDI_DO_MIRRORING",
                            FALSE },
                    };
                    testCases[0].source = testReorderingMode_textIn[tc];
                    testCases[0].paraLevel = testReorderingMode_paraLevels[level];

                    destLen = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);

                    destCharsLen = u16ToPseudo(destLen, dest, destChars);

                    if (!((testReorderingMode_modes[mode].value == UBIDI_REORDER_INVERSE_NUMBERS_AS_L) &&
                          (testReorderingMode_options[option].value == UBIDI_OPTION_INSERT_MARKS))) {
                        checkWhatYouCan(pBiDi, testReorderingMode_textIn[tc], destChars, __FILE__, __LINE__, UEncoding_U16);
                    }

                    if (testReorderingMode_modes[mode].value == UBIDI_REORDER_INVERSE_NUMBERS_AS_L) {
                        idx = -1;
                        expectedChars = _testReorderingModeInverseBasic(pBiDi2, testReorderingMode_textIn[tc], -1,
                            testReorderingMode_options[option].value, testReorderingMode_paraLevels[level], destChars);
                    }
                    else {
                        idx = testReorderingMode_outIndices[tc][mode][option][level];
                        expectedChars = testReorderingMode_textOut[idx];
                    }

                    if (memcmp(expectedChars, destChars, destCharsLen)) {
                        char formatChars[MAXLEN];
                        log_err("%s(%d): %s(%s, tests[%d]: %s): Actual and expected output mismatch.\n"
                            "Input\t: %s\nGot\t: %s\nExpected: %s\nLevels\t: %s\nReordering mode\t: %d %s\nParagraph level\t: %u\nReordering option\t: %d %s\n",
                            __FILE__, __LINE__, "ubidi_getReorderingOptions", Encodings[UEncoding_U16].description, tc, "testReorderingMode",
                            testReorderingMode_textIn[tc], destChars, expectedChars, formatLevels(pBiDi, formatChars),
                            ubidi_getReorderingMode(pBiDi), testReorderingMode_modes[mode].description, ubidi_getParaLevel(pBiDi), ubidi_getReorderingOptions(pBiDi), testReorderingMode_options[option].description);

                        testOK = FALSE;
                    }

                    if (testReorderingMode_options[option].value == UBIDI_OPTION_INSERT_MARKS &&
                        !_testReorderingModeAssertRoundTrip(pBiDi3, tc, idx, testReorderingMode_textIn[tc],
                            destChars, dest, destLen,
                            mode, option, testReorderingMode_paraLevels[level])) {
                        testOK = FALSE;
                    }
                    else if (!_testReorderingModeCheckResultLength(pBiDi, testReorderingMode_textIn[tc], destChars,
                        destLen, testReorderingMode_modes[mode].description,
                        testReorderingMode_options[option].description,
                        testReorderingMode_paraLevels[level])) {

                        testOK = FALSE;
                    }
                    else if (idx > -1 && !_testReorderingModeCheckMaps(pBiDi, idx, testReorderingMode_textIn[tc],
                        destChars, testReorderingMode_modes[mode].description,
                        testReorderingMode_options[option].description, testReorderingMode_paraLevels[level],
                        TRUE)) {

                        testOK = FALSE;
                    }
                }
            }
        }
    }

    if (testOK == TRUE) {
        log_verbose("\nReordering mode test OK\n");
    }

    ubidi_close(pBiDi3);
    ubidi_close(pBiDi2);
    ubidi_close(pBiDi);

    log_verbose("\nExiting TestReorderingMode\n\n");
}

static const char* _testReorderingModeInverseBasic(UBiDi *pBiDi, 
    const char *srcChars, 
    int32_t srcLen,
    uint32_t option, 
    UBiDiLevel level, 
    char *destChars)
{
    static UChar dest[MAXLEN];
    int32_t destLen;

    if (pBiDi == NULL || srcChars == NULL) {
        return NULL;
    }

    ubidi_setReorderingOptions(pBiDi, option);

    UBidiWriteReorderedTestCases testCases[] = {
        { NULL,
            -1,
            0,
            NULL,
            dest,
            MAXLEN,
            UBIDI_DO_MIRRORING,
            U_ZERO_ERROR,
            NULL,
            UInputType_pseudo16_char,
            -1,
            "Reordering Inverse Basic UBIDI_DO_MIRRORING",
            FALSE },
        };
    testCases[0].source = srcChars;
    testCases[0].sourceLength = srcLen;
    testCases[0].paraLevel = level;

    destLen = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);

    u16ToPseudo(destLen, dest, destChars);

    return destChars;
}

static UBool
_testReorderingModeAssertRoundTrip(UBiDi *pBiDi, 
    int32_t tc, 
    int32_t outIndex, 
    const char *srcChars,
    const char *destChars, 
    const UChar *dest, 
    int32_t destLen,
    int mode, 
    int option, 
    UBiDiLevel level)
{
    static const char roundtrip[TC_COUNT][MODES_COUNT][OPTIONS_COUNT]
                [LEVELS_COUNT] = {
        { /* TC 0: 123 */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 1: .123->4.5 */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 2: 678 */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 3: .678->8.9 */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 0,  0}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 4: MLK1.2,3JIH */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 5: FE.>12-> */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 0,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 6: JIH.>12->a */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 0,  0}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 7: CBA.>67->89=a */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 0,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 0,  0}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 8: CBA.>123->xyz */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 0,  0}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 9: .>12->xyz */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  0}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 10: a.>67->xyz */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  0}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 11: 123JIH */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        },
        { /* TC 12: 123 JIH */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_GROUP_NUMBERS_WITH_R */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_LIKE_DIRECT */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}, /* UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL */
            {{ 1,  1}, { 1,  1}}  /* UBIDI_REORDER_INVERSE_NUMBERS_AS_L */
        }
    };

    #define SET_ROUND_TRIP_MODE(mode) \
        ubidi_setReorderingMode(pBiDi, mode); \
        desc = #mode; \
        break;

    static UChar dest2[MAXLEN];
    int32_t destLen2;
    char destChars2[MAXLEN];
    char destChars3[MAXLEN];
    const char* desc;

    switch (testReorderingMode_modes[mode].value) {
        case UBIDI_REORDER_NUMBERS_SPECIAL:
            SET_ROUND_TRIP_MODE(UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL)
        case UBIDI_REORDER_GROUP_NUMBERS_WITH_R:
            SET_ROUND_TRIP_MODE(UBIDI_REORDER_GROUP_NUMBERS_WITH_R)
        case UBIDI_REORDER_RUNS_ONLY:
            SET_ROUND_TRIP_MODE(UBIDI_REORDER_RUNS_ONLY)
        case UBIDI_REORDER_INVERSE_NUMBERS_AS_L:
            SET_ROUND_TRIP_MODE(UBIDI_REORDER_DEFAULT)
        case UBIDI_REORDER_INVERSE_LIKE_DIRECT:
            SET_ROUND_TRIP_MODE(UBIDI_REORDER_DEFAULT)
        case UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL:
            SET_ROUND_TRIP_MODE(UBIDI_REORDER_NUMBERS_SPECIAL)
        default:
            SET_ROUND_TRIP_MODE(UBIDI_REORDER_INVERSE_LIKE_DIRECT)
    }
    ubidi_setReorderingOptions(pBiDi, UBIDI_OPTION_REMOVE_CONTROLS);

    UBidiWriteReorderedTestCases testCases[] = {
        { NULL,
            -1,
            0,
            NULL,
            dest2,
            MAXLEN,
            UBIDI_DO_MIRRORING,
            U_ZERO_ERROR,
            NULL,
            UInputType_UChar,
            -1,
            "Reordering Round Trip UBIDI_DO_MIRRORING",
            FALSE },
        };
    testCases[0].source = (const char*)dest;
    testCases[0].sourceLength = destLen;
    testCases[0].paraLevel = level;
    testCases[0].expectedChars = (const char*)dest;

    destLen2 = doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);

    u16ToPseudo(destLen, dest, destChars3);
    destLen2 = u16ToPseudo(destLen2, dest2, destChars2);

    checkWhatYouCan(pBiDi, destChars3, destChars2, __FILE__, __LINE__, UEncoding_U16);

    if (memcmp(srcChars, destChars2, destLen2)) {
        if (roundtrip[tc][mode][option][level]) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): Round trip failed for case=%d mode=%d option=%d.\n%20s %s\n%20s %s\n%20s %s\n%20s %s\n%20s %s\n%20s %u\n",
                __FILE__, __LINE__, "ubidi_writeReordered", Encodings[UEncoding_U16].description, tc, "_testReorderingModeAssertRoundTrip",
                tc, mode, option,
                "Original text:", srcChars,
                "Round-tripped text:", destChars2,
                "Intermediate  text:", srcChars,
                "Reordering mode:", testReorderingMode_modes[mode].description,
                "Reordering option:", testReorderingMode_options[option].description,
                "Paragraph level:", level);
        }
        else {
            log_verbose("\nExpected round trip failure for case=%d mode=%d option=%d.\n"
                "%20s %s\n%20s %s\n%20s %s\n%20s %s\n%20s %s\n%20s %u\n", 
                tc, mode, option,
                "Original text:", srcChars,
                "Round-tripped text:", destChars2,
                "Intermediate  text:", srcChars,
                "Reordering mode:", testReorderingMode_modes[mode].description,
                "Reordering option:", testReorderingMode_options[option].description,
                "Paragraph level:", level);
        }
        return FALSE;
    }

    if (!_testReorderingModeCheckResultLength(pBiDi, destChars, destChars2, destLen2,
        desc, "UBIDI_OPTION_REMOVE_CONTROLS", level)) {

        return FALSE;
    }

    if (outIndex > -1 && !_testReorderingModeCheckMaps(pBiDi, outIndex, srcChars, destChars,
        desc, "UBIDI_OPTION_REMOVE_CONTROLS",
        level, FALSE)) {

        return FALSE;
    }

    return TRUE;
}

static UBool
_testReorderingModeCheckResultLength(UBiDi *pBiDi, 
    const char *srcChars, 
    const char *destChars,
    int32_t destLen, 
    const char* mode,
    const char* option, 
    UBiDiLevel level)
{
    int32_t actualLen;

    if (strcmp(mode, "UBIDI_REORDER_INVERSE_NUMBERS_AS_L") == 0)
        actualLen = (int32_t)strlen(destChars);
    else
        actualLen = ubidi_getResultLength(pBiDi);

    if (actualLen != destLen) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): failed.\n%20s %7d\n%20s %7d\n%20s %s\n%20s %s\n%20s %s\n%20s %s\n%20s %u\n",
            __FILE__, __LINE__, "ubidi_getResultLength", Encodings[UEncoding_U16].description, 0, "_testReorderingModeAssertRoundTrip",
            "Expected:", destLen, "Actual:", actualLen,
            "Input:", srcChars, "Output:", destChars,
            "Reordering mode:", mode, "Reordering option:", option,
            "Paragraph level:", level);

        return FALSE;
    }

    return TRUE;
}

static char * formatMap(const int32_t * map, int len, char * buffer)
{
    int32_t i, k;
    char c;
    for (i = 0; i < len; i++) {
        k = map[i];
        if (k < 0)
            c = '-';
        else if (k >= sizeof(columns))
            c = '+';
        else
            c = columns[k];
        buffer[i] = c;
    }
    buffer[len] = '\0';
    return buffer;
}

static UBool
_testReorderingModeCheckMaps(UBiDi *pBiDi, 
    int32_t stringIndex, 
    const char *src, 
    const char *dest,
    const char *mode, 
    const char* option, 
    UBiDiLevel level, 
    UBool forward)
{
    int32_t actualLogicalMap[MAX_MAP_LENGTH];
    int32_t actualVisualMap[MAX_MAP_LENGTH];
    int32_t getIndexMap[MAX_MAP_LENGTH];
    int32_t i, srcLen, resLen, idx;
    const int32_t *expectedLogicalMap, *expectedVisualMap;
    UErrorCode rc = U_ZERO_ERROR;
    UBool testOK = TRUE;

    if (forward) {
        expectedLogicalMap = testReorderingMode_forwardMap[stringIndex];
        expectedVisualMap  = testReorderingMode_inverseMap[stringIndex];
    }
    else {
        expectedLogicalMap = testReorderingMode_inverseMap[stringIndex];
        expectedVisualMap  = testReorderingMode_forwardMap[stringIndex];
    }
    ubidi_getLogicalMap(pBiDi, actualLogicalMap, &rc);
    if (!assertSuccessful("ubidi_getLogicalMap", &rc)) {
        testOK = FALSE;
    }
    srcLen = ubidi_getProcessedLength(pBiDi);
    if (memcmp(expectedLogicalMap, actualLogicalMap, srcLen * sizeof(int32_t))) {
        char expChars[MAX_MAP_LENGTH];
        char actChars[MAX_MAP_LENGTH];
        log_err("%s(%d): %s(%s, tests[%d]: %s): ubidi_getLogicalMap() returns unexpected map for output string "
                "index %d\n"
                "source: %s\n"
                "dest  : %s\n"
                "Scale : %s\n"
                "ExpMap: %s\n"
                "Actual: %s\n"
                "Paragraph level  : %d == %d\n"
                "Reordering mode  : %s == %d\n"
                "Reordering option: %s == %d\n"
                "Forward flag     : %d\n",
            __FILE__, __LINE__, "ubidi_getLogicalMap", Encodings[UEncoding_U16].description, 0, "_testReorderingModeCheckMaps",
            stringIndex, src, dest, columns,
                formatMap(expectedLogicalMap, srcLen, expChars),
                formatMap(actualLogicalMap, srcLen, actChars),
                level, ubidi_getParaLevel(pBiDi),
                mode, ubidi_getReorderingMode(pBiDi),
                option, ubidi_getReorderingOptions(pBiDi),
                forward
                );
        testOK = FALSE;
    }

    resLen = ubidi_getResultLength(pBiDi);
    ubidi_getVisualMap(pBiDi, actualVisualMap, &rc);
    assertSuccessful("ubidi_getVisualMap", &rc);
    if (memcmp(expectedVisualMap, actualVisualMap, resLen * sizeof(int32_t))) {
        char expChars[MAX_MAP_LENGTH];
        char actChars[MAX_MAP_LENGTH];
        log_err("%s(%d): %s(%s, tests[%d]: %s): ubidi_getVisualMap() returns unexpected map for output string "
                "index %d\n"
                "source: %s\n"
                "dest  : %s\n"
                "Scale : %s\n"
                "ExpMap: %s\n"
                "Actual: %s\n"
                "Paragraph level  : %d == %d\n"
                "Reordering mode  : %s == %d\n"
                "Reordering option: %s == %d\n"
                "Forward flag     : %d\n",
            __FILE__, __LINE__, "ubidi_getVisualMap", Encodings[UEncoding_U16].description, 0, "_testReorderingModeCheckMaps",
            stringIndex, src, dest, columns,
                formatMap(expectedVisualMap, resLen, expChars),
                formatMap(actualVisualMap, resLen, actChars),
                level, ubidi_getParaLevel(pBiDi),
                mode, ubidi_getReorderingMode(pBiDi),
                option, ubidi_getReorderingOptions(pBiDi),
                forward
                );
        testOK = FALSE;
    }
    for (i = 0; i < srcLen; i++) {
        idx = ubidi_getVisualIndex(pBiDi, i, &rc);
        assertSuccessful("ubidi_getVisualIndex", &rc);
        getIndexMap[i] = idx;
    }
    if (memcmp(actualLogicalMap, getIndexMap, srcLen * sizeof(int32_t))) {
        char actChars[MAX_MAP_LENGTH];
        char gotChars[MAX_MAP_LENGTH];
        log_err("%s(%d): %s(%s, tests[%d]: %s): Mismatch between ubidi_getLogicalMap and ubidi_getVisualIndex for output string "
                "index %d\n"
                "source: %s\n"
                "dest  : %s\n"
                "Scale : %s\n"
                "ActMap: %s\n"
                "IdxMap: %s\n"
                "Paragraph level  : %d == %d\n"
                "Reordering mode  : %s == %d\n"
                "Reordering option: %s == %d\n"
                "Forward flag     : %d\n",
            __FILE__, __LINE__, "ubidi_getVisualIndex", Encodings[UEncoding_U16].description, 0, "_testReorderingModeCheckMaps",
            stringIndex, src, dest, columns,
                formatMap(actualLogicalMap, srcLen, actChars),
                formatMap(getIndexMap, srcLen, gotChars),
                level, ubidi_getParaLevel(pBiDi),
                mode, ubidi_getReorderingMode(pBiDi),
                option, ubidi_getReorderingOptions(pBiDi),
                forward
                );
        testOK = FALSE;
    }
    for (i = 0; i < resLen; i++) {
        idx = ubidi_getLogicalIndex(pBiDi, i, &rc);
        assertSuccessful("ubidi_getLogicalIndex", &rc);
        getIndexMap[i] = idx;
    }
    if (memcmp(actualVisualMap, getIndexMap, resLen * sizeof(int32_t))) {
        char actChars[MAX_MAP_LENGTH];
        char gotChars[MAX_MAP_LENGTH];
        log_err("%s(%d): %s(%s, tests[%d]: %s): Mismatch between ubidi_getVisualMap and ubidi_getLogicalIndex for output string "
                "index %d\n"
                "source: %s\n"
                "dest  : %s\n"
                "Scale : %s\n"
                "ActMap: %s\n"
                "IdxMap: %s\n"
                "Paragraph level  : %d == %d\n"
                "Reordering mode  : %s == %d\n"
                "Reordering option: %s == %d\n"
                "Forward flag     : %d\n",
            __FILE__, __LINE__, "ubidi_getLogicalIndex", Encodings[UEncoding_U16].description, 0, "_testReorderingModeCheckMaps",
            stringIndex, src, dest, columns,
                formatMap(actualVisualMap, resLen, actChars),
                formatMap(getIndexMap, resLen, gotChars),
                level, ubidi_getParaLevel(pBiDi),
                mode, ubidi_getReorderingMode(pBiDi),
                option, ubidi_getReorderingOptions(pBiDi),
                forward
                );
        testOK = FALSE;
    }
    return testOK;
}

// complex/bidi/TestReorderRunsOnly

static void
testReorderRunsOnly(void) {
    static const struct {
        const char* textIn;
        const char* textOut[2][2];
        const char noroundtrip[2];
    } testCases[] = {
        {"ab 234 896 de", {{"de 896 ab 234", "de 896 ab 234"},                   /*0*/
                           {"ab 234 @896@ de", "de 896 ab 234"}}, {0, 0}},
        {"abcGHI", {{"GHIabc", "GHIabc"}, {"GHIabc", "GHIabc"}}, {0, 0}},        /*1*/
        {"a.>67->", {{"<-67<.a", "<-67<.a"}, {"<-67<.a", "<-67<.a"}}, {0, 0}},   /*2*/
        {"-=%$123/ *", {{"* /%$123=-", "* /%$123=-"},                            /*3*/
                        {"* /%$123=-", "* /%$123=-"}}, {0, 0}},
        {"abc->12..>JKL", {{"JKL<..12<-abc", "JKL<..abc->12"},                   /*4*/
                           {"JKL<..12<-abc", "JKL<..abc->12"}}, {0, 0}},
        {"JKL->12..>abc", {{"abc<..JKL->12", "abc<..12<-JKL"},                   /*5*/
                           {"abc<..JKL->12", "abc<..12<-JKL"}}, {0, 0}},
        {"123->abc", {{"abc<-123", "abc<-123"},                                  /*6*/
                      {"abc&<-123", "abc<-123"}}, {1, 0}},
        {"123->JKL", {{"JKL<-123", "123->JKL"},                                  /*7*/
                      {"JKL<-123", "JKL<-@123"}}, {0, 1}},
        {"*>12.>34->JKL", {{"JKL<-34<.12<*", "12.>34->JKL<*"},                   /*8*/
                           {"JKL<-34<.12<*", "JKL<-@34<.12<*"}}, {0, 1}},
        {"*>67.>89->JKL", {{"67.>89->JKL<*", "67.>89->JKL<*"},                   /*9*/
                           {"67.>89->JKL<*", "67.>89->JKL<*"}}, {0, 0}},
        {"* /abc-=$%123", {{"$%123=-abc/ *", "abc-=$%123/ *"},                   /*10*/
                           {"$%123=-abc/ *", "abc-=$%123/ *"}}, {0, 0}},
        {"* /$%def-=123", {{"123=-def%$/ *", "def-=123%$/ *"},                   /*11*/
                           {"123=-def%$/ *", "def-=123%$/ *"}}, {0, 0}},
        {"-=GHI* /123%$", {{"GHI* /123%$=-", "123%$/ *GHI=-"},                   /*12*/
                           {"GHI* /123%$=-", "123%$/ *GHI=-"}}, {0, 0}},
        {"-=%$JKL* /123", {{"JKL* /%$123=-", "123/ *JKL$%=-"},                   /*13*/
                           {"JKL* /%$123=-", "123/ *JKL$%=-"}}, {0, 0}},
        {"ab =#CD *?450", {{"CD *?450#= ab", "450?* CD#= ab"},                   /*14*/
                           {"CD *?450#= ab", "450?* CD#= ab"}}, {0, 0}},
        {"ab 234 896 de", {{"de 896 ab 234", "de 896 ab 234"},                   /*15*/
                           {"ab 234 @896@ de", "de 896 ab 234"}}, {0, 0}},
        {"abc-=%$LMN* /123", {{"LMN* /%$123=-abc", "123/ *LMN$%=-abc"},          /*16*/
                              {"LMN* /%$123=-abc", "123/ *LMN$%=-abc"}}, {0, 0}},
        {"123->JKL&MN&P", {{"JKLMNP<-123", "123->JKLMNP"},                       /*17*/
                           {"JKLMNP<-123", "JKLMNP<-@123"}}, {0, 1}},
        {"123", {{"123", "123"},                /* just one run */               /*18*/
                 {"123", "123"}}, {0, 0}}
    };

    static UChar dest[MAXLEN];
    int32_t destLen = 0;
    char destChars[MAXLEN];
    int32_t destCharsLen = 0;

    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t option, i, j, nCases, paras;

    log_verbose("\nEntering TestReorderRunsOnly\n\n");

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testReorderRunsOnly");

        return;
    }

    UBiDi *pL2VBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testReorderRunsOnly");

        ubidi_close(pBiDi);
        return;
    }

    ubidi_setReorderingMode(pBiDi, UBIDI_REORDER_RUNS_ONLY);
    ubidi_setReorderingOptions(pL2VBiDi, UBIDI_OPTION_REMOVE_CONTROLS);

    for (option = 0; option < 2; option++) {
        ubidi_setReorderingOptions(pBiDi, option == 0 ? UBIDI_OPTION_REMOVE_CONTROLS
            : UBIDI_OPTION_INSERT_MARKS);

        for (i = 0, nCases = UPRV_LENGTHOF(testCases); i < nCases; i++) {
            for (j = 0; j < 2; j++) {
                log_verbose("Now doing test for option %d, case %d, level %d\n", i, option, j);
                UBiDiLevel level = testReorderingMode_paraLevels[j];

                {
                    UBidiWriteReorderedTestCases writeReorderedTestCases[] = {
                        { NULL,
                            -1,
                            0,
                            NULL,
                            dest,
                            MAXLEN,
                            UBIDI_DO_MIRRORING,
                            U_ZERO_ERROR,
                            NULL,
                            UInputType_pseudo16_char,
                            -1,
                            "testReorderRunsOnly() UBIDI_DO_MIRRORING",
                            FALSE,
                            TRUE},
                    };
                    writeReorderedTestCases[0].source = testCases[i].textIn;
                    writeReorderedTestCases[0].paraLevel = level;
                    writeReorderedTestCases[0].expectedChars = testCases[i].textOut[option][level];

                    destLen = doWriteReordered(pBiDi, writeReorderedTestCases, 0, __FILE__, __LINE__);
                }

                destCharsLen = u16ToPseudo(destLen, dest, destChars);

                checkWhatYouCan(pBiDi, testCases[i].textIn, destChars, __FILE__, __LINE__, UEncoding_U16);

                if (memcmp(testCases[i].textOut[option][level], destChars, destCharsLen)) {
                    char formatChars[MAXLEN];

                    log_err("%s(%d): %s(%s, tests[%d]: %s): Actual and expected output mismatch.\n"
                        "Input\t: %s\nGot\t: %s\nExpected: %s\nLevels\t: %s\nReordering mode\t: %d %s\nParagraph level\t: %u\nReordering option\t: %d %s\n",
                        __FILE__, __LINE__, "ubidi_getReorderingOptions", Encodings[UEncoding_U16].description, i, "testReorderRunsOnly",
                        testCases[i].textOut[option][level], destChars, testCases[i].textIn, formatLevels(pBiDi, formatChars),
                        ubidi_getReorderingMode(pBiDi), "UBIDI_REORDER_RUNS_ONLY", ubidi_getParaLevel(pBiDi), ubidi_getReorderingOptions(pBiDi),
                        option == 0 ? "0" : "UBIDI_OPTION_INSERT_MARKS");
                }

                if ((option == 0) && testCases[i].noroundtrip[level]) {
                    continue;
                }

                static UChar visual1[MAXLEN];
                static UChar visual2[MAXLEN];
                int32_t vis1Len = 0;
                int32_t vis2Len = 0;
                char vis1Chars[MAXLEN];
                char vis2Chars[MAXLEN];

                {
                    UBidiWriteReorderedTestCases writeReorderedTestCases[] = {
                        { NULL,
                            -1,
                            0,
                            NULL,
                            visual1,
                            MAXLEN,
                            UBIDI_DO_MIRRORING,
                            U_ZERO_ERROR,
                            0, // Compare occurs below
                            UInputType_pseudo16_char,
                            -1,
                            "testReorderRunsOnly() UBIDI_DO_MIRRORING",
                            FALSE,
                            TRUE },
                    };
                    writeReorderedTestCases[0].source = testCases[i].textIn;
                    writeReorderedTestCases[0].paraLevel = level;

                    vis1Len = doWriteReordered(pL2VBiDi, writeReorderedTestCases, 0, __FILE__, __LINE__);
                }

                u16ToPseudo(vis1Len, visual1, vis1Chars);

                checkWhatYouCan(pL2VBiDi, testCases[i].textIn, vis1Chars, __FILE__, __LINE__, UEncoding_U16);

                {
                    UBidiWriteReorderedTestCases writeReorderedTestCases[] = {
                        { NULL,
                            -1,
                            0,
                            NULL,
                            visual2,
                            MAXLEN,
                            UBIDI_DO_MIRRORING,
                            U_ZERO_ERROR,
                            NULL,
                            UInputType_pseudo16_char,
                            -1,
                            "testReorderRunsOnly() UBIDI_DO_MIRRORING",
                            FALSE,
                            TRUE },
                    };
                    writeReorderedTestCases[0].source = destChars;
                    writeReorderedTestCases[0].sourceLength = destCharsLen;
                    writeReorderedTestCases[0].paraLevel = level ^ 1;
                    writeReorderedTestCases[0].expectedChars = vis1Chars;

                    vis2Len = doWriteReordered(pL2VBiDi, writeReorderedTestCases, 0, __FILE__, __LINE__);
                }

                u16ToPseudo(vis2Len, visual2, vis2Chars);

                checkWhatYouCan(pL2VBiDi, destChars, vis2Chars, __FILE__, __LINE__, UEncoding_U16);

                if (memcmp(vis1Chars, vis2Chars, destCharsLen)) {
                    log_err("%s(%d): %s(%s, tests[%d]: %s): Actual and expected output mismatch.\n"
                        "Reordering mode\t: %d %s\nParagraph level\t: %u\nReordering option\t: %d %s\n",
                        __FILE__, __LINE__, "ubidi_getReorderingOptions", Encodings[UEncoding_U16].description, i, "testReorderingMode",
                        ubidi_getReorderingMode(pBiDi), "UBIDI_REORDER_RUNS_ONLY", ubidi_getParaLevel(pBiDi), ubidi_getReorderingOptions(pBiDi),
                        option == 0 ? "0" : "UBIDI_OPTION_INSERT_MARKS");
                }
            }
        }
    }

    // Test with null or empty text
    ubidi_setPara(pBiDi, NULL, 0, UBIDI_LTR, NULL, &errorCode);
    if ((errorCode != U_STRING_NOT_TERMINATED_WARNING) && (errorCode != U_ZERO_ERROR)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_setPara", Encodings[UEncoding_U16].description, 0, "testReorderingMode",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
    }

    paras = ubidi_countParagraphs(pBiDi);
    if (paras != 0) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): Invalid number of paras (should be 0): %d\n",
            __FILE__, __LINE__, "ubidi_countParagraphs", Encodings[UEncoding_U16].description, 0, "testReorderingMode",
            paras);
    }

    ubidi_close(pBiDi);
    ubidi_close(pL2VBiDi);

    log_verbose("\nExiting TestReorderRunsOnly\n\n");
}

// complex/bidi/TestMultipleParagraphs

static void
testMultipleParagraphs(void) {
    static const char* const text = "__ABC\\u001c"          /* Para #0 offset 0 */
        "__\\u05d0DE\\u001c"    /*       1        6 */
        "__123\\u001c"          /*       2       12 */
        "\\u000d\\u000a"        /*       3       18 */
        "FG\\u000d"             /*       4       20 */
        "\\u000d"               /*       5       23 */
        "HI\\u000d\\u000a"      /*       6       24 */
        "\\u000d\\u000a"        /*       7       28 */
        "\\u000a"               /*       8       30 */
        "\\u000a"               /*       9       31 */
        "JK\\u001c";            /*      10       32 */
    static const int32_t paraCount = 11;

    static const int32_t paraBounds[3][12] = {
        { 0, 6, 12, 18, 20, 23, 24, 28, 30, 31, 32, 35 }, // U16
        { 0, 6, 13, 19, 21, 24, 25, 29, 31, 32, 33, 36 }, // U8
        { 0, 6, 12, 18, 20, 23, 24, 28, 30, 31, 32, 35 }  // U32
    };

    static const UBiDiLevel paraLevels[] = { UBIDI_LTR, UBIDI_RTL, UBIDI_DEFAULT_LTR, UBIDI_DEFAULT_RTL, 22, 23 };
    static const int32_t rtlLevels[3][2] = {
        { 26, 32 }, // U16
        { 28, 33 }, // U8
        { 26, 32 }  // U32
    };

    static const UBiDiLevel multiLevels[6][11] = { {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                                  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
                                                  {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                                  {0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0},
                                                  {22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22},
                                                  {23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23} };

    static const char* const text2 = "\\u05d0 1-2\\u001c\\u0630 1-2\\u001c1-2";

    static const UBiDiLevel levels2[3][15] = {
        { 1,1,2,2,2,0, 1,1,2,1,2,0, 2,2,2 }, // U16
        { 1,1,1,2,2,2, 0,1,1,1,2,1, 2,0,2 }, // U8
        { 1,1,2,2,2,0, 1,1,2,1,2,0, 2,2,2 }  // U32
    };

    static UBiDiLevel myLevels[10] = { 0,0,0,0,0,0,0,0,0,0 };
    static const UChar multiparaTestString[] = {
        0x5de, 0x5e0, 0x5e1, 0x5d4, 0x20,  0x5e1, 0x5e4, 0x5da,
        0x20,  0xa,   0xa,   0x41,  0x72,  0x74,  0x69,  0x73,
        0x74,  0x3a,  0x20,  0x5de, 0x5e0, 0x5e1, 0x5d4, 0x20,
        0x5e1, 0x5e4, 0x5da, 0x20,  0xa,   0xa,   0x41,  0x6c,
        0x62,  0x75,  0x6d,  0x3a,  0x20,  0x5de, 0x5e0, 0x5e1,
        0x5d4, 0x20,  0x5e1, 0x5e4, 0x5da, 0x20,  0xa,   0xa,
        0x54,  0x69,  0x6d,  0x65,  0x3a,  0x20,  0x32,  0x3a,
        0x32,  0x37,  0xa,  0xa
    };
    static const UBiDiLevel multiparaTestLevels[3][60] = {
        { // U16
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 1,
            1, 1, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 1, 1,
            1, 1, 1, 1, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0
        },
        { // U8
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 1, 1, 1, 1, 1, 1,
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 1,
            1, 1, 1, 1
        },
        { // U32
            1, 1, 1, 1, 1, 1, 1, 1,
            1, 1, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 1, 1, 1, 1, 1,
            1, 1, 1, 0, 0, 0, 0, 0,
            0, 0, 0, 0, 0, 1, 1, 1,
            1, 1, 1, 1, 1, 0, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0
        }
    };

    UBiDiLevel gotLevel;
    const UBiDiLevel* gotLevels;
    UBool orderParagraphsLTR;
    UChar src[MAXLEN];
    static UChar dest[MAXLEN];
    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t srcSize, count, paraStart, paraLimit, paraIndex, length;
    int32_t destLen;
    int i, j, k;

    uint8_t u8BufSrc[MAXLEN];
    UText* ut = 0;

    log_verbose("\nEntering TestMultipleParagraphs\n\n");

    UBiDi* pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "testMultipleParagraphs");

        return;
    }

    UBiDi* pLine;
    int32_t encoding = 0;

    u_unescape(text, src, MAXLEN);
    srcSize = u_strlen(src);

    // Check paragraph count and boundaries 
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (doSetPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            if (paraCount != (count = ubidi_countParagraphs(pBiDi))) {
                log_err("%s(%d): %s(%s): count %d (expected %d)\n",
                    __FILE__, __LINE__, "ubidi_countParagraphs", Encodings[encoding].description,
                    count, paraCount);
            }

            for (i = 0; i < paraCount; i++) {
                errorCode = U_ZERO_ERROR;
                ubidi_getParagraphByIndex(pBiDi, i, &paraStart, &paraLimit, NULL, &errorCode);
                if ((paraStart != paraBounds[encoding][i]) || (paraLimit != paraBounds[encoding][i + 1])) {
                    log_err("%s(%d): %s(%s): boundaries of paragraph %d: %d-%d (expected: %d-%d)\n",
                        __FILE__, __LINE__, "ubidi_getParagraphByIndex", Encodings[encoding].description,
                        i, paraStart, paraLimit, paraBounds[encoding][i], paraBounds[encoding][i + 1]);
                }
            }
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    src[srcSize - 1] = 'L';

    // Check with last paragraph not terminated by B
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (doSetPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            if (paraCount != (count = ubidi_countParagraphs(pBiDi))) {
                log_err("%s(%d): %s(%s): count %d (expected %d)\n",
                    __FILE__, __LINE__, "ubidi_countParagraphs", Encodings[encoding].description,
                    count, paraCount);
            }

            errorCode = U_ZERO_ERROR;
            i = paraCount - 1;
            ubidi_getParagraphByIndex(pBiDi, i, &paraStart, &paraLimit, NULL, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getParagraphByIndex", Encodings[encoding].description,
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }

            if ((paraStart != paraBounds[encoding][i]) || (paraLimit != paraBounds[encoding][i + 1])) {
                log_err("%s(%d): %s(%s): boundaries of paragraph %d: %d-%d (expected: %d-%d)\n",
                    __FILE__, __LINE__, "ubidi_getParagraphByIndex", Encodings[encoding].description,
                    i, paraStart, paraLimit, paraBounds[encoding][i], paraBounds[encoding][i + 1]);
            }
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    // Check paraLevel for all paragraphs under various paraLevel specs
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        for (k = 0; k < 6; k++) {
            if (doSetPara(pBiDi, src, srcSize, paraLevels[k], NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
                for (i = 0; i < paraCount; i++) {
                    errorCode = U_ZERO_ERROR;
                    paraIndex = ubidi_getParagraph(pBiDi, paraBounds[encoding][i], NULL, NULL, &gotLevel, &errorCode);
                    if (!U_SUCCESS(errorCode)) {
                        log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
                            __FILE__, __LINE__, "ubidi_getParagraph", Encodings[encoding].description,
                            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
                    }

                    if (paraIndex != i) {
                        log_err("%s(%d): %s(%s): paraLevel %d, paragraph %d, found paragraph index %d (expected %d)\n",
                            __FILE__, __LINE__, "ubidi_getParagraph", Encodings[encoding].description,
                            paraLevels[k], i, paraIndex, i);
                    }

                    if (gotLevel != multiLevels[k][i]) {
                        log_err("%s(%d): %s(%s): paraLevel %d, paragraph %d, found level %d (expected %d)\n",
                            __FILE__, __LINE__, "ubidi_getParagraph", Encodings[encoding].description,
                            paraLevels[k], i, gotLevel, multiLevels[k][i]);
                    }
                }

                gotLevel = ubidi_getParaLevel(pBiDi);
                if (gotLevel != multiLevels[k][0]) {
                    log_err("%s(%d): %s(%s): paraLevel %d, found getParaLevel %d (expected %d)\n",
                        __FILE__, __LINE__, "ubidi_getParaLevel", Encodings[encoding].description,
                        paraLevels[k], i, gotLevel, multiLevels[k][i]);
                }
            }


            if (utext_isValid(ut)) {
                ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

                utext_close(ut);
                ut = 0;
            }
        }
    }

    src[0] = 0x05d2; // Hebrew letter Gimel

    // Check that the result of ubidi_getParaLevel changes if the first
    // paragraph has a different level
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (doSetPara(pBiDi, src, srcSize, UBIDI_DEFAULT_LTR, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            gotLevel = ubidi_getParaLevel(pBiDi);
            if (gotLevel != UBIDI_RTL) {
                log_err("%s(%d): %s(%s): paraLevel UBIDI_DEFAULT_LTR, found getParaLevel %d (expected %d)\n",
                    __FILE__, __LINE__, "ubidi_getParaLevel", Encodings[encoding].description,
                    gotLevel, UBIDI_RTL);
            }
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    // Set back to underscore so that paraBounds are correct.
    // Hebrew letter Gimel = 2 code points, underscore = 1 code point.
    src[0] = '_'; 

    // Check that line cannot overlap paragraph boundaries
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (doSetPara(pBiDi, src, srcSize, UBIDI_DEFAULT_LTR, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            pLine = ubidi_open();
            if (pLine == NULL) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
                    __FILE__, __LINE__, "ubidi_open", Encodings[encoding].description, 0, "testMultipleParagraphs");

                return;
            }

            errorCode = U_ZERO_ERROR;
            i = paraBounds[encoding][1];
            k = paraBounds[encoding][2] + 1;
            ubidi_setLine(pBiDi, i, k, pLine, &errorCode);
            if (U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): for line limits %d-%d got success %s\n",
                    __FILE__, __LINE__, "ubidi_setLine", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    i, k, u_errorName(errorCode));
            }

            errorCode = U_ZERO_ERROR;
            i = paraBounds[encoding][1];
            k = paraBounds[encoding][2];
            ubidi_setLine(pBiDi, i, k, pLine, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): for line limits %d-%d got error %s\n",
                    __FILE__, __LINE__, "ubidi_setLine", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    i, k, u_errorName(errorCode));

                errorCode = U_ZERO_ERROR;
            }
        }
    }

    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        // Get levels through para Line block
        if (doSetPara(pBiDi, src, srcSize, UBIDI_RTL, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            i = paraBounds[encoding][1];
            k = paraBounds[encoding][2];
            ubidi_setLine(pBiDi, i, k, pLine, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): for line limits %d-%d got error %s\n",
                    __FILE__, __LINE__, "ubidi_setLine", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    i, k, u_errorName(errorCode));

                ubidi_close(pLine);
                ubidi_close(pBiDi);
                return;
            }

            errorCode = U_ZERO_ERROR;
            paraIndex = ubidi_getParagraph(pLine, i, &paraStart, &paraLimit, &gotLevel, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getParagraph", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }

            errorCode = U_ZERO_ERROR;
            gotLevels = ubidi_getLevels(pLine, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

                ubidi_close(pLine);
                ubidi_close(pBiDi);
                return;
            }

            length = ubidi_getLength(pLine);
            if ((gotLevel != UBIDI_RTL) || (gotLevels[length - 1] != UBIDI_RTL)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): for paragraph %d with limits %d-%d, paraLevel=%d expected=%d, level of separator=%d expected=%d\n",
                    __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    paraIndex, paraStart, paraLimit, gotLevel, UBIDI_RTL, gotLevels[length - 1], UBIDI_RTL);
            }
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

            utext_close(ut);
            ut = 0;
        }

        orderParagraphsLTR = ubidi_isOrderParagraphsLTR(pBiDi);
        if (orderParagraphsLTR) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): found orderParagraphsLTR=%d expected=%d\n",
                __FILE__, __LINE__, "ubidi_isOrderParagraphsLTR", Encodings[UEncoding_U16].description, 0, "testMultipleParagraphs",
                orderParagraphsLTR, FALSE);
        }

        ubidi_orderParagraphsLTR(pBiDi, TRUE);
        orderParagraphsLTR = ubidi_isOrderParagraphsLTR(pBiDi);
        if (!orderParagraphsLTR) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): found orderParagraphsLTR=%d expected=%d\n"
                __FILE__, __LINE__, "ubidi_isOrderParagraphsLTR", Encodings[UEncoding_U16].description, 0, "testMultipleParagraphs",
                orderParagraphsLTR, TRUE);
        }

        // Check level of block separator at end of paragraph when orderParagraphsLTR==TRUE

        if (doSetPara(pBiDi, src, srcSize, UBIDI_RTL, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            // Get levels through para Bidi block
            gotLevels = ubidi_getLevels(pBiDi, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }

            for (i = rtlLevels[encoding][0]; i < rtlLevels[encoding][1]; i++) {
                if (gotLevels[i] != 0) {
                    log_err("%s(%d): %s(%s): char %d(%04x), level %d (expected %d)\n",
                        __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                        i, src[i], gotLevels[i], 0);
                }
            }

            // Get levels through para Line block
            errorCode = U_ZERO_ERROR;
            ubidi_setLine(pBiDi, paraStart, paraLimit, pLine, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_setLine", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }

            paraIndex = ubidi_getParagraph(pLine, i, &paraStart, &paraLimit, &gotLevel, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getParagraph", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }

            gotLevels = ubidi_getLevels(pLine, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

                ubidi_close(pLine);
                ubidi_close(pBiDi);
                return;
            }

            length = ubidi_getLength(pLine);
            if ((gotLevel != UBIDI_RTL) || (gotLevels[length - 1] != 0)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): for paragraph %d with limits %d-%d, paraLevel=%d expected=%d, level of separator=%d expected=%d\n",
                    __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description, 0, "testMultipleParagraphs",
                    paraIndex, paraStart, paraLimit, gotLevel, UBIDI_RTL, gotLevels[length - 1], 0);

                log_verbose("levels=");
                for (count = 0; count < length; count++) {
                    log_verbose(" %d", gotLevels[count]);
                }
                log_verbose("\n");
            }
        }

        ubidi_orderParagraphsLTR(pBiDi, FALSE);

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    u_unescape(text, src, MAXLEN); // Restore original content
    srcSize = u_strlen(src);
    ubidi_orderParagraphsLTR(pBiDi, FALSE);

    // Test that the concatenation of separate invocations of the bidi code
    // on each individual paragraph in order matches the levels array that
    // results from invoking bidi once over the entire multiparagraph tests
    // (with orderParagraphsLTR false, of course)
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (doSetPara(pBiDi, src, srcSize, UBIDI_DEFAULT_RTL, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            errorCode = U_ZERO_ERROR;
            gotLevels = ubidi_getLevels(pBiDi, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }

            for (i = 0; i < paraCount; i++) {
                // Use pLine for individual paragraphs
                paraStart = paraBounds[encoding][i];
                length = paraBounds[encoding][i + 1] - paraStart;
                if (doSetPara(pLine, src + paraBounds[UEncoding_U16][i], length, UBIDI_DEFAULT_RTL, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
                    for (j = 0; j < length; j++) {
                        if ((k = ubidi_getLevelAt(pLine, j)) != (gotLevel = gotLevels[paraStart + j])) {
                            log_err("%s(%d): %s(%s): Checking paragraph concatenation: for paragraph=%d, char %d(%04x), level %d (expected %d)\n",
                                __FILE__, __LINE__, "ubidi_getLevelAt", Encodings[encoding].description,
                                i, j, src[paraStart + j], k, gotLevel);
                        }
                    }
                }
            }
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    u_unescape(text2, src, MAXLEN);
    srcSize = u_strlen(src);
    ubidi_orderParagraphsLTR(pBiDi, TRUE);

    // Ensure that leading numerics in a paragraph are not treated as arabic
    // numerals because of arabic text in a preceding paragraph
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (doSetPara(pBiDi, src, srcSize, UBIDI_RTL, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            errorCode = U_ZERO_ERROR;
            gotLevels = ubidi_getLevels(pBiDi, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }

            for (i = 0; i < srcSize; i++) {
                if (gotLevels[i] != levels2[encoding][i]) {
                    log_err("%s(%d): %s(%s): Checking leading numerics: for char %d(%04x), level %d (expected %d)\n",
                        __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                        i, src[i], gotLevels[i], levels2[encoding][i]);
                }
            }
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    u_memset(src, 0x0020, MAXLEN);
    srcSize = 5;
    ubidi_orderParagraphsLTR(pBiDi, TRUE);

    // Check handling of whitespace before end of paragraph separator when
    // orderParagraphsLTR==TRUE, when last paragraph has, and lacks, a terminating B
    for (i = 0x001c; i <= 0x0020; i += (0x0020 - 0x001c)) {
        src[4] = (UChar)i; // with and without terminating B
        for (j = 0x0041; j <= 0x05d0; j += (0x05d0 - 0x0041)) {
            src[0] = (UChar)j; // leading 'A' or Alef
            for (gotLevel = 4; gotLevel <= 5; gotLevel++) {
                for (encoding = 0; encoding < EncodingsCount; encoding++)
                {
                    // Test even and odd paraLevel
                    if (doSetPara(pBiDi, src, srcSize, gotLevel, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
                        errorCode = U_ZERO_ERROR;
                        gotLevels = ubidi_getLevels(pBiDi, &errorCode);
                        if (!U_SUCCESS(errorCode)) {
                            log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
                                __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                                u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
                        }

                        if ((encoding == UEncoding_U8) && (j == 0x05d0)) {
                            for (k = 2; k <= 4; k++) {
                                if (gotLevels[k] != gotLevel) {
                                    log_err("%s(%d): %s(%s): Checking trailing spaces: for leading_char %04x, last_char %04x, index %d, level %d (expected %d)\n",
                                        __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                                        src[0], src[4], k, gotLevels[k], gotLevel);
                                }
                            }
                        }
                        else {
                            for (k = 1; k <= 3; k++) {
                                if (gotLevels[k] != gotLevel) {
                                    log_err("%s(%d): %s(%s): Checking trailing spaces: for leading_char %04x, last_char %04x, index %d, level %d (expected %d)\n",
                                        __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                                        src[0], src[4], k, gotLevels[k], gotLevel);
                                }
                            }
                        }
                    }

                    if (utext_isValid(ut)) {
                        ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

                        utext_close(ut);
                        ut = 0;
                    }
                }
            }
        }
    }

    static const char reorderTextIn1[] = "abc \\u05d2\\u05d1\n";
    static const char reorderTextOut1[] = "\\u05d1\\u05d2 abc\n";
    static const char reorderTextIn2[] = "abc \\u05d2\\u05d1";
    static const char reorderTextOut2[] = "\\u05d1\\u05d2 abc";
    static const char reorderTextIn3[] = "ab\\u05d1\\u05d2\n\\u05d3\\u05d4123";
    static const char reorderTextOut3[] = "ab\\u05d2\\u05d1\\n123\\u05d4\\u05d3";

    static const UBidiWriteReorderedTestCases testCases[] = {
        // Check default orientation when inverse bidi and paragraph starts
        // with LTR strong char and ends with RTL strong char, with and without
        // a terminating B
        { reorderTextIn1,
            -1,
            UBIDI_DEFAULT_LTR,
            NULL,
            dest,
            MAXLEN,
            0,
            U_ZERO_ERROR,
            reorderTextOut1,
            UInputType_unescape_char,
            -1,
            "testMultipleParagraphs",
            FALSE },
        { reorderTextIn2,
            -1,
            UBIDI_DEFAULT_LTR,
            NULL,
            dest,
            MAXLEN,
            0,
            U_ZERO_ERROR,
            reorderTextOut2,
            UInputType_unescape_char,
            -1,
            "testMultipleParagraphs",
            FALSE },
        // Check multiple paragraphs together with explicit levels
        { reorderTextIn3,
            -1,
            UBIDI_LTR,
            myLevels,
            dest,
            MAXLEN,
            0,
            U_ZERO_ERROR,
            reorderTextOut3,
            UInputType_unescape_char,
            -1,
            "testMultipleParagraphs",
            FALSE },
    };

    int32_t nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    for (i = 0; i < nTestCases; i++) {
        if (i == 0) {
            // Check default orientation when inverse bidi and paragraph starts
            // with LTR strong char and ends with RTL strong char, with and without
            // a terminating B.
            ubidi_setReorderingMode(pBiDi, UBIDI_REORDER_INVERSE_LIKE_DIRECT);
        }
        else if (i == 2) {
            // Check multiple paragraphs together with explicit levels
            ubidi_setReorderingMode(pBiDi, UBIDI_REORDER_DEFAULT);
        }

        destLen = doWriteReordered(pBiDi, testCases, i, __FILE__, __LINE__);
    }

    count = ubidi_countParagraphs(pBiDi);
    if (count != 2) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): invalid number of paras, should be 2, got %d\n",
            __FILE__, __LINE__, "ubidi_countParagraphs", Encodings[UEncoding_U16].description, 0, "testMultipleParagraphs",
            count);
    }

    ubidi_close(pLine);
    ubidi_close(pBiDi);

    pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "testMultipleParagraphs");

        return;
    }

    // Check levels in multiple paragraphs with default para level
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (doSetPara(pBiDi, multiparaTestString, UPRV_LENGTHOF(multiparaTestString), UBIDI_DEFAULT_LTR, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            errorCode = U_ZERO_ERROR;
            gotLevels = ubidi_getLevels(pBiDi, &errorCode);
            if (!U_SUCCESS(errorCode)) {
                log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
                    __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                    u_errorName(errorCode), u_errorName(U_ZERO_ERROR));
            }

            for (i = 0; i < UPRV_LENGTHOF(multiparaTestString); i++) {
                if (gotLevels[i] != multiparaTestLevels[encoding][i]) {
                    log_err("%s(%d): %s(%s): Error on level for multiparaTestString at index %d, level %d (expected %d)\n",
                        __FILE__, __LINE__, "ubidi_getLevels", Encodings[encoding].description,
                        i, gotLevels[i], multiparaTestLevels[encoding][i]);
                }
            }

            if (utext_isValid(ut)) {
                ubidi_setPara(pBiDi, src, srcSize, UBIDI_LTR, NULL, &errorCode);

                utext_close(ut);
                ut = 0;
            }
        }
    }

    ubidi_close(pBiDi);

    log_verbose("\nExiting TestMultipleParagraphs\n\n");
}

// complex/bidi/TestStreaming

#define NULL_CHAR '\0'
#define MAXPORTIONS 10

static void
testStreaming(void)
{
    static const struct {
        const char* textIn;
        short int chunk;
        short int nPortions[2];
        char  portionLens[2][MAXPORTIONS];
        const char* message[2];
    } testData[] = {
        {   "123\\u000A"
            "abc45\\u000D"
            "67890\\u000A"
            "\\u000D"
            "02468\\u000D"
            "ghi",
            6, { 6, 6 }, {{ 4, 6, 6, 1, 6, 3}, { 4, 6, 6, 1, 6, 3 }},
            {"4, 6, 6, 1, 6, 3", "4, 6, 6, 1, 6, 3"}
        },
        {   "abcd\\u000Afgh\\u000D12345\\u000A456",
            6, { 4, 4 }, {{ 5, 4, 6, 3 }, { 5, 4, 6, 3 }},
            {"5, 4, 6, 3", "5, 4, 6, 3"}
        },
        {   "abcd\\u000Afgh\\u000D12345\\u000A45\\u000D",
            6, { 4, 4 }, {{ 5, 4, 6, 3 }, { 5, 4, 6, 3 }},
            {"5, 4, 6, 3", "5, 4, 6, 3"}
        },
        {   "abcde\\u000Afghi",
            10, { 2, 2 }, {{ 6, 4 }, { 6, 4 }},
            {"6, 4", "6, 4"}
        }
    };
    UChar src[MAXLEN];
    UChar *pSrc;
    UErrorCode rc = U_ZERO_ERROR;
    int32_t srcLen, processedLen, chunk, len, nPortions;
    int i, j, levelIndex;
    UBiDiLevel level;
    int nTests = UPRV_LENGTHOF(testData), nLevels = UPRV_LENGTHOF(testReorderingMode_paraLevels);
    UBool mismatch, testOK = TRUE;
    char processedLenStr[MAXPORTIONS * 5];

    uint8_t u8BufSrc[MAXLEN];
    UText* ut = 0;

    log_verbose("testStreaming(): Started\n");

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testInverse");

        return;
    }

    ubidi_orderParagraphsLTR(pBiDi, TRUE);

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        for (levelIndex = 0; levelIndex < nLevels; levelIndex++)
        {
            for (i = 0; i < nTests; i++)
            {
                srcLen = u_unescape(testData[i].textIn, src, MAXLEN);
                chunk = testData[i].chunk;
                nPortions = testData[i].nPortions[levelIndex];
                level = testReorderingMode_paraLevels[levelIndex];
                processedLenStr[0] = NULL_CHAR;
                log_verbose("  Testing level %d, case %d\n", level, i);

                mismatch = FALSE;

                ubidi_setReorderingOptions(pBiDi, UBIDI_OPTION_STREAMING);
                for (j = 0, pSrc = src; j < MAXPORTIONS && srcLen > 0; j++)
                {
                    len = chunk < srcLen ? chunk : srcLen;

                    if (doSetPara(pBiDi, pSrc, len, level, NULL, 0, U_ZERO_ERROR, j, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
                        processedLen = ubidi_getProcessedLength(pBiDi);
                        if (processedLen == 0) {
                            ubidi_setReorderingOptions(pBiDi, UBIDI_OPTION_DEFAULT);
                            j--;
                            continue;
                        }
                        ubidi_setReorderingOptions(pBiDi, UBIDI_OPTION_STREAMING);

                        mismatch |= (UBool)(j >= nPortions || processedLen != testData[i].portionLens[levelIndex][j]);

                        sprintf(processedLenStr + j * 4, "%4d", processedLen);
                        srcLen -= processedLen, pSrc += processedLen;
                    }

                    if (utext_isValid(ut)) {
                        ubidi_setPara(pBiDi, pSrc, len, level, NULL, &rc);

                        utext_close(ut);
                        ut = 0;
                    }
                }

                if (mismatch || j != nPortions) {
                    log_err("%s(%d): %s(%s): Processed lengths mismatch\n"
                        "\tParagraph level: %u\n"
                        "\tInput string: %s\n"
                        "\tActually processed portion lengths: { %s }\n"
                        "\tExpected portion lengths          : { %s }\n",
                        __FILE__, __LINE__, "ubidi_setClassCallback", Encodings[UEncoding_U16].description,
                        testReorderingMode_paraLevels[levelIndex], testData[i].textIn,
                        processedLenStr, testData[i].message[levelIndex]);

                    testOK = FALSE;
                }
            }
        }
    }

    ubidi_close(pBiDi);

    if (testOK == TRUE) {
        log_verbose("\nBiDi streaming test OK\n");
    }

    log_verbose("testStreaming(): Finished\n");
}

// complex/bidi/TestClassOverride

U_CDECL_BEGIN

#define DEF U_BIDI_CLASS_DEFAULT

static UCharDirection U_CALLCONV
overrideBidiClass(const void *context, UChar32 c)
{
    static const UCharDirection customClasses[] = {
       /* 0/8    1/9    2/A    3/B    4/C    5/D    6/E    7/F  */
          DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 00-07 */
          DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 08-0F */
          DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 10-17 */
          DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 18-1F */
          DEF,   DEF,   DEF,   DEF,   DEF,   DEF,     R,   DEF, /* 20-27 */
          DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 28-2F */
           EN,    EN,    EN,    EN,    EN,    EN,    AN,    AN, /* 30-37 */
           AN,    AN,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 38-3F */
            L,    AL,    AL,    AL,    AL,    AL,    AL,     R, /* 40-47 */
            R,     R,     R,     R,     R,     R,     R,     R, /* 48-4F */
            R,     R,     R,     R,     R,     R,     R,     R, /* 50-57 */
            R,     R,     R,   LRE,   DEF,   RLE,   PDF,     S, /* 58-5F */
          NSM,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 60-67 */
          DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 68-6F */
          DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF,   DEF, /* 70-77 */
          DEF,   DEF,   DEF,   LRO,     B,   RLO,    BN,   DEF  /* 78-7F */
    };

    static const int nEntries = UPRV_LENGTHOF(customClasses);
    const char *dummy = context;        /* just to avoid a compiler warning */
    dummy++;

    return c >= nEntries ? U_BIDI_CLASS_DEFAULT : customClasses[c];
}

U_CDECL_END

static void
testClassOverride(void) {
    static const char textSrc[] = "JIH.>12->a \\u05D0\\u05D1 6 ABC78";
    static const char textResult[] = "12<.HIJ->a 78CBA 6 \\u05D1\\u05D0";

    static UChar dest[MAXLEN];

    static UBidiWriteReorderedTestCases testCases[] = {
        { textSrc,
            -1,
            UBIDI_LTR,
            NULL,
            dest,
            MAXLEN,
            UBIDI_DO_MIRRORING,
            U_ZERO_ERROR,
            textResult,
            UInputType_unescape_char,
            -1,
            "Class Override UBIDI_DO_MIRRORING",
            FALSE },
    };

    UBiDiClassCallback* oldFn = NULL;
    UBiDiClassCallback* newFn = overrideBidiClass;
    const void* oldContext = NULL;
    int32_t textSrcSize = (int32_t)uprv_strlen(textSrc);
    UErrorCode errorCode = U_ZERO_ERROR;

    log_verbose("testClassOverride(): Started\n");

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testInverse");

        return;
    }

    ubidi_getClassCallback(pBiDi, &oldFn, &oldContext);
    _testClassOverrideVerifyCallbackParams(oldFn, oldContext, NULL, NULL, 0);

    ubidi_setClassCallback(pBiDi, newFn, textSrc, &oldFn, &oldContext, &errorCode);
    if (!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_setClassCallback", Encodings[UEncoding_U16].description,
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }

    _testClassOverrideVerifyCallbackParams(oldFn, oldContext, NULL, NULL, 0);

    ubidi_getClassCallback(pBiDi, &oldFn, &oldContext);
    _testClassOverrideVerifyCallbackParams(oldFn, oldContext, newFn, textSrc, textSrcSize);

    ubidi_setClassCallback(pBiDi, newFn, textSrc, &oldFn, &oldContext, &errorCode);
    if (!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
            __FILE__, __LINE__, "ubidi_setClassCallback", Encodings[UEncoding_U16].description,
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return;
    }

    _testClassOverrideVerifyCallbackParams(oldFn, oldContext, newFn, textSrc, textSrcSize);

    doWriteReordered(pBiDi, testCases, 0, __FILE__, __LINE__);

    ubidi_close(pBiDi);

    log_verbose("testClassOverride(): Finished\n");
}

static void _testClassOverrideVerifyCallbackParams(UBiDiClassCallback* fn, 
    const void* context,
    UBiDiClassCallback* expectedFn,
    const void* expectedContext,
    int32_t sizeOfContext)
{
    if (fn != expectedFn) {
        log_err("%s(%d): %s(%s): Class callback pointer is not set properly\n",
            __FILE__, __LINE__, "ubidi_setClassCallback", Encodings[UEncoding_U16].description);
    }
    if (context != expectedContext) {
        log_err("%s(%d): %s(%s): Class callback context is not set properly\n",
            __FILE__, __LINE__, "ubidi_setClassCallback", Encodings[UEncoding_U16].description);
    }
    else if (context != NULL && memcmp(context, expectedContext, sizeOfContext)) {
        log_err("%s(%d): %s(%s): Callback context content doesn't match the expected one\n",
            __FILE__, __LINE__, "ubidi_setClassCallback", Encodings[UEncoding_U16].description);
    }
}

// complex/bidi/testGetBaseDirection

static void testGetBaseDirection(void) {

    static const UChar
        // Mixed Start with L
        stringMixedEnglishFirst[] = { 0x61, 0x627, 0x32, 0x6f3, 0x61, 0x34, 0 },
        // Mixed Start with AL
        stringMixedArabicFirst[] = { 0x661, 0x627, 0x662, 0x6f3, 0x61, 0x664, 0 },
        // Mixed Start with R
        stringMixedHebrewFirst[] = { 0x05EA, 0x627, 0x662, 0x6f3, 0x61, 0x664, 0 },
        // All AL (Arabic. Persian)
        stringPersian[] = { 0x0698, 0x067E, 0x0686, 0x06AF, 0 },
        // All R (Hebrew etc.)
        stringHebrew[] = { 0x0590, 0x05D5, 0x05EA, 0x05F1, 0 },
        // All L (English)
        stringEnglish[] = { 0x71, 0x61, 0x66, 0 },
        // Mixed Start with weak AL an then L
        stringStartWeakAL[] = { 0x0663, 0x71, 0x61, 0x66, 0 },
        // Mixed Start with weak L and then AL
        stringStartWeakL[] = { 0x31, 0x0698, 0x067E, 0x0686, 0x06AF, 0 },
        // Empty
        stringEmpty[] = { 0 },
        // Surrogate Char.
        stringSurrogateChar[] = { 0xD800, 0xDC00, 0 },
        // Invalid UChar
        stringInvalidUchar[] = { (UChar)-1 },
        // All weak L (English Digits)
        stringAllEnglishDigits[] = { 0x31, 0x32, 0x33, 0 },
        // All weak AL (Arabic Digits)
        stringAllArabicDigits[] = { 0x0663, 0x0664, 0x0665, 0 },
        // First L (English) others are R (Hebrew etc.) 
        stringFirstL[] = { 0x71, 0x0590, 0x05D5, 0x05EA, 0x05F1, 0 },
        // Last R (Hebrew etc.) others are weak L (English Digits)
        stringLastR[] = { 0x31, 0x32, 0x33, 0x05F1, 0 };

    static UBidiGetBaseDirectionTestCase testCases[] = {
        { stringMixedEnglishFirst,
            UPRV_LENGTHOF(stringMixedEnglishFirst),
            UBIDI_LTR,
            "MixedEnglishFirst" },
        { stringMixedArabicFirst,
            UPRV_LENGTHOF(stringMixedArabicFirst),
            UBIDI_RTL,
            "MixedArabicFirst" },
        { stringMixedHebrewFirst,
            UPRV_LENGTHOF(stringMixedHebrewFirst),
            UBIDI_RTL,
            "MixedHebrewFirst" },
        { stringPersian,
            UPRV_LENGTHOF(stringPersian),
            UBIDI_RTL,
            "Persian" },
        { stringHebrew,
            UPRV_LENGTHOF(stringHebrew),
            UBIDI_RTL,
            "Hebrew" },
        { stringEnglish,
            UPRV_LENGTHOF(stringEnglish),
            UBIDI_LTR,
            "English" },
        { stringStartWeakAL,
            UPRV_LENGTHOF(stringStartWeakAL),
            UBIDI_LTR,
            "StartWeakAL" },
        { stringStartWeakL,
            UPRV_LENGTHOF(stringStartWeakL),
            UBIDI_RTL,
            "stringStartWeakL" },
        { stringEmpty,
            UPRV_LENGTHOF(stringEmpty),
            UBIDI_NEUTRAL,
            "Empty" },
        { stringSurrogateChar,
            UPRV_LENGTHOF(stringSurrogateChar),
            UBIDI_LTR,
            "SurrogateChar" },
        { stringInvalidUchar,
            UPRV_LENGTHOF(stringInvalidUchar),
            UBIDI_NEUTRAL,
            "InvalidUchar" },
        { stringAllEnglishDigits,
            UPRV_LENGTHOF(stringAllEnglishDigits),
            UBIDI_NEUTRAL,
            "AllEnglishDigits" },
        { stringAllArabicDigits,
            UPRV_LENGTHOF(stringAllArabicDigits),
            UBIDI_NEUTRAL,
            "AllArabicDigits" },
        { stringFirstL,
            UPRV_LENGTHOF(stringFirstL),
            UBIDI_LTR,
            "FirstL" },
        { stringLastR,
            UPRV_LENGTHOF(stringLastR),
            UBIDI_RTL,
            "LastR" },
        //  NULL string 
        { NULL,
            3,
            UBIDI_NEUTRAL,
            "NULL string" },
        // All L- English string and length=-3 
        { stringEnglish,
            -3,
            UBIDI_NEUTRAL,
            "All L- English string and length=-3" },
        // All L- English string and length=-1 
        { stringEnglish,
            -1,
            UBIDI_LTR,
            "All L- English string and length=-1" },
        // All AL- Persian string and length=-1 
        { stringPersian,
            -1,
            UBIDI_RTL,
            "All AL- Persian string and length=-1" },
        // All R- Hebrew string and length=-1 
        { stringHebrew,
            -1,
            UBIDI_RTL,
            "All R- Hebrew string and length=-1" },
        // All weak L- English digits string and length=-1 
        { stringAllEnglishDigits,
            -1,
            UBIDI_NEUTRAL,
            "All weak L- English digits string and length=-1" },
        // All weak AL- Arabic digits string and length=-1 
        { stringAllArabicDigits,
            -1,
            UBIDI_NEUTRAL,
            "All weak AL- Arabic digits string and length=-1" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testGetBaseDirection(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; ++i) {
        UBiDiDirection dir = doGetBaseDirection(testCases, i, __FILE__, __LINE__);

        log_verbose("  Testing case %d\tReceived dir %d\n", i, dir);
    }

    log_verbose("testGetBaseDirection(): Finished\n");
}

static UBiDiDirection
doGetBaseDirection(const UBidiGetBaseDirectionTestCase *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber)
{
    UBiDiDirection direction = UBIDI_NEUTRAL;
    uint8_t u8BufSrc[MAXLEN];
    UChar32 u32BufSrc[MAXLEN];

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        UErrorCode errorCode = U_ZERO_ERROR;

        if (encoding == UEncoding_U16) {
            direction = ubidi_getBaseDirection(testCases[testNumber].source, testCases[testNumber].sourceLength);
        }
        else if (encoding == UEncoding_U8) {
            UConverter *u8Convertor = ucnv_open("UTF8", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u8Convertor == NULL)) {
                return UBIDI_NEUTRAL;
            }

            UText* srcUt8 = NULL;
            if (testCases[testNumber].source) {
                memset(u8BufSrc, 0, MAXLEN);
                int32_t u16Len = testCases[testNumber].sourceLength;
                if (u16Len == (-1))
                    u16Len = u_strlen(testCases[testNumber].source);
                int32_t u8LenSrc = ucnv_fromUChars(u8Convertor, (char *)u8BufSrc, MAXLEN, testCases[testNumber].source, u16Len, &errorCode);

                srcUt8 = utext_openConstU8(NULL, u8BufSrc, u8LenSrc, MAXLEN, &errorCode);
            }

            ucnv_close(u8Convertor);

            direction = ubidi_getUBaseDirection(srcUt8);

            utext_close(srcUt8);
        }
        else if (encoding == UEncoding_U32) {
            UConverter *u32Convertor = ucnv_open("UTF32", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u32Convertor == NULL)) {
                return UBIDI_NEUTRAL;
            }

            UText* srcUt32 = NULL;
            if (testCases[testNumber].source) {
                memset(u32BufSrc, 0, MAXLEN);
                int32_t u16Len = testCases[testNumber].sourceLength;
                if (u16Len == (-1))
                    u16Len = u_strlen(testCases[testNumber].source);
                int32_t u32LenSrc = (ucnv_fromUChars(u32Convertor, (char *)u32BufSrc, MAXLEN, testCases[testNumber].source, u16Len, &errorCode) / sizeof(UChar)) - 1;

                srcUt32 = utext_openConstU32(NULL, &u32BufSrc[1], u32LenSrc, MAXLEN / sizeof(UChar32), &errorCode);
            }

            ucnv_close(u32Convertor);

            direction = ubidi_getUBaseDirection(srcUt32);

            utext_close(srcUt32);
        }

        if (direction != testCases[testNumber].expectedDir) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): direction %d (expected %d)\n",
                fileName, lineNumber, "ubidi_getBaseDirection", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                testCases[testNumber].expectedDir, direction);
        }
    }

    return direction;
}

// complex/bidi/testContext

static UBool
assertIllegalArgument(const char* message, UErrorCode* rc) {
    if (*rc != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("%s() failed with error %s.\n", message, u_errorName(*rc));
        return FALSE;
    }
    return TRUE;
}

static void
testContext(void) {

static const UBidiSetContextTestCase testCases[] = {
    { "", 
        "", 
        "", 
        "", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { "", 
        ".-=JKL-+*", 
        "", 
        ".-=LKJ-+*", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { " ", 
        ".-=JKL-+*", 
        " ", 
        ".-=LKJ-+*", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { "a", 
        ".-=JKL-+*", 
        "b", 
        ".-=LKJ-+*", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { "D", 
        ".-=JKL-+*", 
        "", 
        "LKJ=-.-+*", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { "", 
        ".-=JKL-+*", 
        " D", 
        ".-=*+-LKJ", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { "", 
        ".-=JKL-+*", 
        " 2", 
        ".-=*+-LKJ", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { "", 
        ".-=JKL-+*", 
        " 7", 
        ".-=*+-LKJ", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { " G 1", 
        ".-=JKL-+*", 
        " H", 
        "*+-LKJ=-.", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { "7", 
        ".-=JKL-+*", 
        " H", 
        ".-=*+-LKJ", 
        UBIDI_LTR,
        -1,
        -1,
        -1 },
    { "", 
        ".-=abc-+*", 
        "", 
        "*+-abc=-.", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { " ", 
        ".-=abc-+*", 
        " ", 
        "*+-abc=-.", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { "D", 
        ".-=abc-+*", 
        "G", 
        "*+-abc=-.", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { "x", 
        ".-=abc-+*", 
        "", 
        "*+-.-=abc", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { "", 
        ".-=abc-+*", 
        " y", 
        "abc-+*=-.", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { "", 
        ".-=abc-+*", 
        " 2", 
        "abc-+*=-.", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { " x 1", 
        ".-=abc-+*", 
        " 2", 
        ".-=abc-+*", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { " x 7", 
        ".-=abc-+*", 
        " 8", 
        "*+-.-=abc", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { "x|", 
        ".-=abc-+*", 
        " 8", 
        "*+-abc=-.", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { "G|y", 
        ".-=abc-+*", 
        " 8", 
        "*+-.-=abc", 
        UBIDI_RTL,
        -1,
        -1,
        -1 },
    { "", 
        ".-=", 
        "", 
        ".-=", 
        UBIDI_DEFAULT_LTR,
        -1,
        -1,
        -1 },
    { "D", 
        ".-=", 
        "", 
        "=-.", 
        UBIDI_DEFAULT_LTR,
        -1,
        -1,
        -1 },
    { "G", 
        ".-=", 
        "", 
        "=-.", 
        UBIDI_DEFAULT_LTR,
        -1,
        -1,
        -1 },
    { "xG", 
        ".-=", 
        "", 
        ".-=", 
        UBIDI_DEFAULT_LTR,
        -1,
        -1,
        -1 },
    { "x|G", 
        ".-=", 
        "", 
        "=-.", 
        UBIDI_DEFAULT_LTR,
        -1,
        -1,
        -1 },
    { "x|G", 
        ".-=|-+*", 
        "", 
        "=-.|-+*", 
        UBIDI_DEFAULT_LTR,
        -1,
        -1,
        -1 },
    };

    UErrorCode errorCode;
    UBool testOK = TRUE;

    log_verbose("\nEntering TestContext \n\n");

    /* test null BiDi object */
    errorCode = U_ZERO_ERROR;
    ubidi_setContext(NULL, NULL, 0, NULL, 0, &errorCode);
    testOK &= assertIllegalArgument("Error when BiDi object is null", &errorCode);

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_openSized", Encodings[UEncoding_U16].description, 0, "testInverse");

        return;
    }

    ubidi_orderParagraphsLTR(pBiDi, TRUE);

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testSetContext(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; ++i) {
        doSetContext(pBiDi, testCases, i, __FILE__, __LINE__);
    }

    ubidi_close(pBiDi);

    log_verbose("testSetContext(): Finished\n");
}

static int32_t
doSetContext(UBiDi* pBiDi, const UBidiSetContextTestCase *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber)
{
    uint8_t u8BufPrologueSrc[MAXLEN];
    uint8_t u8BufEpilogueSrc[MAXLEN];
    UChar32 u32BufPrologueSrc[MAXLEN];
    UChar32 u32BufEpilogueSrc[MAXLEN];

    UChar u16Prologue[MAXLEN];
    UChar u16Epilogue[MAXLEN];

    UChar u16BufSrc[MAXLEN];
    int32_t u16LenSrc = 0;

    int32_t u16ProLength = testCases[testNumber].proLength;
    if (u16ProLength == (-1))
        u16ProLength = (int32_t)strlen(testCases[testNumber].prologue);
    u16ProLength = pseudoToU16(u16ProLength, testCases[testNumber].prologue, u16Prologue);

    int32_t u16EpiLength = testCases[testNumber].epiLength;
    if (u16EpiLength == (-1))
        u16EpiLength = (int32_t)strlen(testCases[testNumber].epilogue);
    u16EpiLength = pseudoToU16(u16EpiLength, testCases[testNumber].epilogue, u16Epilogue);

    memset(u16BufSrc, 0, sizeof(u16BufSrc));

    UText* paraUt = 0;
    uint8_t u8BufSrc[MAXLEN];

    const char *srcChars = (const char *)testCases[testNumber].source;
    int32_t srcLen = testCases[testNumber].sourceLength;
    if (srcLen == (-1))
        srcLen = (int32_t)strlen(srcChars);
    u16LenSrc = pseudoToU16(srcLen, srcChars, u16BufSrc);

    int32_t u16Len = 0;

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        UErrorCode errorCode = U_ZERO_ERROR;
        UText* srcPrologueUt = NULL;
        UText* srcEpilogueUt = NULL;

        if (encoding == UEncoding_U16) {
            ubidi_setContext(pBiDi, u16Epilogue, u16EpiLength, u16Prologue, u16ProLength, &errorCode);
            assertSuccessful("swapped ubidi_setContext", &errorCode);

            ubidi_setContext(pBiDi, u16Prologue, -1, u16Epilogue, -1, &errorCode);
            assertSuccessful("regular ubidi_setContext", &errorCode);
        }
        else if (encoding == UEncoding_U8) {
            UConverter *u8Convertor = ucnv_open("UTF8", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u8Convertor == NULL)) {
                return UBIDI_NEUTRAL;
            }

            if (testCases[testNumber].prologue) {
                memset(u8BufPrologueSrc, 0, MAXLEN);
                int32_t u8LenSrc = ucnv_fromUChars(u8Convertor, (char *)u8BufPrologueSrc, MAXLEN, u16Prologue, u16ProLength, &errorCode);

                srcPrologueUt = utext_openConstU8(NULL, u8BufPrologueSrc, u8LenSrc, MAXLEN, &errorCode);
            }

            if (testCases[testNumber].epilogue) {
                memset(u8BufEpilogueSrc, 0, MAXLEN);
                int32_t u8LenSrc = ucnv_fromUChars(u8Convertor, (char *)u8BufEpilogueSrc, MAXLEN, u16Epilogue, u16EpiLength, &errorCode);

                srcEpilogueUt = utext_openConstU8(NULL, u8BufEpilogueSrc, u8LenSrc, MAXLEN, &errorCode);
            }

            ucnv_close(u8Convertor);

            ubidi_setUContext(pBiDi, srcPrologueUt, srcEpilogueUt, &errorCode);
        }
        else if (encoding == UEncoding_U32) {
            UConverter *u32Convertor = ucnv_open("UTF32", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u32Convertor == NULL)) {
                return UBIDI_NEUTRAL;
            }

            if (testCases[testNumber].prologue) {
                memset(u32BufPrologueSrc, 0, MAXLEN);
                int32_t u32LenSrc = (ucnv_fromUChars(u32Convertor, (char *)u32BufPrologueSrc, MAXLEN, u16Prologue, u16ProLength, &errorCode) / sizeof(UChar)) - 1;

                srcPrologueUt = utext_openConstU32(NULL, &u32BufPrologueSrc[1], u32LenSrc, MAXLEN / sizeof(UChar32), &errorCode);
            }

            if (testCases[testNumber].epilogue) {
                memset(u32BufEpilogueSrc, 0, MAXLEN);
                int32_t u32LenSrc = (ucnv_fromUChars(u32Convertor, (char *)u32BufEpilogueSrc, MAXLEN, u16Epilogue, u16EpiLength, &errorCode) / sizeof(UChar)) - 1;

                srcEpilogueUt = utext_openConstU32(NULL, &u32BufEpilogueSrc[1], u32LenSrc, MAXLEN / sizeof(UChar32), &errorCode);
            }

            ucnv_close(u32Convertor);

            ubidi_setUContext(pBiDi, srcPrologueUt, srcEpilogueUt, &errorCode);
        }

        if (!U_SUCCESS(errorCode)) {
            log_err("%s(%d): %s(%s): error code %s (expected %s)\n",
                __FILE__, __LINE__, "ubidi_setContext", Encodings[encoding].description,
                u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

            return 0;
        }
        else {
            static UChar dest[MAXLEN];

            // doWriteReordered() calls ubidi_setPara if source != NULL, and
            // context is reset after ubidi_setPara() call, so only call it once.
            if (!doSetPara(pBiDi, testCases[testNumber].source ? u16BufSrc : NULL,
                testCases[testNumber].sourceLength >= 0 ? u16LenSrc : -1, testCases[testNumber].paraLevel,
                NULL, 0, U_ZERO_ERROR, 0, fileName, lineNumber, Encodings[encoding], &paraUt, u8BufSrc, MAXLEN)) {
                return FALSE;
            }

            UBidiWriteReorderedTestCases writeReorderedtestCases[] = {
                { NULL,
                    -1,
                    0,
                    NULL,
                    dest,
                    MAXLEN,
                    UBIDI_DO_MIRRORING,
                    U_ZERO_ERROR,
                    NULL,
                    UInputType_pseudo16_char,
                    -1,
                    "Context UBIDI_DO_MIRRORING",
                    TRUE },
                };
            writeReorderedtestCases[0].source = testCases[testNumber].source;
            writeReorderedtestCases[0].paraLevel = testCases[testNumber].paraLevel;
            writeReorderedtestCases[0].expectedChars = testCases[testNumber].expected;

            doWriteReordered(pBiDi, writeReorderedtestCases, 0, fileName, lineNumber);
        }

        if (utext_isValid(srcPrologueUt)) {
            utext_close(srcPrologueUt);
            srcPrologueUt = 0;
        }

        if (utext_isValid(srcEpilogueUt)) {
            utext_close(srcEpilogueUt);
            srcPrologueUt = 0;
        }

        if (utext_isValid(paraUt)) {
            ubidi_setPara(pBiDi, 0, -1, UBIDI_DEFAULT_LTR, NULL, &errorCode);

            utext_close(paraUt);
            paraUt = 0;
        }
    }

    return u16Len;
}

// complex/bidi/TestBracketOverflow

/* Ticket#11054 ubidi_setPara crash with heavily nested brackets */
static void
testBracketOverflow(void) {
    static const char* TEXT = "(((((((((((((((((((((((((((((((((((((((((a)(A)))))))))))))))))))))))))))))))))))))))))";

    UErrorCode errorCode = U_ZERO_ERROR;
    UChar src[MAXLEN];
    int32_t len = (int32_t)uprv_strlen(TEXT);
    pseudoToU16(len, TEXT, src);

    uint8_t u8BufSrc[MAXLEN];
    UText* ut = 0;

    log_verbose("testBracketOverflow(): Started\n");

    UBiDi* pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "testMultipleParagraphs");

        return;
    }

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (!doSetPara(pBiDi, src, len, UBIDI_DEFAULT_LTR, NULL, 0, U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            log_err("%s(%d): %s(%s): failed with heavily nested brackets\n",
                __FILE__, __LINE__, "ubidi_setPara", Encodings[encoding].description);
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, src, len,
                UBIDI_DEFAULT_LTR, NULL,
                &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    ubidi_close(pBiDi);

    log_verbose("testBracketOverflow(): Finished\n");
}

// complex/bidi/TestExplicitLevel0

// The following used to fail with an error, see ICU ticket #12922.
static void testExplicitLevel0(void) {
    static const UChar text[2] = { 0x202d, 0x05d0 };
    static UBiDiLevel embeddings[2] = { 0, 0 };

    UErrorCode errorCode = U_ZERO_ERROR;
    uint8_t u8BufSrc[MAXLEN];
    UText* ut = 0;

    log_verbose("testExplicitLevel0(): Started\n");

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "testMultipleParagraphs");

        return;
    }

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        if (doSetPara(pBiDi, text, 2, UBIDI_DEFAULT_LTR, embeddings, sizeof(embeddings) / sizeof(UBiDiLevel), U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
            UBiDiLevel level0 = ubidi_getLevelAt(pBiDi, 0);
            UBiDiLevel level1 = ubidi_getLevelAt(pBiDi, 1);

            if (level0 != 1 || level1 != 1) {
                log_err("%s(%d): %s(%s): resolved levels != 1: { %d, %d }\n",
                    __FILE__, __LINE__, "ubidi_setPara", Encodings[encoding].description,
                    level0, level1);
            }

            if (embeddings[0] != 1 || embeddings[1] != 1) {
                log_err("%s(%d): %s(%s): modified embeddings[] levels != 1: { %d, %d }\n",
                    __FILE__, __LINE__, "ubidi_setPara", Encodings[encoding].description,
                    embeddings[0], embeddings[1]);
            }
        }

        if (utext_isValid(ut)) {
            ubidi_setPara(pBiDi, text, 2,
                UBIDI_DEFAULT_LTR, embeddings,
                &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    ubidi_close(pBiDi);

    log_verbose("testExplicitLevel0(): Finished\n");
}

// complex/bidi/TestExplicitLevel0

// The following used to fail with an error, see ICU ticket #12922.
static void testVisualText(void) {
    UErrorCode errorCode = U_ZERO_ERROR;
    uint8_t u8BufSrc[MAXLEN];
    UText* ut = 0;

    log_verbose("testVisualText(): Started\n");

    UBiDi *pBiDi = ubidi_open();
    if (pBiDi == NULL) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): returned NULL, out of memory\n",
            __FILE__, __LINE__, "ubidi_open", Encodings[UEncoding_U16].description, 0, "testVisualText");

        return;
    }

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        log_err("%s(%d): %s(%s, tests[%d]: %s): Not implemented\n",
            __FILE__, __LINE__, "testVisualText", Encodings[encoding].description, 0, "testVisualText");

        //if (doSetPara(pBiDi, text, 2, UBIDI_DEFAULT_LTR, embeddings, sizeof(embeddings) / sizeof(UBiDiLevel), U_ZERO_ERROR, 0, __FILE__, __LINE__, Encodings[encoding], &ut, u8BufSrc, MAXLEN)) {
        //}

        if (utext_isValid(ut)) {
          //  ubidi_setPara(pBiDi, text, 2,
            //    UBIDI_DEFAULT_LTR, embeddings,
              //  &errorCode);

            utext_close(ut);
            ut = 0;
        }
    }

    ubidi_close(pBiDi);

    log_verbose("testVisualText(): Finished\n");
}

/* helpers ------------------------------------------------------------------ */

static void initCharFromDirProps(void) {
    static const UVersionInfo ucd401={ 4, 0, 1, 0 };
    static UVersionInfo ucdVersion={ 0, 0, 0, 0 };

    /* lazy initialization */
    if(ucdVersion[0]>0) {
        return;
    }

    u_getUnicodeVersion(ucdVersion);
    if(memcmp(ucdVersion, ucd401, sizeof(UVersionInfo))>=0) {
        /* Unicode 4.0.1 changes bidi classes for +-/ */
        charFromDirProp[U_EUROPEAN_NUMBER_SEPARATOR]=0x2b; /* change ES character from / to + */
    }
}

/* return a string with characters according to the desired directional properties */
static UChar *
getStringFromDirProps(const uint8_t *dirProps, int32_t length, UChar *buffer) {
    int32_t i;

    initCharFromDirProps();

    /* this part would have to be modified for UTF-x */
    for(i=0; i<length; ++i) {
        buffer[i]=charFromDirProp[dirProps[i]];
    }
    buffer[length]=0;
    return buffer;
}

static void printUnicode(const UChar *s, int32_t length, const UBiDiLevel *levels) {
    int32_t i;

    log_verbose("{ ");
    for(i=0; i<length; ++i) {
        if(levels!=NULL) {
            log_verbose("%4x.%u  ", s[i], levels[i]);
        } else {
            log_verbose("%4x    ", s[i]);
        }
    }
    log_verbose(" }");
}

#define TABLE_SIZE  256
static UBool   tablesInitialized = FALSE;
static UChar   pseudoToUChar[TABLE_SIZE];
static uint8_t UCharToPseudo[TABLE_SIZE];    /* used for Unicode chars < 0x0100 */
static uint8_t UCharToPseud2[TABLE_SIZE];    /* used for Unicode chars >=0x0100 */

static void buildPseudoTables(void)
/*
    The rules for pseudo-Bidi are as follows:
    - [ == LRE
    - ] == RLE
    - { == LRO
    - } == RLO
    - ^ == PDF
    - @ == LRM
    - & == RLM
    - A-F == Arabic Letters 0631-0636
    - G-V == Hebrew letters 05d7-05e6
    - W-Z == Unassigned RTL 08d0-08d3
        Unicode 6.1 changes U+08A0..U+08FF from R to AL which works ok.
        Unicode 11 adds U+08D3 ARABIC SMALL LOW WAW which has bc=NSM
            so we stop using Z in this test.
    - 0-5 == western digits 0030-0035
    - 6-9 == Arabic-Indic digits 0666-0669
    - ` == Combining Grave Accent 0300 (NSM)
    - ~ == Delete 007f (BN)
    - | == Paragraph Separator 2029 (B)
    - _ == Info Separator 1 001f (S)
    All other characters represent themselves as Latin-1, with the corresponding
    Bidi properties.
*/
{
    int             i;
    UChar           uchar;
    uint8_t         c;
    /* initialize all tables to unknown */
    for (i=0; i < TABLE_SIZE; i++) {
        pseudoToUChar[i] = 0xFFFD;
        UCharToPseudo[i] = '?';
        UCharToPseud2[i] = '?';
    }
    /* initialize non letters or digits */
    pseudoToUChar[(uint8_t) 0 ] = 0x0000;    UCharToPseudo[0x00] = (uint8_t) 0 ;
    pseudoToUChar[(uint8_t)' '] = 0x0020;    UCharToPseudo[0x20] = (uint8_t)' ';
    pseudoToUChar[(uint8_t)'!'] = 0x0021;    UCharToPseudo[0x21] = (uint8_t)'!';
    pseudoToUChar[(uint8_t)'"'] = 0x0022;    UCharToPseudo[0x22] = (uint8_t)'"';
    pseudoToUChar[(uint8_t)'#'] = 0x0023;    UCharToPseudo[0x23] = (uint8_t)'#';
    pseudoToUChar[(uint8_t)'$'] = 0x0024;    UCharToPseudo[0x24] = (uint8_t)'$';
    pseudoToUChar[(uint8_t)'%'] = 0x0025;    UCharToPseudo[0x25] = (uint8_t)'%';
    pseudoToUChar[(uint8_t)'\'']= 0x0027;    UCharToPseudo[0x27] = (uint8_t)'\'';
    pseudoToUChar[(uint8_t)'('] = 0x0028;    UCharToPseudo[0x28] = (uint8_t)'(';
    pseudoToUChar[(uint8_t)')'] = 0x0029;    UCharToPseudo[0x29] = (uint8_t)')';
    pseudoToUChar[(uint8_t)'*'] = 0x002A;    UCharToPseudo[0x2A] = (uint8_t)'*';
    pseudoToUChar[(uint8_t)'+'] = 0x002B;    UCharToPseudo[0x2B] = (uint8_t)'+';
    pseudoToUChar[(uint8_t)','] = 0x002C;    UCharToPseudo[0x2C] = (uint8_t)',';
    pseudoToUChar[(uint8_t)'-'] = 0x002D;    UCharToPseudo[0x2D] = (uint8_t)'-';
    pseudoToUChar[(uint8_t)'.'] = 0x002E;    UCharToPseudo[0x2E] = (uint8_t)'.';
    pseudoToUChar[(uint8_t)'/'] = 0x002F;    UCharToPseudo[0x2F] = (uint8_t)'/';
    pseudoToUChar[(uint8_t)':'] = 0x003A;    UCharToPseudo[0x3A] = (uint8_t)':';
    pseudoToUChar[(uint8_t)';'] = 0x003B;    UCharToPseudo[0x3B] = (uint8_t)';';
    pseudoToUChar[(uint8_t)'<'] = 0x003C;    UCharToPseudo[0x3C] = (uint8_t)'<';
    pseudoToUChar[(uint8_t)'='] = 0x003D;    UCharToPseudo[0x3D] = (uint8_t)'=';
    pseudoToUChar[(uint8_t)'>'] = 0x003E;    UCharToPseudo[0x3E] = (uint8_t)'>';
    pseudoToUChar[(uint8_t)'?'] = 0x003F;    UCharToPseudo[0x3F] = (uint8_t)'?';
    pseudoToUChar[(uint8_t)'\\']= 0x005C;    UCharToPseudo[0x5C] = (uint8_t)'\\';
    /* initialize specially used characters */
    pseudoToUChar[(uint8_t)'`'] = 0x0300;    UCharToPseud2[0x00] = (uint8_t)'`';  /* NSM */
    pseudoToUChar[(uint8_t)'@'] = 0x200E;    UCharToPseud2[0x0E] = (uint8_t)'@';  /* LRM */
    pseudoToUChar[(uint8_t)'&'] = 0x200F;    UCharToPseud2[0x0F] = (uint8_t)'&';  /* RLM */
    pseudoToUChar[(uint8_t)'_'] = 0x001F;    UCharToPseudo[0x1F] = (uint8_t)'_';  /* S   */
    pseudoToUChar[(uint8_t)'|'] = 0x2029;    UCharToPseud2[0x29] = (uint8_t)'|';  /* B   */
    pseudoToUChar[(uint8_t)'['] = 0x202A;    UCharToPseud2[0x2A] = (uint8_t)'[';  /* LRE */
    pseudoToUChar[(uint8_t)']'] = 0x202B;    UCharToPseud2[0x2B] = (uint8_t)']';  /* RLE */
    pseudoToUChar[(uint8_t)'^'] = 0x202C;    UCharToPseud2[0x2C] = (uint8_t)'^';  /* PDF */
    pseudoToUChar[(uint8_t)'{'] = 0x202D;    UCharToPseud2[0x2D] = (uint8_t)'{';  /* LRO */
    pseudoToUChar[(uint8_t)'}'] = 0x202E;    UCharToPseud2[0x2E] = (uint8_t)'}';  /* RLO */
    pseudoToUChar[(uint8_t)'~'] = 0x007F;    UCharToPseudo[0x7F] = (uint8_t)'~';  /* BN  */
    /* initialize western digits */
    for (i = 0, uchar = 0x0030; i < 6; i++, uchar++) {
        c = (uint8_t)columns[i];
        pseudoToUChar[c] = uchar;
        UCharToPseudo[uchar & 0x00ff] = c;
    }
    /* initialize Hindi digits */
    for (i = 6, uchar = 0x0666; i < 10; i++, uchar++) {
        c = (uint8_t)columns[i];
        pseudoToUChar[c] = uchar;
        UCharToPseud2[uchar & 0x00ff] = c;
    }
    /* initialize Arabic letters */
    for (i = 10, uchar = 0x0631; i < 16; i++, uchar++) {
        c = (uint8_t)columns[i];
        pseudoToUChar[c] = uchar;
        UCharToPseud2[uchar & 0x00ff] = c;
    }
    /* initialize Hebrew letters */
    for (i = 16, uchar = 0x05D7; i < 32; i++, uchar++) {
        c = (uint8_t)columns[i];
        pseudoToUChar[c] = uchar;
        UCharToPseud2[uchar & 0x00ff] = c;
    }
    /* initialize Unassigned code points */
    for (i = 32, uchar=0x08D0; i < 36; i++, uchar++) {
        c = (uint8_t)columns[i];
        pseudoToUChar[c] = uchar;
        UCharToPseud2[uchar & 0x00ff] = c;
    }
    /* initialize Latin lower case letters */
    for (i = 36, uchar = 0x0061; i < 62; i++, uchar++) {
        c = (uint8_t)columns[i];
        pseudoToUChar[c] = uchar;
        UCharToPseudo[uchar & 0x00ff] = c;
    }
    tablesInitialized = TRUE;
}

/*----------------------------------------------------------------------*/

/*  This function converts a pseudo-Bidi string into a UChar string.
    It returns the length of the UChar string.
*/
static int pseudoToU16(const int length, const char * input, UChar * output)
{
    int             i;
    if (!tablesInitialized) {
        buildPseudoTables();
    }
    for (i = 0; i < length; i++)
        output[i] = pseudoToUChar[(uint8_t)input[i]];
    output[length] = 0;
    return length;
}

/*----------------------------------------------------------------------*/

/*  This function converts a UChar string into a pseudo-Bidi string.
    It returns the length of the pseudo-Bidi string.
*/
static int u16ToPseudo(const int length, const UChar * input, char * output)
{
    int             i;
    UChar           uchar;
    if (!tablesInitialized) {
        buildPseudoTables();
    }
    for (i = 0; i < length; i++)
    {
        uchar = input[i];
        output[i] = uchar < 0x0100 ? UCharToPseudo[uchar] :
            UCharToPseud2[uchar & 0x00ff];
    }
    output[length] = '\0';
    return length;
}

static char * formatLevels(UBiDi *pBiDi, char *buffer) {
    if (buffer) {
        UErrorCode ec = U_ZERO_ERROR;
        const UBiDiLevel* gotLevels = ubidi_getLevels(pBiDi, &ec);
        int len = ubidi_getLength(pBiDi);
        char c;
        int i, k;

        if (!U_SUCCESS(ec)) {
            strcpy(buffer, "BAD LEVELS");
            return buffer;
        }
        for (i = 0; i < len; i++) {
            k = gotLevels[i];
            if (k >= sizeof(columns))
                c = '+';
            else
                c = columns[k];
            buffer[i] = c;
        }
        buffer[len] = '\0';
    }
    return buffer;
}

static const char *reorderingModeNames[] = {
    "UBIDI_REORDER_DEFAULT",
    "UBIDI_REORDER_NUMBERS_SPECIAL",
    "UBIDI_REORDER_GROUP_NUMBERS_WITH_R",
    "UBIDI_REORDER_RUNS_ONLY",
    "UBIDI_REORDER_INVERSE_NUMBERS_AS_L",
    "UBIDI_REORDER_INVERSE_LIKE_DIRECT",
    "UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL"};

static char *reorderingOptionNames(char *buffer, int options) {
    buffer[0] = 0;
    if (options & UBIDI_OPTION_INSERT_MARKS) {
        strcat(buffer, " UBIDI_OPTION_INSERT_MARKS");
    }
    if (options & UBIDI_OPTION_REMOVE_CONTROLS) {
        strcat(buffer, " UBIDI_OPTION_REMOVE_CONTROLS");
    }
    if (options & UBIDI_OPTION_STREAMING) {
        strcat(buffer, " UBIDI_OPTION_STREAMING");
    }
    return buffer;
}

/* src and dst are char arrays encoded as pseudo Bidi */
static void printCaseInfo(UBiDi *pBiDi, const char *src, const char *dst)
{
    /* Since calls to log_err with a \n within the pattern increment the
     * error count, new lines are issued via fputs, except when we want the
     * increment to happen.
     */
    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t i, length = ubidi_getProcessedLength(pBiDi);
    const UBiDiLevel *levels;
    UBiDiLevel lev;
    char levelChars[MAXLEN];
    char buffer[100];
    char display[MAXLEN];

    levels = ubidi_getLevels(pBiDi, &errorCode);
    if (!U_SUCCESS(errorCode)) {
        strcpy(levelChars, "BAD LEVELS");
    }
    else {
        for (i = 0; i < length; i++) {
            lev = levels[i];
            if (lev < sizeof(columns)) {
                levelChars[i] = columns[lev];
            }
            else {
                levelChars[i] = '+';
            }
        }
        levelChars[length] = 0;
    }

    int32_t reorderingMode = ubidi_getReorderingMode(pBiDi);
    int32_t reorderingOptions = ubidi_getReorderingOptions(pBiDi);
    int32_t runCount = ubidi_countRuns(pBiDi, &errorCode);

    memset(display, 0, MAXLEN);

    if (!U_SUCCESS(errorCode)) {
        strcpy(display, "BAD RUNS");
    }
    else {
        strcat(display, "Map\t: { ");
        for (i = 0; i < runCount; i++) {
            UBiDiDirection dir;
            int32_t start, len;
            dir = ubidi_getVisualRun(pBiDi, i, &start, &len);
            if (i > 0)
                strcat(display, ", ");
            sprintf(&display[strlen(display)], "%d.%d/%d", start, len, dir);
        }
        strcat(display, " }\n");
    }

    log_err("Details:\n"
        "Processed Length\t: %d\n"
        "Direction\t: %d\n"
        "paraLevel\t: %d\n"
        "reorderingMode\t: %d = %s\n"
        "reorderingOptions\t: %d = %s\n"
        "Runs\t: %d => logicalStart.length/level: %s",
        length, ubidi_getDirection(pBiDi), ubidi_getParaLevel(pBiDi),
        reorderingMode, reorderingModeNames[reorderingMode], reorderingOptions, reorderingOptionNames(buffer, reorderingOptions),
        runCount, display);
}

static UBool matchingPair(UBiDi *pBiDi, int32_t i, char c1, char c2)
{
    /* No test for []{} since they have special meaning for pseudo Bidi */
    static char mates1Chars[] = "<>()";
    static char mates2Chars[] = "><)(";
    UBiDiLevel level;
    int32_t k, len;

    if (c1 == c2) {
        return TRUE;
    }
    /* For UBIDI_REORDER_RUNS_ONLY, it would not be correct to check levels[i],
       so we use the appropriate run's level, which is good for all cases.
     */
    ubidi_getLogicalRun(pBiDi, i, NULL, &level);
    if ((level & 1) == 0) {
        return FALSE;
    }

    len = (int32_t)strlen(mates1Chars);
    for (k = 0; k < len; k++) {
        if ((c1 == mates1Chars[k]) && (c2 == mates2Chars[k])) {
            return TRUE;
        }
    }
    return FALSE;
}

static UBool checkWhatYouCan(UBiDi *pBiDi, 
    const char *srcChars, 
    const char *dstChars,
    const char *fileName,
    int32_t lineNumber,
    int32_t encoding)
/* srcChars and dstChars are char arrays encoded as pseudo Bidi */
{
    int32_t i, idx, logLimit, visLimit;
    UBool testOK, errMap, errDst;
    UErrorCode errorCode = U_ZERO_ERROR;
    int32_t visMap[MAXLEN];
    int32_t logMap[MAXLEN];
    char accumSrc[MAXLEN];
    char accumDst[MAXLEN];
    char display[MAXLEN];

    ubidi_getVisualMap(pBiDi, visMap, &errorCode);
    if (!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): errorCode=%sd (expected %s)\n",
            fileName, lineNumber, "ubidi_getVisualMap", Encodings[encoding].description, 0, "checkWhatYouCan",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return FALSE;
    }

    ubidi_getLogicalMap(pBiDi, logMap, &errorCode);
    if (!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): errorCode=%sd (expected %s)\n",
            fileName, lineNumber, "ubidi_getLogicalMap", Encodings[encoding].description, 0, "checkWhatYouCan",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return FALSE;
    }

    testOK = TRUE;
    errMap = errDst = FALSE;
    logLimit = ubidi_getProcessedLength(pBiDi);
    visLimit = ubidi_getResultLength(pBiDi);
    memset(accumSrc, '?', logLimit);
    memset(accumDst, '?', visLimit);
    memset(display, 0, MAXLEN);

    for (i = 0; i < logLimit; i++) {
        idx = ubidi_getVisualIndex(pBiDi, i, &errorCode);
        if (idx != logMap[i]) {
            errMap = TRUE;
        }
        if (idx == UBIDI_MAP_NOWHERE) {
            continue;
        }
        if (idx >= visLimit) {
            continue;
        }
        accumDst[idx] = srcChars[i];
        if (!matchingPair(pBiDi, i, srcChars[i], dstChars[idx])) {
            errDst = TRUE;
        }
    }

    accumDst[visLimit] = 0;

    if (!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): errorCode=%sd (expected %s)\n",
            fileName, lineNumber, "ubidi_getVisualIndex", Encodings[encoding].description, 0, "checkWhatYouCan",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return FALSE;
    }

    if (errMap) {
        if (testOK) {
            printCaseInfo(pBiDi, srcChars, dstChars);
            testOK = FALSE;
        }

        memset(display, 0, MAXLEN);

        strcat(display, "Map\t: { ");
        for (i = 0; i < logLimit; i++) {
            if (i > 0)
                strcat(display, ", ");
            sprintf(&display[strlen(display)], "%d", logMap[i]);
        }
        strcat(display, " }\n");

        strcat(display, "Index\t: { ");
        for (i = 0; i < logLimit; i++) {
            if (i > 0)
                strcat(display, ", ");
            sprintf(&display[strlen(display)], "%d", ubidi_getVisualIndex(pBiDi, i, &errorCode));
        }
        strcat(display, " }\n");

        log_err("%s(%d): %s(%s, tests[%d]: %s): Mismatch between ubidi_getLogicalMap() and ubidi_getVisualIndex()\n%s",
            fileName, lineNumber, "ubidi_getVisualIndex", Encodings[encoding].description, 0, "checkWhatYouCan",
            display);
    }

    if (errDst) {
        if (testOK) {
            printCaseInfo(pBiDi, srcChars, dstChars);
            testOK = FALSE;
        }

        log_err("%s(%d): %s(%s, tests[%d]: %s): Result does not map to Source\nExpected: %s\nGot:\t: %s\nIndexed\t: %s\n",
            fileName, lineNumber, "ubidi_getVisualIndex", Encodings[encoding].description, 0, "checkWhatYouCan",
            srcChars, dstChars, accumDst);
    }

    errMap = errDst = FALSE;
    for (i = 0; i < visLimit; i++) {
        idx = ubidi_getLogicalIndex(pBiDi, i, &errorCode);
        if (idx != visMap[i]) {
            errMap = TRUE;
        }
        if (idx == UBIDI_MAP_NOWHERE) {
            continue;
        }
        if (idx >= logLimit) {
            continue;
        }
        accumSrc[idx] = dstChars[i];
        if (!matchingPair(pBiDi, idx, srcChars[idx], dstChars[i])) {
            errDst = TRUE;
        }
    }

    accumSrc[logLimit] = 0;

    if (!U_SUCCESS(errorCode)) {
        log_err("%s(%d): %s(%s, tests[%d]: %s): errorCode=%sd (expected %s)\n",
            fileName, lineNumber, "ubidi_getLogicalIndex", Encodings[encoding].description, 0, "checkWhatYouCan",
            u_errorName(errorCode), u_errorName(U_ZERO_ERROR));

        return FALSE;
    }

    if (errMap) {
        if (testOK) {
            printCaseInfo(pBiDi, srcChars, dstChars);
            testOK = FALSE;
        }

        memset(display, 0, MAXLEN);

        strcat(display, "Map\t: { ");
        for (i = 0; i < logLimit; i++) {
            if (i > 0)
                strcat(display, ", ");
            sprintf(&display[strlen(display)], "%d", visMap[i]);
        }
        strcat(display, " }\n");

        strcat(display, "Index\t: { ");
        for (i = 0; i < logLimit; i++) {
            if (i > 0)
                strcat(display, ", ");
            sprintf(&display[strlen(display)], "%d", ubidi_getLogicalIndex(pBiDi, i, &errorCode));
        }
        strcat(display, " }\n");

        log_err("%s(%d): %s(%s, tests[%d]: %s): Mismatch between ubidi_getVisualMap() and ubidi_getLogicalIndex()\n%s",
            fileName, lineNumber, "ubidi_getLogicalIndex", Encodings[encoding].description, 0, "checkWhatYouCan",
            display);
    }

    if (errDst) {
        if (testOK) {
            printCaseInfo(pBiDi, srcChars, dstChars);
            testOK = FALSE;
        }

        log_err("%s(%d): %s(%s, tests[%d]: %s): Source does not map to Result\nExpected: %s\nGot:\t: %s\nIndexed\t: %s\n",
            fileName, lineNumber, "ubidi_getLogicalIndex", Encodings[encoding].description, 0, "checkWhatYouCan",
            srcChars, dstChars, accumSrc);
    }

    return testOK;
}

/*
 * Arabic shaping -----------------------------------------------------------
 *
 */

static void
testArabicShaping(void)
{
    static const UChar
        source[] = {
            0x31,   /* en:1 */
            0x627,  /* arabic:alef */
            0x32,   /* en:2 */
            0x6f3,  /* an:3 */
            0x61,   /* latin:a */
            0x34,   /* en:4 */
            0
    }, en2an[] = {
        0x661, 0x627, 0x662, 0x6f3, 0x61, 0x664, 0
    }, an2en[] = {
        0x31, 0x627, 0x32, 0x33, 0x61, 0x34, 0
    }, logical_alen2an_init_lr[] = {
        0x31, 0x627, 0x662, 0x6f3, 0x61, 0x34, 0
    }, logical_alen2an_init_al[] = {
        0x6f1, 0x627, 0x6f2, 0x6f3, 0x61, 0x34, 0
    }, reverse_alen2an_init_lr[] = {
        0x661, 0x627, 0x32, 0x6f3, 0x61, 0x34, 0
    }, reverse_alen2an_init_al[] = {
        0x6f1, 0x627, 0x32, 0x6f3, 0x61, 0x6f4, 0
    }, lamalef[] = {
        0xfefb, 0
    };

    static UChar dest[MAXLEN];

    static UBidiShapeTestCase testCases[] = {
        // European->arabic
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_EN2AN | U_SHAPE_DIGIT_TYPE_AN,
            U_ZERO_ERROR,
            en2an,
            UPRV_LENGTHOF(source),
            "en2an" },
        // Arabic->european
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_AN2EN | U_SHAPE_DIGIT_TYPE_AN_EXTENDED,
            U_ZERO_ERROR,
            an2en,
            UPRV_LENGTHOF(source),
            "an2en" },
        // European->arabic with context, logical order, initial state not AL
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_ALEN2AN_INIT_LR | U_SHAPE_DIGIT_TYPE_AN,
            U_ZERO_ERROR,
            logical_alen2an_init_lr,
            UPRV_LENGTHOF(source),
            "logical_alen2an_init_lr" },
        // European->arabic with context, logical order, initial state AL
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_ALEN2AN_INIT_AL | U_SHAPE_DIGIT_TYPE_AN_EXTENDED,
            U_ZERO_ERROR,
            logical_alen2an_init_al,
            UPRV_LENGTHOF(source),
            "logical_alen2an_init_al" },
        // European->arabic with context, reverse order, initial state not AL
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_ALEN2AN_INIT_LR | U_SHAPE_DIGIT_TYPE_AN | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
            U_ZERO_ERROR,
            reverse_alen2an_init_lr,
            UPRV_LENGTHOF(source),
            "reverse_alen2an_init_lr" },
        // European->arabic with context, reverse order, initial state AL
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_ALEN2AN_INIT_AL | U_SHAPE_DIGIT_TYPE_AN_EXTENDED | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
            U_ZERO_ERROR,
            reverse_alen2an_init_al,
            UPRV_LENGTHOF(source),
            "reverse_alen2an_init_al" },
        // Test noop
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            0,
            U_ZERO_ERROR,
            0,
            UPRV_LENGTHOF(source),
            "noop" },
        { source,
            0,
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_EN2AN | U_SHAPE_DIGIT_TYPE_AN,
            U_ZERO_ERROR,
            0,
            0,
            "en2an, sourceLength=0" },
        // Preflight digit shaping
        { source,
            UPRV_LENGTHOF(source),
            NULL,
            0,
            U_SHAPE_DIGITS_EN2AN | U_SHAPE_DIGIT_TYPE_AN,
            U_BUFFER_OVERFLOW_ERROR,
            0,
            UPRV_LENGTHOF(source),
            "en2an preflighting" },
        // Test illegal arguments
        { NULL,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_EN2AN | U_SHAPE_DIGIT_TYPE_AN,
            U_ILLEGAL_ARGUMENT_ERROR,
            0,
            0,
            "source=NULL" },
        { source,
            UPRV_LENGTHOF(source),
            NULL,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_EN2AN | U_SHAPE_DIGIT_TYPE_AN,
            U_BUFFER_OVERFLOW_ERROR,
            0,
            UPRV_LENGTHOF(source),
            "dest=NULL" },
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_RESERVED | U_SHAPE_DIGIT_TYPE_AN,
            U_ILLEGAL_ARGUMENT_ERROR,
            0,
            0,
            "U_SHAPE_DIGITS_RESERVED" },
        { source,
            UPRV_LENGTHOF(source),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_EN2AN | U_SHAPE_DIGIT_TYPE_RESERVED,
            U_ILLEGAL_ARGUMENT_ERROR,
            0,
            0,
            "U_SHAPE_DIGIT_TYPE_RESERVED" },
        // Overlap source and destination
        { source,
            UPRV_LENGTHOF(source),
            (UChar *)(source + 2),
            UPRV_LENGTHOF(dest),
            U_SHAPE_DIGITS_EN2AN | U_SHAPE_DIGIT_TYPE_AN,
            U_ILLEGAL_ARGUMENT_ERROR,
            0,
            0,
            "overlap source and destination" },
        { lamalef,
            UPRV_LENGTHOF(lamalef),
            dest,
            UPRV_LENGTHOF(dest),
            U_SHAPE_LETTERS_UNSHAPE | U_SHAPE_LENGTH_GROW_SHRINK | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
            U_ZERO_ERROR,
            0,
            3,
            "U_SHAPE_LETTERS_UNSHAPE | U_SHAPE_LENGTH_GROW_SHRINK | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testArabicShaping(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; i++) {
        int32_t length = doShapeArabic(testCases, i, __FILE__, __LINE__);

        if (testCases[i].source == lamalef) {
            if (length == UPRV_LENGTHOF(lamalef)) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                    __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, "testArabicShaping",
                    length, 3);
            }
        }
    }

    log_verbose("testArabicShaping(): Finished\n");
}

static int32_t
doShapeArabic(const UBidiShapeTestCase *testCases,
    int32_t testNumber,
    const char* fileName,
    int32_t lineNumber)
{
    UErrorCode errorCode = U_ZERO_ERROR;
    UChar u16Buf[MAXLEN];
    int32_t u16Len = 0;

    int32_t encoding = 0;
    for (encoding = 0; encoding < EncodingsCount; encoding++)
    {
        memset(u16Buf, 0, sizeof(u16Buf));

        if (encoding == UEncoding_U16) {
            u16Len = u_shapeArabic(testCases[testNumber].source, testCases[testNumber].sourceLength,
                testCases[testNumber].dest, testCases[testNumber].destSize,
                testCases[testNumber].options,
                &errorCode);

            if (testCases[testNumber].dest)
                u_strcpy(u16Buf, testCases[testNumber].dest);
        }
        else if (encoding == UEncoding_U8) {
            errorCode = U_ZERO_ERROR;
            UConverter *u8Convertor = ucnv_open("UTF8", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u8Convertor == NULL)) {
                return 0;
            }

            uint8_t u8BufDst[MAXLEN];

            memset(u8BufDst, 0, sizeof(u8BufDst) / sizeof(char));

            UText* srcUt16 = NULL;
            if (testCases[testNumber].source)
                srcUt16 = utext_openUChars(NULL, testCases[testNumber].source, testCases[testNumber].sourceLength, &errorCode);

            UText* dstUt8 = NULL;
            if (testCases[testNumber].dest)
                dstUt8 = utext_openU8(NULL, u8BufDst, 0, (!testCases[testNumber].dest) ? 0 : (testCases[testNumber].destSize < 0 ? testCases[testNumber].destSize : sizeof(u8BufDst)), &errorCode);

            int32_t u8LenDst = u_shapeUArabic(srcUt16,
                dstUt8,
                testCases[testNumber].options,
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
            errorCode = U_ZERO_ERROR;
            UConverter *u32Convertor = ucnv_open("UTF32", &errorCode);

            if ((!U_SUCCESS(errorCode)) || (u32Convertor == NULL)) {
                return 0;
            }

            UChar32 u32BufDst[MAXLEN];

            memset(u32BufDst, 0, sizeof(u32BufDst) / sizeof(UChar32));
            u32BufDst[0] = 0x0000FEFF;

            UText* srcUt16 = NULL;
            if (testCases[testNumber].source)
                srcUt16 = utext_openUChars(NULL, testCases[testNumber].source, testCases[testNumber].sourceLength, &errorCode);

            UText* dstUt32 = NULL;
            if (testCases[testNumber].dest)
                dstUt32 = utext_openU32(NULL, &u32BufDst[1], 0, (!testCases[testNumber].dest) ? 0 : (testCases[testNumber].destSize < 0 ? testCases[testNumber].destSize : sizeof(u32BufDst)), &errorCode);

            int32_t u32LenDst = u_shapeUArabic(srcUt16,
                dstUt32,
                testCases[testNumber].options,
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

        if ((errorCode != U_STRING_NOT_TERMINATED_WARNING) && (errorCode != testCases[testNumber].errorCode)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): error code %s (expected %s)\n",
                fileName, lineNumber, "u_shapeArabic", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                u_errorName(errorCode), u_errorName(testCases[testNumber].errorCode));
        }
        else if ((testCases[testNumber].expectedLength >= 0) && (u16Len != testCases[testNumber].expectedLength)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                fileName, lineNumber, "u_shapeArabic", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                u16Len, testCases[testNumber].expectedLength);
        }
        else if ((testCases[testNumber].expectedChars) && (memcmp(u16Buf, testCases[testNumber].expectedChars, u16Len * sizeof(UChar)) != 0)) {
            log_err("%s(%d): %s(%s, tests[%d]: %s): result mismatch\n"
                "Input\t: %s\nGot\t: %s\nExpected: %s\n",
                fileName, lineNumber, "u_shapeArabic", Encodings[encoding].description, testNumber, testCases[testNumber].pMessage,
                testCases[testNumber].source, u16Buf, testCases[testNumber].expectedChars);
        }

        // Do input and output overlap?
        if ((testCases[testNumber].dest != NULL) && ((testCases[testNumber].source >= testCases[testNumber].dest && testCases[testNumber].source < testCases[testNumber].dest + testCases[testNumber].destSize)
            || (testCases[testNumber].dest >= testCases[testNumber].source && testCases[testNumber].dest < testCases[testNumber].source + testCases[testNumber].sourceLength))) {
            return u16Len;
        }
    }

    return u16Len;
}

static void
testArabicShapingLamAlefSpecialVLTR(void)
{
    static const UChar
        source[] = {
            0x20 ,0x646,0x622,0x644,0x627,0x20, // a
            0x646,0x623,0x64E,0x644,0x627,0x20, // b
            0x646,0x627,0x670,0x644,0x627,0x20, // c
            0x646,0x622,0x653,0x644,0x627,0x20, // d
            0x646,0x625,0x655,0x644,0x627,0x20, // e
            0x646,0x622,0x654,0x644,0x627,0x20, // f
            0xFEFC,0x639                        // g
    }, shape_near[] = {
        0x20,0xfee5,0x20,0xfef5,0xfe8d,0x20,0xfee5,0x20,0xfe76,0xfef7,0xfe8d,0x20,
        0xfee5,0x20,0x670,0xfefb,0xfe8d,0x20,0xfee5,0x20,0x653,0xfef5,0xfe8d,0x20,
        0xfee5,0x20,0x655,0xfef9,0xfe8d,0x20,0xfee5,0x20,0x654,0xfef5,0xfe8d,0x20,
        0xfefc,0xfecb
    }, shape_at_end[] = {
        0x20,0xfee5,0xfef5,0xfe8d,0x20,0xfee5,0xfe76,0xfef7,0xfe8d,0x20,0xfee5,0x670,
        0xfefb,0xfe8d,0x20,0xfee5,0x653,0xfef5,0xfe8d,0x20,0xfee5,0x655,0xfef9,0xfe8d,
        0x20,0xfee5,0x654,0xfef5,0xfe8d,0x20,0xfefc,0xfecb,0x20,0x20,0x20,0x20,0x20,0x20
    }, shape_at_begin[] = {
        0x20,0x20,0x20,0x20,0x20,0x20,0x20,0xfee5,0xfef5,0xfe8d,0x20,0xfee5,0xfe76,
        0xfef7,0xfe8d,0x20,0xfee5,0x670,0xfefb,0xfe8d,0x20,0xfee5,0x653,0xfef5,0xfe8d,
        0x20,0xfee5,0x655,0xfef9,0xfe8d,0x20,0xfee5,0x654,0xfef5,0xfe8d,0x20,0xfefc,0xfecb
    }, shape_grow_shrink[] = {
        0x20,0xfee5,0xfef5,0xfe8d,0x20,0xfee5,0xfe76,0xfef7,0xfe8d,0x20,0xfee5,
        0x670,0xfefb,0xfe8d,0x20,0xfee5,0x653,0xfef5,0xfe8d,0x20,0xfee5,0x655,0xfef9,
        0xfe8d,0x20,0xfee5,0x654,0xfef5,0xfe8d,0x20,0xfefc,0xfecb
    }, shape_excepttashkeel_near[] = {
        0x20,0xfee5,0x20,0xfef5,0xfe8d,0x20,0xfee5,0x20,0xfe76,0xfef7,0xfe8d,0x20,
        0xfee5,0x20,0x670,0xfefb,0xfe8d,0x20,0xfee5,0x20,0x653,0xfef5,0xfe8d,0x20,
        0xfee5,0x20,0x655,0xfef9,0xfe8d,0x20,0xfee5,0x20,0x654,0xfef5,0xfe8d,0x20,
        0xfefc,0xfecb
    }, shape_excepttashkeel_at_end[] = {
        0x20,0xfee5,0xfef5,0xfe8d,0x20,0xfee5,0xfe76,0xfef7,0xfe8d,0x20,0xfee5,
        0x670,0xfefb,0xfe8d,0x20,0xfee5,0x653,0xfef5,0xfe8d,0x20,0xfee5,0x655,0xfef9,
        0xfe8d,0x20,0xfee5,0x654,0xfef5,0xfe8d,0x20,0xfefc,0xfecb,0x20,0x20,0x20,
        0x20,0x20,0x20
    }, shape_excepttashkeel_at_begin[] = {
        0x20,0x20,0x20,0x20,0x20,0x20,0x20,0xfee5,0xfef5,0xfe8d,0x20,0xfee5,0xfe76,
        0xfef7,0xfe8d,0x20,0xfee5,0x670,0xfefb,0xfe8d,0x20,0xfee5,0x653,0xfef5,0xfe8d,
        0x20,0xfee5,0x655,0xfef9,0xfe8d,0x20,0xfee5,0x654,0xfef5,0xfe8d,0x20,0xfefc,0xfecb
    }, shape_excepttashkeel_grow_shrink[] = {
        0x20,0xfee5,0xfef5,0xfe8d,0x20,0xfee5,0xfe76,0xfef7,0xfe8d,0x20,0xfee5,0x670,
        0xfefb,0xfe8d,0x20,0xfee5,0x653,0xfef5,0xfe8d,0x20,0xfee5,0x655,0xfef9,0xfe8d,
        0x20,0xfee5,0x654,0xfef5,0xfe8d,0x20,0xfefc,0xfecb
    };

    static UChar dest[MAXLEN];

    static UBidiShapeTestCase testCases[] = {
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE | U_SHAPE_LENGTH_FIXED_SPACES_NEAR | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_near,
                UPRV_LENGTHOF(shape_near),
                "LAMALEF shape_near" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE | U_SHAPE_LENGTH_FIXED_SPACES_AT_END | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_at_end,
                UPRV_LENGTHOF(shape_at_end),
                "LAMALEF shape_at_end" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE | U_SHAPE_LENGTH_FIXED_SPACES_AT_BEGINNING | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_at_begin,
                UPRV_LENGTHOF(shape_at_begin),
                "LAMALEF shape_at_begin" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE | U_SHAPE_LENGTH_GROW_SHRINK | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_grow_shrink,
                UPRV_LENGTHOF(shape_grow_shrink),
                "LAMALEF shape_grow_shrink" },
            // ==================== U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED ====================
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED | U_SHAPE_LENGTH_FIXED_SPACES_NEAR | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_excepttashkeel_near,
                UPRV_LENGTHOF(shape_excepttashkeel_near),
                "LAMALEF shape_excepttashkeel_near" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED | U_SHAPE_LENGTH_FIXED_SPACES_AT_END | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_excepttashkeel_at_end,
                UPRV_LENGTHOF(shape_excepttashkeel_at_end),
                "LAMALEF shape_excepttashkeel_at_end" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED | U_SHAPE_LENGTH_FIXED_SPACES_AT_BEGINNING | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_excepttashkeel_at_begin,
                UPRV_LENGTHOF(shape_excepttashkeel_at_begin),
                "LAMALEF shape_excepttashkeel_at_begin" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED | U_SHAPE_LENGTH_GROW_SHRINK | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_excepttashkeel_grow_shrink,
                UPRV_LENGTHOF(shape_excepttashkeel_grow_shrink),
                "LAMALEF shape_excepttashkeel_grow_shrink" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testArabicShapingLamAlefSpecialVLTR(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; i++) {
        doShapeArabic(testCases, i, __FILE__, __LINE__);
    }

    log_verbose("testArabicShapingLamAlefSpecialVLTR(): Finished\n");
}

static void
testArabicShapingTashkeelSpecialVLTR(void)
{
    static const UChar
        source[] = {
            0x64A,0x628,0x631,0x639,0x20,
            0x64A,0x628,0x651,0x631,0x64E,0x639,0x20,
            0x64C,0x64A,0x628,0x631,0x64F,0x639,0x20,
            0x628,0x670,0x631,0x670,0x639,0x20,
            0x628,0x653,0x631,0x653,0x639,0x20,
            0x628,0x654,0x631,0x654,0x639,0x20,
            0x628,0x655,0x631,0x655,0x639,0x20,
    }, shape_near[] = {
        0xfef2,0xfe91,0xfeae,0xfecb,0x20,0xfef2,0xfe91,0xfe7c,0xfeae,0xfe77,0xfecb,
        0x20,0xfe72,0xfef2,0xfe91,0xfeae,0xfe79,0xfecb,0x20,0xfe8f,0x670,0xfeae,0x670,
        0xfecb,0x20,0xfe8f,0x653,0xfeae,0x653,0xfecb,0x20,0xfe8f,0x654,0xfeae,0x654,
        0xfecb,0x20,0xfe8f,0x655,0xfeae,0x655,0xfecb,0x20
    }, shape_excepttashkeel_near[] = {
        0xfef2,0xfe91,0xfeae,0xfecb,0x20,0xfef2,0xfe91,0xfe7c,0xfeae,0xfe76,0xfecb,0x20,
        0xfe72,0xfef2,0xfe91,0xfeae,0xfe78,0xfecb,0x20,0xfe8f,0x670,0xfeae,0x670,0xfecb,
        0x20,0xfe8f,0x653,0xfeae,0x653,0xfecb,0x20,0xfe8f,0x654,0xfeae,0x654,0xfecb,0x20,
        0xfe8f,0x655,0xfeae,0x655,0xfecb,0x20
    };

    static UChar dest[43];

    static UBidiShapeTestCase testCases[] = {
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE | U_SHAPE_LENGTH_FIXED_SPACES_NEAR | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_near,
                UPRV_LENGTHOF(shape_near),
                "TASHKEEL shape_near" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED | U_SHAPE_LENGTH_FIXED_SPACES_NEAR | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                shape_excepttashkeel_near,
                UPRV_LENGTHOF(shape_excepttashkeel_near),
                "TASHKEEL shape_excepttashkeel_near" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testArabicShapingTashkeelSpecialVLTR(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; i++) {
        doShapeArabic(testCases, i, __FILE__, __LINE__);
    }

    log_verbose("testArabicShapingTashkeelSpecialVLTR(): Finished\n");
}

static void
testArabicDeShapingLOGICAL(void)
{
    static const UChar
        source[] = {
            0x0020,0x0020,0x0020,0xFE8D,0xFEF5,0x0020,0xFEE5,0x0020,0xFE8D,0xFEF7,0x0020,
            0xFED7,0xFEFC,0x0020,0xFEE1,0x0020,0xFE8D,0xFEDF,0xFECC,0xFEAE,0xFE91,0xFEF4,
            0xFE94,0x0020,0xFE8D,0xFEDF,0xFEA4,0xFEAE,0xFE93,0x0020,0x0020,0x0020,0x0020
    }, unshape_near[] = {
        0x20,0x20,0x20,0x627,0x644,0x622,0x646,0x20,0x627,0x644,0x623,0x642,0x644,0x627,
        0x645,0x20,0x627,0x644,0x639,0x631,0x628,0x64a,0x629,0x20,0x627,0x644,0x62d,0x631,
        0x629,0x20,0x20,0x20,0x20
    }, unshape_at_end[] = {
        0x20,0x20,0x20,0x627,0x644,0x622,0x20,0x646,0x20,0x627,0x644,0x623,0x20,0x642,
        0x644,0x627,0x20,0x645,0x20,0x627,0x644,0x639,0x631,0x628,0x64a,0x629,0x20,0x627,
        0x644,0x62d,0x631,0x629,0x20
    }, unshape_at_begin[] = {
        0x627,0x644,0x622,0x20,0x646,0x20,0x627,0x644,0x623,0x20,0x642,0x644,0x627,0x20,
        0x645,0x20,0x627,0x644,0x639,0x631,0x628,0x64a,0x629,0x20,0x627,0x644,0x62d,0x631,
        0x629,0x20,0x20,0x20,0x20
    }, unshape_grow_shrink[] = {
        0x20,0x20,0x20,0x627,0x644,0x622,0x20,0x646,0x20,0x627,0x644,0x623,0x20,0x642,
        0x644,0x627,0x20,0x645,0x20,0x627,0x644,0x639,0x631,0x628,0x64a,0x629,0x20,0x627,
        0x644,0x62d,0x631,0x629,0x20,0x20,0x20,0x20
    };

    static UChar dest[MAXLEN];

    static UBidiShapeTestCase testCases[] = {
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_UNSHAPE | U_SHAPE_LENGTH_FIXED_SPACES_NEAR | U_SHAPE_TEXT_DIRECTION_LOGICAL,
                U_ZERO_ERROR,
                unshape_near,
                UPRV_LENGTHOF(unshape_near),
                "unshape_near" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_UNSHAPE | U_SHAPE_LENGTH_FIXED_SPACES_AT_END | U_SHAPE_TEXT_DIRECTION_LOGICAL,
                U_ZERO_ERROR,
                unshape_at_end,
                UPRV_LENGTHOF(unshape_at_end),
                "unshape_at_end" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_UNSHAPE | U_SHAPE_LENGTH_FIXED_SPACES_AT_BEGINNING | U_SHAPE_TEXT_DIRECTION_LOGICAL,
                U_ZERO_ERROR,
                unshape_at_begin,
                UPRV_LENGTHOF(unshape_at_begin),
                "unshape_at_begin" },
            { source,
                UPRV_LENGTHOF(source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_UNSHAPE | U_SHAPE_LENGTH_GROW_SHRINK | U_SHAPE_TEXT_DIRECTION_LOGICAL,
                U_ZERO_ERROR,
                unshape_grow_shrink,
                UPRV_LENGTHOF(unshape_grow_shrink),
                "unshape_grow_shrink" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testArabicDeShapingLOGICAL(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; i++) {
        doShapeArabic(testCases, i, __FILE__, __LINE__);
    }

    log_verbose("testArabicDeShapingLOGICAL(): Finished\n");
}

static void
testArabicShapingTailTest(void)
{
    static const UChar src[] = { 0x0020, 0x0633, 0 };
    static const UChar dst_old[] = { 0xFEB1, 0x200B,0 };
    static const UChar dst_new[] = { 0xFEB1, 0xFE73,0 };

    static UChar dest[3] = { 0x0000, 0x0000,0 };

    static UBidiShapeTestCase testCases[] = {
            { src,
                -1,
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE | U_SHAPE_SEEN_TWOCELL_NEAR,
                U_ZERO_ERROR,
                dst_old,
                2,
                "old tail" },
            { src,
                -1,
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE | U_SHAPE_SEEN_TWOCELL_NEAR | U_SHAPE_TAIL_NEW_UNICODE,
                U_ZERO_ERROR,
                dst_new,
                2,
                "new tail" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testArabicShapingTailTest(): Started, %u test cases:\n", nTestCases);
    log_verbose("SRC: U+%04X U+%04X\n", src[0], src[1]);

    for (i = 0; i < nTestCases; i++) {
        if (i == 0) {
            log_verbose("Trying old tail\n");
        }
        else {
            log_verbose("Trying new tail\n");
        }

        int32_t length = doShapeArabic(testCases, i, __FILE__, __LINE__);

        if (i == 0) {
            if (length != 2) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                    __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                    length, 3);
            }
            else if (u_strncmp(dest, dst_old, UPRV_LENGTHOF(dest))) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): got U+%04X U+%04X expected U+%04X U+%04X\n",
                    __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                    dest[0], dest[1], dst_old[0], dst_old[1]);
            }
            else {
                log_verbose("OK:  U+%04X U+%04X len %d\n",
                    dest[0], dest[1], length);
            }
        }
        else {
            if (length != 2) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): length=%d (expected %d)\n",
                    __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                    length, 3);
            }
            else if (u_strncmp(dest, dst_new, UPRV_LENGTHOF(dest))) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): got U+%04X U+%04X expected U+%04X U+%04X\n",
                    __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                    dest[0], dest[1], dst_new[0], dst_new[1]);
            }
            else {
                log_verbose("OK:  U+%04X U+%04X len %d\n",
                    dest[0], dest[1], length);
            }
        }
    }

    log_verbose("testArabicShapingTailTest(): Finished\n");
}

static void
testArabicShapingForBug5421(void)
{
    static const UChar
        persian_letters_source[] = {
        0x0020, 0x0698, 0x067E, 0x0686, 0x06AF, 0x0020
    }, persian_letters[] = {
        0x0020, 0xFB8B, 0xFB59, 0xFB7D, 0xFB94, 0x0020
    }, tashkeel_aggregation_source[] = {
        0x0020, 0x0628, 0x0651, 0x064E, 0x062A, 0x0631, 0x0645, 0x0020,
        0x0628, 0x064E, 0x0651, 0x062A, 0x0631, 0x0645, 0x0020
    }, tashkeel_aggregation[] = {
        0x0020, 0xFE90, 0xFC60, 0xFE97, 0xFEAE, 0xFEE3,
        0x0020, 0xFE90, 0xFC60, 0xFE97, 0xFEAE, 0xFEE3, 0x0020
    }, untouched_presentation_source[] = {
        0x0020 ,0x0627, 0xfe90,0x0020
    }, untouched_presentation[] = {
        0x0020,0xfe8D, 0xfe90,0x0020
    }, untouched_presentation_r_source[] = {
        0x0020 ,0xfe90, 0x0627, 0x0020
    }, untouched_presentation_r[] = {
        0x0020, 0xfe90,0xfe8D,0x0020
    };

    static UChar dest[MAXLEN];

    static UBidiShapeTestCase testCases[] = {
            { persian_letters_source,
                UPRV_LENGTHOF(persian_letters_source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_LETTERS_SHAPE | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                persian_letters,
                UPRV_LENGTHOF(persian_letters),
                "persian_letters" },
            { tashkeel_aggregation_source,
                UPRV_LENGTHOF(tashkeel_aggregation_source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_AGGREGATE_TASHKEEL | U_SHAPE_PRESERVE_PRESENTATION | U_SHAPE_LETTERS_SHAPE_TASHKEEL_ISOLATED | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                tashkeel_aggregation,
                UPRV_LENGTHOF(tashkeel_aggregation),
                "tashkeel_aggregation" },
            { untouched_presentation_source,
                UPRV_LENGTHOF(untouched_presentation_source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_PRESERVE_PRESENTATION | U_SHAPE_LETTERS_SHAPE | U_SHAPE_TEXT_DIRECTION_VISUAL_LTR,
                U_ZERO_ERROR,
                untouched_presentation,
                UPRV_LENGTHOF(untouched_presentation),
                "untouched_presentation" },
            { untouched_presentation_r_source,
                UPRV_LENGTHOF(untouched_presentation_r_source),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_PRESERVE_PRESENTATION | U_SHAPE_LETTERS_SHAPE | U_SHAPE_TEXT_DIRECTION_LOGICAL,
                U_ZERO_ERROR,
                untouched_presentation_r,
                UPRV_LENGTHOF(untouched_presentation_r),
                "untouched_presentation_r" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testArabicShapingForBug5421(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; i++) {
        doShapeArabic(testCases, i, __FILE__, __LINE__);
    }

    log_verbose("testArabicShapingForBug5421(): Finished\n");
}

static void
testArabicShapingForBug8703(void)
{
    static const UChar
        letters_source1[] = {
            0x0634,0x0651,0x0645,0x0652,0x0633
    }, letters_source2[] = {
        0x0634,0x0651,0x0645,0x0652,0x0633
    }, letters_source3[] = {
       0x0634,0x0651,0x0645,0x0652,0x0633
    }, letters_source4[] = {
        0x0634,0x0651,0x0645,0x0652,0x0633
    }, letters_source5[] = {
        0x0633,0x0652,0x0645,0x0651,0x0634
    }, letters_source6[] = {
        0x0633,0x0652,0x0645,0x0651,0x0634
    }, letters_source7[] = {
        0x0633,0x0652,0x0645,0x0651,0x0634
    }, letters_source8[] = {
        0x0633,0x0652,0x0645,0x0651,0x0634
    }, letters_dest1[] = {
        0x0020,0xFEB7,0xFE7D,0xFEE4,0xFEB2
    }, letters_dest2[] = {
        0xFEB7,0xFE7D,0xFEE4,0xFEB2,0x0020
    }, letters_dest3[] = {
        0xFEB7,0xFE7D,0xFEE4,0xFEB2
    }, letters_dest4[] = {
        0xFEB7,0xFE7D,0xFEE4,0x0640,0xFEB2
    }, letters_dest5[] = {
        0x0020,0xFEB2,0xFEE4,0xFE7D,0xFEB7
    }, letters_dest6[] = {
        0xFEB2,0xFEE4,0xFE7D,0xFEB7,0x0020
    }, letters_dest7[] = {
        0xFEB2,0xFEE4,0xFE7D,0xFEB7
    }, letters_dest8[] = {
        0xFEB2,0x0640,0xFEE4,0xFE7D,0xFEB7
    };

    static UChar dest[MAXLEN];

    static UBidiShapeTestCase testCases[] = {
            { letters_source1,
                UPRV_LENGTHOF(letters_source1),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_RTL | U_SHAPE_TASHKEEL_BEGIN | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest1,
                UPRV_LENGTHOF(letters_dest1),
                "letters_source1" },
            { letters_source2,
                UPRV_LENGTHOF(letters_source2),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_RTL | U_SHAPE_TASHKEEL_END | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest2,
                UPRV_LENGTHOF(letters_dest2),
                "letters_source2" },
            { letters_source3,
                UPRV_LENGTHOF(letters_source3),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_RTL | U_SHAPE_TASHKEEL_RESIZE | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest3,
                UPRV_LENGTHOF(letters_dest3),
                "letters_source3" },
            { letters_source4,
                UPRV_LENGTHOF(letters_source4),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_RTL | U_SHAPE_TASHKEEL_REPLACE_BY_TATWEEL | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest4,
                UPRV_LENGTHOF(letters_dest4),
                "letters_source4" },
            { letters_source5,
                UPRV_LENGTHOF(letters_source5),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_LTR | U_SHAPE_TASHKEEL_BEGIN | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest5,
                UPRV_LENGTHOF(letters_dest5),
                "letters_source5" },
            { letters_source6,
                UPRV_LENGTHOF(letters_source6),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_LTR | U_SHAPE_TASHKEEL_END | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest6,
                UPRV_LENGTHOF(letters_dest6),
                "letters_source6" },
            { letters_source7,
                UPRV_LENGTHOF(letters_source7),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_LTR | U_SHAPE_TASHKEEL_RESIZE | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest7,
                UPRV_LENGTHOF(letters_dest7),
                "letters_source7" },
            { letters_source8,
                UPRV_LENGTHOF(letters_source8),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_LTR | U_SHAPE_TASHKEEL_REPLACE_BY_TATWEEL | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest8,
                UPRV_LENGTHOF(letters_dest8),
                "letters_source8" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testArabicShapingForBug8703(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; i++) {
        doShapeArabic(testCases, i, __FILE__, __LINE__);
    }

    log_verbose("testArabicShapingForBug8703(): Finished\n");
}

static void
testArabicShapingForBug9024(void)
{
    static const UChar
        // Arabic mathematical Symbols 0x1EE00 - 0x1EE1B
        letters_source1[] = {
            0xD83B, 0xDE00, 0xD83B, 0xDE01, 0xD83B, 0xDE02, 0xD83B, 0xDE03, 0x20,
            0xD83B, 0xDE24, 0xD83B, 0xDE05, 0xD83B, 0xDE06, 0x20,
            0xD83B, 0xDE07, 0xD83B, 0xDE08, 0xD83B, 0xDE09, 0x20,
            0xD83B, 0xDE0A, 0xD83B, 0xDE0B, 0xD83B, 0xDE0C, 0xD83B, 0xDE0D, 0x20,
            0xD83B, 0xDE0E, 0xD83B, 0xDE0F, 0xD83B, 0xDE10, 0xD83B, 0xDE11, 0x20,
            0xD83B, 0xDE12, 0xD83B, 0xDE13, 0xD83B, 0xDE14, 0xD83B, 0xDE15, 0x20,
            0xD83B, 0xDE16, 0xD83B, 0xDE17, 0xD83B, 0xDE18, 0x20,
            0xD83B, 0xDE19, 0xD83B, 0xDE1A, 0xD83B, 0xDE1B
            // Arabic mathematical Symbols - Looped Symbols, 0x1EE80 - 0x1EE9B
    }, letters_source2[] = {
        0xD83B, 0xDE80, 0xD83B, 0xDE81, 0xD83B, 0xDE82, 0xD83B, 0xDE83, 0x20,
        0xD83B, 0xDE84, 0xD83B, 0xDE85, 0xD83B, 0xDE86, 0x20,
        0xD83B, 0xDE87, 0xD83B, 0xDE88, 0xD83B, 0xDE89, 0x20,
        0xD83B, 0xDE8B, 0xD83B, 0xDE8C, 0xD83B, 0xDE8D, 0x20,
        0xD83B, 0xDE8E, 0xD83B, 0xDE8F, 0xD83B, 0xDE90, 0xD83B, 0xDE91, 0x20,
        0xD83B, 0xDE92, 0xD83B, 0xDE93, 0xD83B, 0xDE94, 0xD83B, 0xDE95, 0x20,
        0xD83B, 0xDE96, 0xD83B, 0xDE97, 0xD83B, 0xDE98, 0x20,
        0xD83B, 0xDE99, 0xD83B, 0xDE9A, 0xD83B, 0xDE9B
        // Arabic mathematical Symbols - Double-struck Symbols, 0x1EEA1 - 0x1EEBB
    }, letters_source3[] = {
        0xD83B, 0xDEA1, 0xD83B, 0xDEA2, 0xD83B, 0xDEA3, 0x20,
        0xD83B, 0xDEA5, 0xD83B, 0xDEA6, 0x20,
        0xD83B, 0xDEA7, 0xD83B, 0xDEA8, 0xD83B, 0xDEA9, 0x20,
        0xD83B, 0xDEAB, 0xD83B, 0xDEAC, 0xD83B, 0xDEAD, 0x20,
        0xD83B, 0xDEAE, 0xD83B, 0xDEAF, 0xD83B, 0xDEB0, 0xD83B, 0xDEB1, 0x20,
        0xD83B, 0xDEB2, 0xD83B, 0xDEB3, 0xD83B, 0xDEB4, 0xD83B, 0xDEB5, 0x20,
        0xD83B, 0xDEB6, 0xD83B, 0xDEB7, 0xD83B, 0xDEB8, 0x20,
        0xD83B, 0xDEB9, 0xD83B, 0xDEBA, 0xD83B, 0xDEBB
        // Arabic mathematical Symbols - Initial Symbols, 0x1EE21 - 0x1EE3B
    }, letters_source4[] = {
        0xD83B, 0xDE21, 0xD83B, 0xDE22, 0x20,
        0xD83B, 0xDE27, 0xD83B, 0xDE29, 0x20,
        0xD83B, 0xDE2A, 0xD83B, 0xDE2B, 0xD83B, 0xDE2C, 0xD83B, 0xDE2D, 0x20,
        0xD83B, 0xDE2E, 0xD83B, 0xDE2F, 0xD83B, 0xDE30, 0xD83B, 0xDE31, 0x20,
        0xD83B, 0xDE32, 0xD83B, 0xDE34, 0xD83B, 0xDE35, 0x20,
        0xD83B, 0xDE36, 0xD83B, 0xDE37, 0x20,
        0xD83B, 0xDE39, 0xD83B, 0xDE3B
        // Arabic mathematical Symbols - Tailed Symbols
    }, letters_source5[] = {
        0xD83B, 0xDE42, 0xD83B, 0xDE47, 0xD83B, 0xDE49, 0xD83B, 0xDE4B, 0x20,
        0xD83B, 0xDE4D, 0xD83B, 0xDE4E, 0xD83B, 0xDE4F, 0x20,
        0xD83B, 0xDE51, 0xD83B, 0xDE52, 0xD83B, 0xDE54, 0xD83B, 0xDE57, 0x20,
        0xD83B, 0xDE59, 0xD83B, 0xDE5B, 0xD83B, 0xDE5D, 0xD83B, 0xDE5F
        // Arabic mathematical Symbols - Stretched Symbols with 06 range
    }, letters_source6[] = {
        0xD83B, 0xDE21, 0x0633, 0xD83B, 0xDE62, 0x0647
    }, letters_dest1[] = {
        0xD83B, 0xDE00, 0xD83B, 0xDE01, 0xD83B, 0xDE02, 0xD83B, 0xDE03, 0x20,
        0xD83B, 0xDE24, 0xD83B, 0xDE05, 0xD83B, 0xDE06, 0x20,
        0xD83B, 0xDE07, 0xD83B, 0xDE08, 0xD83B, 0xDE09, 0x20,
        0xD83B, 0xDE0A, 0xD83B, 0xDE0B, 0xD83B, 0xDE0C, 0xD83B, 0xDE0D, 0x20,
        0xD83B, 0xDE0E, 0xD83B, 0xDE0F, 0xD83B, 0xDE10, 0xD83B, 0xDE11, 0x20,
        0xD83B, 0xDE12, 0xD83B, 0xDE13, 0xD83B, 0xDE14, 0xD83B, 0xDE15, 0x20,
        0xD83B, 0xDE16, 0xD83B, 0xDE17, 0xD83B, 0xDE18, 0x20,
        0xD83B, 0xDE19, 0xD83B, 0xDE1A, 0xD83B, 0xDE1B
    }, letters_dest2[] = {
        0xD83B, 0xDE80, 0xD83B, 0xDE81, 0xD83B, 0xDE82, 0xD83B, 0xDE83, 0x20,
        0xD83B, 0xDE84, 0xD83B, 0xDE85, 0xD83B, 0xDE86, 0x20,
        0xD83B, 0xDE87, 0xD83B, 0xDE88, 0xD83B, 0xDE89, 0x20,
        0xD83B, 0xDE8B, 0xD83B, 0xDE8C, 0xD83B, 0xDE8D, 0x20,
        0xD83B, 0xDE8E, 0xD83B, 0xDE8F, 0xD83B, 0xDE90, 0xD83B, 0xDE91, 0x20,
        0xD83B, 0xDE92, 0xD83B, 0xDE93, 0xD83B, 0xDE94, 0xD83B, 0xDE95, 0x20,
        0xD83B, 0xDE96, 0xD83B, 0xDE97, 0xD83B, 0xDE98, 0x20,
        0xD83B, 0xDE99, 0xD83B, 0xDE9A, 0xD83B, 0xDE9B
    }, letters_dest3[] = {
        0xD83B, 0xDEA1, 0xD83B, 0xDEA2, 0xD83B, 0xDEA3, 0x20,
        0xD83B, 0xDEA5, 0xD83B, 0xDEA6, 0x20,
        0xD83B, 0xDEA7, 0xD83B, 0xDEA8, 0xD83B, 0xDEA9, 0x20,
        0xD83B, 0xDEAB, 0xD83B, 0xDEAC, 0xD83B, 0xDEAD, 0x20,
        0xD83B, 0xDEAE, 0xD83B, 0xDEAF, 0xD83B, 0xDEB0, 0xD83B, 0xDEB1, 0x20,
        0xD83B, 0xDEB2, 0xD83B, 0xDEB3, 0xD83B, 0xDEB4, 0xD83B, 0xDEB5, 0x20,
        0xD83B, 0xDEB6, 0xD83B, 0xDEB7, 0xD83B, 0xDEB8, 0x20,
        0xD83B, 0xDEB9, 0xD83B, 0xDEBA, 0xD83B, 0xDEBB
    }, letters_dest4[] = {
        0xD83B, 0xDE21, 0xD83B, 0xDE22, 0x20,
        0xD83B, 0xDE27, 0xD83B, 0xDE29, 0x20,
        0xD83B, 0xDE2A, 0xD83B, 0xDE2B, 0xD83B, 0xDE2C, 0xD83B, 0xDE2D, 0x20,
        0xD83B, 0xDE2E, 0xD83B, 0xDE2F, 0xD83B, 0xDE30, 0xD83B, 0xDE31, 0x20,
        0xD83B, 0xDE32, 0xD83B, 0xDE34, 0xD83B, 0xDE35, 0x20,
        0xD83B, 0xDE36, 0xD83B, 0xDE37, 0x20,
        0xD83B, 0xDE39, 0xD83B, 0xDE3B
    }, letters_dest5[] = {
        0xD83B, 0xDE42, 0xD83B, 0xDE47, 0xD83B, 0xDE49, 0xD83B, 0xDE4B, 0x20,
        0xD83B, 0xDE4D, 0xD83B, 0xDE4E, 0xD83B, 0xDE4F, 0x20,
        0xD83B, 0xDE51, 0xD83B, 0xDE52, 0xD83B, 0xDE54, 0xD83B, 0xDE57, 0x20,
        0xD83B, 0xDE59, 0xD83B, 0xDE5B, 0xD83B, 0xDE5D, 0xD83B, 0xDE5F
    }, letters_dest6[] = {
        0xD83B, 0xDE21, 0xFEB1, 0xD83B, 0xDE62, 0xFEE9
    };

    static UChar dest[MAXLEN];

    static UBidiShapeTestCase testCases[] = {
            { letters_source1,
                UPRV_LENGTHOF(letters_source1),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_RTL | U_SHAPE_TASHKEEL_BEGIN | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest1,
                UPRV_LENGTHOF(letters_dest1),
                "letters_source1" },
            { letters_source2,
                UPRV_LENGTHOF(letters_source2),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_RTL | U_SHAPE_TASHKEEL_END | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest2,
                UPRV_LENGTHOF(letters_dest2),
                "letters_source2" },
            { letters_source3,
                UPRV_LENGTHOF(letters_source3),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_RTL | U_SHAPE_TASHKEEL_RESIZE | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest3,
                UPRV_LENGTHOF(letters_dest3),
                "letters_source3" },
            { letters_source4,
                UPRV_LENGTHOF(letters_source4),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_RTL | U_SHAPE_TASHKEEL_REPLACE_BY_TATWEEL | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest4,
                UPRV_LENGTHOF(letters_dest4),
                "letters_source4" },
            { letters_source5,
                UPRV_LENGTHOF(letters_source5),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_LTR | U_SHAPE_TASHKEEL_BEGIN | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest5,
                UPRV_LENGTHOF(letters_dest5),
                "letters_source5" },
            { letters_source6,
                UPRV_LENGTHOF(letters_source6),
                dest,
                UPRV_LENGTHOF(dest),
                U_SHAPE_TEXT_DIRECTION_VISUAL_LTR | U_SHAPE_TASHKEEL_END | U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                letters_dest6,
                UPRV_LENGTHOF(letters_dest6),
                "letters_source6" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("testArabicShapingForBug9024(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; i++) {
        doShapeArabic(testCases, i, __FILE__, __LINE__);
    }

    log_verbose("testArabicShapingForBug9024(): Finished\n");
}

static void
testArabicShapingForNewCharacters(void)
{
    static const UChar letterForms[][5] = {
      { 0x0679, 0xFB66, 0xFB67, 0xFB68, 0xFB69 },  /* TTEH */
      { 0x067A, 0xFB5E, 0xFB5F, 0xFB60, 0xFB61 },  /* TTEHEH */
      { 0x067B, 0xFB52, 0xFB53, 0xFB54, 0xFB55 },  /* BEEH */
      { 0x0688, 0xFB88, 0xFB89, 0, 0 },            /* DDAL */
      { 0x068C, 0xFB84, 0xFB85, 0, 0 },            /* DAHAL */
      { 0x068D, 0xFB82, 0xFB83, 0, 0 },            /* DDAHAL */
      { 0x068E, 0xFB86, 0xFB87, 0, 0 },            /* DUL */
      { 0x0691, 0xFB8C, 0xFB8D, 0, 0 },            /* RREH */
      { 0x06BA, 0xFB9E, 0xFB9F, 0, 0 },            /* NOON GHUNNA */
      { 0x06BB, 0xFBA0, 0xFBA1, 0xFBA2, 0xFBA3 },  /* RNOON */
      { 0x06BE, 0xFBAA, 0xFBAB, 0xFBAC, 0xFBAD },  /* HEH DOACHASHMEE */
      { 0x06C0, 0xFBA4, 0xFBA5, 0, 0 },            /* HEH WITH YEH ABOVE */
      { 0x06C1, 0xFBA6, 0xFBA7, 0xFBA8, 0xFBA9 },  /* HEH GOAL */
      { 0x06C5, 0xFBE0, 0xFBE1, 0, 0 },            /* KIRGIHIZ OE */
      { 0x06C6, 0xFBD9, 0xFBDA, 0, 0 },            /* OE */
      { 0x06C7, 0xFBD7, 0xFBD8, 0, 0 },            /* U */
      { 0x06C8, 0xFBDB, 0xFBDC, 0, 0 },            /* YU */
      { 0x06C9, 0xFBE2, 0xFBE3, 0, 0 },            /* KIRGIZ YU */
      { 0x06CB, 0xFBDE, 0xFBDF, 0, 0},             /* VE */
      { 0x06D0, 0xFBE4, 0xFBE5, 0xFBE6, 0xFBE7 },  /* E */
      { 0x06D2, 0xFBAE, 0xFBAF, 0, 0 },            /* YEH BARREE */
      { 0x06D3, 0xFBB0, 0xFBB1, 0, 0 },            /* YEH BARREE WITH HAMZA ABOVE */
      { 0x0622, 0xFE81, 0xFE82, 0, 0, },           /* ALEF WITH MADDA ABOVE */
      { 0x0623, 0xFE83, 0xFE84, 0, 0, },           /* ALEF WITH HAMZA ABOVE */
      { 0x0624, 0xFE85, 0xFE86, 0, 0, },           /* WAW WITH HAMZA ABOVE */
      { 0x0625, 0xFE87, 0xFE88, 0, 0, },           /* ALEF WITH HAMZA BELOW */
      { 0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C, }, /* YEH WITH HAMZA ABOVE */
      { 0x0627, 0xFE8D, 0xFE8E, 0, 0, },           /* ALEF */
      { 0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92, }, /* BEH */
      { 0x0629, 0xFE93, 0xFE94, 0, 0, },           /* TEH MARBUTA */
      { 0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98, }, /* TEH */
      { 0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C, }, /* THEH */
      { 0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0, }, /* JEEM */
      { 0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4, }, /* HAH */
      { 0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8, }, /* KHAH */
      { 0x062F, 0xFEA9, 0xFEAA, 0, 0, },           /* DAL */
      { 0x0630, 0xFEAB, 0xFEAC, 0, 0, },           /* THAL */
      { 0x0631, 0xFEAD, 0xFEAE, 0, 0, },           /* REH */
      { 0x0632, 0xFEAF, 0xFEB0, 0, 0, },           /* ZAIN */
      { 0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4, }, /* SEEN */
      { 0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8, }, /* SHEEN */
      { 0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC, }, /* SAD */
      { 0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0, }, /* DAD */
      { 0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4, }, /* TAH */
      { 0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8, }, /* ZAH */
      { 0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC, }, /* AIN */
      { 0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0, }, /* GHAIN */
      { 0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4, }, /* FEH */
      { 0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8, }, /* QAF */
      { 0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC, }, /* KAF */
      { 0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0, }, /* LAM */
      { 0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4, }, /* MEEM */
      { 0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8, }, /* NOON */
      { 0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC, }, /* HEH */
      { 0x0648, 0xFEED, 0xFEEE, 0, 0, },           /* WAW */
      { 0x0649, 0xFEEF, 0xFEF0, 0, 0, },           /* ALEF MAKSURA */
      { 0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4, }, /* YEH */
      { 0x064E, 0xFE76, 0, 0, 0xFE77, },           /* FATHA */
      { 0x064F, 0xFE78, 0, 0, 0xFE79, },           /* DAMMA */
      { 0x0650, 0xFE7A, 0, 0, 0xFE7B, },           /* KASRA */
      { 0x0651, 0xFE7C, 0, 0, 0xFE7D, },           /* SHADDA */
      { 0x0652, 0xFE7E, 0, 0, 0xFE7F, },           /* SUKUN */
      { 0x0679, 0xFB66, 0xFB67, 0xFB68, 0xFB69, }, /* TTEH */
      { 0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59, }, /* PEH */
      { 0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D, }, /* TCHEH */
      { 0x0688, 0xFB88, 0xFB89, 0, 0, },           /* DDAL */
      { 0x0691, 0xFB8C, 0xFB8D, 0, 0, },           /* RREH */
      { 0x0698, 0xFB8A, 0xFB8B, 0, 0, },           /* JEH */
      { 0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91, }, /* KEHEH */
      { 0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95, }, /* GAF */
      { 0x06BA, 0xFB9E, 0xFB9F, 0, 0, },           /* NOON GHUNNA */
      { 0x06BE, 0xFBAA, 0xFBAB, 0xFBAC, 0xFBAD, }, /* HEH DOACHASHMEE */
      { 0x06C0, 0xFBA4, 0xFBA5, 0, 0, },           /* HEH WITH YEH ABOVE */
      { 0x06C1, 0xFBA6, 0xFBA7, 0xFBA8, 0xFBA9, }, /* HEH GOAL */
      { 0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF, }, /* FARSI YEH */
      { 0x06D2, 0xFBAE, 0xFBAF, 0, 0, },           /* YEH BARREE */
      { 0x06D3, 0xFBB0, 0xFBB1, 0, 0, } };          /* YEH BARREE WITH HAMZA ABOVE */

    int32_t i;

    log_verbose("testArabicShapingForNewCharacters(): Started, %u characters:\n", UPRV_LENGTHOF(letterForms));

    for (i = 0; i < UPRV_LENGTHOF(letterForms); ++i) {
        _testPresentationForms(letterForms[i]);
    }

    log_verbose("testArabicShapingForNewCharacters(): Finished\n");

}

static void _testPresentationForms(const UChar* in)
{
    enum Forms { GENERIC, ISOLATED, FINAL, INITIAL, MEDIAL };

    // This character is used to check whether the in-character is rewritten correctly
    // and whether the surrounding characters are shaped correctly as well.
    UChar otherChar[] = { 0x0628, 0xfe8f, 0xfe90, 0xfe91, 0xfe92 };
    static UChar src[3];
    static UChar dst[3];

    static UBidiShapeTestCase testCases[] = {
            { src,
                1,
                dst,
                1,
                U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                0,
                1,
                "_testAllForms: shaping isolated" },
            { src,
                1,
                dst,
                1,
                U_SHAPE_LETTERS_UNSHAPE,
                U_ZERO_ERROR,
                0,
                1,
                "_testAllForms: unshaping isolated" },
            { src,
                2,
                dst,
                2,
                U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                0,
                2,
                "_testAllForms: shaping final" },
            { src,
                2,
                dst,
                2,
                U_SHAPE_LETTERS_UNSHAPE,
                U_ZERO_ERROR,
                0,
                2,
                "_testAllForms: unshaping final" },
            { src,
                2,
                dst,
                2,
                U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                0,
                2,
                "_testAllForms: shaping initial" },
            { src,
                2,
                dst,
                2,
                U_SHAPE_LETTERS_UNSHAPE,
                U_ZERO_ERROR,
                0,
                2,
                "_testAllForms: unshaping initial" },
            { src,
                3,
                dst,
                3,
                U_SHAPE_LETTERS_SHAPE,
                U_ZERO_ERROR,
                0,
                3,
                "_testAllForms: shaping medial" },
            { src,
                3,
                dst,
                3,
                U_SHAPE_LETTERS_UNSHAPE,
                U_ZERO_ERROR,
                0,
                3,
                "_testAllForms: unshaping medial" },
    };

    uint32_t i, nTestCases = sizeof(testCases) / sizeof(testCases[0]);

    log_verbose("_testPresentationForms(): Started, %u test cases:\n", nTestCases);

    for (i = 0; i < nTestCases; i++) {
        if (i < 2) {
            /* Testing isolated shaping */
            src[0] = in[GENERIC];
        }
        else if (i < 4) {
            /* Testing final shaping */
            src[0] = otherChar[GENERIC];
            src[1] = in[GENERIC];
        }
        else if (i < 6) {
            /* Testing initial shaping */
            src[0] = in[GENERIC];
            src[1] = otherChar[GENERIC];
        }
        else if (i < 8) {
            /* Testing medial shaping */
            src[0] = otherChar[0];
            src[1] = in[GENERIC];
            src[2] = otherChar[0];
        }

        doShapeArabic(testCases, i, __FILE__, __LINE__);

        if (i == 0) {
            if (dst[0] != in[ISOLATED]) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                    __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                    in[GENERIC]);
            }
        }
        else if (i == 1) {
            if (src[0] != in[GENERIC]) {
                log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                    __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                    in[GENERIC]);
            }
        }
        else if (i < 4) {
            if (in[FINAL] != 0) {
                if (i == 2) {
                    if (dst[0] != otherChar[INITIAL] || dst[1] != in[FINAL]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
                else if (i == 3) {
                    if (src[0] != otherChar[GENERIC] || src[1] != in[GENERIC]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
            }
            else {
                if (i == 2) {
                    if (dst[0] != otherChar[ISOLATED] || dst[1] != in[ISOLATED]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
                else if (i == 3) {
                    if (src[0] != otherChar[GENERIC] || src[1] != in[GENERIC]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
            }
        }
        else if (i < 6) {
            if (in[INITIAL] != 0) {
                if (i == 4) {
                    if (dst[0] != in[INITIAL] || dst[1] != otherChar[FINAL]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
                else if (i == 5) {
                    if (src[0] != in[GENERIC] || src[1] != otherChar[GENERIC]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
            }
            else {
                if (i == 4) {
                    if (dst[0] != in[ISOLATED] || dst[1] != otherChar[ISOLATED]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
                else if (i == 5) {
                    if (src[0] != in[GENERIC] || src[1] != otherChar[GENERIC]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
            }
        }
        else if (i < 8) {
            if (in[MEDIAL] != 0) {
                if (i == 6) {
                    if (dst[0] != otherChar[INITIAL] || dst[1] != in[MEDIAL] || dst[2] != otherChar[FINAL]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
                else if (i == 7) {
                    if (src[0] != otherChar[GENERIC] || src[1] != in[GENERIC] || src[2] != otherChar[GENERIC]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
            }
            else {
                if (i == 6) {
                    if (dst[0] != otherChar[INITIAL] || dst[1] != in[FINAL] || dst[2] != otherChar[ISOLATED]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
                else if (i == 7) {
                    if (src[0] != otherChar[GENERIC] || src[1] != in[GENERIC] || src[2] != otherChar[GENERIC]) {
                        log_err("%s(%d): %s(%s, tests[%d]: %s): %x\n",
                            __FILE__, __LINE__, "u_shapeArabic", Encodings[UEncoding_U16].description, i, testCases[i].pMessage,
                            in[GENERIC]);
                    }
                }
            }
        }
    }

    log_verbose("_testPresentationForms(): Finished\n");
}
