﻿// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2000-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ubidiwrt.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999aug06
*   created by: Markus W. Scherer
*
*   Contributions:
*   Updated by Matitiahu Allouche
*   UText enhancements by Paul Werbicki
*/

/*
 * This file contains implementations for BiDi functions that use
 * the core algorithm and core API to write reordered text.
 */

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/uchar.h"
#include "unicode/ubidi.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "ustr_imp.h"
#include "ubidiimp.h"

 /*
  * The function implementations in this file are designed for
  * all encodings using UText.
  * - Code points can use any number of code units
  * - BiDi control characters can use any number of code units
  * - u_charMirror(c) can use a different number of code units as c
  */

// ubidi_writeReordered() assumes UTF16 due to the use of UChar
#if UTF_SIZE == 8
# error Reimplement ubidi_writeReordered() for UTF-8
#endif

#define IS_COMBINING(type) ((1UL<<(type))&(1UL<<U_NON_SPACING_MARK|1UL<<U_COMBINING_SPACING_MARK|1UL<<U_ENCLOSING_MARK))

static int32_t
utext_append32(UText *ut, int64_t start, UChar32 uchar, UErrorCode *pErrorCode)
{
    UChar uchars[2] = { (UChar)uchar, 0 };
    int32_t length = 1;
    int32_t nativeLength = 0;

    if (pErrorCode == NULL) {
        return 0;
    }

    if ((!U16_IS_SINGLE(uchar)) || (U_IS_SUPPLEMENTARY(uchar)))
    {
        uchars[0] = U16_LEAD(uchar);
        uchars[1] = U16_TRAIL(uchar);
        length = 2;
    };

    if (!U_FAILURE(*pErrorCode)) {
        nativeLength = utext_replace(ut, start, start, uchars, length, pErrorCode);
    }

    if (*pErrorCode == U_BUFFER_OVERFLOW_ERROR) {
        nativeLength = length;
    }

    return nativeLength;
}

/*
 * When we have UBIDI_OUTPUT_REVERSE set on ubidi_writeReordered(),
 * then we semantically write RTL runs in reverse and later reverse
 * them again.  Instead, we actually write them in forward order to
 * begin with. However, if the RTL run was to be mirrored, we need
 * to mirror here now since the implicit second reversal must not
 * do it. It looks strange to do mirroring in LTR output, but it is
 * only because we are writing RTL output in reverse.
 */
static int32_t
doWriteForward(UText *srcUt, 
    int32_t srcNativeStart,
    int32_t srcNativeLength,
    UText *dstUt,
    int32_t dstNativeStart,
    uint16_t options,
    UErrorCode *pErrorCode)
{
    //if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
    //	return 0;
    //}

    int32_t dstNativeLimit = dstNativeStart;
    int32_t nativeStart = 0;
    UChar32 uchar = U_SENTINEL;
    int32_t nativeLimit = 0;
    int32_t length = 0;

    UTEXT_SETNATIVEINDEX(srcUt, srcNativeStart);

    if (!U_FAILURE(*pErrorCode)) {
        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
        uchar = UTEXT_NEXT32(srcUt);
        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
    }

    // Optimize for several combinations of options
    switch (options & (UBIDI_REMOVE_BIDI_CONTROLS | UBIDI_DO_MIRRORING)) {
    case 0:
    {
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart < srcNativeStart + srcNativeLength);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(srcUt), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            length = utext_append32(dstUt, dstNativeLimit, uchar, pErrorCode);
            if (!U_FAILURE(*pErrorCode)) {
                dstNativeLimit += length;
            }
            else {
                UTEXT_PREVIOUS32(srcUt);
                break;
            }
        }
        break;
    }
    case UBIDI_DO_MIRRORING:
    {
        // Do mirroring
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart < srcNativeStart + srcNativeLength);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(srcUt), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            uchar = u_charMirror(uchar);

            length = utext_append32(dstUt, dstNativeLimit, uchar, pErrorCode);
            if (!U_FAILURE(*pErrorCode)) {
                dstNativeLimit += length;
            }
            else {
                UTEXT_PREVIOUS32(srcUt);
                break;
            }
        }
        break;
    }
    case UBIDI_REMOVE_BIDI_CONTROLS:
    {
        // Copy the LTR run and remove any BiDi control characters
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart < srcNativeStart + srcNativeLength);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(srcUt), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            if (!IS_BIDI_CONTROL_CHAR(uchar)) {
                length = utext_append32(dstUt, dstNativeLimit, uchar, pErrorCode);
                if (!U_FAILURE(*pErrorCode)) {
                    dstNativeLimit += length;
                }
                else {
                    UTEXT_PREVIOUS32(srcUt);
                    break;
                }
            }
        }
        break;
    }
    default:
    {
        // Remove BiDi control characters and do mirroring
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart < srcNativeStart + srcNativeLength);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(srcUt), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            if (!IS_BIDI_CONTROL_CHAR(uchar)) {
                uchar = u_charMirror(uchar);

                length = utext_append32(dstUt, dstNativeLimit, uchar, pErrorCode);
                if (!U_FAILURE(*pErrorCode)) {
                    dstNativeLimit += length;
                }
                else {
                    UTEXT_PREVIOUS32(srcUt);
                    break;
                }
            }
        }
        break;
    }
    } // End of switch

    if (U_FAILURE(*pErrorCode)) {
        // Preflight the length
        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
        uchar = UTEXT_NEXT32(srcUt);
        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
        for (; (uchar != U_SENTINEL) && (nativeStart < srcNativeStart + srcNativeLength);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(srcUt), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            if (!((options & UBIDI_REMOVE_BIDI_CONTROLS) && (IS_BIDI_CONTROL_CHAR(uchar)))) {
                dstNativeLimit += nativeLimit - nativeStart;
            }
        }
    }

    return dstNativeLimit - dstNativeStart;
}

/*
 * RTL run -
 *
 * RTL runs need to be copied to the destination in reverse order
 * of code points, not code units, to keep Unicode characters intact.
 *
 * The general strategy for this is to read the source text
 * in backward order, collect all code units for a code point
 * (and optionally following combining characters, see below),
 * and copy all these code units in ascending order
 * to the destination for this run.
 *
 * Several options request whether combining characters
 * should be kept after their base characters,
 * whether BiDi control characters should be removed, and
 * whether characters should be replaced by their mirror-image
 * equivalent Unicode characters.
 */
static int32_t
doWriteReverse(UText *srcUt,
    int32_t srcNativeStart,
    int32_t srcNativeLength,
    UText *dstUt,
    int32_t dstNativeStart,
    uint16_t options,
    UErrorCode *pErrorCode)
{
    //if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
    //	return 0;
    //}

    int32_t dstNativeLimit = dstNativeStart;
    int32_t nativeLimit = 0;
    UChar32 uchar = U_SENTINEL;
    int32_t nativeStart = 0;
    int32_t length = 0;

    UTEXT_SETNATIVEINDEX(srcUt, srcNativeStart + srcNativeLength);

    if (!U_FAILURE(*pErrorCode)) {
        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
        uchar = UTEXT_PREVIOUS32(srcUt);
        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
    }

    // Optimize for several combinations of options
    switch (options & (UBIDI_REMOVE_BIDI_CONTROLS | UBIDI_DO_MIRRORING | UBIDI_KEEP_BASE_COMBINING)) {
    case 0:
    {
        // With none of the "complicated" options set, the destination
        // run will have the same length as the source run,
        // and there is no mirroring and no keeping combining characters
        // with their base characters.
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart >= srcNativeStart);
            nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(srcUt), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            length = utext_append32(dstUt, dstNativeLimit, uchar, pErrorCode);
            if (!U_FAILURE(*pErrorCode)) {
                dstNativeLimit += length;
            }
            else {
                UTEXT_NEXT32(srcUt);
                break;
            }
        }
        break;
    }
    case UBIDI_KEEP_BASE_COMBINING:
    {
        // Here, too, the destination
        // run will have the same length as the source run,
        // and there is no mirroring.
        // We do need to keep combining characters with their base characters.
        //
        // Preserve character integrity
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart >= srcNativeStart);
            nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(srcUt), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            if (IS_COMBINING(u_charType(uchar))) {
                UTEXT_NEXT32(srcUt);
                int32_t j = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
                UTEXT_PREVIOUS32(srcUt);

                // Collect code units and modifier letters for one base character
                for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart >= srcNativeStart) && (IS_COMBINING(u_charType(uchar)));
                    uchar = UTEXT_PREVIOUS32(srcUt), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
                {
                    // Do nothing.
                }

                // Copy this "user character"
                int32_t srcLength = j - nativeStart;
                int32_t nativeStart2 = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
                uchar = UTEXT_NEXT32(srcUt);
                int32_t nativeLimit2 = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (srcLength > 0);
                    nativeStart2 = nativeLimit2, uchar = UTEXT_NEXT32(srcUt), nativeLimit2 = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
                {
                    length = utext_append32(dstUt, dstNativeLimit, uchar, pErrorCode);
                    if (!U_FAILURE(*pErrorCode)) {
                        dstNativeLimit += length;
                        j -= nativeLimit2 - nativeStart2;
                    }
                    else {
                        UTEXT_NEXT32(srcUt);
                        break;
                    }
                    srcLength -= nativeLimit2 - nativeStart2;
                }

                UTEXT_SETNATIVEINDEX(srcUt, j);
            }
            else {
                length = utext_append32(dstUt, dstNativeLimit, uchar, pErrorCode);
                if (!U_FAILURE(*pErrorCode)) {
                    dstNativeLimit += length;
                }
                else {
                    UTEXT_NEXT32(srcUt);
                    break;
                }
            }
        }
        break;
    }
    default:
    {
        // With several "complicated" options set, this is the most
        // general and the slowest copying of an RTL run.
        // We will do mirroring, remove BiDi controls, and
        // keep combining characters with their base characters
        // as requested.
        //
        // Preserve character integrity
        for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart >= srcNativeStart);
            nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(srcUt), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            UTEXT_NEXT32(srcUt);
            int32_t j = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
            UTEXT_PREVIOUS32(srcUt);

            if (options & UBIDI_KEEP_BASE_COMBINING) {
                // Collect code units and modifier letters for one base character
                for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (nativeStart >= srcNativeStart) && (IS_COMBINING(u_charType(uchar)));
                    uchar = UTEXT_PREVIOUS32(srcUt), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt)) {
                }
            }

            if ((options & UBIDI_REMOVE_BIDI_CONTROLS) && (IS_BIDI_CONTROL_CHAR(uchar))) {
                // Do not copy this BiDi control character
                continue;
            }

            // Copy this "user character"
            int32_t srcLength = j - nativeStart;
            int32_t nativeStart2 = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
            uchar = UTEXT_NEXT32(srcUt);
            int32_t nativeLimit2 = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

            if (options & UBIDI_DO_MIRRORING) // Mirror only the base character
                uchar = u_charMirror(uchar);

            for (; (!U_FAILURE(*pErrorCode)) && (uchar != U_SENTINEL) && (srcLength > 0);
                nativeStart2 = nativeLimit2, uchar = UTEXT_NEXT32(srcUt), nativeLimit2 = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
            {
                length = utext_append32(dstUt, dstNativeLimit, uchar, pErrorCode);
                if (!U_FAILURE(*pErrorCode)) {
                    dstNativeLimit += length;
                    j -= nativeLimit2 - nativeStart2;
                }
                else {
                    UTEXT_NEXT32(srcUt);
                    break;
                }

                srcLength -= nativeLimit2 - nativeStart2;
            }

            UTEXT_SETNATIVEINDEX(srcUt, j);
        }
        break;
    }
    } // End of switch

    if (U_FAILURE(*pErrorCode)) {
        // Preflight the length
        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
        uchar = UTEXT_PREVIOUS32(srcUt);
        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);
        for (; (uchar != U_SENTINEL) && (nativeStart >= srcNativeStart);
            nativeLimit = nativeStart, uchar = UTEXT_PREVIOUS32(srcUt), nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(srcUt))
        {
            if (!((options & UBIDI_REMOVE_BIDI_CONTROLS) && (IS_BIDI_CONTROL_CHAR(uchar)))) {
                dstNativeLimit += nativeLimit - nativeStart;
            }
        }
    }

    return dstNativeLimit - dstNativeStart;
}

U_CAPI int32_t U_EXPORT2
ubidi_writeUReverse(UText *srcUt,
    UText *dstUt,
    uint16_t options,
    UErrorCode *pErrorCode)
{
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

    int32_t srcNativeLength = (int32_t)utext_nativeLength(srcUt);
    int32_t dstNativeStart = 0;

    return doWriteReverse(srcUt, 0, srcNativeLength, dstUt, dstNativeStart, options, pErrorCode);
}

U_CAPI int32_t U_EXPORT2
ubidi_writeReverse(const UChar *src,
    int32_t srcLength,
    UChar *dest,
    int32_t destSize,
    uint16_t options,
    UErrorCode *pErrorCode)
{
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
    int32_t length = ubidi_writeUReverse(&srcUt, &dstUt, options, pErrorCode);

    utext_close(&srcUt);
    utext_close(&dstUt);

    return length;
}

U_CAPI int32_t U_EXPORT2
ubidi_getVisualText(UBiDi *pBiDi,
    UText *dstUt,
    int32_t start,
    int32_t limit,
    uint16_t options,
    UErrorCode *pErrorCode)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    if (pBiDi == NULL || dstUt == NULL) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if (!utext_isWritable(dstUt)) {
        *pErrorCode = U_NO_WRITE_PERMISSION;
        return 0;
    }

    UText *srcUt = &pBiDi->ut;
    int32_t srcLength = pBiDi->length;

    // Do input and output overlap?
    if (utext_equals(srcUt, dstUt)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    // Nothing to do?
    if (srcLength == 0) {
        return 0;
    }

    int32_t runCount = ubidi_countRuns(pBiDi, pErrorCode);
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }

    // Option "insert marks" implies UBIDI_INSERT_LRM_FOR_NUMERIC if the
    // reordering mode (checked below) is appropriate.
    if (pBiDi->reorderingOptions & UBIDI_OPTION_INSERT_MARKS) {
        options |= UBIDI_INSERT_LRM_FOR_NUMERIC;
        options &= ~UBIDI_REMOVE_BIDI_CONTROLS;
    }

    // Option "remove controls" implies UBIDI_REMOVE_BIDI_CONTROLS
    // and cancels UBIDI_INSERT_LRM_FOR_NUMERIC.
    if (pBiDi->reorderingOptions & UBIDI_OPTION_REMOVE_CONTROLS) {
        options |= UBIDI_REMOVE_BIDI_CONTROLS;
        options &= ~UBIDI_INSERT_LRM_FOR_NUMERIC;
    }

    // If we do not perform the "inverse BiDi" algorithm, then we
    // don't need to insert any LRMs, and don't need to test for it.
    if ((pBiDi->reorderingMode != UBIDI_REORDER_INVERSE_NUMBERS_AS_L) &&
        (pBiDi->reorderingMode != UBIDI_REORDER_INVERSE_LIKE_DIRECT) &&
        (pBiDi->reorderingMode != UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL) &&
        (pBiDi->reorderingMode != UBIDI_REORDER_RUNS_ONLY)) {
        options &= ~UBIDI_INSERT_LRM_FOR_NUMERIC;
    }

    // Iterate through all visual runs and copy the run text segments to
    // the destination, according to the options.
    //
    // The tests for where to insert LRMs ignore the fact that there may be
    // BN codes or non-BMP code points at the beginning and end of a run;
    // they may insert LRMs unnecessarily but the tests are faster this way
    // (this would have to be improved for UTF-8).
    //
    // Note that the only errors that are set by doWriteXY() are buffer overflow
    // errors. Ignore them until the end, and continue for preflighting.

    int32_t logicalStart = 0;
    int32_t runLength = 0;
    int32_t runIndex = 0;
    int32_t dstNativeStart = 0;
    int32_t visualStart = 0;
    UBiDiDirection direction = UBIDI_LTR;

    if (!(options & UBIDI_OUTPUT_REVERSE)) {
        // Forward output
        if (!(options & UBIDI_INSERT_LRM_FOR_NUMERIC)) {
            // Do not insert BiDi controls
            for (runIndex = 0; runIndex < runCount; ++runIndex)
            {
                direction = ubidi_getVisualRun(pBiDi, runIndex, &logicalStart, &runLength);

                if ((start > visualStart) && (start < visualStart + runLength - 1)) {
                    logicalStart += start - visualStart;
                    runLength -= start - visualStart;
                }
                if ((limit > visualStart) && (limit < visualStart + runLength - 1)) {
                    runLength -= visualStart + runLength - limit;
                }

                if (UBIDI_LTR == direction) {
                    runLength = doWriteForward(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        (uint16_t)(options & ~UBIDI_DO_MIRRORING), pErrorCode);
                }
                else {
                    runLength = doWriteReverse(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        options, pErrorCode);
                }

                dstNativeStart += runLength;
            }
        }
        else {
            // Insert BiDi controls for "inverse BiDi"
            const DirProp *dirProps = pBiDi->dirProps;
            UChar32 uchar = 0;
            int32_t nativeLimit = 0;
            int32_t markFlag;

            for (runIndex = 0; runIndex < runCount; ++runIndex)
            {
                direction = ubidi_getVisualRun(pBiDi, runIndex, &logicalStart, &runLength);

                // Check if something relevant in insertPoints
                markFlag = pBiDi->runs[runIndex].insertRemove;
                if (markFlag < 0) { // BiDi controls count
                    markFlag = 0;
                }

                if (UBIDI_LTR == direction) {
                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart);
                    uchar = UTEXT_NEXT32(srcUt);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if ((pBiDi->isInverse) && (/*(runIndex > 0) &&*/ (dirProps[nativeLimit - 1] != L))) {
                        markFlag |= LRM_BEFORE;
                    }

                    uchar = 0;
                    if (markFlag & LRM_BEFORE) {
                        uchar = LRM_CHAR;
                    }
                    else if (markFlag & RLM_BEFORE) {
                        uchar = RLM_CHAR;
                    }

                    if (uchar) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, uchar, pErrorCode);
                    }

                    runLength = doWriteForward(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        (uint16_t)(options & ~UBIDI_DO_MIRRORING), pErrorCode);
                    dstNativeStart += runLength;

                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart + runLength);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if ((pBiDi->isInverse) && (/*(runIndex < runCount - 1) &&*/ (dirProps[nativeLimit - 1] != L))) {
                        markFlag |= LRM_AFTER;
                    }

                    uchar = 0;
                    if (markFlag & LRM_AFTER) {
                        uchar = LRM_CHAR;
                    }
                    else if (markFlag & RLM_AFTER) {
                        uchar = RLM_CHAR;
                    }

                    if (uchar) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, uchar, pErrorCode);
                    }
                }
                else { // RTL run
                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart + runLength);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if ((pBiDi->isInverse) && (/*(runIndex > 0) &&*/ (!(MASK_R_AL & DIRPROP_FLAG(dirProps[nativeLimit - 1]))))) {
                        markFlag |= RLM_BEFORE;
                    }

                    if (markFlag & LRM_BEFORE) {
                        uchar = LRM_CHAR;
                    }
                    else if (markFlag & RLM_BEFORE) {
                        uchar = RLM_CHAR;
                    }

                    if (uchar) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, uchar, pErrorCode);
                    }

                    runLength = doWriteReverse(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        options, pErrorCode);
                    dstNativeStart += runLength;

                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart);
                    uchar = UTEXT_NEXT32(srcUt);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if ((pBiDi->isInverse) && (/*(runIndex < runCount - 1) &&*/ (!(MASK_R_AL & DIRPROP_FLAG(dirProps[nativeLimit - 1]))))) {
                        markFlag |= RLM_AFTER;
                    }

                    uchar = 0;
                    if (markFlag & LRM_AFTER) {
                        uchar = LRM_CHAR;
                    }
                    else if (markFlag & RLM_AFTER) {
                        uchar = RLM_CHAR;
                    }

                    if (uchar) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, uchar, pErrorCode);
                    }
                }
            }
        }
    }
    else {
        // Reverse output
        if (!(options & UBIDI_INSERT_LRM_FOR_NUMERIC)) {
            // Do not insert BiDi controls
            for (runIndex = runCount; --runIndex >= 0;)
            {
                direction = ubidi_getVisualRun(pBiDi, runIndex, &logicalStart, &runLength);

                if ((start > visualStart) && (start < visualStart + runLength - 1)) {
                    logicalStart += start - visualStart;
                    runLength -= start - visualStart;
                }
                if ((limit > visualStart) && (limit < visualStart + runLength - 1)) {
                    runLength -= visualStart + runLength - limit;
                }

                if (UBIDI_LTR == direction) {
                    runLength = doWriteReverse(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        (uint16_t)(options & ~UBIDI_DO_MIRRORING), pErrorCode);
                }
                else {
                    runLength = doWriteForward(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        options, pErrorCode);
                }
                dstNativeStart += runLength;
            }
        }
        else {
            // Insert BiDi controls for "inverse BiDi"
            const DirProp *dirProps = pBiDi->dirProps;
            UChar32 uchar = 0;
            int32_t nativeLimit = 0;

            for (runIndex = runCount; --runIndex >= 0;)
            {
                // Reverse output
                direction = ubidi_getVisualRun(pBiDi, runIndex, &logicalStart, &runLength);

                if ((start > visualStart) && (start < visualStart + runLength - 1)) {
                    logicalStart += start - visualStart;
                    runLength -= start - visualStart;
                }
                if ((limit > visualStart) && (limit < visualStart + runLength - 1)) {
                    runLength -= visualStart + runLength - limit;
                }

                if (UBIDI_LTR == direction) {
                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart + runLength);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if (/*(runIndex < runCount - 1) &&*/ (dirProps[nativeLimit - 1] != L)) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, LRM_CHAR, pErrorCode);
                    }

                    runLength = doWriteReverse(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        (uint16_t)(options & ~UBIDI_DO_MIRRORING), pErrorCode);
                    dstNativeStart += runLength;

                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart);
                    uchar = UTEXT_NEXT32(srcUt);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if (/*(runIndex > 0) &&*/ (dirProps[nativeLimit - 1] != L)) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, LRM_CHAR, pErrorCode);
                    }
                }
                else {
                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart);
                    uchar = UTEXT_NEXT32(srcUt);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if (/*(runIndex < runCount - 1) &&*/ (!(MASK_R_AL & DIRPROP_FLAG(dirProps[nativeLimit - 1])))) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, RLM_CHAR, pErrorCode);
                    }

                    runLength = doWriteForward(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        options, pErrorCode);
                    dstNativeStart += runLength;

                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart + runLength);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if (/*(runIndex > 0) &&*/ (!(MASK_R_AL & DIRPROP_FLAG(dirProps[nativeLimit - 1])))) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, RLM_CHAR, pErrorCode);
                    }
                }
            }
        }
    }

    return dstNativeStart;
}

U_CAPI int32_t U_EXPORT2
ubidi_writeUReordered(UBiDi *pBiDi,
    UText *dstUt,
    uint16_t options,
    UErrorCode *pErrorCode)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    if (pBiDi == NULL || dstUt == NULL) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if (!utext_isWritable(dstUt)) {
        *pErrorCode = U_NO_WRITE_PERMISSION;
        return 0;
    }

    UText *srcUt = &pBiDi->ut;
    int32_t srcLength = pBiDi->length;

    // Do input and output overlap?
    if (utext_equals(srcUt, dstUt)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    // Nothing to do?
    if (srcLength == 0) {
        return 0;
    }

    int32_t runCount = ubidi_countRuns(pBiDi, pErrorCode);
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }

    int32_t logicalStart = 0;
    int32_t runLength = 0;
    int32_t runIndex = 0;
    int32_t dstNativeStart = 0;

    // Option "insert marks" implies UBIDI_INSERT_LRM_FOR_NUMERIC if the
    // reordering mode (checked below) is appropriate.
    if (pBiDi->reorderingOptions & UBIDI_OPTION_INSERT_MARKS) {
        options |= UBIDI_INSERT_LRM_FOR_NUMERIC;
        options &= ~UBIDI_REMOVE_BIDI_CONTROLS;
    }

    // Option "remove controls" implies UBIDI_REMOVE_BIDI_CONTROLS
    // and cancels UBIDI_INSERT_LRM_FOR_NUMERIC.
    if (pBiDi->reorderingOptions & UBIDI_OPTION_REMOVE_CONTROLS) {
        options |= UBIDI_REMOVE_BIDI_CONTROLS;
        options &= ~UBIDI_INSERT_LRM_FOR_NUMERIC;
    }

    // If we do not perform the "inverse BiDi" algorithm, then we
    // don't need to insert any LRMs, and don't need to test for it.
    if ((pBiDi->reorderingMode != UBIDI_REORDER_INVERSE_NUMBERS_AS_L) &&
        (pBiDi->reorderingMode != UBIDI_REORDER_INVERSE_LIKE_DIRECT) &&
        (pBiDi->reorderingMode != UBIDI_REORDER_INVERSE_FOR_NUMBERS_SPECIAL) &&
        (pBiDi->reorderingMode != UBIDI_REORDER_RUNS_ONLY)) {
        options &= ~UBIDI_INSERT_LRM_FOR_NUMERIC;
    }

    // Iterate through all visual runs and copy the run text segments to
    // the destination, according to the options.
    //
    // The tests for where to insert LRMs ignore the fact that there may be
    // BN codes or non-BMP code points at the beginning and end of a run;
    // they may insert LRMs unnecessarily but the tests are faster this way
    // (this would have to be improved for UTF-8).
    //
    // Note that the only errors that are set by doWriteXY() are buffer overflow
    // errors. Ignore them until the end, and continue for preflighting.

    if (!(options & UBIDI_OUTPUT_REVERSE)) {
        // Forward output
        if (!(options & UBIDI_INSERT_LRM_FOR_NUMERIC)) {
            // Do not insert BiDi controls
            for (runIndex = 0; runIndex < runCount; ++runIndex)
            {
                if (UBIDI_LTR == ubidi_getVisualRun(pBiDi, runIndex, &logicalStart, &runLength)) {
                    runLength = doWriteForward(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        (uint16_t)(options & ~UBIDI_DO_MIRRORING), pErrorCode);
                }
                else {
                    runLength = doWriteReverse(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        options, pErrorCode);
                }

                dstNativeStart += runLength;
            }
        }
        else {
            // Insert BiDi controls for "inverse BiDi"
            const DirProp *dirProps = pBiDi->dirProps;
            UChar32 uchar = 0;
            int32_t nativeLimit = 0;
            UBiDiDirection direction;
            int32_t markFlag;

            for (runIndex = 0; runIndex < runCount; ++runIndex)
            {
                direction = ubidi_getVisualRun(pBiDi, runIndex, &logicalStart, &runLength);

                // Check if something relevant in insertPoints
                markFlag = pBiDi->runs[runIndex].insertRemove;
                if (markFlag < 0) { // BiDi controls count
                    markFlag = 0;
                }

                if (UBIDI_LTR == direction) {
                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart);
                    uchar = UTEXT_NEXT32(srcUt);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if ((pBiDi->isInverse) && (/*(runIndex > 0) &&*/ (dirProps[nativeLimit - 1] != L))) {
                        markFlag |= LRM_BEFORE;
                    }

                    uchar = 0;
                    if (markFlag & LRM_BEFORE) {
                        uchar = LRM_CHAR;
                    }
                    else if (markFlag & RLM_BEFORE) {
                        uchar = RLM_CHAR;
                    }

                    if (uchar) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, uchar, pErrorCode);
                    }

                    runLength = doWriteForward(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        (uint16_t)(options & ~UBIDI_DO_MIRRORING), pErrorCode);
                    dstNativeStart += runLength;

                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart + runLength);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if ((pBiDi->isInverse) && (/*(runIndex < runCount - 1) &&*/ (dirProps[nativeLimit - 1] != L))) {
                        markFlag |= LRM_AFTER;
                    }

                    uchar = 0;
                    if (markFlag & LRM_AFTER) {
                        uchar = LRM_CHAR;
                    }
                    else if (markFlag & RLM_AFTER) {
                        uchar = RLM_CHAR;
                    }

                    if (uchar) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, uchar, pErrorCode);
                    }
                }
                else { // RTL run
                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart + runLength);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if ((pBiDi->isInverse) && (/*(runIndex > 0) &&*/ (!(MASK_R_AL & DIRPROP_FLAG(dirProps[nativeLimit - 1]))))) {
                        markFlag |= RLM_BEFORE;
                    }

                    if (markFlag & LRM_BEFORE) {
                        uchar = LRM_CHAR;
                    }
                    else if (markFlag & RLM_BEFORE) {
                        uchar = RLM_CHAR;
                    }

                    if (uchar) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, uchar, pErrorCode);
                    }

                    runLength = doWriteReverse(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        options, pErrorCode);
                    dstNativeStart += runLength;

                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart);
                    uchar = UTEXT_NEXT32(srcUt);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if ((pBiDi->isInverse) && (/*(runIndex < runCount - 1) &&*/ (!(MASK_R_AL & DIRPROP_FLAG(dirProps[nativeLimit - 1]))))) {
                        markFlag |= RLM_AFTER;
                    }

                    uchar = 0;
                    if (markFlag & LRM_AFTER) {
                        uchar = LRM_CHAR;
                    }
                    else if (markFlag & RLM_AFTER) {
                        uchar = RLM_CHAR;
                    }

                    if (uchar) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, uchar, pErrorCode);
                    }
                }
            }
        }
    }
    else {
        // Reverse output
        if (!(options & UBIDI_INSERT_LRM_FOR_NUMERIC)) {
            // Do not insert BiDi controls
            for (runIndex = runCount; --runIndex >= 0;)
            {
                if (UBIDI_LTR == ubidi_getVisualRun(pBiDi, runIndex, &logicalStart, &runLength)) {
                    runLength = doWriteReverse(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        (uint16_t)(options & ~UBIDI_DO_MIRRORING), pErrorCode);
                }
                else {
                    runLength = doWriteForward(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        options, pErrorCode);
                }
                dstNativeStart += runLength;
            }
        }
        else {
            // Insert BiDi controls for "inverse BiDi"
            const DirProp *dirProps = pBiDi->dirProps;
            UChar32 uchar = 0;
            int32_t nativeLimit = 0;
            UBiDiDirection direction;

            for (runIndex = runCount; --runIndex >= 0;)
            {
                // Reverse output
                direction = ubidi_getVisualRun(pBiDi, runIndex, &logicalStart, &runLength);
                if (UBIDI_LTR == direction) {
                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart + runLength);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if (/*(runIndex < runCount - 1) &&*/ (dirProps[nativeLimit - 1] != L)) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, LRM_CHAR, pErrorCode);
                    }

                    runLength = doWriteReverse(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        (uint16_t)(options & ~UBIDI_DO_MIRRORING), pErrorCode);
                    dstNativeStart += runLength;

                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart);
                    uchar = UTEXT_NEXT32(srcUt);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if (/*(runIndex > 0) &&*/ (dirProps[nativeLimit - 1] != L)) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, LRM_CHAR, pErrorCode);
                    }
                }
                else {
                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart);
                    uchar = UTEXT_NEXT32(srcUt);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if (/*(runIndex < runCount - 1) &&*/ (!(MASK_R_AL & DIRPROP_FLAG(dirProps[nativeLimit - 1])))) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, RLM_CHAR, pErrorCode);
                    }

                    runLength = doWriteForward(srcUt, logicalStart, runLength,
                        dstUt, dstNativeStart,
                        options, pErrorCode);
                    dstNativeStart += runLength;

                    UTEXT_SETNATIVEINDEX(srcUt, logicalStart + runLength);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(srcUt);

                    if (/*(runIndex > 0) &&*/ (!(MASK_R_AL & DIRPROP_FLAG(dirProps[nativeLimit - 1])))) {
                        dstNativeStart += utext_append32(dstUt, dstNativeStart, RLM_CHAR, pErrorCode);
                    }
                }
            }
        }
    }

    return dstNativeStart;
}

U_CAPI int32_t U_EXPORT2
ubidi_writeReordered(UBiDi *pBiDi,
    UChar *dest,
    int32_t destSize,
    uint16_t options,
    UErrorCode *pErrorCode)
{
    if ((pErrorCode == NULL) || (U_FAILURE(*pErrorCode))) {
        return 0;
    }

    if ((destSize < 0) || ((destSize > 0 && dest == NULL))) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

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
    int32_t length = ubidi_writeUReordered(pBiDi, &dstUt, options, pErrorCode);

    utext_close(&dstUt);

    return length;
}

