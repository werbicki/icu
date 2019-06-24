// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1999-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  ubidiln.c
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

#include "cmemory.h"
#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/uchar.h"
#include "unicode/ubidi.h"
#include "ubidiimp.h"
#include "uassert.h"

/*
 * General remarks about the functions in this file:
 *
 * These functions deal with the aspects of potentially mixed-directional
 * text in a single paragraph or in a line of a single paragraph
 * which has already been processed according to
 * the Unicode 6.3 BiDi algorithm as defined in
 * http://www.unicode.org/unicode/reports/tr9/ , version 28,
 * also described in The Unicode Standard, Version 6.3.0 .
 *
 * This means that there is a UBiDi object with a levels
 * and a dirProps array.
 * paraLevel and direction are also set.
 * Only if the length of the text is zero, then levels==dirProps==NULL.
 *
 * The overall directionality of the paragraph
 * or line is used to bypass the reordering steps if possible.
 * Even purely RTL text does not need reordering there because
 * the ubidi_getLogical/VisualIndex() functions can compute the
 * index on the fly in such a case.
 *
 * The implementation of the access to same-level-runs and of the reordering
 * do attempt to provide better performance and less memory usage compared to
 * a direct implementation of especially rule (L2) with an array of
 * one (32-bit) integer per text character.
 *
 * Here, the levels array is scanned as soon as necessary, and a vector of
 * same-level-runs is created. Reordering then is done on this vector.
 * For each run of text positions that were resolved to the same level,
 * only 8 bytes are stored: the first text position of the run and the visual
 * position behind the run after reordering.
 * One sign bit is used to hold the directionality of the run.
 * This is inefficient if there are many very short runs. If the average run
 * length is <2, then this uses more memory.
 *
 * In a further attempt to save memory, the levels array is never changed
 * after all the resolution rules (Xn, Wn, Nn, In).
 * Many functions have to consider the field trailingWSStart:
 * if it is less than length, then there is an implicit trailing run
 * at the paraLevel,
 * which is not reflected in the levels array.
 * This allows a line UBiDi object to use the same levels array as
 * its paragraph parent object.
 *
 * When a UBiDi object is created for a line of a paragraph, then the
 * paragraph's levels and dirProps arrays are reused by way of setting
 * a pointer into them, not by copying. This again saves memory and forbids to
 * change the now shared levels for (L1).
 */

 /* handle trailing WS (L1) -------------------------------------------------- */

 /*
  * setTrailingWSStart() sets the start index for a trailing
  * run of WS in the line. This is necessary because we do not modify
  * the paragraph's levels array that we just point into.
  * Using trailingWSStart is another form of performing (L1).
  *
  * To make subsequent operations easier, we also include the run
  * before the WS if it is at the paraLevel - we merge the two here.
  *
  * This function is called only from ubidi_setLine(), so pBiDi->paraLevel is
  * set correctly for the line even when contextual multiple paragraphs.
  */
static void
setTrailingWSStart(UBiDi *pBiDi)
{
    // pBiDi->direction != UBIDI_MIXED

    const DirProp *dirProps = pBiDi->dirProps;
    UBiDiLevel *levels = pBiDi->levels;
    UBiDiLevel paraLevel = pBiDi->paraLevel;
    int32_t start = pBiDi->length;

    // If the line is terminated by a block separator, all preceding WS etc...
    // are already set to paragraph level.
    //
    // Setting trailingWSStart to pBidi->length will avoid changing the
    // level of B chars from 0 to paraLevel in ubidi_getLevels when
    // orderParagraphsLTR==TRUE.

    if (dirProps[start - 1] == B) {
        pBiDi->trailingWSStart = start; // Currently == pBiDi->length
        return;
    }

    // Go backwards across all WS, BN, explicit codes
    while ((start > 0) && (DIRPROP_FLAG(dirProps[start - 1]) & MASK_WS)) {
        --start;
    }

    // If the WS run can be merged with the previous run then do so here
    while ((start > 0) && (levels[start - 1] == paraLevel)) {
        --start;
    }

    pBiDi->trailingWSStart = start;
}

/* ubidi_setLine ------------------------------------------------------------ */

U_CAPI void U_EXPORT2
ubidi_setLine(const UBiDi *pParaBiDi,
    int32_t start, int32_t limit,
    UBiDi *pLineBiDi,
    UErrorCode *pErrorCode)
{
    RETURN_VOID_IF_NULL_OR_FAILING_ERRCODE(pErrorCode);
    RETURN_VOID_IF_NOT_VALID_PARA(pParaBiDi, *pErrorCode);
    RETURN_VOID_IF_BAD_RANGE(start, 0, limit, *pErrorCode);
    RETURN_VOID_IF_BAD_RANGE(limit, 0, pParaBiDi->length + 1, *pErrorCode);
    if (pLineBiDi == NULL) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (ubidi_getParagraph(pParaBiDi, start, NULL, NULL, NULL, pErrorCode) !=
        ubidi_getParagraph(pParaBiDi, limit - 1, NULL, NULL, NULL, pErrorCode)) {
        // The line crosses a paragraph boundary
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    UText ut = UTEXT_INITIALIZER;
    utext_clone(&ut, &pParaBiDi->ut, FALSE, TRUE, pErrorCode);
    if (U_FAILURE(*pErrorCode))
        return;
    UTEXT_SETNATIVEINDEX(&ut, start);

    // Set the values in pLineBiDi from its pParaBiDi parent
    pLineBiDi->pParaBiDi = NULL; // Mark unfinished setLine
    pLineBiDi->ut = ut;
    pLineBiDi->utNativeStart = start;
    int32_t length = pLineBiDi->length = limit - start;
    pLineBiDi->resultLength = pLineBiDi->originalLength = length;
    pLineBiDi->paraLevel = GET_PARALEVEL(pParaBiDi, start);
    pLineBiDi->paraCount = pParaBiDi->paraCount;
    pLineBiDi->runs = NULL;
    pLineBiDi->flags = 0;
    pLineBiDi->reorderingMode = pParaBiDi->reorderingMode;
    pLineBiDi->reorderingOptions = pParaBiDi->reorderingOptions;
    pLineBiDi->controlCount = 0;

    if (pParaBiDi->controlCount > 0) {
        UTEXT_SETNATIVEINDEX(&ut, 0);
        int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(&ut);
        UChar32 uchar = UTEXT_NEXT32(&ut);
        int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(&ut);
        for (; (uchar != U_SENTINEL) && (nativeStart < length);
            nativeStart = nativeLimit, uchar = UTEXT_NEXT32(&ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(&ut))
        {
            if (IS_BIDI_CONTROL_CHAR(uchar)) {
                pLineBiDi->controlCount += nativeLimit - nativeStart;
            }
        }

        pLineBiDi->resultLength -= pLineBiDi->controlCount;
    }

    pLineBiDi->dirProps = pParaBiDi->dirProps + start;
    pLineBiDi->levels = pParaBiDi->levels + start;
    pLineBiDi->runCount = -1;

    if (pParaBiDi->direction != UBIDI_MIXED) {
        // The parent is already trivial
        pLineBiDi->direction = pParaBiDi->direction;

        // The parent's levels are all either implicitly or 
        // explicitly == paraLevel; do the same here.
        if (pParaBiDi->trailingWSStart <= start) {
            pLineBiDi->trailingWSStart = 0;
        }
        else if (pParaBiDi->trailingWSStart < limit) {
            pLineBiDi->trailingWSStart = pParaBiDi->trailingWSStart - start;
        }
        else {
            pLineBiDi->trailingWSStart = length;
        }
    }
    else {
        const UBiDiLevel *levels = pLineBiDi->levels;
        int32_t trailingWSStart;

        setTrailingWSStart(pLineBiDi);
        trailingWSStart = pLineBiDi->trailingWSStart;

        // Recalculate pLineBiDi->direction
        if (trailingWSStart == 0) {
            // All levels are at paraLevel
            pLineBiDi->direction = (UBiDiDirection)(pLineBiDi->paraLevel & 1);
        }
        else {
            // Get the level of the first character
            UBiDiLevel level = (UBiDiLevel)(levels[0] & 1);

            // If there is anything of a different level, then the line is mixed
            if (trailingWSStart < length && (pLineBiDi->paraLevel & 1) != level) {
                // The trailing WS is at paraLevel, which differs from levels[0]
                pLineBiDi->direction = UBIDI_MIXED;
            }
            else {
                // See if levels[1..trailingWSStart-1] have the same direction as levels[0] and paraLevel
                int32_t i = 0;
                for (i = 1; i <= trailingWSStart; i++)
                {
                    if (i == trailingWSStart) {
                        // The direction values match those in level
                        pLineBiDi->direction = (UBiDiDirection)level;
                        break;
                    }
                    else if ((levels[i] & 1) != level) {
                        pLineBiDi->direction = UBIDI_MIXED;
                        break;
                    }
                }
            }
        }

        switch (pLineBiDi->direction) {
        case UBIDI_LTR:
            // Make sure paraLevel is even
            pLineBiDi->paraLevel = (UBiDiLevel)((pLineBiDi->paraLevel + 1)&~1);

            // All levels are implicitly at paraLevel (important for ubidi_getLevels())
            pLineBiDi->trailingWSStart = 0;
            break;
        case UBIDI_RTL:
            // Make sure paraLevel is odd
            pLineBiDi->paraLevel |= 1;

            // All levels are implicitly at paraLevel (important for ubidi_getLevels())
            pLineBiDi->trailingWSStart = 0;
            break;
        default:
            break;
        }
    }

    pLineBiDi->pParaBiDi = pParaBiDi; // Mark successful setLine

    return;
}

U_CAPI UBiDiLevel U_EXPORT2
ubidi_getLevelAt(const UBiDi *pBiDi,
    int32_t charIndex)
{
    // Return paraLevel if in the trailing WS run, otherwise the real level
    if ((!IS_VALID_PARA_OR_LINE(pBiDi)) || (charIndex < 0) || (pBiDi->length <= charIndex)) {
        return 0;
    }
    else if ((pBiDi->direction != UBIDI_MIXED) || (charIndex >= pBiDi->trailingWSStart)) {
        return GET_PARALEVEL(pBiDi, charIndex);
    }
    else {
        return pBiDi->levels[charIndex];
    }
}

U_CAPI const UBiDiLevel * U_EXPORT2
ubidi_getLevels(UBiDi *pBiDi,
    UErrorCode *pErrorCode)
{
    RETURN_IF_NULL_OR_FAILING_ERRCODE(pErrorCode, NULL);
    RETURN_IF_NOT_VALID_PARA_OR_LINE(pBiDi, *pErrorCode, NULL);

    int32_t start = pBiDi->trailingWSStart;
    int32_t length = pBiDi->length;

    if (length <= 0) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if (start == length) {
        // The current levels array reflects the WS run
        return pBiDi->levels;
    }

    // After the previous if(), we know that the levels array
    // has an implicit trailing WS run and therefore does not fully
    // reflect itself all the levels.
    //
    // This must be a UBiDi object for a line, and
    // we need to create a new levels array.

    if (getLevelsMemory(pBiDi, length)) {
        UBiDiLevel *levels = pBiDi->levelsMemory;

        if ((start > 0) && (levels != pBiDi->levels)) {
            uprv_memcpy(levels, pBiDi->levels, start);
        }

        // pBiDi->paraLevel is ok even if contextual multiple paragraphs,
        // since pBidi is a line object
        uprv_memset(&levels[start], pBiDi->paraLevel, length - start);

        // This new levels array is set for the line and reflects the WS run
        pBiDi->trailingWSStart = length;
        return pBiDi->levels = levels;
    }
    else {
        *pErrorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }
}

/* runs API functions ------------------------------------------------------- */

U_CAPI int32_t U_EXPORT2
ubidi_countRuns(UBiDi *pBiDi,
    UErrorCode *pErrorCode)
{
    RETURN_IF_NULL_OR_FAILING_ERRCODE(pErrorCode, -1);
    RETURN_IF_NOT_VALID_PARA_OR_LINE(pBiDi, *pErrorCode, -1);

    ubidi_getRuns(pBiDi, pErrorCode);
    if (U_FAILURE(*pErrorCode)) {
        return -1;
    }

    return pBiDi->runCount;
}

// In trivial cases there is only one trivial run; called by ubidi_getRuns()
static void
getSingleRun(UBiDi *pBiDi,
    UBiDiLevel level)
{
    // Simple, single-run case
    pBiDi->runs = pBiDi->simpleRuns;
    pBiDi->runCount = 1;

    // Fill and reorder the single run
    pBiDi->runs[0].logicalStart = MAKE_INDEX_ODD_PAIR(0, level);
    pBiDi->runs[0].visualLimit = pBiDi->length;
    pBiDi->runs[0].insertRemove = 0;
}

/* reorder the runs array (L2) ---------------------------------------------- */

/*
 * Reorder the same-level runs in the runs array.
 * Here, runCount>1 and maxLevel>=minLevel>=paraLevel.
 * All the visualStart fields=logical start before reordering.
 * The "odd" bits are not set yet.
 *
 * Reordering with this data structure lends itself to some handy shortcuts:
 *
 * Since each run is moved but not modified, and since at the initial maxLevel
 * each sequence of same-level runs consists of only one run each, we
 * don't need to do anything there and can predecrement maxLevel.
 * In many simple cases, the reordering is thus done entirely in the
 * index mapping.
 * Also, reordering occurs only down to the lowest odd level that occurs,
 * which is minLevel|1. However, if the lowest level itself is odd, then
 * in the last reordering the sequence of the runs at this level or higher
 * will be all runs, and we don't need the elaborate loop to search for them.
 * This is covered by ++minLevel instead of minLevel|=1 followed
 * by an extra reorder-all after the reorder-some loop.
 * About a trailing WS run:
 * Such a run would need special treatment because its level is not
 * reflected in levels[] if this is not a paragraph object.
 * Instead, all characters from trailingWSStart on are implicitly at
 * paraLevel.
 * However, for all maxLevel>paraLevel, this run will never be reordered
 * and does not need to be taken into account. maxLevel==paraLevel is only reordered
 * if minLevel==paraLevel is odd, which is done in the extra segment.
 * This means that for the main reordering loop we don't need to consider
 * this run and can --runCount. If it is later part of the all-runs
 * reordering, then runCount is adjusted accordingly.
 */
static void
reorderLine(UBiDi *pBiDi,
    UBiDiLevel minLevel,
    UBiDiLevel maxLevel)
{
    // Nothing to do?
    if (maxLevel <= (minLevel | 1)) {
        return;
    }

    // Reorder only down to the lowest odd level
    // and reorder at an odd minLevel in a separate, simpler loop.
    // See comments above for why minLevel is always incremented.
    ++minLevel;

    Run *runs = pBiDi->runs;
    UBiDiLevel *levels = pBiDi->levels;
    int32_t runCount = pBiDi->runCount;

    // Do not include the WS run at paraLevel<=old minLevel except in the simple loop
    if (pBiDi->trailingWSStart < pBiDi->length) {
        --runCount;
    }

    int32_t firstRun = 0;
    int32_t endRun = 0;
    int32_t limitRun = 0;

    while (--maxLevel >= minLevel) {
        // Loop for all sequences of runs
        for (firstRun = 0; firstRun < runCount; )
        {
            // Look for a sequence of runs that are all at >=maxLevel
            // Look for the first run of such a sequence
            while ((firstRun < runCount) && (levels[runs[firstRun].logicalStart] < maxLevel)) {
                ++firstRun;
            }

            if (firstRun >= runCount) {
                break; // No more such runs
            }

            // Look for the limit run of such a sequence (the run behind it)
            for (limitRun = firstRun; (++limitRun < runCount) && (levels[runs[limitRun].logicalStart] >= maxLevel); )
            {
                // Do nothing.
            }

            // Swap the entire sequence of runs from firstRun to limitRun-1
            endRun = limitRun - 1;
            while (firstRun < endRun) {
                Run tempRun = runs[firstRun];
                runs[firstRun] = runs[endRun];
                runs[endRun] = tempRun;
                ++firstRun;
                --endRun;
            }

            if (limitRun == runCount) {
                break; // No more such runs
            }
            else {
                firstRun = limitRun + 1;
            }
        }
    }

    // Now do maxLevel==old minLevel (==odd!), see above
    if (!(minLevel & 1)) {
        firstRun = 0;

        // Include the trailing WS run in this complete reordering
        if (pBiDi->trailingWSStart == pBiDi->length) {
            --runCount;
        }

        // Swap the entire sequence of all runs. (endRun==runCount)
        while (firstRun < runCount) {
            Run tempRun = runs[firstRun];
            runs[firstRun] = runs[runCount];
            runs[runCount] = tempRun;
            ++firstRun;
            --runCount;
        }
    }
}

/* compute the runs array --------------------------------------------------- */

static int32_t getRunFromLogicalIndex(UBiDi *pBiDi,
    int32_t logicalIndex,
    UErrorCode *pErrorCode)
{
    Run *runs = pBiDi->runs;
    int32_t runCount = pBiDi->runCount;
    int32_t runIndex = 0;
    int32_t visualStart = 0;
    int32_t runLength = 0;
    int32_t logicalStart = 0;

    for (runIndex = 0; runIndex < runCount; runIndex++)
    {
        runLength = runs[runIndex].visualLimit - visualStart;
        logicalStart = GET_INDEX(runs[runIndex].logicalStart);

        if ((logicalIndex >= logicalStart) && (logicalIndex < (logicalStart + runLength))) {
            return runIndex;
        }

        visualStart += runLength;
    }

    // We should never get here
    U_ASSERT(FALSE);
    *pErrorCode = U_INVALID_STATE_ERROR;

    return 0;
}

/*
 * Compute the runs array from the levels array.
 * After ubidi_getRuns() returns TRUE, runCount is guaranteed to be >0
 * and the runs are reordered.
 * Odd-level runs have visualStart on their visual right edge and
 * they progress visually to the left.
 * If option UBIDI_OPTION_INSERT_MARKS is set, insertRemove will contain the
 * sum of appropriate LRM/RLM_BEFORE/AFTER flags.
 * If option UBIDI_OPTION_REMOVE_CONTROLS is set, insertRemove will contain the
 * negative number of BiDi control characters within this run.
 */
U_CFUNC UBool
ubidi_getRuns(UBiDi *pBiDi,
    UErrorCode *pErrorCode)
{
    UBool success = TRUE;

    UText *ut = &pBiDi->ut;

    // This method returns immediately if the runs are already set. This
    // includes the case of length==0 (handled in setPara).
    if (pBiDi->runCount < 0) {
        if (pBiDi->direction != UBIDI_MIXED) {
            // Simple, single-run case - this covers length==0
            // pBiDi->paraLevel is ok even for contextual multiple paragraphs
            getSingleRun(pBiDi, pBiDi->paraLevel);
        }
        else { // UBIDI_MIXED, length>0
            // Mixed directionality
            int32_t length = pBiDi->length;
            UBiDiLevel *levels = pBiDi->levels;
            UBiDiLevel level = UBIDI_DEFAULT_LTR; // Initialize with no valid level

            // If there are WS characters at the end of the line
            // and the run preceding them has a level different from
            // paraLevel, then they will form their own run at paraLevel (L1).
            // Count them separately.
            //
            // We need some special treatment for this in order to not
            // modify the levels array which a line UBiDi object shares
            // with its paragraph parent and its other line siblings.
            // In other words, for the trailing WS, it may be
            // levels[]!=paraLevel but we have to treat it like it were so.

            int32_t limit = pBiDi->trailingWSStart;

            // Count the runs, there is at least one non-WS run, and limit>0
            int32_t runCount = 0;
            int32_t runLimit = 0;

            for (runLimit = 0; runLimit < limit; runLimit++)
            {
                // Increment runCount at the start of each run
                if (levels[runLimit] != level) {
                    ++runCount;
                    level = levels[runLimit];
                }
            }

            // We don't need to see if the last run can be merged with a trailing
            // WS run because setTrailingWSStart() would have done that.
            if ((runCount == 1) && (limit == length)) {
                // There is only one non-WS run and no trailing WS-run.
                getSingleRun(pBiDi, levels[0]);
            }
            else { // runCount>1 || limit<length
                // Allocate and set the runs.
                Run *runs = NULL;

                // Now, count a (non-mergeable) WS run.
                if (limit < length) {
                    ++runCount;
                }

                // runCount > 1
                if (getRunsMemory(pBiDi, runCount)) {
                    uprv_memset(pBiDi->runsMemory, 0, pBiDi->runsSize);

                    runs = pBiDi->runsMemory;

                    // Set the runs.
                    // FOOD FOR THOUGHT: this could be optimized, e.g.:
                    // 464->444, 484->444, 575->555, 595->555
                    // However, that would take longer. Check also how it would
                    // interact with BiDi control removal and inserting Marks.

                    int32_t runIndex = 0;
                    int32_t runStart = 0;
                    UBiDiLevel minLevel = UBIDI_MAX_EXPLICIT_LEVEL + 1;
                    UBiDiLevel maxLevel = 0;

                    // Search for the run limits and initialize visualLimit values with the run lengths
                    for (runLimit = 0; runLimit < limit; runIndex++)
                    {
                        runStart = runLimit;
                        level = levels[runLimit];
                        if (level < minLevel) {
                            minLevel = level;
                        }
                        if (level > maxLevel) {
                            maxLevel = level;
                        }

                        // Look for the run limit
                        while ((++runLimit < limit) && (levels[runLimit] == level)) {
                            // Do nothing.
                        }

                        runs[runIndex].logicalStart = runStart;
                        runs[runIndex].visualLimit = runLimit - runStart;
                        runs[runIndex].insertRemove = 0;
                    };

                    if (limit < length) {
                        // There is a separate WS run
                        runs[runIndex].logicalStart = limit;
                        runs[runIndex].visualLimit = length - limit;

                        // For the trailing WS run, pBiDi->paraLevel is ok even
                        // if contextual multiple paragraphs.
                        if (pBiDi->paraLevel < minLevel) {
                            minLevel = pBiDi->paraLevel;
                        }
                    }

                    // Set the object fields
                    pBiDi->runs = runs;
                    pBiDi->runCount = runCount;

                    reorderLine(pBiDi, minLevel, maxLevel);

                    // Now add the direction flags and adjust the visualLimit's to be just that
                    // this loop will also handle the trailing WS run
                    limit = 0;

                    int32_t i = 0;
                    for (i = 0; i < runCount; i++)
                    {
                        ADD_ODD_BIT_FROM_LEVEL(runs[i].logicalStart, levels[runs[i].logicalStart]);
                        limit += runs[i].visualLimit;
                        runs[i].visualLimit = limit;
                    }

                    // Set the "odd" bit for the trailing WS run.
                    // For a RTL paragraph, it will be the *first* run in visual order.
                    // For the trailing WS run, pBiDi->paraLevel is ok even if
                    // contextual multiple paragraphs.
                    if (runIndex < runCount) {
                        int32_t trailingRun = ((pBiDi->paraLevel & 1) != 0) ? 0 : runIndex;

                        ADD_ODD_BIT_FROM_LEVEL(runs[trailingRun].logicalStart, pBiDi->paraLevel);
                    }
                }
                else {
                    success = FALSE;
                }
            }
        }

        // Handle insert LRM/RLM BEFORE/AFTER run
        if ((success) && (pBiDi->insertPoints.size > 0)) {
            Point *point, *start = pBiDi->insertPoints.points;
            Point *limit = start + pBiDi->insertPoints.size;
            int32_t runIndex;

            for (point = start; point < limit; point++)
            {
                runIndex = getRunFromLogicalIndex(pBiDi, point->pos, pErrorCode);
                pBiDi->runs[runIndex].insertRemove |= point->flag;
            }
        }

        // Handle remove BiDi control characters
        if ((success) && (pBiDi->controlCount > 0)) {
            UTEXT_SETNATIVEINDEX(ut, 0);
            int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
            UChar32 uchar = UTEXT_NEXT32(ut);
            int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
            for (; (uchar != U_SENTINEL) && (nativeStart < pBiDi->length);
                nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
            {
                if (IS_BIDI_CONTROL_CHAR(uchar)) {
                    int32_t runIndex = getRunFromLogicalIndex(pBiDi, nativeStart, pErrorCode);
                    pBiDi->runs[runIndex].insertRemove -= nativeLimit - nativeStart;
                }
            }
        }
    }

    return success;
}

U_CAPI int32_t U_EXPORT2
ubidi_getLogicalRun(const UBiDi *pBiDi,
    int32_t logicalPosition,
    int32_t *pLogicalLimit,
    UBiDiLevel *pLevel)
{
    UErrorCode errorCode = U_ZERO_ERROR;

    RETURN_IF_BAD_RANGE(logicalPosition, 0, pBiDi->length, errorCode, -1);

    // ubidi_countRuns will check VALID_PARA_OR_LINE
    int32_t runCount = ubidi_countRuns((UBiDi *)pBiDi, &errorCode);
    if (!U_FAILURE(errorCode)) {
        // This is done based on runs rather than on levels since levels have
        // a special interpretation when UBIDI_REORDER_RUNS_ONLY
        Run* runs = pBiDi->runs;
        int32_t visualStart = 0;
        int32_t logicalFirst = 0;
        int32_t logicalLimit = 0;
        int32_t runIndex = 0;

        for (runIndex = 0; runIndex < runCount; runIndex++)
        {
            logicalFirst = GET_INDEX(runs[runIndex].logicalStart);
            logicalLimit = logicalFirst + runs[runIndex].visualLimit - visualStart;

            if ((logicalPosition >= logicalFirst) && (logicalPosition < logicalLimit)) {
                break;
            }

            visualStart = runs[runIndex].visualLimit;
        }

        if (pLogicalLimit) {
            *pLogicalLimit = logicalLimit;
        }

        if (pLevel) {
            if (pBiDi->reorderingMode == UBIDI_REORDER_RUNS_ONLY) {
                *pLevel = (UBiDiLevel)GET_ODD_BIT(runs[runIndex].logicalStart);
            }
            else if ((pBiDi->direction != UBIDI_MIXED) || (logicalPosition >= pBiDi->trailingWSStart)) {
                *pLevel = GET_PARALEVEL(pBiDi, logicalPosition);
            }
            else {
                *pLevel = pBiDi->levels[logicalPosition];
            }
        }

        return runIndex;
    }

    return (-1);
}

U_CAPI UBiDiDirection U_EXPORT2
ubidi_getVisualRun(UBiDi *pBiDi,
    int32_t runIndex,
    int32_t *pLogicalStart,
    int32_t *pLength)
{
    UErrorCode errorCode = U_ZERO_ERROR;

    RETURN_IF_NOT_VALID_PARA_OR_LINE(pBiDi, errorCode, UBIDI_LTR);

    ubidi_getRuns(pBiDi, &errorCode);
    if (U_FAILURE(errorCode)) {
        return UBIDI_LTR;
    }

    RETURN_IF_BAD_RANGE(runIndex, 0, pBiDi->runCount, errorCode, UBIDI_LTR);

    int32_t logicalStart = pBiDi->runs[runIndex].logicalStart;
    if (pLogicalStart != NULL) {
        *pLogicalStart = GET_INDEX(logicalStart);
    }

    if (pLength != NULL) {
        if (runIndex > 0) {
            *pLength = pBiDi->runs[runIndex].visualLimit - pBiDi->runs[runIndex - 1].visualLimit;
        }
        else {
            *pLength = pBiDi->runs[0].visualLimit;
        }
    }

    return (UBiDiDirection)GET_ODD_BIT(logicalStart);
}

/* reorder a line based on a levels array (L2) ------------------------------ */

static UBool
prepareReorder(const UBiDiLevel *levels,
    int32_t length,
    int32_t *indexMap,
    UBiDiLevel *pMinLevel,
    UBiDiLevel *pMaxLevel)
{
    if ((levels == NULL) || (length <= 0)) {
        return FALSE;
    }

    // Determine minLevel and maxLevel
    UBiDiLevel minLevel = UBIDI_MAX_EXPLICIT_LEVEL + 1;
    UBiDiLevel maxLevel = 0;
    UBiDiLevel level = UBIDI_DEFAULT_LTR;
    int32_t start = 0;

    for (start = length; start > 0; )
    {
        level = levels[--start];

        if (level > UBIDI_MAX_EXPLICIT_LEVEL + 1) {
            return FALSE;
        }

        if (level < minLevel) {
            minLevel = level;
        }

        if (level > maxLevel) {
            maxLevel = level;
        }
    }

    *pMinLevel = minLevel;
    *pMaxLevel = maxLevel;

    // Initialize the index map
    for (start = length; start > 0; )
    {
        --start;
        indexMap[start] = start;
    }

    return TRUE;
}

U_CAPI void U_EXPORT2
ubidi_reorderLogical(const UBiDiLevel *levels,
    int32_t length,
    int32_t *indexMap)
{
    UBiDiLevel minLevel = 0, maxLevel = 0;

    if ((indexMap == NULL) || (!prepareReorder(levels, length, indexMap, &minLevel, &maxLevel))) {
        return;
    }

    // Nothing to do?
    if ((minLevel == maxLevel) && ((minLevel & 1) == 0)) {
        return;
    }

    // Reorder only down to the lowest odd level
    minLevel |= 1;

    int32_t start = 0;
    int32_t limit = 0;
    int32_t sumOfSosEos = 0;

    // Loop maxLevel..minLevel
    do {
        // Loop for all sequences of levels to reorder at the current maxLevel
        for (start = 0; start < length; )
        {
            // Look for a sequence of levels that are all at >=maxLevel
            // Look for the first index of such a sequence
            while (start < length && levels[start] < maxLevel) {
                ++start;
            }

            if (start >= length) {
                break; // No more such sequences
            }

            // Look for the limit of such a sequence (the index behind it)
            for (limit = start; (++limit < length) && (levels[limit] >= maxLevel); )
            {
                // Do nothing.
            }

            // sos=start of sequence, eos=end of sequence
            //
            // The closed (inclusive) interval from sos to eos includes all the logical
            // and visual indexes within this sequence. They are logically and
            // visually contiguous and in the same range.
            //
            // For each run, the new visual index=sos+eos-old visual index;
            // we pre-add sos+eos into sumOfSosEos ->
            // new visual index=sumOfSosEos-old visual index;

            sumOfSosEos = start + limit - 1;

            // Reorder each index in the sequence
            do {
                indexMap[start] = sumOfSosEos - indexMap[start];
            } while (++start < limit);

            // start==limit
            if (limit == length) {
                break; // No more such sequences
            }
            else {
                start = limit + 1;
            }
        }
    } while (--maxLevel >= minLevel);
}

U_CAPI void U_EXPORT2
ubidi_reorderVisual(const UBiDiLevel *levels,
    int32_t length,
    int32_t *indexMap)
{
    UBiDiLevel minLevel = 0, maxLevel = 0;

    if ((indexMap == NULL) || (!prepareReorder(levels, length, indexMap, &minLevel, &maxLevel))) {
        return;
    }

    // Nothing to do?
    if ((minLevel == maxLevel) && ((minLevel & 1) == 0)) {
        return;
    }

    // Reorder only down to the lowest odd level
    minLevel |= 1;

    int32_t start, end, limit, temp;

    // Loop maxLevel..minLevel
    do {
        // Loop for all sequences of levels to reorder at the current maxLevel
        for (start = 0; start < length; )
        {
            // Look for a sequence of levels that are all at >=maxLevel
            // Look for the first index of such a sequence
            while (start < length && levels[start] < maxLevel) {
                ++start;
            }

            if (start >= length) {
                break; // No more such runs
            }

            // Look for the limit of such a sequence (the index behind it)
            for (limit = start; (++limit < length) && (levels[limit] >= maxLevel); )
            {
                // Do nothing.
            }

            // Swap the entire interval of indexes from start to limit-1.
            // We don't need to swap the levels for the purpose of this
            // algorithm: the sequence of levels that we look at does not
            // move anyway.

            end = limit - 1;
            while (start < end) {
                temp = indexMap[start];
                indexMap[start] = indexMap[end];
                indexMap[end] = temp;

                ++start;
                --end;
            }

            if (limit == length) {
                break; // No more such sequences
            }
            else {
                start = limit + 1;
            }
        }
    } while (--maxLevel >= minLevel);
}

/* API functions for logical<->visual mapping ------------------------------- */

U_CAPI int32_t U_EXPORT2
ubidi_getLogicalIndex(UBiDi *pBiDi,
    int32_t visualIndex,
    UErrorCode *pErrorCode)
{
    int32_t logicalIndex = UBIDI_MAP_NOWHERE;

    RETURN_IF_NULL_OR_FAILING_ERRCODE(pErrorCode, -1);
    RETURN_IF_NOT_VALID_PARA_OR_LINE(pBiDi, *pErrorCode, -1);
    RETURN_IF_BAD_RANGE(visualIndex, 0, pBiDi->resultLength, *pErrorCode, -1);

    UText *ut = &pBiDi->ut;

    // We can do the trivial cases without the runs array
    if ((pBiDi->insertPoints.size == 0) && (pBiDi->controlCount == 0)) {
        if (pBiDi->direction == UBIDI_LTR) {
            logicalIndex = visualIndex;
        }
        else if (pBiDi->direction == UBIDI_RTL) {
            logicalIndex = pBiDi->length - visualIndex - 1;
        }
    }

    if (logicalIndex == UBIDI_MAP_NOWHERE) {
        if (ubidi_getRuns(pBiDi, pErrorCode)) {
            Run *runs = pBiDi->runs;
            int32_t runCount = pBiDi->runCount;
            int32_t runIndex = 0;
            int32_t visualStart = 0;

            if (pBiDi->insertPoints.size > 0) {
                // Handle inserted LRM/RLM
                int32_t markFound = 0;
                int32_t runLength = 0;
                int32_t insertRemove = 0;

                // Subtract number of marks until visual index
                for (runIndex = 0; runIndex < runCount; runIndex++, visualStart += runLength)
                {
                    runLength = runs[runIndex].visualLimit - visualStart;
                    insertRemove = runs[runIndex].insertRemove;

                    if (insertRemove & (LRM_BEFORE | RLM_BEFORE)) {
                        if (visualIndex <= (visualStart + markFound)) {
                            visualIndex = UBIDI_MAP_NOWHERE;
                            break;
                        }

                        markFound++;
                    }

                    // Is adjusted visual index within this run?
                    if (visualIndex < (runs[runIndex].visualLimit + markFound)) {
                        visualIndex -= markFound;
                        break;
                    }

                    if (insertRemove & (LRM_AFTER | RLM_AFTER)) {
                        if (visualIndex == (visualStart + runLength + markFound)) {
                            visualIndex = UBIDI_MAP_NOWHERE;
                            break;
                        }

                        markFound++;
                    }
                }
            }
            else if (pBiDi->controlCount > 0) {
                // Handle removed BiDi control characters
                int32_t controlFound = 0;
                int32_t runLength = 0;
                int32_t insertRemove = 0;
                int32_t logicalStart = 0;
                UBool evenRun = FALSE;
                int32_t logicalEnd = 0;

                int32_t nativeStart = 0;
                UChar32 uchar = 0;
                int32_t nativeLimit = 0;

                // Add number of controls until visual index
                for (runIndex = 0; runIndex < runCount; runIndex++, visualStart += runLength)
                {
                    runLength = runs[runIndex].visualLimit - visualStart;
                    insertRemove = runs[runIndex].insertRemove;

                    // Is adjusted visual index beyond current run?
                    if (visualIndex >= (runs[runIndex].visualLimit - controlFound + insertRemove)) {
                        controlFound -= insertRemove;
                        continue;
                    }

                    // Adjusted visual index is within current run
                    if (insertRemove == 0) {
                        visualIndex += controlFound;
                        break;
                    }

                    // Count non-control chars until visualIndex
                    logicalStart = runs[runIndex].logicalStart;
                    evenRun = IS_EVEN_RUN(logicalStart);
                    REMOVE_ODD_BIT(logicalStart);
                    logicalEnd = logicalStart + runLength;

                    if (evenRun) {
                        UTEXT_SETNATIVEINDEX(ut, logicalStart);
                        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        uchar = UTEXT_NEXT32(ut);
                        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                    }
                    else {
                        UTEXT_SETNATIVEINDEX(ut, logicalEnd);
                        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        uchar = UTEXT_PREVIOUS32(ut);
                        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                    }
                    for (; (uchar != U_SENTINEL) && (nativeStart >= logicalStart) && (nativeStart < logicalEnd); )
                    {
                        if (IS_BIDI_CONTROL_CHAR(uchar)) {
                            controlFound += nativeLimit - nativeStart;
                        }

                        if ((visualIndex + controlFound) == (visualStart + (evenRun ? nativeStart - logicalStart : logicalEnd - nativeStart - 1))) {
                            break;
                        }

                        if (evenRun) {
                            nativeStart = nativeLimit;
                            uchar = UTEXT_NEXT32(ut);
                            nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        }
                        else {
                            nativeLimit = nativeStart;
                            uchar = UTEXT_PREVIOUS32(ut);
                            nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        }
                    }

                    visualIndex += controlFound;
                    break;
                }
            }

            // Handle all cases
            if (visualIndex != UBIDI_MAP_NOWHERE) {
                if (runCount <= 10) {
                    // Linear search for the run
                    for (runIndex = 0; visualIndex >= runs[runIndex].visualLimit; runIndex++)
                    {
                        // Do nothing.
                    }
                }
                else {
                    // Binary search for the run
                    int32_t begin = 0;
                    int32_t limit = 0;

                    // The middle if() is guaranteed to find the run, we don't need a loop limit
                    for (begin = 0, limit = runCount; ; )
                    {
                        runIndex = (begin + limit) / 2;

                        if (visualIndex >= runs[runIndex].visualLimit) {
                            begin = runIndex + 1;
                        }
                        else if ((runIndex == 0) || (visualIndex >= runs[runIndex - 1].visualLimit)) {
                            break;
                        }
                        else {
                            limit = runIndex;
                        }
                    }
                }

                if (runIndex < runCount) {
                    int32_t logicalStart = GET_INDEX(runs[runIndex].logicalStart);

                    if (IS_EVEN_RUN(runs[runIndex].logicalStart)) {
                        // LTR

                        // The offset in runs[runIndex] is visualIndex-runs[runIndex - 1].visualLimit
                        if (runIndex > 0) {
                            visualIndex -= runs[runIndex - 1].visualLimit;
                        }

                        logicalIndex = logicalStart + visualIndex;
                    }
                    else {
                        // RTL
                        logicalIndex = logicalStart + runs[runIndex].visualLimit - visualIndex - 1;
                    }
                }
            }
        }
        else {
            *pErrorCode = U_MEMORY_ALLOCATION_ERROR;
        }
    }

    return logicalIndex;
}

U_CAPI int32_t U_EXPORT2
ubidi_getVisualIndex(UBiDi *pBiDi,
    int32_t logicalIndex,
    UErrorCode *pErrorCode)
{
    int32_t visualIndex = UBIDI_MAP_NOWHERE;

    RETURN_IF_NULL_OR_FAILING_ERRCODE(pErrorCode, -1);
    RETURN_IF_NOT_VALID_PARA_OR_LINE(pBiDi, *pErrorCode, -1);
    RETURN_IF_BAD_RANGE(logicalIndex, 0, pBiDi->length, *pErrorCode, -1);

    UText *ut = &pBiDi->ut;

    // We can do the trivial cases without the runs array
    switch (pBiDi->direction) {
    case UBIDI_LTR:
        visualIndex = logicalIndex;
        break;
    case UBIDI_RTL:
        visualIndex = pBiDi->length - logicalIndex - 1;
        break;
    default:
        if (ubidi_getRuns(pBiDi, pErrorCode)) {
            Run *runs = pBiDi->runs;
            int32_t runCount = pBiDi->runCount;
            int32_t runIndex = 0;
            int32_t visualStart = 0;
            int32_t logicalStart = 0;
            int32_t runLength = 0;
            int32_t offset = 0;

            // Linear search for the run, search on the visual runs
            for (runIndex = 0; runIndex < runCount; runIndex++)
            {
                logicalStart = GET_INDEX(runs[runIndex].logicalStart);
                runLength = runs[runIndex].visualLimit - visualStart;
                offset = logicalIndex - logicalStart;

                if ((offset >= 0) && (offset < runLength)) {
                    if (IS_EVEN_RUN(runs[runIndex].logicalStart)) {
                        // LTR
                        visualIndex = visualStart + offset;
                    }
                    else {
                        // RTL
                        visualIndex = (visualStart + runLength - 1) - offset;
                    }

                    break;
                }

                visualStart += runLength;
            }

            if (runIndex >= pBiDi->runCount) {
                visualIndex = UBIDI_MAP_NOWHERE;
            }
        }
        else {
            *pErrorCode = U_MEMORY_ALLOCATION_ERROR;
        }
    }

    if ((!U_FAILURE(*pErrorCode)) && (visualIndex != UBIDI_MAP_NOWHERE)) {
        Run *runs = pBiDi->runs;
        int32_t runCount = pBiDi->runCount;
        int32_t runIndex = 0;
        int32_t visualStart = 0;

        if (pBiDi->insertPoints.size > 0) {
            // Add the number of added marks until the calculated visual index
            int32_t markFound = 0;
            int32_t runLength = 0;
            int32_t insertRemove = 0;

            for (runIndex = 0; runIndex < runCount; runIndex++, visualStart += runLength)
            {
                runLength = runs[runIndex].visualLimit - visualStart;
                insertRemove = runs[runIndex].insertRemove;

                if (insertRemove & (LRM_BEFORE | RLM_BEFORE)) {
                    markFound++;
                }

                // Is it the run containing the visual index?
                if (visualIndex < runs[runIndex].visualLimit) {
                    visualIndex += markFound;
                    break;
                }

                if (insertRemove & (LRM_AFTER | RLM_AFTER)) {
                    markFound++;
                }
            }
        }
        else if (pBiDi->controlCount > 0) {
            // Subtract the number of controls until the calculated visual index
            int32_t controlFound = 0;
            int32_t runLength = 0;
            int32_t insertRemove = 0;

            UTEXT_SETNATIVEINDEX(ut, logicalIndex);
            int32_t nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
            UChar32 uchar = UTEXT_NEXT32(ut);
            int32_t nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);

            // Is the logical index pointing to a control?
            if (!IS_BIDI_CONTROL_CHAR(uchar)) {
                for (runIndex = 0; runIndex < runCount; runIndex++, visualStart += runLength)
                {
                    runLength = runs[runIndex].visualLimit - visualStart;
                    insertRemove = runs[runIndex].insertRemove;

                    // Calculated visual index is beyond this run?
                    if (visualIndex >= runs[runIndex].visualLimit) {
                        controlFound -= insertRemove;
                        continue;
                    }

                    // Calculated visual index must be within current run
                    if (insertRemove == 0) {
                        visualIndex -= controlFound;
                        break;
                    }

                    int32_t start = 0;
                    int32_t limit = 0;

                    if (IS_EVEN_RUN(runs[runIndex].logicalStart)) {
                        // LTR: check from run start to logical index
                        start = runs[runIndex].logicalStart;
                        limit = logicalIndex;
                    }
                    else {
                        // RTL: check from logical index to run end
                        start = logicalIndex + 1;
                        limit = GET_INDEX(runs[runIndex].logicalStart) + runLength;
                    }

                    UTEXT_SETNATIVEINDEX(ut, start);
                    nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                    uchar = UTEXT_NEXT32(ut);
                    nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                    for (; (uchar != U_SENTINEL) && (nativeLimit <= limit);
                        nativeStart = nativeLimit, uchar = UTEXT_NEXT32(ut), nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut))
                    {
                        if (IS_BIDI_CONTROL_CHAR(uchar)) {
                            controlFound += nativeLimit - nativeStart;
                        }
                    }

                    visualIndex -= controlFound;
                    break;
                }
            }
            else {
                visualIndex = UBIDI_MAP_NOWHERE;
            }
        }
    }

    return visualIndex;
}

U_CAPI void U_EXPORT2
ubidi_getLogicalMap(UBiDi *pBiDi,
    int32_t *indexMap,
    UErrorCode *pErrorCode)
{
    RETURN_VOID_IF_NULL_OR_FAILING_ERRCODE(pErrorCode);

    if (indexMap == NULL) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    UText *ut = &pBiDi->ut;

    // ubidi_countRuns() checks for VALID_PARA_OR_LINE
    ubidi_countRuns(pBiDi, pErrorCode);
    if (!U_FAILURE(*pErrorCode)) {
        // Fill a logical-to-visual index map using the runs[]

        if (pBiDi->length > 0) {
            uprv_memset(indexMap, UBIDI_MAP_NOWHERE, (pBiDi->resultLength > pBiDi->length ? pBiDi->resultLength : pBiDi->length) * sizeof(int32_t));

            Run *runs = pBiDi->runs;
            int32_t runCount = pBiDi->runCount;
            int32_t runIndex = 0;
            int32_t visualStart = 0;
            int32_t logicalStart = 0;
            int32_t visualLimit = 0;

            for (runIndex = 0; runIndex < runCount; runIndex++)
            {
                logicalStart = GET_INDEX(runs[runIndex].logicalStart);
                visualLimit = runs[runIndex].visualLimit;

                if (IS_EVEN_RUN(runs[runIndex].logicalStart)) {
                    do { // LTR
                        indexMap[logicalStart++] = visualStart++;
                    } while (visualStart < visualLimit);
                }
                else {
                    logicalStart += visualLimit - visualStart; // logicalLimit
                    do { // RTL
                        indexMap[--logicalStart] = visualStart++;
                    } while (visualStart < visualLimit);
                }

                //U_ASSERT(visualStart == visualLimit);
            }

            if (pBiDi->insertPoints.size > 0) {
                int32_t markFound = 0;
                int32_t runLength = 0;
                int32_t insertRemove = 0;
                int32_t logicalEnd = 0;
                int32_t i = 0;

                visualStart = 0;

                // Add number of marks found until each index
                for (runIndex = 0; runIndex < runCount; runIndex++, visualStart += runLength)
                {
                    runLength = runs[runIndex].visualLimit - visualStart;
                    insertRemove = runs[runIndex].insertRemove;

                    if (insertRemove & (LRM_BEFORE | RLM_BEFORE)) {
                        markFound++;
                    }

                    if (markFound > 0) {
                        logicalStart = GET_INDEX(runs[runIndex].logicalStart);
                        logicalEnd = logicalStart + runLength;
                        i = 0;

                        for (i = logicalStart; i < logicalEnd; i++)
                        {
                            indexMap[i] += markFound;
                        }
                    }

                    if (insertRemove & (LRM_AFTER | RLM_AFTER)) {
                        markFound++;
                    }
                }
            }
            else if (pBiDi->controlCount > 0) {
                int32_t controlFound = 0;
                int32_t runLength = 0;
                int32_t insertRemove = 0;
                int32_t logicalEnd = 0;
                UBool evenRun = FALSE;
                int32_t i = 0;

                int32_t nativeStart = 0;
                UChar32 uchar = 0;
                int32_t nativeLimit = 0;

                visualStart = 0;

                // Subtract number of controls found until each index
                for (runIndex = 0; runIndex < runCount; runIndex++, visualStart += runLength)
                {
                    runLength = runs[runIndex].visualLimit - visualStart;
                    insertRemove = runs[runIndex].insertRemove;

                    // No control found within previous runs nor within this run
                    if ((controlFound - insertRemove) == 0) {
                        continue;
                    }

                    logicalStart = runs[runIndex].logicalStart;
                    evenRun = IS_EVEN_RUN(logicalStart);
                    REMOVE_ODD_BIT(logicalStart);
                    logicalEnd = logicalStart + runLength;

                    // If no control within this run
                    if (insertRemove == 0) {
                        for (i = logicalStart; i < logicalEnd; i++)
                        {
                            indexMap[i] -= controlFound;
                        }

                        continue;
                    }

                    if (evenRun) {
                        UTEXT_SETNATIVEINDEX(ut, logicalStart);
                        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        uchar = UTEXT_NEXT32(ut);
                        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                    }
                    else {
                        UTEXT_SETNATIVEINDEX(ut, logicalEnd);
                        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        uchar = UTEXT_PREVIOUS32(ut);
                        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                    }
                    for (; (uchar != U_SENTINEL) && (nativeStart >= logicalStart) && (nativeStart < logicalEnd); )
                    {
                        if (IS_BIDI_CONTROL_CHAR(uchar)) {
                            controlFound += nativeLimit - nativeStart;

                            for (i = nativeStart; i < nativeLimit; i++)
                            {
                                indexMap[nativeStart] = UBIDI_MAP_NOWHERE;
                            }
                        }
                        else {
                            for (i = nativeStart; i < nativeLimit; i++)
                            {
                                indexMap[nativeStart] -= controlFound;
                            }
                        }

                        if (evenRun) {
                            nativeStart = nativeLimit;
                            uchar = UTEXT_NEXT32(ut);
                            nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        }
                        else {
                            nativeLimit = nativeStart;
                            uchar = UTEXT_PREVIOUS32(ut);
                            nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        }
                    }
                }
            }
        }
    }
}

U_CAPI void U_EXPORT2
ubidi_getVisualMap(UBiDi *pBiDi,
    int32_t *indexMap,
    UErrorCode *pErrorCode)
{
    RETURN_VOID_IF_NULL_OR_FAILING_ERRCODE(pErrorCode);

    if (indexMap == NULL) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    UText *ut = &pBiDi->ut;

    // ubidi_countRuns() checks for VALID_PARA_OR_LINE
    ubidi_countRuns(pBiDi, pErrorCode);
    if (U_SUCCESS(*pErrorCode)) {
        // Fill a visual-to-logical index map using the runs[]

        if (pBiDi->resultLength > 0) {
            uprv_memset(indexMap, UBIDI_MAP_NOWHERE, (pBiDi->resultLength > pBiDi->length ? pBiDi->resultLength : pBiDi->length) * sizeof(int32_t));

            Run *runs = pBiDi->runs;
            int32_t runCount = pBiDi->runCount;
            int32_t runIndex = 0;
            int32_t visualStart = 0;
            int32_t logicalStart = 0;
            int32_t visualLimit = 0;

            for (runIndex = 0; runIndex < runCount; runIndex++)
            {
                logicalStart = runs[runIndex].logicalStart;
                visualLimit = runs[runIndex].visualLimit;

                if (IS_EVEN_RUN(logicalStart)) {
                    do { // LTR
                        indexMap[visualStart++] = logicalStart++;
                    } while (visualStart < visualLimit);
                }
                else {
                    REMOVE_ODD_BIT(logicalStart);
                    logicalStart += visualLimit - visualStart; // logicalLimit
                    do { // RTL
                        indexMap[visualStart++] = --logicalStart;
                    } while (visualStart < visualLimit);
                }

                //U_ASSERT(visualStart == visualLimit);
            }

            int32_t lookVisualMap[2000];
            memcpy(lookVisualMap, indexMap, pBiDi->resultLength * sizeof(int32_t));

            if (pBiDi->insertPoints.size > 0) {
                int32_t markFound = 0;
                int32_t mapIndex = 0;
                int32_t insertRemove = 0;
                int32_t i = 0;

                // Count all inserted marks
                for (runIndex = 0; runIndex < runCount; runIndex++)
                {
                    insertRemove = runs[runIndex].insertRemove;

                    if (insertRemove & (LRM_BEFORE | RLM_BEFORE)) {
                        markFound++;
                    }

                    if (insertRemove & (LRM_AFTER | RLM_AFTER)) {
                        markFound++;
                    }
                }

                // Move back indexes by number of preceding marks
                mapIndex = pBiDi->resultLength;
                for (runIndex = runCount - 1; (runIndex >= 0) && (markFound > 0); runIndex--)
                {
                    insertRemove = runs[runIndex].insertRemove;

                    if (insertRemove & (LRM_AFTER | RLM_AFTER)) {
                        indexMap[--mapIndex] = UBIDI_MAP_NOWHERE;
                        markFound--;
                    }

                    visualStart = runIndex > 0 ? runs[runIndex - 1].visualLimit : 0;

                    for (i = runs[runIndex].visualLimit - 1; (i >= visualStart) && (markFound > 0); i--)
                    {
                        indexMap[--mapIndex] = indexMap[i];
                    }

                    if (insertRemove & (LRM_BEFORE | RLM_BEFORE)) {
                        indexMap[--mapIndex] = UBIDI_MAP_NOWHERE;
                        markFound--;
                    }
                }
            }
            else if (pBiDi->controlCount > 0) {
                int32_t mapIndex = 0;
                int32_t runLength = 0;
                int32_t insertRemove = 0;
                UBool evenRun = FALSE;
                int32_t logicalEnd = 0;
                int32_t i = 0;

                int32_t nativeStart = 0;
                UChar32 uchar = 0;
                int32_t nativeLimit = 0;

                visualStart = 0;

                // Move forward indexes by number of preceding controls
                for (runIndex = 0; runIndex < runCount; runIndex++, visualStart += runLength)
                {
                    runLength = runs[runIndex].visualLimit - visualStart;
                    insertRemove = runs[runIndex].insertRemove;

                    // If no control found yet, nothing to do in this run
                    if ((insertRemove == 0) && (mapIndex == visualStart)) {
                        mapIndex += runLength;
                        continue;
                    }

                    // If no control in this run
                    if (insertRemove == 0) {
                        visualLimit = runs[runIndex].visualLimit;

                        for (i = visualStart; i < visualLimit; i++)
                        {
                            indexMap[mapIndex++] = indexMap[i];
                        }

                        continue;
                    }

                    logicalStart = runs[runIndex].logicalStart;
                    evenRun = IS_EVEN_RUN(logicalStart);
                    REMOVE_ODD_BIT(logicalStart);
                    logicalEnd = logicalStart + runLength;

                    if (evenRun) {
                        UTEXT_SETNATIVEINDEX(ut, logicalStart);
                        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        uchar = UTEXT_NEXT32(ut);
                        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                    }
                    else {
                        UTEXT_SETNATIVEINDEX(ut, logicalEnd);
                        nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        uchar = UTEXT_PREVIOUS32(ut);
                        nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                    }

                    for (; (uchar != U_SENTINEL) && (nativeStart >= logicalStart) && (nativeStart < logicalEnd); )
                    {
                        if (!IS_BIDI_CONTROL_CHAR(uchar)) {
                            for (i = nativeStart; i < nativeLimit; i++)
                            {
                                indexMap[mapIndex++] = nativeStart;
                            }
                        }

                        if (evenRun) {
                            nativeStart = nativeLimit;
                            uchar = UTEXT_NEXT32(ut);
                            nativeLimit = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        }
                        else {
                            nativeLimit = nativeStart;
                            uchar = UTEXT_PREVIOUS32(ut);
                            nativeStart = (int32_t)UTEXT_GETNATIVEINDEX(ut);
                        }
                    }
                }
            }
        }
    }
}

U_CAPI void U_EXPORT2
ubidi_invertMap(const int32_t *srcMap,
    int32_t *destMap,
    int32_t length)
{
    if (srcMap != NULL && destMap != NULL && length > 0) {
        int32_t destLength = -1;
        int32_t count = 0;
        int32_t i = 0;

        // Find highest value and count positive indexes in srcMap
        i = length;
        while (i > 0) {
            if (srcMap[--i] > destLength) {
                destLength = srcMap[i];
            }
            if (srcMap[i] >= 0) {
                count++;
            }
        }

        // Add 1 for origin 0
        destLength++;

        if (count < destLength) {
            // Fill unmatched destMap entries with -1
            uprv_memset(destMap, UBIDI_MAP_NOWHERE, destLength * sizeof(int32_t));
        }

        i = length;
        while (length > 0) {
            if (srcMap[--i] >= 0) {
                destMap[srcMap[i]] = --length;
            }
            else {
                length--;
            }
        }
    }
}
