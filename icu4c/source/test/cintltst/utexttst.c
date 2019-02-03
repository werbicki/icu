// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2005-2013, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
 /*
 * File utexttst.c
 *
 * Modification History:
 *
 *   Date          Name               Description
 *   06/13/2005    Andy Heninger      Creation
 *******************************************************************************
 */

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/utext.h"
#include "unicode/ucnv.h"
#include "cintltst.h"
#include "memory.h"
#include "string.h"

static void TestAPI(void);
static void TestU8(void);
static void TestU16(void);
static void TestU32(void);
void addUTextTest(TestNode** root);

void
addUTextTest(TestNode** root)
{
    addTest(root, &TestAPI, "tsutil/UTextTest/TestAPI");
    addTest(root, &TestU8, "tsutil/UTextTest/TestU8");
    addTest(root, &TestU16, "tsutil/UTextTest/TestU16");
    addTest(root, &TestU32, "tsutil/UTextTest/TestU32");
}

static UBool gFailed = FALSE;
static int gTestNum = 0;

#define TEST_ASSERT(x) \
{ if ((x)==FALSE) {log_err("Test #%d failure in file %s at line %d\n", gTestNum, __FILE__, __LINE__);\
                     gFailed = TRUE;\
   }}

#define TEST_SUCCESS(status) \
{ if (U_FAILURE(status)) {log_err("Test #%d failure in file %s at line %d. Error = \"%s\"\n", \
       gTestNum, __FILE__, __LINE__, u_errorName(status)); \
       gFailed = TRUE;\
   }}

#define BUFFER_SIZE 2000

enum UEncoding {
    UEncoding_U8 = 8,
    UEncoding_U16 = 16,
    UEncoding_U32 = 32
};

// Quick and dirty random number generator.
// Don't use library so that results are portable and predictable.
static uint32_t m_seed = 1;
inline static uint32_t m_rand(void)
{
    m_seed = m_seed * 1103515245 + 12345;
    return (uint32_t)(m_seed / 65536) % 32768;
}

static int64_t
u32_strlen(const UChar32 *s)
{
    const UChar32 *t = s;
    while (*t != 0) {
        ++t;
    }
    return t - s;
}

static int32_t
u32_strcmp(const UChar32 *s1,
    const UChar32 *s2)
{
    UChar32 c1, c2;

    for (;;) {
        c1 = *s1++;
        c2 = *s2++;
        if (c1 != c2 || c1 == 0) {
            break;
        }
    }
    return (int32_t)c1 - (int32_t)c2;
}

// Build up a mapping between code points and UTF-8 code unit indexes.
static void BuildU8Map(
    const uint8_t* u8String,
    int32_t u8Len,
    int32_t cpCount,
    int32_t* u8NativeIdx,
    UChar32* u8Map
)
{
    int32_t k, j;

    for (k = 0, j = 0; j < cpCount; j++) { // Code point number
        UChar32 c;
        u8NativeIdx[j] = k;
        U8_NEXT(u8String, k, u8Len, c);
        u8Map[j] = c;
    }
    u8NativeIdx[cpCount] = u8Len; // Position following the last char in utf-8 string.
}

// Build up a mapping between code points and UTF-16 code unit indexes.
static int32_t BuildU16Map(
    const UChar* u16String,
    int32_t u16Len,
    int32_t* u16NativeIdx,
    UChar32* u16Map
)
{
    int32_t cpCount, k, j;

    for (cpCount = 0, k = 0, j = 0; k < u16Len; ) {
        UChar32 c;
        u16NativeIdx[j] = k;
        U16_NEXT(u16String, k, u16Len, c);
        u16Map[j] = c;
        j++;
        cpCount++;
    }
    u16NativeIdx[j] = k; // Position following the last char in utf-16 string.

    return cpCount;
}

// Build up a mapping between code points and UTF-32 code unit indexes.
static void BuildU32Map(
    const UChar32* u32String,
    int32_t cpCount,
    int32_t* u32NativeIdx,
    UChar32* u32Map
)
{
    int32_t i;

    for (i = 0; i < cpCount; i++) { // Code point number
        UChar32 c = u32String[i];
        u32NativeIdx[i] = i;
        u32Map[i] = c;
    }
    u32NativeIdx[cpCount] = i; // Position following the last char in utf-8 string.
}

// Build up a mapping between code points and UTF-8 code unit indexes.
// Uses UChar string as input.
static void BuildU8MapFromU16(
    const UChar* u16String,
    int32_t u16Len,
    int32_t* u8NativeIdx
)
{
    int32_t i, j, l;

    for (i = 0, j = 0, l = 0; i < u16Len; ) {
        UChar32 c;
        u8NativeIdx[j] = l;
        U16_NEXT(u16String, i, u16Len, c);
        if ((!U16_IS_SINGLE(c)) || (U_IS_SUPPLEMENTARY(c)))
        {
            u8NativeIdx[j++] = l;
        }
        l += U8_LENGTH(c);
        j++;
    }
    u8NativeIdx[j] = l; // Position following the last char in utf-16 string.
}

static const struct {
    const char* u8In;
    int32_t u8InLen;
    int32_t u8InCap;
    UErrorCode errorCode;
} testCasesUTextOpen[] = {
    { "", -1, -1, U_ZERO_ERROR }
    , { "", 0, -1, U_ZERO_ERROR }
    , { "", -1, 0, U_ZERO_ERROR }
    , { "", 0, 0, U_ZERO_ERROR }
    // Terminate
    , { NULL, 0, 0, U_ZERO_ERROR, } // Always last! Do not change! Only entry with u8In == NULL
};

static const struct {
    const char* u8In;
    int32_t u8InLen;
    int32_t u8InCap;
    UErrorCode errorCode;
} testCasesUTextAccess[] = {
    { "ABC", -1, -1, U_ZERO_ERROR }
    // Terminate
    , { NULL, 0, 0, U_ZERO_ERROR } // Always last! Do not change! Only entry with u8In == NULL
};

static const struct {
    const char* u8In;
    int32_t u8InLen;
    int32_t u8InCap;
    UErrorCode errorCode;
} testCasesUTextExtract[] = {
    { "ABC", -1, -1, U_ZERO_ERROR }
    // Terminate
    , { NULL, 0, 0, U_ZERO_ERROR } // Always last! Do not change! Only entry with u8In == NULL
};

static const struct {
    const char* u8In;
    int32_t u8InLen;
    int32_t u8InCap;
    int32_t start;
    int32_t limit;
    const char* u8Replace;
    int32_t u8ReplaceLen;
    UErrorCode errorCode;
    const char* u8Out;
    int32_t u8OutChangeLen;
} testCasesUTextReplace[] = {
    { "", -1, -1, 0, 0, NULL, 0, U_ZERO_ERROR, "", 0 }
    , { "", -1, -1, -1, -1, NULL, -1, U_ILLEGAL_ARGUMENT_ERROR, "", 0 } // src == NULL && length != 0
    , { "", -1, -1, 1, 1, NULL, 1, U_ILLEGAL_ARGUMENT_ERROR, "", 0 } // src == NULL && length != 0
    , { "", -1, -1, 0, 0, NULL, -1, U_ILLEGAL_ARGUMENT_ERROR, NULL, 0 } // src == NULL && length != 0
    , { "", -1, -1, 0, 0, NULL, 1, U_ILLEGAL_ARGUMENT_ERROR, NULL, 0 } // src == NULL && length != 0
    , { "", -1, -1, 1, 0, NULL, 1, U_ILLEGAL_ARGUMENT_ERROR, NULL, 0 } // start > limit, src == NULL && length != 0, U_ILLEGAL_ARGUMENT_ERROR > U_INDEX_OUTOFBOUNDS_ERROR
    , { "", -1, -1, 0, 0, "", 0, U_ZERO_ERROR, "", 0 }
    , { "", -1, -1, 0, 0, "", -1, U_ZERO_ERROR, "", 0 } // length == -1
    , { "", -1, 0, 0, 0, "D", 0, U_ZERO_ERROR, "", 0 } // capacity == 0, length == 0
    , { "", -1, 0, 0, 0, "D", 1, U_BUFFER_OVERFLOW_ERROR, "", 0 } // capacity == 0, length == 1
    , { "", -1, 1, 0, 0, "D", 1, U_STRING_NOT_TERMINATED_WARNING, "D", 1 } // capacity == 0, length == 0
    , { "", -1, -1, 0, 0, "D", 0, U_ZERO_ERROR, "", 0 }
    , { "", -1, -1, 0, 0, "D", 1, U_ZERO_ERROR, "D", 1 }
    , { "", -1, -1, 0, 0, "D", -1, U_ZERO_ERROR, "D", 1 } // length == -1
    , { "", -1, -1, 0, 1, "DD", 1, U_ZERO_ERROR, "D", 1 }
    , { "", -1, -1, 0, 1, "DD", -1, U_ZERO_ERROR, "DD", 2 }
    // Insert
    , /*16*/ { "ABC", -1, -1, 0, -1, "D", -1, U_INDEX_OUTOFBOUNDS_ERROR, "ABC", 0 } // start > limit
    , { "ABC", -1, -1, -1, -1, "D", -1, U_ZERO_ERROR, "DABC", 1 } // start, limit are pinned
    , { "ABC", -1, -1, 0, 0, "D", -1, U_ZERO_ERROR, "DABC", 1 }
    , { "ABC", -1, -1, 1, 1, "D", -1, U_ZERO_ERROR, "ADBC", 1 }
    , { "ABC", -1, -1, 3, 3, "D", -1, U_ZERO_ERROR, "ABCD", 1 }
    , { "ABC", -1, -1, 3, 4, "D", -1, U_ZERO_ERROR, "ABCD", 1 } // limit is pinned
    , { "ABC", -1, -1, 4, 4, "D", -1, U_ZERO_ERROR, "ABCD", 1 } // start, limit are pinned
    // Delete
    , /*23*/ { "ABC", -1, -1, 0, 3, "", -1, U_ZERO_ERROR, "", -3 } // Full delete
    , { "ABC", -1, -1, 0, 3, "DEF", -1, U_ZERO_ERROR, "DEF", 0 } // Full replace
    , { "DABC", -1, -1, 0, -1, "", -1, U_INDEX_OUTOFBOUNDS_ERROR, "DABC", 0 } // start > limit
    , { "DABC", -1, -1, -1, -1, "", -1, U_ZERO_ERROR, "DABC", 0 } // start, limit are pinned
    , { "DABC", -1, -1, 0, 1, "", -1, U_ZERO_ERROR, "ABC", -1 }
    , { "ADBC", -1, -1, 1, 2, "", -1, U_ZERO_ERROR, "ABC", -1 }
    , { "ABCD", -1, -1, 3, 4, "", -1, U_ZERO_ERROR, "ABC", -1 }
    , { "ABCD", -1, -1, 4, 5, "", -1, U_ZERO_ERROR, "ABCD", 0 } // limit is pinned
    , { "ABCD", -1, -1, 5, 5, "", -1, U_ZERO_ERROR, "ABCD", 0 } // start, limit are pinned
    // Both
    , /*32*/ { "ABC", -1, -1, 1, 2, "D", -1, U_ZERO_ERROR, "ADC", 0 }
    , { "ABC", -1, -1, 1, 2, "DE", -1, U_ZERO_ERROR, "ADEC", 1 }
    , { "ABC", -1, -1, 1, 2, "DEF", -1, U_ZERO_ERROR, "ADEFC", 2 }
    , { "ABC", -1, -1, 0, 2, "DEF", -1, U_ZERO_ERROR, "DEFC", 1 }
    , { "ABC", -1, -1, 2, 2, "DEF", -1, U_ZERO_ERROR, "ABDEFC", 3 }
    // Terminate
    , { NULL, 0, 0, 0, 0, NULL, 0, U_ZERO_ERROR, NULL, 0 } // Always last! Do not change! Only entry with u8In == NULL
};

static const struct {
    const char* u8In;
    int32_t u8InLen;
    int32_t u8InCap;
    int32_t start;
    int32_t limit;
    int32_t dest;
    UBool move;
    UErrorCode errorCode;
    const char* u8Out;
} testCasesUTextCopy[] = {
    { "", -1, -1, 0, 0, 0, FALSE, U_ZERO_ERROR, "" }
    , { "", -1, -1, -1, -1, -1, FALSE, U_ZERO_ERROR, "" }
    , { "", -1, -1, 1, 1, 1, FALSE, U_ZERO_ERROR, "" }
    , { "", -1, -1, 0, 1, 0, FALSE, U_ZERO_ERROR, "" }
    , { "", -1, -1, 1, 0, 0, FALSE, U_INDEX_OUTOFBOUNDS_ERROR, NULL } // start > limit
    , { "ABC", -1, -1, 0, 1, 0, FALSE, U_ZERO_ERROR, NULL } // [start, limit) can overlap [dest, limit-start) on the start
    , { "", -1, 0, 0, 0, 0, FALSE, U_ZERO_ERROR, "" } // capacity == 0, length == 0
    , { "", -1, 0, 0, 0, 1, FALSE, U_ZERO_ERROR, NULL } // capacity == 0, length == 1
    , { "", -1, 0, 0, 0, 0, TRUE, U_ZERO_ERROR, "" } // capacity == 0, length == 0
    , { "", -1, 0, 0, 0, 1, TRUE, U_ZERO_ERROR, "" } // capacity == 0, length == 1
    // Copy
    , { "ABC", -1, -1, 0, 1, 2, FALSE, U_ZERO_ERROR, "ABAC" } // start < dest
    , { "ABC", -1, -1, 2, 3, 1, FALSE, U_ZERO_ERROR, "ACBC" } // start > dest
    // Move
    , { "ABC", -1, -1, 0, 1, 2, TRUE, U_ZERO_ERROR, "BAC" } // start < dest
    , { "ABC", -1, -1, 2, 3, 1, TRUE, U_ZERO_ERROR, "ACB" } // start > dest
    // Terminate
    , { NULL, 0, 0, 0, 0, 0, FALSE, U_ZERO_ERROR, NULL } // Always last! Do not change! Only entry with u8In == NULL
};

static const struct {
    const char* u8In;
} testCasesTestString[] = {
    { "abcd\\U00010001xyz" }
    , { "" }
    , { "\\U00010001" }
    , { "abc\\U00010001" }
    , { "\\U00010001abc" }
    // Terminate
    , { NULL }
};

static void TestAccessAPI(UText* ut, int32_t i, UBool isClone)
{
    gTestNum++;
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    UConverter* u16Convertor = ucnv_open("UTF8", &status);
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u16Convertor != NULL);
    if (gFailed) {
        return;
    }

    UChar u16BufIn[BUFFER_SIZE + sizeof(void *)];
    u_memset(u16BufIn, 0, sizeof(u16BufIn) / sizeof(UChar));

    if (testCasesUTextAccess[i].u8In)
    {
        ucnv_toUChars(u16Convertor, u16BufIn, sizeof(u16BufIn), testCasesUTextAccess[i].u8In, -1, &status);
        TEST_ASSERT(!U_FAILURE(status));
    }

    status = U_ZERO_ERROR;

    UChar32 c;
    int64_t len;
    UBool b;
    int64_t j;

    status = U_ZERO_ERROR;

    TEST_ASSERT(ut != NULL);
    TEST_SUCCESS(status);

    b = utext_isLengthExpensive(ut);
    if ((testCasesUTextAccess[i].u8InCap >= 0) || (isClone) || (utext_isWritable(ut))) {
        TEST_ASSERT(b == FALSE);
    }
    else {
        TEST_ASSERT(b == TRUE);
    }

    len = utext_nativeLength(ut);
    TEST_ASSERT(len == u_strlen(u16BufIn));
    b = utext_isLengthExpensive(ut);
    TEST_ASSERT(b == FALSE);

    c = utext_char32At(ut, 0);
    TEST_ASSERT(c == u16BufIn[0]);

    c = utext_current32(ut);
    TEST_ASSERT(c == u16BufIn[0]);

    c = utext_next32(ut);
    TEST_ASSERT(c == u16BufIn[0]);
    c = utext_current32(ut);
    TEST_ASSERT(c == u16BufIn[1]);

    c = utext_previous32(ut);
    TEST_ASSERT(c == u16BufIn[0]);
    c = utext_current32(ut);
    TEST_ASSERT(c == u16BufIn[0]);

    c = utext_next32From(ut, 1);
    TEST_ASSERT(c == u16BufIn[1]);
    c = utext_next32From(ut, u_strlen(u16BufIn));
    TEST_ASSERT(c == U_SENTINEL);

    c = utext_previous32From(ut, 2);
    TEST_ASSERT(c == u16BufIn[1]);
    j = utext_getNativeIndex(ut);
    TEST_ASSERT(j == 1);

    utext_setNativeIndex(ut, 0);
    b = utext_moveIndex32(ut, 1);
    TEST_ASSERT(b == TRUE);
    j = utext_getNativeIndex(ut);
    TEST_ASSERT(j == 1);

    b = utext_moveIndex32(ut, u_strlen(u16BufIn) - 1);
    TEST_ASSERT(b == TRUE);
    j = utext_getNativeIndex(ut);
    TEST_ASSERT(j == u_strlen(u16BufIn));

    b = utext_moveIndex32(ut, 1);
    TEST_ASSERT(b == FALSE);
    j = utext_getNativeIndex(ut);
    TEST_ASSERT(j == u_strlen(u16BufIn));

    utext_setNativeIndex(ut, 0);
    c = UTEXT_NEXT32(ut);
    TEST_ASSERT(c == u16BufIn[0]);
    c = utext_current32(ut);
    TEST_ASSERT(c == u16BufIn[1]);

    c = UTEXT_PREVIOUS32(ut);
    TEST_ASSERT(c == u16BufIn[0]);
    c = UTEXT_PREVIOUS32(ut);
    TEST_ASSERT(c == U_SENTINEL);

    ucnv_close(u16Convertor);
}

static void TestExtractAPI(UText* ut, int32_t i)
{
    gTestNum++;
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    UConverter* u16Convertor = ucnv_open("UTF8", &status);
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u16Convertor != NULL);
    if (gFailed) {
        return;
    }

    UChar u16BufIn[BUFFER_SIZE + sizeof(void *)];
    u_memset(u16BufIn, 0, sizeof(u16BufIn) / sizeof(UChar));

    if (testCasesUTextExtract[i].u8In)
    {
        ucnv_toUChars(u16Convertor, u16BufIn, sizeof(u16BufIn), testCasesUTextExtract[i].u8In, -1, &status);
        TEST_ASSERT(!U_FAILURE(status));
    }

    status = U_ZERO_ERROR;

    UChar u16Buf[100];
    int32_t j;

    status = U_ZERO_ERROR;
    j = utext_extract(ut, 0, 100, NULL, 0, &status);
    TEST_ASSERT(status == U_BUFFER_OVERFLOW_ERROR);
    TEST_ASSERT(j == u_strlen(u16BufIn));

    status = U_ZERO_ERROR;
    memset(u16Buf, 0, sizeof(u16Buf));
    j = utext_extract(ut, 0, 100, u16Buf, 100, &status);
    TEST_SUCCESS(status);
    TEST_ASSERT(j == u_strlen(u16BufIn));
    j = u_strcmp(u16BufIn, u16Buf);
    TEST_ASSERT(j == 0);

    ucnv_close(u16Convertor);
}

static void TestReplaceAPI(UText* ut, int32_t i, int32_t length)
{
    gTestNum++;
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    TEST_ASSERT(utext_isWritable(ut) == TRUE);
    if (gFailed) {
        return;
    }

    UConverter* u16Convertor = ucnv_open("UTF8", &status);
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u16Convertor != NULL);
    if (gFailed) {
        return;
    }

    char u8Buf[BUFFER_SIZE + sizeof(void *)];
    UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
    UChar u16BufOut[BUFFER_SIZE + sizeof(void *)];

    memset(u8Buf, 0, sizeof(u8Buf));
    u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
    u_memset(u16BufOut, 0, sizeof(u16BufOut) / sizeof(UChar));

    UChar *u16Replace = NULL;
    int32_t u16ReplaceLenBefore = 0;
    int32_t u16ReplaceLen = testCasesUTextReplace[i].u8ReplaceLen;
    UChar u16BufReplace[BUFFER_SIZE + sizeof(void *)];

    u_memset(u16BufReplace, 0, sizeof(u16BufReplace) / sizeof(UChar));

    status = U_ZERO_ERROR;

    if (testCasesUTextReplace[i].u8Replace)
    {
        u16ReplaceLenBefore = ucnv_toUChars(u16Convertor, u16BufReplace, sizeof(u16BufReplace), testCasesUTextReplace[i].u8Replace, testCasesUTextReplace[i].u8ReplaceLen, &status);
        TEST_ASSERT(!U_FAILURE(status));

        u16Replace = u16BufReplace;
        if (testCasesUTextReplace[i].u8ReplaceLen == -1)
            u16ReplaceLen = -1;
        else
            u16ReplaceLen = u16ReplaceLenBefore;
    }

    if (testCasesUTextReplace[i].u8Out)
    {
        ucnv_toUChars(u16Convertor, u16BufOut, sizeof(u16BufOut), testCasesUTextReplace[i].u8Out, -1, &status);
        TEST_ASSERT(!U_FAILURE(status));
    }

    status = U_ZERO_ERROR;

    TEST_ASSERT(utext_isWritable(ut) == TRUE);
    TEST_ASSERT(utext_hasMetaData(ut) == FALSE);

    int32_t utLen = utext_replace(ut, testCasesUTextReplace[i].start, testCasesUTextReplace[i].limit, u16Replace, u16ReplaceLen, &status);

    TEST_ASSERT(status == testCasesUTextReplace[i].errorCode);
    if (status != testCasesUTextReplace[i].errorCode)
        log_err("\nutext_replace(testCase[%d]): ErrorCode is wrong, expected %s, got %s\n", i, u_errorName(testCasesUTextReplace[i].errorCode), u_errorName(status));

    TEST_ASSERT(utLen == testCasesUTextReplace[i].u8OutChangeLen);
    if (utLen != testCasesUTextReplace[i].u8OutChangeLen)
        log_err("\nutext_replace(testCase[%d]): Length delta is wrong, expected %d, got %d\n", i, testCasesUTextReplace[i].u8OutChangeLen, utLen);

    int32_t index = 0;
    if (!U_FAILURE(status))
    {
        int32_t start = testCasesUTextReplace[i].start;
        if (start < 0)
            start = 0;
        else if (start > length)
            start = length;

        int32_t limit = testCasesUTextReplace[i].limit;
        if (limit < 0)
            limit = 0;
        else if (limit > length)
            limit = length;

        if ((u16ReplaceLenBefore > 0) || (limit - start > 0))
            index = limit + u16ReplaceLenBefore - (limit - start);
    }
    int32_t utPos = (int32_t)utext_getNativeIndex(ut);
    TEST_ASSERT(utPos == index);
    if (utPos != index)
        log_err("\nutext_replace(testCase[%d]): Iterator index is wrong, expected %d, got %d\n", i, index, utPos);

    if (testCasesUTextReplace[i].u8Out)
    {
        TEST_ASSERT(utext_nativeLength(ut) == u_strlen(u16BufOut));
        if (utext_nativeLength(ut) != u_strlen(u16BufOut))
            log_err("\nutext_replace(testCase[%d]): Length is wrong, expected %d, got %d\n", i, u_strlen(u16BufOut), utext_nativeLength(ut));

        status = U_ZERO_ERROR;

        utext_extract(ut, 0, utext_nativeLength(ut), u16Buf, sizeof(u16Buf), &status);

        TEST_ASSERT(u_strcmp(u16Buf, u16BufOut) == 0);
        if (u_strcmp(u16Buf, u16BufOut) != 0) {
            ucnv_fromUChars(u16Convertor, u8Buf, sizeof(u8Buf), u16Buf, -1, &status);
            log_err("\nutext_replace(testCase[%d]): Result is wrong, expected '%s', got '%s'\n", i, testCasesUTextReplace[i].u8Out, u8Buf);
        }
    }

    ucnv_close(u16Convertor);
}

static void TestCopyAPI(UText* ut, int32_t i, int32_t length)
{
    gTestNum++;
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    TEST_ASSERT(utext_isWritable(ut) == TRUE);
    if (gFailed) {
        return;
    }

    UConverter* u16Convertor = ucnv_open("UTF8", &status);
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u16Convertor != NULL);
    if (gFailed) {
        return;
    }

    uint8_t u8Buf[BUFFER_SIZE + sizeof(void *)];
    UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
    UChar u16BufOut[BUFFER_SIZE + sizeof(void *)];

    memset(u8Buf, 0, sizeof(u8Buf));
    u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
    u_memset(u16BufOut, 0, sizeof(u16BufOut) / sizeof(UChar));

    if (testCasesUTextCopy[i].u8Out)
    {
        ucnv_toUChars(u16Convertor, u16BufOut, sizeof(u16BufOut), testCasesUTextCopy[i].u8Out, -1, &status);
        TEST_ASSERT(!U_FAILURE(status));
    }

    status = U_ZERO_ERROR;

    TEST_ASSERT(utext_isWritable(ut) == TRUE);
    TEST_ASSERT(utext_hasMetaData(ut) == FALSE);

    utext_copy(ut, testCasesUTextCopy[i].start, testCasesUTextCopy[i].limit, testCasesUTextCopy[i].dest, testCasesUTextCopy[i].move, &status);

    TEST_ASSERT(status == testCasesUTextCopy[i].errorCode);
    if (status != testCasesUTextCopy[i].errorCode)
        log_err("\nutext_copy(testCase[%d]): ErrorCode is wrong, expected %s, got %s\n", i, u_errorName(testCasesUTextCopy[i].errorCode), u_errorName(status));

    int32_t index = 0;
    if (!U_FAILURE(status))
    {
        index = testCasesUTextCopy[i].dest;
        if (index < 0)
            index = 0;
        else if (index > length)
            index = length;

        int32_t start = testCasesUTextCopy[i].start;
        if (start < 0)
            start = 0;
        else if (start > length)
            start = length;

        int32_t limit = testCasesUTextCopy[i].limit;
        if (limit < 0)
            limit = 0;
        else if (limit > length)
            limit = length;

        if ((!testCasesUTextCopy[i].move)
            || ((testCasesUTextCopy[i].move) && (start > index))
            )
        {
            index += (limit - start);
        }
    }
    int32_t utPos = (int32_t)utext_getNativeIndex(ut);
    TEST_ASSERT(utPos == index);
    if (utPos != index)
        log_err("\nutext_copy(testCase[%d]): Iterator index is wrong, expected %d, got %d\n", i, index, utPos);

    if (testCasesUTextCopy[i].u8Out)
    {
        TEST_ASSERT(utext_nativeLength(ut) == u_strlen(u16BufOut));
        if (utext_nativeLength(ut) != u_strlen(u16BufOut))
            log_err("\nutext_copy(testCase[%d]): Length is wrong, expected %d, got %d\n", i, u_strlen(u16BufOut), utext_nativeLength(ut));

        status = U_ZERO_ERROR;

        utext_extract(ut, 0, utext_nativeLength(ut), u16Buf, sizeof(u16Buf), &status);

        TEST_ASSERT(u_strcmp(u16Buf, u16BufOut) == 0);
        if (u_strcmp(u16Buf, u16BufOut) != 0) {
            ucnv_fromUChars(u16Convertor, (char*)u8Buf, sizeof(u8Buf), u16Buf, -1, &status);
            log_err("\nutext_copy(testCase[%d]): Result is wrong, expected '%s', got '%s'\n", i, testCasesUTextCopy[i].u8Out, u8Buf);
        }
    }

    ucnv_close(u16Convertor);
}

void TestAccess(UText *ut, int32_t cpCount, int32_t* cpNativeIdx, UChar32* cpMap)
{
    gTestNum++;
    gFailed = FALSE;
    //UErrorCode status = U_ZERO_ERROR;

    //
    //  Check the length from the UText
    //
    int64_t expectedLen = cpNativeIdx[cpCount];
    int64_t utlen = utext_nativeLength(ut);
    TEST_ASSERT(expectedLen == utlen);

    //
    //  Iterate forwards, verify that we get the correct code points
    //   at the correct native offsets.
    //
    int         i = 0;
    int64_t     index;
    int64_t     expectedIndex = 0;
    int64_t     foundIndex = 0;
    UChar32     expectedC;
    UChar32     foundC;
    int64_t     len;

    for (i = 0; i < cpCount; i++) {
        expectedIndex = cpNativeIdx[i];
        foundIndex = utext_getNativeIndex(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        expectedC = cpMap[i];
        foundC = utext_next32(ut);
        TEST_ASSERT(expectedC == foundC);
        foundIndex = utext_getPreviousNativeIndex(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        if (gFailed) {
            return;
        }
    }
    foundC = utext_next32(ut);
    TEST_ASSERT(foundC == U_SENTINEL);

    // Repeat above, using macros
    utext_setNativeIndex(ut, 0);
    for (i = 0; i < cpCount; i++) {
        expectedIndex = cpNativeIdx[i];
        foundIndex = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        expectedC = cpMap[i];
        foundC = UTEXT_NEXT32(ut);
        TEST_ASSERT(expectedC == foundC);
        if (gFailed) {
            return;
        }
    }
    foundC = UTEXT_NEXT32(ut);
    TEST_ASSERT(foundC == U_SENTINEL);

    //
    //  Forward iteration (above) should have left index at the
    //   end of the input, which should == length().
    //
    len = utext_nativeLength(ut);
    foundIndex = utext_getNativeIndex(ut);
    TEST_ASSERT(len == foundIndex);
    if (len != foundIndex)
        TEST_ASSERT(len != foundIndex);

    //
    // Iterate backwards over entire test string
    //
    len = utext_getNativeIndex(ut);
    utext_setNativeIndex(ut, len);
    for (i = cpCount - 1; i >= 0; i--) {
        expectedC = cpMap[i];
        expectedIndex = cpNativeIdx[i];
        int64_t prevIndex = utext_getPreviousNativeIndex(ut);
        foundC = utext_previous32(ut);
        foundIndex = utext_getNativeIndex(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        TEST_ASSERT(expectedC == foundC);
        TEST_ASSERT(prevIndex == foundIndex);
        if (gFailed) {
            return;
        }
    }

    //
    //  Backwards iteration, above, should have left our iterator
    //   position at zero, and continued backwards iterationshould fail.
    //
    foundIndex = utext_getNativeIndex(ut);
    TEST_ASSERT(foundIndex == 0);
    foundIndex = utext_getPreviousNativeIndex(ut);
    TEST_ASSERT(foundIndex == 0);

    foundC = utext_previous32(ut);
    TEST_ASSERT(foundC == U_SENTINEL);
    foundIndex = utext_getNativeIndex(ut);
    TEST_ASSERT(foundIndex == 0);
    foundIndex = utext_getPreviousNativeIndex(ut);
    TEST_ASSERT(foundIndex == 0);

    // And again, with the macros
    utext_setNativeIndex(ut, len);
    for (i = cpCount - 1; i >= 0; i--) {
        expectedC = cpMap[i];
        expectedIndex = cpNativeIdx[i];
        foundC = UTEXT_PREVIOUS32(ut);
        foundIndex = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == foundIndex);
        TEST_ASSERT(expectedC == foundC);
        if (gFailed) {
            return;
        }
    }

    //
    //  Backwards iteration, above, should have left our iterator
    //   position at zero, and continued backwards iterationshould fail.
    //
    foundIndex = UTEXT_GETNATIVEINDEX(ut);
    TEST_ASSERT(foundIndex == 0);

    foundC = UTEXT_PREVIOUS32(ut);
    TEST_ASSERT(foundC == U_SENTINEL);
    foundIndex = UTEXT_GETNATIVEINDEX(ut);
    TEST_ASSERT(foundIndex == 0);
    if (gFailed) {
        return;
    }

    //
    //  next32From(), prevous32From(), Iterate in a somewhat random order.
    //
    int  cpIndex = 0;
    for (i = 0; i < cpCount; i++) {
        cpIndex = (cpIndex + 9973) % cpCount;
        index = cpNativeIdx[cpIndex];
        expectedC = cpMap[cpIndex];
        foundC = utext_next32From(ut, index);
        TEST_ASSERT(expectedC == foundC);
        if (gFailed) {
            return;
        }
    }

    cpIndex = 0;
    for (i = 0; i < cpCount; i++) {
        cpIndex = (cpIndex + 9973) % cpCount;
        index = cpNativeIdx[cpIndex + 1];
        expectedC = cpMap[cpIndex];
        foundC = utext_previous32From(ut, index);
        TEST_ASSERT(expectedC == foundC);
        if (gFailed) {
            return;
        }
    }

    //
    // moveIndex(int32_t delta);
    //

    // Walk through frontwards, incrementing by one
    utext_setNativeIndex(ut, 0);
    for (i = 1; i <= cpCount; i++) {
        utext_moveIndex32(ut, 1);
        index = utext_getNativeIndex(ut);
        expectedIndex = cpNativeIdx[i];
        TEST_ASSERT(expectedIndex == index);
        index = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == index);
    }

    // Walk through frontwards, incrementing by two
    utext_setNativeIndex(ut, 0);
    for (i = 2; i < cpCount; i += 2) {
        utext_moveIndex32(ut, 2);
        index = utext_getNativeIndex(ut);
        expectedIndex = cpNativeIdx[i];
        TEST_ASSERT(expectedIndex == index);
        index = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == index);
    }

    // walk through the string backwards, decrementing by one.
    i = cpNativeIdx[cpCount];
    utext_setNativeIndex(ut, i);
    for (i = cpCount; i >= 0; i--) {
        expectedIndex = cpNativeIdx[i];
        index = utext_getNativeIndex(ut);
        TEST_ASSERT(expectedIndex == index);
        index = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == index);
        utext_moveIndex32(ut, -1);
    }


    // walk through backwards, decrementing by three
    i = cpNativeIdx[cpCount];
    utext_setNativeIndex(ut, i);
    for (i = cpCount; i >= 0; i -= 3) {
        expectedIndex = cpNativeIdx[i];
        index = utext_getNativeIndex(ut);
        TEST_ASSERT(expectedIndex == index);
        index = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(expectedIndex == index);
        utext_moveIndex32(ut, -3);
    }
}

void TestExtract(
    UText *ut,
    const UChar* u16String,
    int32_t u16Len,
    int32_t cpCount,
    int32_t* cpNativeIdx
)
{
    gTestNum++;
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    //
    //  Check the length from the UText
    //
    int64_t expectedLen = cpNativeIdx[cpCount];
    int64_t utlen = utext_nativeLength(ut);
    TEST_ASSERT(expectedLen == utlen);

    //
    // Extract
    //
    UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
    status = U_ZERO_ERROR;
    expectedLen = u16Len;
    int32_t len = utext_extract(ut, 0, utlen, u16Buf, sizeof(u16Buf) / sizeof(UChar), &status);
    TEST_SUCCESS(status);
    TEST_ASSERT(len == expectedLen);
    if (len != expectedLen)
        log_err("\nTestExtract(): Length is wrong, expected %d, got %d\n", expectedLen, len);
    int compareResult = u_strcmp(u16String, u16Buf);
    TEST_ASSERT(compareResult == 0);
    if (compareResult != 0)
        log_err("\nTestExtract(): Result is wrong\n");

    status = U_ZERO_ERROR;
    len = utext_extract(ut, 0, utlen, NULL, 0, &status);
    if (utlen == 0) {
        TEST_ASSERT(status == U_STRING_NOT_TERMINATED_WARNING);
    }
    else {
        TEST_ASSERT(status == U_BUFFER_OVERFLOW_ERROR);
    }
    TEST_ASSERT(len == expectedLen);

    status = U_ZERO_ERROR;
    u_memset(u16Buf, 0x5555, sizeof(u16Buf) / sizeof(UChar));
    len = utext_extract(ut, 0, utlen, u16Buf, 1, &status);
    if (u16Len == 0) {
        TEST_SUCCESS(status);
        TEST_ASSERT(u16Buf[0] == 0);
    }
    else {
        // Buf len == 1, extracting a single 16 bit value.
        // If the data char is supplementary, it doesn't matter whether the buffer remains unchanged,
        //   or whether the lead surrogate of the pair is extracted.
        //   It's a buffer overflow error in either case.
        TEST_ASSERT(u16Buf[0] == u16String[0] ||
            (u16Buf[0] == 0x5555 && U_IS_SUPPLEMENTARY(u16String[0])));
        TEST_ASSERT(u16Buf[1] == 0x5555);
        if (u16Len == 1) {
            TEST_ASSERT(status == U_STRING_NOT_TERMINATED_WARNING);
        }
        else {
            TEST_ASSERT(status == U_BUFFER_OVERFLOW_ERROR);
        }
    }
}

void TestReplace(
    UText *ut,
    const UChar* u16String,
    int32_t u16Len,
    int32_t nativeStart,        // Range to be replaced, in UText native units.
    int32_t nativeLimit,
    int32_t u16Start,           // Range to be replaced, in UTF-16 units
    int32_t u16Limit,           //    for use in the reference UnicodeString.
    const UChar* u16RepString,
    int32_t u16RepLen,
    enum UEncoding encoding
)
{
    gTestNum++;
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
    u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));

    // Do the replace operation in the buffer, to produce a reference result.
    u_memcpy(u16Buf, u16String, u16Start);
    u_memcpy(&u16Buf[u16Start], u16RepString, u16RepLen);
    u_memcpy(&u16Buf[u16Start + u16RepLen], &u16String[u16Limit], (u16Len + u16RepLen) - (u16Start + u16RepLen));
    u16Buf[u16Len + u16RepLen] = 0;

    // Clone the target UText.  The test will be run in the cloned copy
    // so that we don't alter the original.
    UText* targetUT = utext_clone(NULL, ut, TRUE, FALSE, &status);
    TEST_SUCCESS(status);

    // Do the replace on the UText under test.
    int32_t actualDelta = utext_replace(targetUT, nativeStart, nativeLimit, u16RepString, u16RepLen, &status);
    int32_t expectedDelta = u16RepLen - (nativeLimit - nativeStart);
    TEST_ASSERT(actualDelta == expectedDelta);

    int32_t u8NativeIdx[BUFFER_SIZE + sizeof(void *)];
    BuildU8MapFromU16(u16Buf, u16Len + u16RepLen, u8NativeIdx);

    // Compare the results.
    int32_t usi = 0;    // UnicodeString postion, utf-16 index.
    int64_t uti = 0;    // UText position, native index.
    int32_t cpi;        // char32 position (code point index)
    UChar32 usc;        // code point from Unicode String
    UChar32 utc;        // code point from UText
    int64_t expectedNativeLength = 0;
    utext_setNativeIndex(targetUT, 0);
    for (cpi = 0; ; cpi++) {
        U16_GET(u16Buf, 0, usi, u16Len + u16RepLen, usc);
        utc = utext_next32(targetUT);
        if (utc < 0) {
            break;
        }
        if (encoding == UEncoding_U8) {
            TEST_ASSERT(uti == u8NativeIdx[usi]); // Pass in U16 index, get U8 index back
            if (uti != u8NativeIdx[usi])
                log_err("\nTestReplace(): Index is wrong expected %d, got %d\n", u8NativeIdx[usi], uti);
        }
        else if (encoding == UEncoding_U32) {
            TEST_ASSERT(uti == cpi); // Match by code point
            if (uti != cpi)
                log_err("\nTestReplace(): Index is wrong expected %d, got %d\n", cpi, uti);
        }
        else {
            TEST_ASSERT(uti == usi);
            if (uti != usi)
                log_err("\nTestReplace(): Index is wrong expected %d, got %d\n", usi, uti);
        }
        TEST_ASSERT(utc == usc);
        if (utc != usc)
            log_err("\nTestReplace(): Result is wrong at %d, expected %d, got %d\n", uti, utc, usc);
        U16_FWD_N(u16Buf, usi, u16Len + u16RepLen, 1);
        uti = utext_getNativeIndex(targetUT);
        if (gFailed) {
            goto cleanupAndReturn;
        }
    }
    expectedNativeLength = utext_nativeLength(ut) + expectedDelta;
    uti = utext_getNativeIndex(targetUT);
    TEST_ASSERT(uti == expectedNativeLength);

cleanupAndReturn:
    utext_close(targetUT);
}

void TestCopyMove(
    UText *ut,
    const UChar* u16String,
    int32_t u16Len,
    UBool move,
    int32_t nativeStart,
    int32_t nativeLimit,
    int32_t nativeDest,
    int32_t u16Start,
    int32_t u16Limit,
    int32_t u16Dest,
    enum UEncoding encoding
)
{
    gTestNum++;
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
    int32_t u16RepLen = ((move) ? 0 : u16Limit - u16Start);
    u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));

    // Do the copy/move operation in the buffer, to produce a reference result.
    u_memcpy(u16Buf, u16String, u16Dest);
    u_memcpy(&u16Buf[u16Dest], &u16String[u16Start], u16Limit - u16Start);
    u_memcpy(&u16Buf[u16Dest + (u16Limit - u16Start)], &u16String[u16Dest], (u16Len + u16RepLen) - u16Dest);
    if (move) {
        if (u16Start <= u16Dest) {
            u_memcpy(&u16Buf[u16Start], &u16Buf[u16Limit], (u16Len + (u16Limit - u16Start)) - u16Limit);
        }
        else {
            u_memcpy(&u16Buf[u16Start + (u16Limit - u16Start)], &u16Buf[u16Limit + (u16Limit - u16Start)], (u16Len + u16RepLen) - (u16Start + u16Limit - u16Start));
        }
    }
    u16Buf[u16Len + u16RepLen] = 0;

    // Clone the target UText.  The test will be run in the cloned copy
    // so that we don't alter the original.
    UText* targetUT = utext_clone(NULL, ut, TRUE, FALSE, &status);
    TEST_SUCCESS(status);

    // Do the copy/move on the UText under test.
    utext_copy(targetUT, nativeStart, nativeLimit, nativeDest, move, &status);

    // Compare the results.
    if (nativeDest > nativeStart && nativeDest < nativeLimit) {
        TEST_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
    }
    else {
        TEST_SUCCESS(status);

        int32_t u8NativeIdx[BUFFER_SIZE + sizeof(void *)];
        BuildU8MapFromU16(u16Buf, u16Len + u16RepLen, u8NativeIdx);

        // Compare the results of the two parallel tests
        int32_t  usi = 0;    // UnicodeString postion, utf-16 index.
        int64_t  uti = 0;    // UText position, native index.
        int32_t  cpi;        // char32 position (code point index)
        UChar32  usc;        // code point from Unicode String
        UChar32  utc;        // code point from UText
        utext_setNativeIndex(targetUT, 0);
        for (cpi = 0; ; cpi++) {
            U16_GET(u16Buf, 0, usi, u16Len + u16RepLen, usc);
            utc = utext_next32(targetUT);
            if (utc < 0) {
                break;
            }
            if (encoding == UEncoding_U8) {
                TEST_ASSERT(uti == u8NativeIdx[usi]); // Pass in U16 index, get U8 index back
                if (uti != u8NativeIdx[usi])
                    log_err("\nTestCopyMove(): Index is wrong expected %d, got %d\n", u8NativeIdx[usi], uti);
            }
            else if (encoding == UEncoding_U32) {
                TEST_ASSERT(uti == cpi); // Match by code point
                if (uti != cpi)
                    log_err("\nTestCopyMove(): Index is wrong expected %d, got %d\n", cpi, uti);
            }
            else {
                TEST_ASSERT(uti == usi);
                if (uti != usi)
                    log_err("\nTestCopyMove(): Index is wrong expected %d, got %d\n", usi, uti);
            }
            TEST_ASSERT(utc == usc);
            if (utc != usc)
                log_err("\nTestCopyMove(): Result is wrong at %d, expected %d, got %d\n", uti, utc, usc);
            U16_FWD_N(u16Buf, usi, u16Len + u16RepLen, 1);
            uti = utext_getNativeIndex(targetUT);
            if (gFailed) {
                goto cleanupAndReturn;
            }
        }
        int64_t expectedNativeLength = utext_nativeLength(ut);
        if (move == FALSE) {
            expectedNativeLength += nativeLimit - nativeStart;
        }
        uti = utext_getNativeIndex(targetUT);
        TEST_ASSERT(uti == expectedNativeLength);
    }

cleanupAndReturn:
    utext_close(targetUT);
}

void TestCMR(
    UText *ut,
    const UChar* u16String,
    int32_t u16Len,
    int32_t cpCount,
    int32_t* cpNativeIdx,
    int32_t* u16NativeIdx,
    const char* fullRepString,
    enum UEncoding encoding
)
{
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    TEST_ASSERT(utext_isWritable(ut) == TRUE);
    if (gFailed) {
        return;
    }

    UConverter *u16Convertor = ucnv_open("UTF8", &status);
    UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u16Convertor != NULL);
    if (gFailed) {
        return;
    }

    status = U_ZERO_ERROR;

    int  srcLengthType;       // Loop variables for selecting the postion and length
    int  srcPosType;          //   of the block to operate on within the source text.
    int  destPosType;

    int  srcIndex = 0;       // Code Point indexes of the block to operate on for
    int  srcLength = 0;       //   a specific test.

    int  destIndex = 0;       // Code point index of the destination for a copy/move test.

    int32_t  nativeStart = 0; // Native unit indexes for a test.
    int32_t  nativeLimit = 0;
    int32_t  nativeDest = 0;

    int32_t  u16Start = 0; // UTF-16 indexes for a test.
    int32_t  u16Limit = 0; //   used when performing the same operation in a Unicode String
    int32_t  u16Dest = 0;

    // Iterate over a whole series of source index, length and a target indexes.
    // This is done with code point indexes; these will be later translated to native
    // indexes using the cpMap.
    for (srcLengthType = 1; srcLengthType <= 3; srcLengthType++) {
        switch (srcLengthType) {
        case 1: srcLength = 1; break;
        case 2: srcLength = 5; break;
        case 3: srcLength = cpCount / 3;
        }
        for (srcPosType = 1; srcPosType <= 5; srcPosType++) {
            switch (srcPosType) {
            case 1: srcIndex = 0; break;
            case 2: srcIndex = 1; break;
            case 3: srcIndex = cpCount - srcLength; break;
            case 4: srcIndex = cpCount - srcLength - 1; break;
            case 5: srcIndex = cpCount / 2; break;
            }
            if (srcIndex < 0 || srcIndex + srcLength > cpCount) {
                // filter out bogus test cases -
                //   those with a source range that falls of an edge of the string.
                continue;
            }

            // Copy and move tests.
            // Iterate over a variety of destination positions.
            for (destPosType = 1; destPosType <= 4; destPosType++) {
                switch (destPosType) {
                case 1: destIndex = 0; break;
                case 2: destIndex = 1; break;
                case 3: destIndex = srcIndex - 1; break;
                case 4: destIndex = srcIndex + srcLength + 1; break;
                case 5: destIndex = cpCount - 1; break;
                case 6: destIndex = cpCount; break;
                }
                if (destIndex<0 || destIndex>cpCount) {
                    // filter out bogus test cases.
                    continue;
                }

                nativeStart = cpNativeIdx[srcIndex];
                nativeLimit = cpNativeIdx[srcIndex + srcLength];
                nativeDest = cpNativeIdx[destIndex];

                u16Start = u16NativeIdx[srcIndex];
                u16Limit = u16NativeIdx[srcIndex + srcLength];
                u16Dest = u16NativeIdx[destIndex];

                gFailed = FALSE;
                TestCopyMove(ut, u16String, u16Len, FALSE,
                    nativeStart, nativeLimit, nativeDest,
                    u16Start, u16Limit, u16Dest, encoding);

                TestCopyMove(ut, u16String, u16Len, TRUE,
                    nativeStart, nativeLimit, nativeDest,
                    u16Start, u16Limit, u16Dest, encoding);

                if (gFailed) {
                    return;
                }
            }

            //  Replace tests.
            for (int32_t replStrLen = 0; replStrLen < 20; replStrLen++) {
                u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
                int32_t u16RepLen = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), fullRepString, replStrLen, &status);

                TestReplace(ut, u16String, u16Len,
                    nativeStart, nativeLimit,
                    u16Start, u16Limit,
                    u16Buf, u16RepLen, encoding);

                if (gFailed) {
                    return;
                }
            }

        }
    }

    ucnv_close(u16Convertor);
}

// Check isWritable() and freeze() behavior.
void TestFreeze(UText* ut) {
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    UBool writable = utext_isWritable(ut);
    TEST_ASSERT(writable == TRUE);

    utext_freeze(ut);

    writable = utext_isWritable(ut);
    TEST_ASSERT(writable == FALSE);

    utext_replace(ut, 1, 2, NULL, 0, &status);
    TEST_ASSERT(status == U_NO_WRITE_PERMISSION);

    utext_copy(ut, 1, 2, 0, TRUE, &status);
    TEST_ASSERT(status == U_NO_WRITE_PERMISSION);
}

// Index to positions not on code point boundaries.
static void TestNotOnCodePoints(
    UText* ut,
    int32_t* startMap,
    int32_t* nextMap,
    int32_t* prevMap,
    UChar32* c32Map,
    UChar32* pr32Map,
    int32_t* exLen
)
{
    gFailed = FALSE;
    UErrorCode status = U_ZERO_ERROR;

    // Check setIndex
    int32_t i;
    int32_t startMapLimit = (int32_t)(sizeof(startMap) / sizeof((startMap)[0]));
    for (i = 0; i < startMapLimit; i++) {
        utext_setNativeIndex(ut, i);
        int64_t cpIndex = utext_getNativeIndex(ut);
        TEST_ASSERT(cpIndex == startMap[i]);
        cpIndex = UTEXT_GETNATIVEINDEX(ut);
        TEST_ASSERT(cpIndex == startMap[i]);
    }

    // Check char32At
    for (i = 0; i < startMapLimit; i++) {
        UChar32 c32 = utext_char32At(ut, i);
        TEST_ASSERT(c32 == c32Map[i]);
        int64_t cpIndex = utext_getNativeIndex(ut);
        TEST_ASSERT(cpIndex == startMap[i]);
    }

    // Check utext_next32From
    for (i = 0; i < startMapLimit; i++) {
        UChar32 c32 = utext_next32From(ut, i);
        TEST_ASSERT(c32 == c32Map[i]);
        int64_t cpIndex = utext_getNativeIndex(ut);
        TEST_ASSERT(cpIndex == nextMap[i]);
    }

    // check utext_previous32From
    for (i = 0; i < startMapLimit; i++) {
        gTestNum++;
        UChar32 c32 = utext_previous32From(ut, i);
        TEST_ASSERT(c32 == pr32Map[i]);
        int64_t cpIndex = utext_getNativeIndex(ut);
        TEST_ASSERT(cpIndex == prevMap[i]);
    }

    // check Extract
    //   Extract from i to i+1, which may be zero or one code points,
    //     depending on whether the indices straddle a cp boundary.
    for (i = 0; i < startMapLimit; i++) {
        UChar buf[3];
        status = U_ZERO_ERROR;
        int32_t  extractedLen = utext_extract(ut, i, i + 1, buf, 3, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(extractedLen == exLen[i]);
        if (extractedLen > 0) {
            UChar32  c32;
            /* extractedLen-extractedLen == 0 is used to get around a compiler warning. */
            U16_GET(buf, 0, extractedLen - extractedLen, extractedLen, c32);
            TEST_ASSERT(c32 == c32Map[i]);
        }
    }
}

static void TestAPI(void)
{
    // Close of an unitialized UText.  Shouldn't blow up.
    {
        UText ut;
        memset(&ut, 0, sizeof(UText));
        utext_close(&ut);
        utext_close(NULL);
    }

    // Double-close of a UText.  Shouldn't blow up.  UText should still be usable.
    {
        UErrorCode status = U_ZERO_ERROR;
        UText ut = UTEXT_INITIALIZER;
        const char *s = "Hello, World";
        UText *ut2 = utext_openConstU8(&ut, (const uint8_t*)s, -1, -1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(ut2 == &ut);

        UText *ut3 = utext_close(&ut);
        TEST_ASSERT(ut3 == &ut);

        UText *ut4 = utext_close(&ut);
        TEST_ASSERT(ut4 == &ut);

        utext_openConstU8(&ut, (const uint8_t*)s, -1, -1, &status);
        TEST_SUCCESS(status);
        utext_close(&ut);
    }

    // Re-use of a UText, chaining through each of the types of UText.
    // If it doesn't blow up, and doesn't leak, it's probably working fine.
    {
        UErrorCode status = U_ZERO_ERROR;
        UText ut = UTEXT_INITIALIZER;
        UText  *utp;
        const char *s = "Hello, World";
        const char *s1 = "\x66\x67\x68";
        UChar s2[] = { (UChar)0x41, (UChar)0x42, (UChar)0 };
        UChar32 s3[] = { (UChar32)0x41, (UChar32)0x42, (UChar32)0 };

        utp = utext_openConstU8(&ut, (const uint8_t*)s1, -1, -1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);

        utp = utext_openConstU16(&ut, s2, -1, -1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);

        utp = utext_openConstU32(&ut, s3, -1, -1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);

        utp = utext_close(&ut);
        TEST_ASSERT(utp == &ut);

        utp = utext_openConstU8(&ut, (const uint8_t*)s, -1, -1, &status);
        TEST_SUCCESS(status);
        TEST_ASSERT(utp == &ut);
    }
}

static void TestU8(void)
{
    UErrorCode status = U_ZERO_ERROR;

    const char* fullRepString = "This is an arbitrary string that will be used as replacement text";

    UConverter *u16Convertor = ucnv_open("UTF8", &status);
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u16Convertor != NULL);

    if ((U_FAILURE(status)) || (u16Convertor == NULL)) {
        return;
    }

    // utext_openU8
    {
        // Simple Access Test Cases
        for (int32_t i = 0; (testCasesUTextAccess[i].u8In != NULL); i++)
        {
            uint8_t *u8In = NULL;
            int32_t u8InLenBefore = 0;
            int32_t u8InLen = testCasesUTextAccess[i].u8InLen;
            int32_t u8InCap = testCasesUTextAccess[i].u8InCap;
            uint8_t u8BufIn[BUFFER_SIZE + sizeof(void *)];

            memset(u8BufIn, 0, sizeof(u8BufIn));

            if (u8InCap == -1)
                u8InCap = BUFFER_SIZE;

            if (testCasesUTextAccess[i].u8In)
            {
                u8InLenBefore = (int32_t)strlen(strcpy((char*)u8BufIn, testCasesUTextAccess[i].u8In));

                u8In = u8BufIn;
                if (testCasesUTextAccess[i].u8InLen == -1)
                    u8InLen = -1;
                else
                    u8InLen = u8InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText *ut = utext_openConstU8(NULL, u8In, u8InLen, u8InCap, &status);
            TEST_SUCCESS(status);
            UText* utsc;
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccessAPI(ut, i, FALSE);

                utsc = utext_clone(NULL, ut, FALSE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utsc, i, TRUE);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utdc, i, TRUE);
                    utext_close(utdc);
                }

                utext_close(ut);
            }

            // Reuse ut, utsc, utc
            ut = utext_openU8(NULL, u8In, u8InLen, u8InCap, &status);
            TEST_SUCCESS(status);
            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccessAPI(ut, i, FALSE);

                utsc = utext_clone(NULL, ut, FALSE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utsc, i, TRUE);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utdc, i, TRUE);
                    utext_close(utdc);
                }

                utext_close(ut);
            }
        }

        // Simple Extract Test Cases
        for (int32_t i = 0; (testCasesUTextExtract[i].u8In != NULL); i++)
        {
            uint8_t *u8In = NULL;
            int32_t u8InLenBefore = 0;
            int32_t u8InLen = testCasesUTextExtract[i].u8InLen;
            int32_t u8InCap = testCasesUTextExtract[i].u8InCap;
            uint8_t u8BufIn[BUFFER_SIZE + sizeof(void *)];

            memset(u8BufIn, 0, sizeof(u8BufIn));

            if (u8InCap == -1)
                u8InCap = BUFFER_SIZE;

            if (testCasesUTextExtract[i].u8In)
            {
                u8InLenBefore = (int32_t)strlen(strcpy((char*)u8BufIn, testCasesUTextExtract[i].u8In));

                u8In = u8BufIn;
                if (testCasesUTextExtract[i].u8InLen == -1)
                    u8InLen = -1;
                else
                    u8InLen = u8InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText *ut = utext_openConstU8(NULL, u8In, u8InLen, u8InCap, &status);
            TEST_SUCCESS(status);
            UText* utsc;
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                TestExtractAPI(ut, i);

                utsc = utext_clone(NULL, ut, FALSE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utsc, i);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utdc, i);
                    utext_close(utdc);
                }

                utext_close(ut);
            }

            // Reuse ut, utsc, utc
            ut = utext_openU8(NULL, u8In, u8InLen, u8InCap, &status);
            TEST_SUCCESS(status);
            if ((ut) && (!U_FAILURE(status)))
            {
                TestExtractAPI(ut, i);

                utsc = utext_clone(NULL, ut, FALSE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utsc, i);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utdc, i);
                    utext_close(utdc);
                }

                utext_close(ut);
            }
        }

        // Simple Replace Test Cases
        for (int32_t i = 0; (testCasesUTextReplace[i].u8In != NULL); i++)
        {
            uint8_t *u8In = NULL;
            int32_t u8InLenBefore = 0;
            int32_t u8InLen = testCasesUTextReplace[i].u8InLen;
            int32_t u8InCap = testCasesUTextReplace[i].u8InCap;
            uint8_t u8BufIn[BUFFER_SIZE + sizeof(void *)];

            memset(u8BufIn, 0, sizeof(u8BufIn));

            if (u8InCap == -1)
                u8InCap = BUFFER_SIZE;

            if (testCasesUTextReplace[i].u8In)
            {
                u8InLenBefore = (int32_t)strlen(strcpy((char*)u8BufIn, testCasesUTextReplace[i].u8In));

                u8In = u8BufIn;
                if (testCasesUTextReplace[i].u8InLen == -1)
                    u8InLen = -1;
                else
                    u8InLen = u8InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText* ut = utext_openU8(NULL, u8In, u8InLen, u8InCap, &status);
            TEST_SUCCESS(status);
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestReplaceAPI(utdc, i, u8InLenBefore);
                    TestFreeze(utdc);
                    utext_close(utdc);
                }

                TestReplaceAPI(ut, i, u8InLenBefore);
                TestFreeze(ut);
                utext_close(ut);
            }
        }

        // Simple Copy Test Cases
        for (int32_t i = 0; (testCasesUTextCopy[i].u8In != NULL); i++)
        {
            uint8_t *u8In = NULL;
            int32_t u8InLenBefore = 0;
            int32_t u8InLen = testCasesUTextCopy[i].u8InLen;
            int32_t u8InCap = testCasesUTextCopy[i].u8InCap;
            uint8_t u8BufIn[BUFFER_SIZE + sizeof(void *)];

            memset(u8BufIn, 0, sizeof(u8BufIn));

            if (u8InCap == -1)
                u8InCap = BUFFER_SIZE;

            if (testCasesUTextCopy[i].u8In)
            {
                u8InLenBefore = (int32_t)strlen(strcpy((char*)u8BufIn, testCasesUTextCopy[i].u8In));

                u8In = u8BufIn;
                if (testCasesUTextCopy[i].u8InLen == -1)
                    u8InLen = -1;
                else
                    u8InLen = u8InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText* ut = utext_openU8(NULL, u8In, u8InLen, u8InCap, &status);
            TEST_SUCCESS(status);
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestCopyAPI(utdc, i, u8InLenBefore);
                    TestFreeze(utdc);
                    utext_close(utdc);
                }

                TestCopyAPI(ut, i, u8InLenBefore);
                TestFreeze(ut);
                utext_close(ut);
            }
        }

        // Simple Test String Test Cases
        for (int32_t i = 0; (testCasesTestString[i].u8In != NULL); i++)
        {
            uint8_t u8Buf[BUFFER_SIZE + sizeof(void *)];
            memset(u8Buf, 0, sizeof(u8Buf));
            strcpy((char*)u8Buf, testCasesTestString[i].u8In);
            int32_t u8Len = (int32_t)strlen(testCasesTestString[i].u8In);

            UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
            int32_t u16Len = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), (const char*)u8Buf, -1, &status);

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            int32_t u8NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u8Map[BUFFER_SIZE + sizeof(void *)];
            BuildU8Map(u8Buf, u8Len, cpCount, u8NativeIdx, u8Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU8(NULL, u8Buf, -1, sizeof(u8Buf), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u8NativeIdx, u8Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u8NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u8NativeIdx, u16NativeIdx, fullRepString, UEncoding_U8);
                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // Simple test of strings of lengths 1 to 60, looking for
        // glitches at buffer boundaries.
        for (int32_t i = 1; i < 60; i++)
        {
            uint8_t u8Buf[BUFFER_SIZE + sizeof(void *)];
            memset(u8Buf, 0, sizeof(u8Buf));

            int32_t u8Len = 0;
            UBool isError = FALSE;
            for (int32_t j = 0; j < i; j++) {
                if (j + 0x30 == 0x5c) {
                    // Backslash. Needs to be escaped.
                    U8_APPEND(u8Buf, u8Len, BUFFER_SIZE, (UChar32)0x5c, isError);
                }
                U8_APPEND(u8Buf, u8Len, BUFFER_SIZE, (UChar32)(j + 0x30), isError);
            }
            u8Buf[u8Len] = 0;

            UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
            int32_t u16Len = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), (const char*)u8Buf, u8Len, &status);

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            int32_t u8NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u8Map[BUFFER_SIZE + sizeof(void *)];
            BuildU8Map(u8Buf, u8Len, cpCount, u8NativeIdx, u8Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU8(NULL, u8Buf, -1, sizeof(u8Buf), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u8NativeIdx, u8Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u8NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u8NativeIdx, u16NativeIdx, fullRepString, UEncoding_U8);
                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // Test strings with odd-aligned supplementary chars,
        // looking for glitches at buffer boundaries.
        for (int32_t i = 1; i < 60; i++)
        {
            uint8_t u8Buf[BUFFER_SIZE + sizeof(void *)];
            memset(u8Buf, 0, sizeof(u8Buf));

            int32_t u8Len = 0;
            UBool isError = FALSE;
            U8_APPEND(u8Buf, u8Len, BUFFER_SIZE, (UChar32)0x41, isError);
            for (int32_t j = 0; j < i; j++) {
                U8_APPEND(u8Buf, u8Len, BUFFER_SIZE, (UChar32)(j + 0x11000), isError);
            }
            u8Buf[u8Len] = 0;

            UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
            int32_t u16Len = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), (const char*)u8Buf, u8Len, &status);

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            int32_t u8NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u8Map[BUFFER_SIZE + sizeof(void *)];
            BuildU8Map(u8Buf, u8Len, cpCount, u8NativeIdx, u8Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU8(NULL, u8Buf, -1, sizeof(u8Buf), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u8NativeIdx, u8Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u8NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u8NativeIdx, u16NativeIdx, fullRepString, UEncoding_U8);
                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // String of chars of randomly varying size in utf-8 representation.
        // Exercise the mapping, and the varying sized buffer.
        {
            uint8_t u8Buf[5000 + sizeof(void *) / sizeof(uint8_t)];
            memset(u8Buf, 0, sizeof(u8Buf));

            int32_t u8Len = 0;
            UBool isError = FALSE;
            UChar32  c1 = 0;
            UChar32  c2 = 0x100;
            UChar32  c3 = 0xa000;
            UChar32  c4 = 0x11000;
            for (int32_t i = 0; (i < 1000) && (u8Len < sizeof(u8Buf) - 10); i++) {
                int len8 = m_rand() % 4 + 1;
                switch (len8) {
                case 1:
                    c1 = (c1 + 1) % 0x80;
                    // don't put 0 into string (0 terminated strings for some tests)
                    // don't put '\', will cause unescape() to fail.
                    if (c1 == 0x5c || c1 == 0) {
                        c1++;
                    }
                    U8_APPEND(u8Buf, u8Len, sizeof(u8Buf), c1, isError);
                    break;
                case 2:
                    U8_APPEND(u8Buf, u8Len, sizeof(u8Buf), c2++, isError);
                    break;
                case 3:
                    U8_APPEND(u8Buf, u8Len, sizeof(u8Buf), c3++, isError);
                    break;
                case 4:
                    U8_APPEND(u8Buf, u8Len, sizeof(u8Buf), c4++, isError);
                    break;
                }
            }
            u8Buf[u8Len] = 0;

            UChar u16Buf[5000 + sizeof(void *)];
            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
            int32_t u16Len = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), (const char*)u8Buf, u8Len, &status);

            int32_t u16NativeIdx[5000 + sizeof(void *) / sizeof(int)];
            UChar32 u16Map[5000 + sizeof(void *) / sizeof(UChar32)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            int32_t u8NativeIdx[5000 + sizeof(void *) / sizeof(int)];
            UChar32 u8Map[5000 + sizeof(void *) / sizeof(UChar32)];
            BuildU8Map(u8Buf, u8Len, cpCount, u8NativeIdx, u8Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU8(NULL, u8Buf, -1, sizeof(u8Buf), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u8NativeIdx, u8Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u8NativeIdx);

                // TestReplace/TestCopyMove use a stack buffer of only BUFFER_SIZE
                if (u8Len < BUFFER_SIZE)
                    TestCMR(ut, u16Buf, u16Len, cpCount, u8NativeIdx, u16NativeIdx, fullRepString, UEncoding_U8);

                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // Index to positions not on code point boundaries.
        {
            const char *u8str = "\xc8\x81\xe1\x82\x83\xf1\x84\x85\x86";
            int32_t startMap[] = { 0,  0,  2,  2,  2,  5,  5,  5,  5,  9,  9 };
            int32_t nextMap[] = { 2,  2,  5,  5,  5,  9,  9,  9,  9,  9,  9 };
            int32_t prevMap[] = { 0,  0,  0,  0,  0,  2,  2,  2,  2,  5,  5 };
            UChar32  c32Map[] = { 0x201, 0x201, 0x1083, 0x1083, 0x1083, 0x044146, 0x044146, 0x044146, 0x044146, -1, -1 };
            UChar32  pr32Map[] = { -1,   -1,  0x201,  0x201,  0x201,   0x1083,   0x1083,   0x1083,   0x1083, 0x044146, 0x044146 };

            // extractLen is the size, in UChars, of what will be extracted between index and index+1.
            //  is zero when both index positions lie within the same code point.
            int32_t  exLen[] = { 0,  1,   0,  0,  1,  0,  0,  0,  2,  0,  0 };

            UText *ut = utext_openConstU8(NULL, (const uint8_t*)u8str, -1, -1, &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestNotOnCodePoints(
                    ut,
                    startMap,
                    nextMap,
                    prevMap,
                    c32Map,
                    pr32Map,
                    exLen
                );

                utext_close(ut);
            }
        }

        // UTF-8 with malformed sequences.
        // These should come through as the Unicode replacement char, \ufffd
        {
            const char *badUTF8 = "\x41\x81\x42\xf0\x81\x81\x43";
            UChar32  c;

            UText *ut = utext_openConstU8(NULL, (const uint8_t*)badUTF8, -1, -1, &status);
            TEST_SUCCESS(status);
            c = utext_char32At(ut, 1);
            TEST_ASSERT(c == 0xfffd);
            c = utext_char32At(ut, 3);
            TEST_ASSERT(c == 0xfffd);
            c = utext_char32At(ut, 5);
            TEST_ASSERT(c == 0xfffd);
            c = utext_char32At(ut, 6);
            TEST_ASSERT(c == 0x43);

            UChar buf[10];
            int n = utext_extract(ut, 0, 9, buf, 10, &status);
            TEST_SUCCESS(status);
            TEST_ASSERT(n == 7);
            TEST_ASSERT(buf[0] == 0x41);
            TEST_ASSERT(buf[1] == 0xfffd);
            TEST_ASSERT(buf[2] == 0x42);
            TEST_ASSERT(buf[3] == 0xfffd);
            TEST_ASSERT(buf[4] == 0xfffd);
            TEST_ASSERT(buf[5] == 0xfffd);
            TEST_ASSERT(buf[6] == 0x43);
            utext_close(ut);
        }
    }

    ucnv_close(u16Convertor);
}

static void TestU16(void)
{
    UErrorCode status = U_ZERO_ERROR;

    const char* fullRepString = "This is an arbitrary string that will be used as replacement text";

    UConverter *u16Convertor = ucnv_open("UTF8", &status);
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u16Convertor != NULL);

    if ((U_FAILURE(status)) || (u16Convertor == NULL)) {
        return;
    }

    // utext_openU16
    {
        // Simple Access Test Cases
        for (int32_t i = 0; (testCasesUTextAccess[i].u8In != NULL); i++)
        {
            UChar *u16In = NULL;
            int32_t u16InLenBefore = 0;
            int32_t u16InLen = testCasesUTextAccess[i].u8InLen;
            int32_t u16InCap = testCasesUTextAccess[i].u8InCap;
            UChar u16BufIn[BUFFER_SIZE + sizeof(void *)];

            u_memset(u16BufIn, 0, sizeof(u16BufIn) / sizeof(UChar));

            if (u16InCap == -1)
                u16InCap = BUFFER_SIZE;

            if (testCasesUTextAccess[i].u8In)
            {
                u16InLenBefore = ucnv_toUChars(u16Convertor, u16BufIn, u16InCap, testCasesUTextAccess[i].u8In, testCasesUTextAccess[i].u8InLen, &status);
                TEST_ASSERT(!U_FAILURE(status));

                u16In = u16BufIn;
                if (testCasesUTextOpen[i].u8InLen == -1)
                    u16InLen = -1;
                else
                    u16InLen = u16InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText *ut = utext_openConstU16(NULL, u16In, u16InLen, u16InCap, &status);
            TEST_SUCCESS(status);
            UText* utsc;
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccessAPI(ut, i, FALSE);

                utsc = utext_clone(NULL, ut, FALSE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utsc, i, TRUE);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utdc, i, TRUE);
                    utext_close(utdc);
                }

                utext_close(ut);
            }

            // Reuse ut, utsc, utc
            ut = utext_openU16(NULL, u16In, u16InLen, u16InCap, &status);
            TEST_SUCCESS(status);
            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccessAPI(ut, i, FALSE);

                utsc = utext_clone(NULL, ut, FALSE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utsc, i, TRUE);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utdc, i, TRUE);
                    utext_close(utdc);
                }

                utext_close(ut);
            }
        }

        // Simple Extract Test Cases
        for (int32_t i = 0; (testCasesUTextExtract[i].u8In != NULL); i++)
        {
            UChar *u16In = NULL;
            int32_t u16InLenBefore = 0;
            int32_t u16InLen = testCasesUTextExtract[i].u8InLen;
            int32_t u16InCap = testCasesUTextExtract[i].u8InCap;
            UChar u16BufIn[BUFFER_SIZE + sizeof(void *)];

            u_memset(u16BufIn, 0, sizeof(u16BufIn) / sizeof(UChar));

            if (u16InCap == -1)
                u16InCap = BUFFER_SIZE;

            if (testCasesUTextExtract[i].u8In)
            {
                //u16InLenBefore = ucnv_toUChars(u16Convertor, u16BufIn, u16InCap, testCasesUTextExtract[i].u8In, testCasesUTextExtract[i].u8InLen, &status);
                u16InLenBefore = u_unescape(testCasesUTextExtract[i].u8In, u16BufIn, u16InCap);
                TEST_ASSERT(!U_FAILURE(status));

                u16In = u16BufIn;
                if (testCasesUTextExtract[i].u8InLen == -1)
                    u16InLen = -1;
                else
                    u16InLen = u16InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText *ut = utext_openConstU16(NULL, u16In, u16InLen, u16InCap, &status);
            TEST_SUCCESS(status);
            UText* utsc;
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                TestExtractAPI(ut, i);

                utsc = utext_clone(NULL, ut, FALSE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utsc, i);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utdc, i);
                    utext_close(utdc);
                }

                utext_close(ut);
            }

            // Reuse ut, utsc, utc
            ut = utext_openU16(NULL, u16In, u16InLen, u16InCap, &status);
            TEST_SUCCESS(status);
            if ((ut) && (!U_FAILURE(status)))
            {
                TestExtractAPI(ut, i);

                utsc = utext_clone(NULL, ut, FALSE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utsc, i);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utdc, i);
                    utext_close(utdc);
                }

                utext_close(ut);
            }
        }

        // Simple Replace Test Cases
        for (int32_t i = 0; (testCasesUTextReplace[i].u8In != NULL); i++)
        {
            UChar *u16In = NULL;
            int32_t u16InLenBefore = 0;
            int32_t u16InLen = testCasesUTextReplace[i].u8InLen;
            int32_t u16InCap = testCasesUTextReplace[i].u8InCap;
            UChar u16BufIn[BUFFER_SIZE + sizeof(void *)];

            u_memset(u16BufIn, 0, sizeof(u16BufIn) / sizeof(UChar));

            if (u16InCap == -1)
                u16InCap = BUFFER_SIZE;

            if (testCasesUTextReplace[i].u8In)
            {
                u16InLenBefore = ucnv_toUChars(u16Convertor, u16BufIn, u16InCap, testCasesUTextReplace[i].u8In, testCasesUTextReplace[i].u8InLen, &status);
                TEST_ASSERT(!U_FAILURE(status));

                u16In = u16BufIn;
                if (testCasesUTextReplace[i].u8InLen == -1)
                    u16InLen = -1;
                else
                    u16InLen = u16InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText* ut = utext_openU16(NULL, u16In, u16InLen, u16InCap, &status);
            TEST_SUCCESS(status);
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestReplaceAPI(utdc, i, u16InLenBefore);
                    TestFreeze(utdc);
                    utext_close(utdc);
                }

                TestReplaceAPI(ut, i, u16InLenBefore);
                TestFreeze(ut);
                utext_close(ut);
            }
        }

        // Simple Copy Test Cases
        for (int32_t i = 0; (testCasesUTextCopy[i].u8In != NULL); i++)
        {
            UChar *u16In = NULL;
            int32_t u16InLenBefore = 0;
            int32_t u16InLen = testCasesUTextCopy[i].u8InLen;
            int32_t u16InCap = testCasesUTextCopy[i].u8InCap;
            UChar u16BufIn[BUFFER_SIZE + sizeof(void *)];

            u_memset(u16BufIn, 0, sizeof(u16BufIn) / sizeof(UChar));

            if (u16InCap == -1)
                u16InCap = BUFFER_SIZE;

            if (testCasesUTextCopy[i].u8In)
            {
                u16InLenBefore = ucnv_toUChars(u16Convertor, u16BufIn, u16InCap, testCasesUTextCopy[i].u8In, testCasesUTextCopy[i].u8InLen, &status);
                TEST_ASSERT(!U_FAILURE(status));

                u16In = u16BufIn;
                if (testCasesUTextCopy[i].u8InLen == -1)
                    u16InLen = -1;
                else
                    u16InLen = u16InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText* ut = utext_openU16(NULL, u16In, u16InLen, u16InCap, &status);
            TEST_SUCCESS(status);
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestCopyAPI(utdc, i, u16InLenBefore);
                    TestFreeze(utdc);
                    utext_close(utdc);
                }

                TestCopyAPI(ut, i, u16InLenBefore);
                TestFreeze(ut);
                utext_close(ut);
            }
        }

        // Simple Test String Test Cases
        for (int32_t i = 0; (testCasesTestString[i].u8In != NULL); i++)
        {
            uint8_t u8Buf[BUFFER_SIZE + sizeof(void *)];
            memset(u8Buf, 0, sizeof(u8Buf));
            strcpy((char*)u8Buf, testCasesTestString[i].u8In);

            UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
            int32_t u16Len = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), (const char*)u8Buf, -1, &status);

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU16(NULL, u16Buf, -1, sizeof(u16Buf), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u16NativeIdx, u16Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u16NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u16NativeIdx, u16NativeIdx, fullRepString, UEncoding_U16);
                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // Simple test of strings of lengths 1 to 60, looking for
        // glitches at buffer boundaries.
        for (int32_t i = 1; i < 60; i++)
        {
            UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(u16Buf));

            int32_t u16Len = 0;
            UBool isError = FALSE;
            int32_t k;
            for (int32_t j = 0; j < i; j++) {
                if (j + 0x30 == 0x5c) {
                    // Backslash. Needs to be escaped.
                    k = (UChar32)0x5c;
                    U16_APPEND(u16Buf, u16Len, BUFFER_SIZE, k, isError);
                }
                k = (UChar32)(j + 0x30);
                U16_APPEND(u16Buf, u16Len, BUFFER_SIZE, k, isError);
            }
            u16Buf[u16Len] = 0;

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU16(NULL, u16Buf, -1, sizeof(u16Buf), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u16NativeIdx, u16Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u16NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u16NativeIdx, u16NativeIdx, fullRepString, UEncoding_U16);
                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // Test strings with odd-aligned supplementary chars,
        // looking for glitches at buffer boundaries.
        for (int32_t i = 1; i < 60; i++)
        {
            UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));

            int32_t u16Len = 0;
            UBool isError = FALSE;
            int32_t k = (UChar32)0x41;
            U16_APPEND(u16Buf, u16Len, BUFFER_SIZE, k, isError);
            for (int32_t j = 0; j < i; j++) {
                k = (UChar32)(j + 0x11000);
                U16_APPEND(u16Buf, u16Len, BUFFER_SIZE, k, isError);
            }
            u16Buf[u16Len] = 0;

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU16(NULL, u16Buf, -1, sizeof(u16Buf), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u16NativeIdx, u16Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u16NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u16NativeIdx, u16NativeIdx, fullRepString, UEncoding_U16);
                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // Index to positions not on code point boundaries.
        {
            const char* u8str = "\\u1000\\U00011000\\u2000\\U00022000";
            int32_t startMap[] = { 0,     1,   1,    3,     4,  4,     6,  6 };
            int32_t nextMap[] = { 1,     3,   3,    4,     6,  6,     6,  6 };
            int32_t prevMap[] = { 0,     0,   0,    1,     3,  3,     4,  4 };
            UChar32  c32Map[] = { 0x1000, 0x11000, 0x11000, 0x2000,  0x22000, 0x22000, -1, -1 };
            UChar32  pr32Map[] = { -1, 0x1000,  0x1000,  0x11000, 0x2000,  0x2000,   0x22000,   0x22000 };
            int32_t  exLen[] = { 1,  0,   2,  1,  0,  2,  0,  0, };

            UChar u16Buf[BUFFER_SIZE + sizeof(void *)];
            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
            int32_t u16Len = u_unescape(u8str, u16Buf, sizeof(u16Buf));

            UText *ut = utext_openConstU16(NULL, u16Buf, u16Len, -1, &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestNotOnCodePoints(
                    ut,
                    startMap,
                    nextMap,
                    prevMap,
                    c32Map,
                    pr32Map,
                    exLen
                );

                utext_close(ut);
            }
        }
    }

    ucnv_close(u16Convertor);
}

static void TestU32(void)
{
    UErrorCode status = U_ZERO_ERROR;

    const char* fullRepString = "This is an arbitrary string that will be used as replacement text";

    UConverter *u16Convertor = ucnv_open("UTF8", &status);
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u16Convertor != NULL);

    if ((U_FAILURE(status)) || (u16Convertor == NULL)) {
        return;
    }

    UConverter *u32Convertor = ucnv_open("UTF32", &status);
    TEST_ASSERT(!U_FAILURE(status));
    TEST_ASSERT(u32Convertor != NULL);

    if ((U_FAILURE(status)) || (u32Convertor == NULL)) {
        return;
    }

    UChar u16Buf[2000 + sizeof(void *)];
    u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));

    // utext_openU32
    {
        // Simple Access Test Cases
        for (int32_t i = 0; (testCasesUTextAccess[i].u8In != NULL); i++)
        {
            UChar32 *u32In = NULL;
            int32_t u32InLenBefore = 0;
            int32_t u32InLen = testCasesUTextAccess[i].u8InLen;
            int32_t u32InCap = testCasesUTextAccess[i].u8InCap;
            UChar32 u32BufIn[BUFFER_SIZE + sizeof(void *)];

            memset(u32BufIn, 0, sizeof(u32BufIn));

            if (u32InCap == -1)
                u32InCap = BUFFER_SIZE;

            if (testCasesUTextAccess[i].u8In)
            {
                int32_t u16InLenBefore = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), testCasesUTextAccess[i].u8In, testCasesUTextAccess[i].u8InLen, &status);
                ucnv_resetFromUnicode(u32Convertor);
                u32InLenBefore = ucnv_fromUChars(u32Convertor, (char*)u32BufIn, u32InCap, u16Buf, u16InLenBefore, &status) / sizeof(UChar32);
                TEST_ASSERT(!U_FAILURE(status));

                u32In = u32BufIn;
                if (u32InLenBefore > 0) {
                    u32In = &u32BufIn[1];
                    u32InLenBefore--;
                }

                if (testCasesUTextOpen[i].u8InLen == -1)
                    u32InLen = -1;
                else
                    u32InLen = u32InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText *ut = utext_openConstU32(NULL, u32In, u32InLen, u32InCap, &status);
            TEST_SUCCESS(status);
            UText* utsc;
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccessAPI(ut, i, FALSE);

                utsc = utext_clone(NULL, ut, FALSE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utsc, i, TRUE);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utdc, i, TRUE);
                    utext_close(utdc);
                }

                utext_close(ut);
            }

            // Reuse ut, utsc, utc
            ut = utext_openU32(NULL, u32In, u32InLen, u32InCap, &status);
            TEST_SUCCESS(status);
            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccessAPI(ut, i, FALSE);

                utsc = utext_clone(NULL, ut, FALSE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utsc, i, TRUE);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestAccessAPI(utdc, i, TRUE);
                    utext_close(utdc);
                }

                utext_close(ut);
            }
        }

        // Simple Extract Test Cases
        for (int32_t i = 0; (testCasesUTextExtract[i].u8In != NULL); i++)
        {
            UChar32 *u32In = NULL;
            int32_t u32InLenBefore = 0;
            int32_t u32InLen = testCasesUTextExtract[i].u8InLen;
            int32_t u32InCap = testCasesUTextExtract[i].u8InCap;
            UChar32 u32BufIn[BUFFER_SIZE + sizeof(void *)];

            memset(u32BufIn, 0, sizeof(u32BufIn));

            if (u32InCap == -1)
                u32InCap = BUFFER_SIZE;

            if (testCasesUTextExtract[i].u8In)
            {
                int32_t u16InLenBefore = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), testCasesUTextExtract[i].u8In, testCasesUTextExtract[i].u8InLen, &status);
                u32InLenBefore = ucnv_fromUChars(u32Convertor, (char*)u32BufIn, u32InCap, u16Buf, u16InLenBefore, &status) / sizeof(UChar32);
                TEST_ASSERT(!U_FAILURE(status));

                u32In = u32BufIn;
                if (u32InLenBefore > 0) {
                    u32In = &u32BufIn[1];
                    u32InLenBefore--;
                }

                if (testCasesUTextExtract[i].u8InLen == -1)
                    u32InLen = -1;
                else
                    u32InLen = u32InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText *ut = utext_openConstU32(NULL, u32In, u32InLen, u32InCap, &status);
            TEST_SUCCESS(status);
            UText* utsc;
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                TestExtractAPI(ut, i);

                utsc = utext_clone(NULL, ut, FALSE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utsc, i);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, TRUE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utdc, i);
                    utext_close(utdc);
                }

                utext_close(ut);
            }

            // Reuse ut, utsc, utc
            ut = utext_openU32(NULL, u32In, u32InLen, u32InCap, &status);
            TEST_SUCCESS(status);
            if ((ut) && (!U_FAILURE(status)))
            {
                TestExtractAPI(ut, i);

                utsc = utext_clone(NULL, ut, FALSE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utsc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utsc, i);
                    utext_close(utsc);
                }

                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestExtractAPI(utdc, i);
                    utext_close(utdc);
                }

                utext_close(ut);
            }
        }

        // Simple Replace Test Cases
        for (int32_t i = 0; (testCasesUTextReplace[i].u8In != NULL); i++)
        {
            UChar32 *u32In = NULL;
            int32_t u32InLenBefore = 0;
            int32_t u32InLen = testCasesUTextReplace[i].u8InLen;
            int32_t u32InCap = testCasesUTextReplace[i].u8InCap;
            UChar32 u32BufIn[BUFFER_SIZE + sizeof(void *)];

            memset(u32BufIn, 0, sizeof(u32BufIn));

            if (u32InCap == -1)
                u32InCap = BUFFER_SIZE;

            if (testCasesUTextReplace[i].u8In)
            {
                int32_t u16InLenBefore = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), testCasesUTextReplace[i].u8In, testCasesUTextReplace[i].u8InLen, &status);
                u32InLenBefore = ucnv_fromUChars(u32Convertor, (char*)u32BufIn, u32InCap, u16Buf, u16InLenBefore, &status) / sizeof(UChar32);
                TEST_ASSERT(!U_FAILURE(status));

                u32In = u32BufIn;
                if (u32InLenBefore > 0) {
                    u32In = &u32BufIn[1];
                    u32InLenBefore--;
                }

                if (testCasesUTextReplace[i].u8InLen == -1)
                    u32InLen = -1;
                else
                    u32InLen = u32InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText* ut = utext_openU32(NULL, u32In, u32InLen, u32InCap, &status);
            TEST_SUCCESS(status);
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestReplaceAPI(utdc, i, u32InLenBefore);
                    TestFreeze(utdc);
                    utext_close(utdc);
                }

                TestReplaceAPI(ut, i, u32InLenBefore);
                TestFreeze(ut);
                utext_close(ut);
            }
        }

        // Simple Copy Test Cases
        for (int32_t i = 0; (testCasesUTextCopy[i].u8In != NULL); i++)
        {
            UChar32 *u32In = NULL;
            int32_t u32InLenBefore = 0;
            int32_t u32InLen = testCasesUTextCopy[i].u8InLen;
            int32_t u32InCap = testCasesUTextCopy[i].u8InCap;
            UChar32 u32BufIn[BUFFER_SIZE + sizeof(void *)];

            memset(u32BufIn, 0, sizeof(u32BufIn));

            if (u32InCap == -1)
                u32InCap = BUFFER_SIZE;

            if (testCasesUTextCopy[i].u8In)
            {
                int32_t u16InLenBefore = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), testCasesUTextCopy[i].u8In, testCasesUTextCopy[i].u8InLen, &status);
                u32InLenBefore = ucnv_fromUChars(u32Convertor, (char*)u32BufIn, u32InCap, u16Buf, u16InLenBefore, &status) / sizeof(UChar32);
                TEST_ASSERT(!U_FAILURE(status));

                u32In = u32BufIn;
                if (u32InLenBefore > 0) {
                    u32In = &u32BufIn[1];
                    u32InLenBefore--;
                }

                if (testCasesUTextCopy[i].u8InLen == -1)
                    u32InLen = -1;
                else
                    u32InLen = u32InLenBefore;
            }

            status = U_ZERO_ERROR;

            UText* ut = utext_openU32(NULL, u32In, u32InLen, u32InCap, &status);
            TEST_SUCCESS(status);
            UText* utdc;

            if ((ut) && (!U_FAILURE(status)))
            {
                utdc = utext_clone(NULL, ut, TRUE, FALSE, &status);
                TEST_SUCCESS(status);
                if ((utdc) && (!U_FAILURE(status)))
                {
                    TestCopyAPI(utdc, i, u32InLenBefore);
                    TestFreeze(utdc);
                    utext_close(utdc);
                }

                TestCopyAPI(ut, i, u32InLenBefore);
                TestFreeze(ut);
                utext_close(ut);
            }
        }

        // Simple Test String Test Cases
        for (int32_t i = 0; (testCasesTestString[i].u8In != NULL); i++)
        {
            uint8_t u8Buf[BUFFER_SIZE + sizeof(void *)];
            memset(u8Buf, 0, sizeof(u8Buf));
            strcpy((char*)u8Buf, testCasesTestString[i].u8In);

            UChar32 u32BufIn[BUFFER_SIZE + sizeof(void *)];
            memset(u32BufIn, 0, sizeof(u32BufIn));
            int32_t u16Len = ucnv_toUChars(u16Convertor, u16Buf, sizeof(u16Buf), testCasesTestString[i].u8In, -1, &status);
            int32_t u32Len = ucnv_fromUChars(u32Convertor, (char*)u32BufIn, sizeof(u32BufIn), u16Buf, u16Len, &status) / sizeof(UChar32);
            TEST_ASSERT(!U_FAILURE(status));

            UChar32* u32Buf = u32BufIn;
            if (u32Len > 0) {
                u32Buf = &u32BufIn[1];
                u32Len--;
            }

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            int32_t u32NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u32Map[BUFFER_SIZE + sizeof(void *)];
            BuildU32Map(u32Buf, cpCount, u32NativeIdx, u32Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU32(NULL, u32Buf, -1, sizeof(u32BufIn), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u32NativeIdx, u32Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u32NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u32NativeIdx, u16NativeIdx, fullRepString, UEncoding_U32);
                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // Simple test of strings of lengths 1 to 60, looking for
        // glitches at buffer boundaries.
        for (int32_t i = 1; i < 60; i++)
        {
            UChar32 u32BufIn[BUFFER_SIZE + sizeof(void *)];
            memset(u32BufIn, 0, sizeof(u32BufIn));
            UChar32* u32Buf = &u32BufIn[1];

            int32_t u32Len = 0;
            for (int32_t j = 0; j < i; j++) {
                if (j + 0x30 == 0x5c) {
                    // Backslash. Needs to be escaped.
                    u32Buf[u32Len++] = (UChar32)0x5c;
                }
                u32Buf[u32Len++] = (UChar32)(j + 0x30);
            }
            u32Buf[u32Len] = 0;
            u32BufIn[0] = 0x0000FEFF;

            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
            int32_t u16Len = ucnv_toUChars(u32Convertor, u16Buf, sizeof(u16Buf), (const char *)u32BufIn, (u32Len + 1) * sizeof(UChar32), &status);

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            int32_t u32NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u32Map[BUFFER_SIZE + sizeof(void *)];
            BuildU32Map(u32Buf, cpCount, u32NativeIdx, u32Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU32(NULL, u32Buf, -1, sizeof(u32BufIn), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u32NativeIdx, u32Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u32NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u32NativeIdx, u16NativeIdx, fullRepString, UEncoding_U32);
                TestFreeze(ut);

                utext_close(ut);
            }
        }

        // Test strings with odd-aligned supplementary chars,
        // looking for glitches at buffer boundaries.
        for (int32_t i = 1; i < 60; i++)
        {
            UChar32 u32BufIn[BUFFER_SIZE + sizeof(void *)];
            memset(u32BufIn, 0, sizeof(u32BufIn));
            UChar32* u32Buf = &u32BufIn[1];

            int32_t u32Len = 0;
            u32Buf[u32Len++] = (UChar32)0x41;
            for (int32_t j = 0; j < i; j++) {
                u32Buf[u32Len++] = (UChar32)(j + 0x11000);
            }
            u32Buf[u32Len] = 0;
            u32BufIn[0] = 0x0000FEFF;

            u_memset(u16Buf, 0, sizeof(u16Buf) / sizeof(UChar));
            int32_t u16Len = ucnv_toUChars(u32Convertor, u16Buf, sizeof(u16Buf), (const char *)u32BufIn, (u32Len + 1) * sizeof(UChar32), &status);

            int32_t u16NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u16Map[BUFFER_SIZE + sizeof(void *)];
            int32_t cpCount = BuildU16Map(u16Buf, u16Len, u16NativeIdx, u16Map);

            int32_t u32NativeIdx[BUFFER_SIZE + sizeof(void *)];
            UChar32 u32Map[BUFFER_SIZE + sizeof(void *)];
            BuildU32Map(u32Buf, cpCount, u32NativeIdx, u32Map);

            status = U_ZERO_ERROR;

            UText* ut = utext_openU32(NULL, u32Buf, -1, sizeof(u32BufIn), &status);
            TEST_SUCCESS(status);

            if ((ut) && (!U_FAILURE(status)))
            {
                TestAccess(ut, cpCount, u32NativeIdx, u32Map);
                TestExtract(ut, u16Buf, u16Len, cpCount, u32NativeIdx);
                TestCMR(ut, u16Buf, u16Len, cpCount, u32NativeIdx, u16NativeIdx, fullRepString, UEncoding_U32);
                TestFreeze(ut);

                utext_close(ut);
            }
        }
    }

    ucnv_close(u16Convertor);
    ucnv_close(u32Convertor);
}
