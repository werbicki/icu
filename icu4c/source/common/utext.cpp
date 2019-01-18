// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2005-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  utext.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created:    2005-04-12, Markus W. Scherer
*   modified:   2018-11-22, Paul Werbicki
*/

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "unicode/unistr.h"
#include "unicode/chariter.h"
#include "unicode/utext.h"
#include "unicode/utf.h"
#include "unicode/utf8.h"
#include "unicode/utf16.h"
#include "ustr_imp.h"
#include "cmemory.h"
#include "cstring.h"
#include "uassert.h"
#include "putilimp.h"

U_NAMESPACE_USE

// UText.flags bit definitions.
enum {
    //  1 if ICU has allocated this UText struct on the heap.
    //  0 if caller provided storage for the UText.
    UTEXT_HEAP_ALLOCATED = 1,

    //  1 if ICU has allocated extra storage as a separate
    //     heap block.
    //  0 if there is no separate allocation.  Either no extra
    //     storage was requested, or it is appended to the end
    //     of the main UText storage.
    UTEXT_EXTRA_HEAP_ALLOCATED = 2,
};

#define I32_FLAG(bitIndex) ((int32_t)1<<(bitIndex))

#define BC_AS_I64(ut) *((int64_t*)(&ut->b))

//------------------------------------------------------------------------------
//
// UText common functions implementation
//
//------------------------------------------------------------------------------

U_CAPI UBool U_EXPORT2
utext_isValid(const UText *ut)
{
    UBool result = (ut) && (ut->magic == UTEXT_MAGIC) && (ut->pFuncs);
    return result;
}

static UBool U_CALLCONV
utext_access(UText *ut, int64_t index, UBool forward)
{
    UBool result = FALSE;
    if ((utext_isValid(ut)) && (ut->pFuncs->access))
        result = ut->pFuncs->access(ut, index, forward);
    return result;
}

static int64_t U_CALLCONV
utext_mapOffsetToNative(const UText *ut)
{
    int64_t nativeIndex = 0;
    if ((utext_isValid(ut)) && (ut->pFuncs->mapOffsetToNative))
        nativeIndex = ut->pFuncs->mapOffsetToNative(ut);
    return nativeIndex;
}

static int32_t U_CALLCONV
utext_mapNativeIndexToUTF16(const UText *ut, int64_t nativeIndex)
{
    int32_t chunkOffset = 0;
    if ((utext_isValid(ut)) && (ut->pFuncs->mapNativeIndexToUTF16))
        chunkOffset = ut->pFuncs->mapNativeIndexToUTF16(ut, nativeIndex);
    return chunkOffset;
}

U_CAPI UBool U_EXPORT2
utext_isLengthExpensive(const UText *ut)
{
    UBool result = FALSE;
    if (utext_isValid(ut))
        result = (ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) != 0;
    return result;
}

U_CAPI int64_t U_EXPORT2
utext_nativeLength(UText *ut)
{
    int64_t nativeLength = 0;
    if ((utext_isValid(ut)) && (ut->pFuncs->nativeLength))
        nativeLength = ut->pFuncs->nativeLength(ut);
    return nativeLength;
}

U_CAPI int64_t U_EXPORT2
utext_getNativeIndex(const UText *ut)
{
    int64_t nativeIndex = 0;
    if (utext_isValid(ut)) {
        if (ut->chunkOffset <= ut->nativeIndexingLimit) {
            // Desired nativeIndex is in the current chunk, with direct 1:1 native to UTF16 indexing.
            nativeIndex = ut->chunkNativeStart + ut->chunkOffset;
        }
        else {
            // Desired nativeIndex is in the current chunk, with non-UTF16 indexing.
            nativeIndex = utext_mapOffsetToNative(ut);
        }
    }
    return nativeIndex;
}

U_CAPI void U_EXPORT2
utext_setNativeIndex(UText *ut, int64_t nativeIndex)
{
    if (utext_isValid(ut)) {
        UBool haveAccess = TRUE;
        if ((nativeIndex < ut->chunkNativeStart) || (nativeIndex >= ut->chunkNativeLimit)) {
            // The desired position is outside of the current chunk.
            //
            // Access the new position. Assume a forward iteration from here,
            // which will also be optimimum for a single random access.
            //
            // Reverse iterations may suffer slightly.
            haveAccess = utext_access(ut, nativeIndex, TRUE);
        }
        else if ((int32_t)(nativeIndex - ut->chunkNativeStart) <= ut->nativeIndexingLimit) {
            // Desired nativeIndex is in the current chunk, with direct 1:1 native to UTF16 indexing.
            ut->chunkOffset = (int32_t)(nativeIndex - ut->chunkNativeStart);
        }
        else {
            // Desired nativeIndex is in the current chunk, with non-UTF16 indexing.
            ut->chunkOffset = utext_mapNativeIndexToUTF16(ut, nativeIndex);
        }
        if (haveAccess) {
            // The convention is that the index must always be on a code point boundary.
            // Adjust the index position if it is in the middle of a surrogate pair.
            if (ut->chunkOffset < ut->chunkLength) {
                UChar c = ut->chunkContents[ut->chunkOffset];
                if (U16_IS_TRAIL(c)) {
                    if (ut->chunkOffset == 0) {
                        haveAccess = utext_access(ut, ut->chunkNativeStart, FALSE);
                    }
                    if ((haveAccess) && (ut->chunkOffset > 0)) {
                        UChar lead = ut->chunkContents[ut->chunkOffset - 1];
                        if (U16_IS_LEAD(lead)) {
                            ut->chunkOffset--;
                        }
                    }
                }
            }
        }
    }
}

U_CAPI int64_t U_EXPORT2
utext_getPreviousNativeIndex(UText *ut)
{
    int64_t nativeIndex = 0;
    if (utext_isValid(ut)) {
        // Fast-path the common case.
        //
        // Common means current position is not at the beginning of a chunk
        // and the preceding character is not supplementary.
        int32_t i = ut->chunkOffset - 1;
        UChar c = (UChar)U_SENTINEL;
        if (i >= 0) {
            c = ut->chunkContents[i];
            if (!U16_IS_TRAIL(c)) {
                if (i <= ut->nativeIndexingLimit) {
                    nativeIndex = ut->chunkNativeStart + i;
                }
                else {
                    ut->chunkOffset = i;
                    nativeIndex = utext_mapOffsetToNative(ut);
                    ut->chunkOffset++;
                }
            }
        }
        if ((i < 0) || (U16_IS_TRAIL(c))) {
            // If at the start of text, simply return 0.
            if ((ut->chunkOffset != 0) || (ut->chunkNativeStart != 0)) {
                // Harder, less common cases.  We are at a chunk boundary, or on a surrogate.
                // Keep it simple, use other functions to handle the edges.
                utext_previous32(ut);
                nativeIndex = UTEXT_GETNATIVEINDEX(ut);
                utext_next32(ut);
            }
        }
    }
    return nativeIndex;
}

U_CAPI UBool U_EXPORT2
utext_moveIndex32(UText *ut, int32_t delta)
{
    UBool result = TRUE;
    if (ut) {
        UChar32 c;
        if (delta > 0) {
            for (; delta > 0; delta--) {
                if (ut->chunkOffset >= ut->chunkLength && !utext_access(ut, ut->chunkNativeLimit, TRUE)) {
                    result = FALSE;
                    break;
                }
                c = ut->chunkContents[ut->chunkOffset];
                if (U16_IS_SURROGATE(c)) {
                    c = utext_next32(ut);
                    if (c == U_SENTINEL) {
                        result = FALSE;
                        break;
                    }
                }
                else {
                    ut->chunkOffset++;
                }
            }
        }
        else if (delta < 0) {
            for (; delta < 0; delta++) {
                if (ut->chunkOffset <= 0 && !utext_access(ut, ut->chunkNativeStart, FALSE)) {
                    result = FALSE;
                    break;
                }
                c = ut->chunkContents[ut->chunkOffset - 1];
                if (U16_IS_SURROGATE(c)) {
                    c = utext_previous32(ut);
                    if (c == U_SENTINEL) {
                        result = FALSE;
                        break;
                    }
                }
                else {
                    ut->chunkOffset--;
                }
            }
        }
    }
    return result;
}

U_CAPI UChar32 U_EXPORT2
utext_current32(UText *ut)
{
    UChar32 c = U_SENTINEL;
    if (utext_isValid(ut)) {
        UBool haveAccess = TRUE;
        if (ut->chunkOffset == ut->chunkLength) {
            // Current position is just off the end of the chunk.
            haveAccess = utext_access(ut, ut->chunkNativeLimit, TRUE);
        }
        // If end of the text, return sentinel.
        if (haveAccess) {
            c = ut->chunkContents[ut->chunkOffset];
            // If c is not a lead character we have a normal case, not supplementary, return c.
            if (U16_IS_LEAD(c)) {
                // Otherwise, possible supplementary char.
                UChar32 trail = 0;
                UChar32 supplementary = c;
                if ((ut->chunkOffset + 1) < ut->chunkLength) {
                    // The trail surrogate is in the same chunk.
                    trail = ut->chunkContents[ut->chunkOffset + 1];
                }
                else {
                    // The trail surrogate is in a different chunk.
                    // 
                    // Because we must maintain the iteration position, we need to switch forward
                    // into the new chunk, get the trail surrogate, then revert the chunk back to the
                    // original one.
                    // 
                    // An edge case to be careful of: the entire text may end with an unpaired
                    // leading surrogate. The attempt to access the trail will fail, but
                    // the original position before the unpaired lead still needs to be restored.
                    int64_t nativePosition = ut->chunkNativeLimit;
                    int32_t originalOffset = ut->chunkOffset;
                    if (utext_access(ut, nativePosition, TRUE)) {
                        trail = ut->chunkContents[ut->chunkOffset];
                    }
                    haveAccess = utext_access(ut, nativePosition, FALSE); // Reverse iteration flag loads preceding chunk
                    U_ASSERT(haveAccess == TRUE);
                    ut->chunkOffset = originalOffset;
                    if (!haveAccess) {
                        c = U_SENTINEL;
                    }
                }
                if (haveAccess) {
                    if (U16_IS_TRAIL(trail)) {
                        supplementary = U16_GET_SUPPLEMENTARY(c, trail);
                    }
                    c = supplementary;
                }
            }
        }
    }
    return c;
}

U_CAPI UChar32 U_EXPORT2
utext_next32(UText *ut)
{
    UChar32 c = U_SENTINEL;
    if (utext_isValid(ut)) {
        UBool haveAccess = TRUE;
        if (ut->chunkOffset >= ut->chunkLength) {
            haveAccess = utext_access(ut, ut->chunkNativeLimit, TRUE);
        }
        // If end of the text, return sentinel.
        if (haveAccess) {
            c = ut->chunkContents[ut->chunkOffset++];
            // If c is not a lead character we have a normal case, not supplementary, return c.
            // (A lead surrogate seen here is just returned as is, as a surrogate value.
            // It cannot be part of a pair).
            if (U16_IS_LEAD(c)) {
                if (ut->chunkOffset >= ut->chunkLength) {
                    haveAccess = utext_access(ut, ut->chunkNativeLimit, TRUE);
                }
                // If there is no access, c is an unpaired lead surrogate at the
                // end of the text, return c.
                // c is Unpaired.
                if (haveAccess) {
                    UChar32 trail = ut->chunkContents[ut->chunkOffset];
                    // If c was an unpaired lead surrogate, not at the end of the text,
                    // return c.
                    // c is Unpaired.
                    //
                    // Iteration position is on the following character, possibly in the
                    // next chunk, where the trail surrogate would have been if it had existed.
                    if (U16_IS_TRAIL(trail)) {
                        // Otherwise, full supplementary character.
                        //
                        // Move iteration position over the trail surrogate.
                        UChar32 supplementary = U16_GET_SUPPLEMENTARY(c, trail);
                        ut->chunkOffset++;
                        c = supplementary;
                    }
                }
            }
        }
    }
    return c;
}

U_CAPI UChar32 U_EXPORT2
utext_previous32(UText *ut)
{
    UChar32 c = U_SENTINEL;
    if (ut) {
        UBool haveAccess = TRUE;
        if (ut->chunkOffset <= 0) {
            haveAccess = utext_access(ut, ut->chunkNativeStart, FALSE);
        }
        // If start of the text, return sentinel.
        if (haveAccess) {
            ut->chunkOffset--;
            c = ut->chunkContents[ut->chunkOffset];
            // If c is not a trail character we have a normal case, not supplementary, return c.
            // (A lead surrogate seen here is just returned as is, as a surrogate value.
            // It cannot be part of a pair).
            if (U16_IS_TRAIL(c)) {
                if (ut->chunkOffset <= 0)
                    haveAccess = utext_access(ut, ut->chunkNativeStart, FALSE);
                // If we have no access, c was an unpaired trail surrogate, at the start of
                // the text, return c.
                // c is Unpaired.
                if (haveAccess) {
                    UChar32 lead = ut->chunkContents[ut->chunkOffset - 1];
                    // If c was an unpaired trail surrogate, not at the end of the text, return c.
                    // c is Unpaired.
                    // 
                    // Iteration position is at c.
                    if (U16_IS_LEAD(lead)) {
                        // Otherwise, full supplementary character.
                        // 
                        // Move iteration position over the lead surrogate.
                        UChar32 supplementary = U16_GET_SUPPLEMENTARY(lead, c);
                        ut->chunkOffset--;
                        c = supplementary;
                    }
                }
            }
        }
    }
    return c;
}

U_CAPI UChar32 U_EXPORT2
utext_next32From(UText *ut, int64_t nativeIndex)
{
    UChar32 c = U_SENTINEL;
    if (ut) {
        UBool haveAccess = TRUE;
        if ((nativeIndex < ut->chunkNativeStart) || (nativeIndex >= ut->chunkNativeLimit)) {
            // Desired nativeIndex is outside of the current chunk.
            haveAccess = utext_access(ut, nativeIndex, TRUE);
        }
        else if (nativeIndex - ut->chunkNativeStart <= (int64_t)ut->nativeIndexingLimit) {
            // Desired nativeIndex is in the current chunk, with direct 1:1 native to UTF16 indexing.
            ut->chunkOffset = (int32_t)(nativeIndex - ut->chunkNativeStart);
        }
        else {
            // Desired nativeIndex is in the current chunk, with non-UTF16 indexing.
            ut->chunkOffset = utext_mapNativeIndexToUTF16(ut, nativeIndex);
        }
        if (haveAccess) {
            // Simple case with no surrogates.
            c = ut->chunkContents[ut->chunkOffset++];
            if (U16_IS_SURROGATE(c)) {
                // Possible supplementary. Many edge cases.
                // Let other functions do the heavy lifting.
                utext_setNativeIndex(ut, nativeIndex);
                c = utext_next32(ut);
            }
        }
    }
    return c;
}

U_CAPI UChar32 U_EXPORT2
utext_previous32From(UText *ut, int64_t nativeIndex)
{
    //  Return the character preceding the specified index.
    //  Leave the iteration position at the start of the character that was returned.
    //
    // The character preceding cCurr, which is what we will return.
    UChar32 c = U_SENTINEL;
    if (utext_isValid(ut)) {
        UBool haveAccess = TRUE;
        // Address the chunk containg the position preceding the incoming index.
        // A tricky edge case:
        //  We try to test the requested native index against the chunkNativeStart to determine
        //  whether the character preceding the one at the index is in the current chunk.
        //  BUT, this test can fail with UTF-8 (or any other multibyte encoding), when the
        //  requested index is on something other than the first position of the first char.
        //
        if ((nativeIndex <= ut->chunkNativeStart) || (nativeIndex > ut->chunkNativeLimit)) {
            // Desired nativeIndex is outside of the current chunk.
            haveAccess = utext_access(ut, nativeIndex, FALSE);
        }
        else if (nativeIndex - ut->chunkNativeStart <= (int64_t)ut->nativeIndexingLimit) {
            // Desired nativeIndex is in the current chunk, with direct 1:1 native to UTF16 indexing.
            ut->chunkOffset = (int32_t)(nativeIndex - ut->chunkNativeStart);
        }
        else {
            // Desired nativeIndex is in the current chunk, with non-UTF16 indexing.
            ut->chunkOffset = utext_mapNativeIndexToUTF16(ut, nativeIndex);
            if (ut->chunkOffset == 0) {
                haveAccess = utext_access(ut, nativeIndex, FALSE);
            }
        }
        if ((haveAccess) && (ut->chunkOffset > 0)) {
            // Simple case with no surrogates.
            ut->chunkOffset--;
            c = ut->chunkContents[ut->chunkOffset];
            if (U16_IS_SURROGATE(c)) {
                // Possible supplementary. Many edge cases.
                // Let other functions do the heavy lifting.
                utext_setNativeIndex(ut, nativeIndex);
                c = utext_previous32(ut);
            }
        }
    }
    return c;
}

U_CAPI UChar32 U_EXPORT2
utext_char32At(UText *ut, int64_t nativeIndex)
{
    UChar32 c = U_SENTINEL;
    if (utext_isValid(ut)) {
        if ((nativeIndex >= ut->chunkNativeStart) && (nativeIndex < ut->chunkNativeStart + ut->nativeIndexingLimit)) {
            // Desired nativeIndex is in the current chunk, with direct 1:1 native to UTF16 indexing.
            ut->chunkOffset = (int32_t)(nativeIndex - ut->chunkNativeStart);
            c = ut->chunkContents[ut->chunkOffset];
        }
        if ((U16_IS_SURROGATE(c)) ||
            (nativeIndex < ut->chunkNativeStart) ||
            (nativeIndex >= ut->chunkNativeStart + ut->nativeIndexingLimit)) {
            // Desired nativeIndex is outside of the current chunk, or desired nativeIndex is in the current
            // chunk, but with non-UTF16 indexing.
            utext_setNativeIndex(ut, nativeIndex);
            if (nativeIndex >= ut->chunkNativeStart && ut->chunkOffset < ut->chunkLength) {
                // Simple case with no surrogates.
                c = ut->chunkContents[ut->chunkOffset];
                if (U16_IS_SURROGATE(c)) {
                    // Possible supplementary. Many edge cases.
                    // Let other functions do the heavy lifting.
                    c = utext_current32(ut);
                }
            }
        }
    }
    return c;
}

U_CAPI int32_t U_EXPORT2
utext_extract(UText *ut,
    int64_t start, int64_t limit,
    UChar *dest, int32_t destCapacity,
    UErrorCode *status)
{
    int32_t length = 0;
    if ((ut) && (ut->pFuncs->extract))
        length = ut->pFuncs->extract(ut, start, limit, dest, destCapacity, status);
    return length;
}

U_CAPI UBool U_EXPORT2
utext_isWritable(const UText *ut)
{
    UBool result = FALSE;
    if (ut)
        result = (ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE)) != 0;
    return result;
}

U_CAPI void U_EXPORT2
utext_freeze(UText *ut) {
    // Zero out the WRITABLE flag.
    if (ut)
        ut->providerProperties &= ~(I32_FLAG(UTEXT_PROVIDER_WRITABLE));
}

U_CAPI UBool U_EXPORT2
utext_hasMetaData(const UText *ut) {
    UBool result = FALSE;
    if (ut)
        result = (ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_HAS_META_DATA)) != 0;
    return result;
}

U_CAPI int32_t U_EXPORT2
utext_replace(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    const UChar *replacementText, int32_t replacementLength,
    UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return 0;
    }
    if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE)) == 0) {
        *status = U_NO_WRITE_PERMISSION;
        return 0;
    }
    int32_t length = 0;
    if ((ut) && (ut->pFuncs->replace))
        length = ut->pFuncs->replace(ut, nativeStart, nativeLimit, replacementText, replacementLength, status);
    return length;
}

U_CAPI void U_EXPORT2
utext_copy(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    int64_t destIndex,
    UBool move,
    UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return;
    }
    if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE)) == 0) {
        *status = U_NO_WRITE_PERMISSION;
        return;
    }
    if ((ut) && (ut->pFuncs->copy))
        ut->pFuncs->copy(ut, nativeStart, nativeLimit, destIndex, move, status);
}

U_CAPI UBool U_EXPORT2
utext_equals(const UText *a, const UText *b)
{
    if (a == NULL || b == NULL ||
        a->magic != UTEXT_MAGIC ||
        b->magic != UTEXT_MAGIC) {
        // Null or invalid arguments don't compare equal to anything.
        return FALSE;
    }
    if (a->pFuncs != b->pFuncs) {
        // Different types of text providers.
        return FALSE;
    }
    if (a->context != b->context) {
        // Different sources (different strings)
        return FALSE;
    }
    if (utext_getNativeIndex(a) != utext_getNativeIndex(b)) {
        // Different current position in the string.
        return FALSE;
    }

    return TRUE;
}

U_CAPI UText * U_EXPORT2
utext_clone(UText *dest, const UText *src, UBool deep, UBool readOnly, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return dest;
    }
    UText *result = 0;
    if ((src) && (src->pFuncs->clone))
        result = src->pFuncs->clone(dest, src, deep, status);
    else
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    if (U_FAILURE(*status)) {
        return result;
    }
    if (result == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return result;
    }
    if (readOnly) {
        utext_freeze(result);
    }
    return result;
}

U_CAPI UText * U_EXPORT2
utext_close(UText *ut)
{
    if (utext_isValid(ut)) {
        // If the provider gave us a close function, call it now.
        // This will clean up anything allocated specifically by the provider.
        if (ut->pFuncs->close) {
            ut->pFuncs->close(ut);
        }

        // If we (the framework) allocated the UText or subsidiary storage,
        // delete it.
        if (ut->flags & UTEXT_EXTRA_HEAP_ALLOCATED) {
            uprv_free(ut->pExtra);
            ut->pExtra = NULL;
            ut->flags &= ~UTEXT_EXTRA_HEAP_ALLOCATED;
            ut->extraSize = 0;
        }

        // Zero out function table of the closed UText.
        ut->pFuncs = NULL;

        if (ut->flags & UTEXT_HEAP_ALLOCATED) {
            // This UText was allocated by UText setup. We need to free it.
            // Clear magic, so we can detect if the user messes up and immediately
            // tries to reopen another UText using the deleted storage.
            ut->magic = 0;
            uprv_free(ut);
            ut = NULL;
        }
    }
    return ut;
}

// Extended form of a UText.  The purpose is to aid in computing the total size required
// when a provider asks for a UText to be allocated with extra storage.

struct ExtendedUText {
    UText          ut;
    UAlignedMemory extension;
};

static const UText emptyText = UTEXT_INITIALIZER;

U_CAPI UText * U_EXPORT2
utext_setup(UText *ut, int32_t extraSpace, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return ut;
    }
    if (ut == NULL) {
        // We need to heap-allocate storage for the new UText.
        size_t spaceRequired = sizeof(UText);
        if (extraSpace > 0) {
            spaceRequired = sizeof(ExtendedUText) + extraSpace - sizeof(UAlignedMemory);
        }
        ut = (UText *)uprv_malloc(spaceRequired);
        if (ut == NULL) {
            *status = U_MEMORY_ALLOCATION_ERROR;
            return NULL;
        }
        else {
            *ut = emptyText;
            ut->flags |= UTEXT_HEAP_ALLOCATED;
            if (spaceRequired > 0) {
                ut->extraSize = extraSpace;
                ut->pExtra = &((ExtendedUText *)ut)->extension;
            }
        }
    }
    else {
        // We have been supplied with an already existing UText.
        // Verify that it really appears to be a UText.
        if (ut->magic != UTEXT_MAGIC) {
            *status = U_ILLEGAL_ARGUMENT_ERROR;
            return ut;
        }
        // If the ut is already open and there's a provider supplied close
        // function, call it.
        if ((utext_isValid(ut)) && (ut->pFuncs->close)) {
            ut->pFuncs->close(ut);
        }

        // If extra space was requested by our caller, check whether
        // sufficient already exists, and allocate new if needed.
        if (extraSpace > ut->extraSize) {
            // Need more space. If there is existing separately allocated space,
            // delete it first, then allocate new space.
            if (ut->flags & UTEXT_EXTRA_HEAP_ALLOCATED) {
                uprv_free(ut->pExtra);
                ut->extraSize = 0;
            }
            ut->pExtra = uprv_malloc((size_t)extraSpace);
            if (ut->pExtra == NULL) {
                *status = U_MEMORY_ALLOCATION_ERROR;
            }
            else {
                ut->extraSize = extraSpace;
                ut->flags |= UTEXT_EXTRA_HEAP_ALLOCATED;
            }
        }
    }
    if (U_SUCCESS(*status)) {
        // Initialize all remaining fields of the UText.
        ut->context = NULL;
        ut->chunkContents = NULL;
        ut->p = NULL;
        ut->q = NULL;
        ut->r = NULL;
        ut->a = 0;
        ut->b = 0;
        ut->c = 0;
        ut->chunkOffset = 0;
        ut->chunkLength = 0;
        ut->chunkNativeStart = 0;
        ut->chunkNativeLimit = 0;
        ut->nativeIndexingLimit = 0;
        ut->providerProperties = 0;
        ut->privA = 0;
        ut->privB = 0;
        ut->privC = 0;
        ut->privP = NULL;
        if ((ut->pExtra != NULL) && (ut->extraSize > 0)) {
            uprv_memset(ut->pExtra, 0, (size_t)ut->extraSize);
        }
    }
    return ut;
}

// Pointer relocation function, a utility used by shallow clone.
// Adjust a pointer that refers to something within one UText (the source)
// to refer to the same relative offset within a another UText (the target)
static void U_CALLCONV
utext_adjustPointer(UText *dest, const void** destPtr, const UText *src)
{
    // Convert all pointers to (char*) so that byte address arithmetic will work.
    char* dptr = (char*)*destPtr;
    char* dUText = (char*)dest;
    char* sUText = (char*)src;

    if (dptr >= (char*)src->pExtra && dptr < ((char*)src->pExtra) + src->extraSize) {
        // target ptr was to something within the src UText's pExtra storage.
        //   relocate it into the target UText's pExtra region.
        *destPtr = ((char*)dest->pExtra) + (dptr - (char*)src->pExtra);
    }
    else if (dptr >= sUText && dptr < sUText + src->sizeOfStruct) {
        // target ptr was pointing to somewhere within the source UText itself.
        //   Move it to the same offset within the target UText.
        *destPtr = dUText + (dptr - sUText);
    }
}

// This is a generic copy-the-utext-by-value clone function that can be
// used as-is with some utext types, and as a helper by other clones.
U_CAPI UText * U_EXPORT2
utext_shallowClone(UText *dest, const UText *src, UErrorCode* status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    int32_t  srcExtraSize = src->extraSize;

    // Use the generic text_setup to allocate storage if required.
    dest = utext_setup(dest, srcExtraSize, status);
    if (U_FAILURE(*status)) {
        return dest;
    }

    // Flags (how the UText was allocated) and the pointer to the
    // extra storage must retain the values in the cloned UText that
    // were set up by utext_setup. Save them separately before
    // copying the whole struct.
    void* destExtra = dest->pExtra;
    int32_t flags = dest->flags;

    // Copy the whole UText struct by value.
    // Any "Extra" storage is copied also.
    int32_t sizeToCopy = src->sizeOfStruct;
    if (sizeToCopy > dest->sizeOfStruct) {
        sizeToCopy = dest->sizeOfStruct;
    }
    uprv_memcpy(dest, src, (size_t)sizeToCopy);
    dest->pExtra = destExtra;
    dest->flags = flags;
    if (srcExtraSize > 0) {
        uprv_memcpy(dest->pExtra, src->pExtra, (size_t)srcExtraSize);
    }

    // Relocate any pointers in the target that refer to the UText itself
    // to point to the cloned copy rather than the original source.
    utext_adjustPointer(dest, &dest->context, src);
    utext_adjustPointer(dest, &dest->p, src);
    utext_adjustPointer(dest, &dest->q, src);
    utext_adjustPointer(dest, &dest->r, src);
    utext_adjustPointer(dest, (const void**)&dest->chunkContents, src);

    // The newly shallow-cloned UText does _not_ own the underlying storage for the text.
    // (The source for the clone may or may not have owned the text.)
    dest->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT);

    return dest;
}

// Reset a chunk to have no contents, so that the next call
// to access will cause new data to load.
// This is needed when copy/move/replace operate directly on the
// backing text, potentially putting it out of sync with the
// contents in the chunk.
static void U_CALLCONV
utext_invalidateAccess(UText *ut)
{
    ut->chunkLength = 0;
    ut->chunkNativeLimit = 0;
    ut->chunkNativeStart = 0;
    ut->chunkOffset = 0;
    ut->nativeIndexingLimit = 0;
}

// Do range pinning on a native index parameter.
// 64 bit pinning is done in place.
// 32 bit truncated result is returned as a convenience for
// use in providers that don't need 64 bits.
static inline int32_t
utext_pinIndex32(int64_t index, int64_t limit)
{
    if (index < 0) {
        index = 0;
    }
    else if (index > limit) {
        index = limit;
    }
    return (int32_t)index;
}

// Do range pinning on a native index parameter, 64-bit version.
static inline int64_t
utext_pinIndex64(int64_t index, int64_t limit)
{
    if (index < 0) {
        index = 0;
    }
    else if (index > limit) {
        index = limit;
    }
    return index;
}

// NUL-terminate a UChar string no matter what its type, 64-bit version.
// Set warning and error codes accordingly.
static inline int64_t
utext_terminateUChars(UChar* dest, int64_t destCapacity, int64_t length, UErrorCode* pErrorCode)
{
    if (pErrorCode != NULL && U_SUCCESS(*pErrorCode)) {
        // Not a public function, so no complete argument checking
        if (length < 0) { // Assume that the caller handles this
        }
        else if (length < destCapacity) {
            // NUL-terminate the string, the NUL fits
            dest[length] = 0;
            // Unset the not-terminated warning but leave all others
            if (*pErrorCode == U_STRING_NOT_TERMINATED_WARNING) {
                *pErrorCode = U_ZERO_ERROR;
            }
        }
        else if (length == destCapacity) {
            // Unable to NUL-terminate, but the string itself fit - set a warning code
            *pErrorCode = U_STRING_NOT_TERMINATED_WARNING;
        }
        else { // length > destCapacity
            // Even the string itself did not fit - set an error code
            *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        }
    }
    return length;
}

// NUL-terminate a uint8_t string no matter what its type, 64-bit version.
// Set warning and error codes accordingly.
static inline int64_t
utext_terminateChars(uint8_t* dest, int64_t destCapacity, int64_t length, UErrorCode* pErrorCode)
{
    if (pErrorCode != NULL && U_SUCCESS(*pErrorCode)) {
        // Not a public function, so no complete argument checking
        if (length < 0) { // Assume that the caller handles this
        }
        else if (length < destCapacity) {
            // NUL-terminate the string, the NUL fits
            dest[length] = 0;
            // Unset the not-terminated warning but leave all others
            if (*pErrorCode == U_STRING_NOT_TERMINATED_WARNING) {
                *pErrorCode = U_ZERO_ERROR;
            }
        }
        else if (length == destCapacity) {
            // Unable to NUL-terminate, but the string itself fit - set a warning code
            *pErrorCode = U_STRING_NOT_TERMINATED_WARNING;
        }
        else { // length > destCapacity
            // Even the string itself did not fit - set an error code
            *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        }
    }
    return length;
}

// NUL-terminate a UChar32 string no matter what its type, 64-bit version.
// Set warning and error codes accordingly.
static inline int64_t
utext_terminateUChars32(UChar32* dest, int64_t destCapacity, int64_t length, UErrorCode* pErrorCode)
{
    if (pErrorCode != NULL && U_SUCCESS(*pErrorCode)) {
        // Not a public function, so no complete argument checking
        if (length < 0) { // Assume that the caller handles this
        }
        else if (length < destCapacity) {
            // NUL-terminate the string, the NUL fits
            dest[length] = 0;
            // Unset the not-terminated warning but leave all others
            if (*pErrorCode == U_STRING_NOT_TERMINATED_WARNING) {
                *pErrorCode = U_ZERO_ERROR;
            }
        }
        else if (length == destCapacity) {
            // Unable to NUL-terminate, but the string itself fit - set a warning code
            *pErrorCode = U_STRING_NOT_TERMINATED_WARNING;
        }
        else { // length > destCapacity
            // Even the string itself did not fit - set an error code
            *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        }
    }
    return length;
}

//------------------------------------------------------------------------------
//
// UText implementation for const UChar* (read-only)/UChar* (read/write) strings.
//
// Use of UText data members:
//   context    pointer to const UChar*/UChar*
//   a          length of string.
//   d          length of buffer (read/write string only).
//   ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE) != 0
//              length of string is not known yet. ut->a can grow.
//   ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT) != 0
//              contents is owned by the UText and should be free'd on close.
//   ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_STABLE_CHUNKS) != 0
//              length of string is less than the chunk size. Chunk contents
//              point to the beginning of the context.
//
// This provider is written as an example of how to write other UText providers.
// Extra comments are provided to help provide clarity on what is required.
//
//------------------------------------------------------------------------------

// Sizes are in increments of sizeof(UChar).
enum {
    U16_TEXT_CHUNK_SIZE = 32,
    U16_TEXT_CHUNK_SCAN_AHEAD = 32,
    U16_CHUCK_TOLERANCE = U16_MAX_LENGTH
};

U_CDECL_BEGIN

static UText * U_CALLCONV
u16TextClone(UText *dest, const UText *src,
    UBool deep, UErrorCode* pErrorCode)
{
    // First, do a generic shallow clone.
    dest = utext_shallowClone(dest, src, pErrorCode);

    if (deep && U_SUCCESS(*pErrorCode)) {
        // Next, for deep clones, make a copy of the string. The copied storage is
        // owned by the newly created clone. UTEXT_PROVIDER_OWNS_TEXT is the flag
        // to know that this needs to be free'd on u16TextClose().
        //
        // If the string is read-only, the cloned string IS going to be NUL
        // terminated, whether or not the original was. If the string is read/write
        // we know the buffer size ahead of time.
        const UChar* s = (const UChar*)src->context;
        int64_t length64 = 0;
        if ((dest->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE)) != 0)
            length64 = BC_AS_I64(dest);
        else {
            // Avoid using u16TextLength() as this is a non-const function where
            // in cases where the input was NUL terminated and the length has not
            // yet been determined the UText could change.
            length64 = src->a;
            if (BC_AS_I64(src) < 0) {
                for (; (s[length64] != 0); length64++) {
                }
            }
            else {
                for (; (length64 < BC_AS_I64(src)) && (s[length64] != 0); length64++) {
                }
            }
            length64++;
        }

        UChar* copyStr = (UChar*)uprv_malloc(length64 * sizeof(UChar));
        if (copyStr == NULL) {
            *pErrorCode = U_MEMORY_ALLOCATION_ERROR;
        }
        else {
            int64_t i;
            if (BC_AS_I64(src) < 0) {
                for (i = 0; (i < length64); i++) {
                    copyStr[i] = s[i];
                }
            }
            else {
                for (i = 0; (i < BC_AS_I64(src)) && (i < length64); i++) {
                    copyStr[i] = s[i];
                }
            }
            dest->context = (void*)copyStr;
            dest->providerProperties |= I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT);
        }
    }
    return dest;
}

static int64_t U_CALLCONV
u16TextNativeLength(UText *ut)
{
    if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) != 0) {
        // NUL terminated string and we don't yet know the length, so scan for it.
        //
        // Avoid using u16TextAccess() becuase we don't want to change the iteration
        // postion.
        const UChar* s = (const UChar*)ut->context;
        int64_t length64 = ut->a;
        if (BC_AS_I64(ut) < 0) {
            for (; (s[length64] != 0); length64++) {
            }
        }
        else {
            for (; (length64 < BC_AS_I64(ut)) && (s[length64] != 0); length64++) {
            }
        }
        ut->a = length64;
        ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);
        if (ut->a >= U16_TEXT_CHUNK_SIZE) {
            ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_STABLE_CHUNKS);
        }
    }
    return ut->a;
}

static int64_t U_CALLCONV
u16ScanLength(UText *ut, int64_t nativeLimit)
{
    const UChar* s = (const UChar*)ut->context;
    if (nativeLimit >= ut->a) {
        if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) != 0) {
            // NUL terminated string and we don't yet know the length. Requested nativeIndex
            // is beyond where we have scanned so far.
            //
            // Scan ahead beyond the requested nativeIndex. Strategy here is to avoid fully
            // scanning a long string when the caller only wants to see a few characters at
            // its beginning.
            int64_t scanLimit64 = nativeLimit + U16_TEXT_CHUNK_SCAN_AHEAD;
            if ((nativeLimit + U16_TEXT_CHUNK_SCAN_AHEAD) < 0) {
                scanLimit64 = INT64_MAX;
            }

            int64_t chunkLimit64 = (int32_t)ut->a;
            if (BC_AS_I64(ut) < 0) {
                for (; (s[chunkLimit64] != 0) && (chunkLimit64 < scanLimit64); chunkLimit64++) {
                }
            }
            else {
                for (; (chunkLimit64 < BC_AS_I64(ut)) && (s[chunkLimit64] != 0) && (chunkLimit64 < scanLimit64); chunkLimit64++) {
                }
            }
            ut->a = chunkLimit64;

            if (chunkLimit64 < scanLimit64) {
                // Found the end of the string. Turn off looking for the end in future calls.
                ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);

                if (nativeLimit > chunkLimit64) {
                    nativeLimit = chunkLimit64;
                }
            }

            if (ut->a >= U16_TEXT_CHUNK_SIZE) {
                ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_STABLE_CHUNKS);
            }

            // Adjust the chunk [chunkNativeStart, chunkNativeLimit) grow it to include
            // the expanded length of the string for chunk test below.
            if ((ut->chunkNativeLimit < ut->a) &&
                ((ut->chunkNativeLimit == 0) || (ut->chunkNativeLimit % U16_TEXT_CHUNK_SIZE) > 0)) {
                ut->chunkNativeLimit = utext_pinIndex64(((ut->chunkNativeLimit / U16_TEXT_CHUNK_SIZE) + 1) * U16_TEXT_CHUNK_SIZE, ut->a);
                ut->chunkLength = (int32_t)(ut->chunkNativeLimit - ut->chunkNativeStart);
                ut->nativeIndexingLimit = ut->chunkLength;
            }
        }
        else {
            // We know the length of this string, and the user is requesting something
            // at or beyond the length. Pin the requested nativeIndex to the length.
            nativeLimit = ut->a;
        }
    }
    else {
        while ((nativeLimit > 0) && (U16_IS_TRAIL(s[nativeLimit]))) { nativeLimit--; }
    }
    return nativeLimit;
}

static UBool U_CALLCONV
u16TextAccess(UText *ut,
    int64_t nativeIndex, UBool forward)
{
    const UChar* s = (const UChar*)ut->context;

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pin 'nativeStart' to a positive index, if it came in out-of-bounds.
    // Snap 'nativeStart64' to the beginning of a code point.
    // Pin 'nativeStart64' to the adjusted length of the string, if it came in
    // out-of-bounds. We may need to scan ahead if the length is not known.
    int64_t nativeIndex64 = utext_pinIndex64(nativeIndex, INT64_MAX);
    nativeIndex64 = u16ScanLength(ut, nativeIndex64);

    // Adjust the chunk [chunkNativeStart, chunkNativeLimit) to contain the
    // access request. Move the goal posts so that we have some room if we are
    // near the edge based on the tolerance.
    UBool updateChunk = FALSE;
    if ((nativeIndex64 >= ut->chunkNativeStart) && (nativeIndex64 <= ut->chunkNativeLimit)) {
        // Forward iteration request.
        if ((forward) && (nativeIndex64 <= ut->a)) {
            int64_t chunkNativeLimit64 = nativeIndex64;
            while ((chunkNativeLimit64 < ut->a) && (U16_IS_TRAIL(s[chunkNativeLimit64]))) { chunkNativeLimit64++; }
            ut->chunkNativeStart = ((chunkNativeLimit64 / U16_TEXT_CHUNK_SIZE)) * U16_TEXT_CHUNK_SIZE;
            ut->chunkNativeLimit = utext_pinIndex64(((chunkNativeLimit64 / U16_TEXT_CHUNK_SIZE) + 2) * U16_TEXT_CHUNK_SIZE, ut->a);
            updateChunk = TRUE;
        }
        // Backward iteration request.
        else if ((!forward) && (nativeIndex64 > 0)) {
            int64_t chunkNativeStart64 = nativeIndex64;
            while ((chunkNativeStart64 > 0) && (U16_IS_TRAIL(s[chunkNativeStart64]))) { chunkNativeStart64--; }
            int8_t offset = ((chunkNativeStart64 % U16_TEXT_CHUNK_SIZE) > U16_CHUCK_TOLERANCE) ? 1 : 0;
            ut->chunkNativeLimit = utext_pinIndex64(((chunkNativeStart64 / U16_TEXT_CHUNK_SIZE) + 1 + offset) * U16_TEXT_CHUNK_SIZE, ut->a);
            ut->chunkNativeStart = utext_pinIndex64(((chunkNativeStart64 / U16_TEXT_CHUNK_SIZE) - 1 + offset) * U16_TEXT_CHUNK_SIZE, ut->a);
            updateChunk = TRUE;
        }
    }
    else {
        // Random access request.
        if (forward) {
            ut->chunkNativeStart = (nativeIndex64 / U16_TEXT_CHUNK_SIZE) * U16_TEXT_CHUNK_SIZE;
            ut->chunkNativeLimit = utext_pinIndex64(((nativeIndex64 / U16_TEXT_CHUNK_SIZE) + 2) * U16_TEXT_CHUNK_SIZE, ut->a);
        }
        else {
            ut->chunkNativeStart = utext_pinIndex64(((nativeIndex64 / U16_TEXT_CHUNK_SIZE) - 1) * U16_TEXT_CHUNK_SIZE, ut->a);
            ut->chunkNativeLimit = utext_pinIndex64(((nativeIndex64 / U16_TEXT_CHUNK_SIZE) + 1) * U16_TEXT_CHUNK_SIZE, ut->a);
        }
        updateChunk = TRUE;
    }

    // Update the chunk.
    // 
    // Make sure that the contents point to the native start, and make sure that the
    // length, nativeIndexingLimit, and offset are relative to the start of the contents.
    //
    // This is how 64-bit is supported by using smaller chunks that point to locations
    // accessible with 32-bit integers.
    if (updateChunk) {
        // The beginning and ending points of a chunk must not be left in the middle of a
        // surrogate pair. Expand the chunk to accomodate.
        //
        // It doesn't matter if the begin/end char happen to be an unpaired surrogate,
        // it's simpler not to worry about it if they are included.
        while ((ut->chunkNativeStart > 0) && (U16_IS_TRAIL(s[ut->chunkNativeStart]))) { ut->chunkNativeStart--; }
        while ((ut->chunkNativeLimit < ut->a) && (U16_IS_TRAIL(s[ut->chunkNativeLimit]))) { ut->chunkNativeLimit++; }

        ut->chunkContents = &(s[ut->chunkNativeStart]);
        ut->chunkLength = (int32_t)(ut->chunkNativeLimit - ut->chunkNativeStart);
        ut->nativeIndexingLimit = ut->chunkLength;
    }

    // Set current iteration position using the exact nativeIndex requested, not the 
    // code point adjusted one used to figure out chunk boundaries.
    ut->chunkOffset = (int32_t)(utext_pinIndex64(nativeIndex, ut->a) - ut->chunkNativeStart);

    // Return whether the request is at the start and or end of the string.
    return ((forward) && (nativeIndex64 < ut->a)) || ((!forward) && (nativeIndex64 > 0));
}

static int32_t U_CALLCONV
u16TextExtract(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    UChar* dest, int32_t destCapacity,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if ((destCapacity < 0) ||
        ((dest == NULL) && (destCapacity > 0)) ||
        (nativeStart > nativeLimit)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    const UChar* s = (UChar*)ut->context;

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Snaps 'nativeStart64' to the beginning of a code point.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, ut->a);
    while ((nativeStart64 > 0) && (U16_IS_TRAIL(s[nativeStart64]))) { nativeStart64--; }
    int64_t nativeLimit64 = u16ScanLength(ut, nativeLimit);

    // Since the destination is 32-bit, ensure that di never logically exceeds
    // INT32_MAX.
    int64_t si;
    int32_t di;
    for (si = nativeStart64, di = 0; (si < nativeLimit64) && (di >= 0); si++, di++) {
        if (di < destCapacity) { // Only store if there is space.
            dest[di] = s[si];
        }
    }

    // If the nativeLimit index points to a lead surrogate of a pair, add the
    // corresponding trail surrogate to the destination.
    if ((si > 0) &&
        (U16_IS_LEAD(s[si - 1])) &&
        (((si < ut->a) || ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) != 0)) &&
        (U16_IS_TRAIL(s[si])))
        ) {
        if (di < destCapacity) { // Only store if there is space.
            dest[di++] = s[si];
        }

        si++;
    }

    // Put iteration position at the point just following the extracted text.
    u16TextAccess(ut, si, TRUE);

    // Add a terminating NUL if space in the buffer permits, and set the error
    // status as required.
    u_terminateUChars(dest, destCapacity, di, pErrorCode);

    return di;
}

static int32_t U_CALLCONV
u16TextReplace(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    const UChar* replacementText, int32_t replacementLength,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if ((replacementText == NULL) && (replacementLength != 0)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if (replacementLength < -1) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if (nativeStart > nativeLimit) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    if (replacementLength < 0) {
        replacementLength = u_strlen(replacementText);
    }

    UChar* s = (UChar*)ut->context;

    if (s == replacementText) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    int64_t length64 = (int32_t)ut->a;
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, length64);
    int64_t nativeLimit64 = utext_pinIndex64(nativeLimit, length64);
    int64_t diff64 = replacementLength - (nativeLimit64 - nativeStart64);

    if (length64 + diff64 > BC_AS_I64(ut)) {
        *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        return 0;
    }

    // The algorithm goal is two-fold: first, do not allocate any extra memory
    // to make the replacement; second, do it in a single pass. Depending on
    // the direction we can tackle the replacement and meet these two goals
    // at the same time.
    if (nativeLimit64 - nativeStart64 < replacementLength) {
        int64_t i;
        for (i = length64 + diff64 - 1; (i >= nativeStart64 + replacementLength); i--)
            s[i] = s[i - diff64];
        for (; (i >= nativeStart64); i--)
            s[i] = replacementText[i - nativeStart64];
    }
    else {
        int64_t i;
        for (i = (int64_t)nativeStart64; (i < nativeStart64 + replacementLength); i++)
            s[i] = replacementText[i - nativeStart64];
        for (; (i < length64); i++)
            s[i] = s[i - diff64];
    }

    if ((replacementLength > 0) || (nativeLimit64 - nativeStart64 > 0)) {
        ut->a += diff64;

        utext_terminateUChars(s, BC_AS_I64(ut), ut->a, pErrorCode);

        // Set the iteration position to the end of the newly inserted replacement text.
        utext_invalidateAccess(ut);
        u16TextAccess(ut, nativeLimit64 + diff64, TRUE);
    }

    ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_STABLE_CHUNKS);

    return (int32_t)diff64;
}

static void U_CALLCONV
u16TextCopy(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    int64_t nativeDest,
    UBool move,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return;
    }
    if (nativeStart > nativeLimit) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeDest64' to the length of the string, if it came in out-of-bounds.
    UChar* s = (UChar*)ut->context;
    int64_t length64 = (int64_t)ut->a;
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, length64);
    int64_t nativeLimit64 = utext_pinIndex64(nativeLimit, length64);
    int64_t nativeDest64 = utext_pinIndex64(nativeDest, length64);
    int64_t diff64 = (move ? 0 : nativeLimit64 - nativeStart64);

    // [nativeStart, nativeLimit) cannot overlap [dest, nativeLimit-nativeStart).
    if ((nativeDest64 > nativeStart64) && (nativeDest64 < nativeLimit64)) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }
    if (length64 + diff64 > BC_AS_I64(ut)) {
        *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        return;
    }

    // The algorithm goal is two-fold: first, do not allocate any extra memory
    // to make the replacement; second, do it in a single pass. Depending on
    // the direction we can tackle the replacement and meet these two goals
    // at the same time if the text is being moved. Otherwise, we use the same
    // algorithm for u16TextReplace() but only the backwards case is needed.
    if (move) {
        if (nativeStart64 < nativeDest64) {
            int64_t i, j;
            for (i = nativeStart64; (i < nativeLimit64); i++) {
                UChar u16char = s[nativeStart64];
                for (j = nativeStart64; (j < nativeDest64 - 1); j++)
                    s[j] = s[j + 1];
                s[j] = u16char;
            }
        }
        else if (nativeStart64 > nativeDest64) {
            int64_t i, j;
            for (i = nativeLimit64 - 1; (i >= nativeStart64); i--) {
                UChar u16char = s[nativeLimit64 - 1];
                for (j = nativeLimit64 - 1; (j > nativeDest64); j--)
                    s[j] = s[j - 1];
                s[j] = u16char;
            }
        }
    }
    else {
        int64_t offset32 = nativeStart64 + (nativeStart64 > nativeDest64 ? diff64 : 0) - nativeDest64;
        int64_t i;
        for (i = length64 + diff64 - 1; (i >= nativeDest64 + diff64); i--)
            s[i] = s[i - diff64];
        for (; (i >= nativeDest64); i--)
            s[i] = s[offset32 + i];

        if (diff64) {
            ut->a += diff64;
        }
    }

    if (diff64) {
        utext_terminateUChars(s, BC_AS_I64(ut), ut->a, pErrorCode);
    }

    // Put iteration position at the newly inserted (moved) block.
    int64_t nativeIndex64 = nativeDest64 + nativeLimit64 - nativeStart64;
    if ((move) && (nativeDest64 > nativeStart64)) {
        nativeIndex64 = nativeDest64;
    }

    utext_invalidateAccess(ut);
    u16TextAccess(ut, nativeIndex64, TRUE);

    ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_STABLE_CHUNKS);
}

static void U_CALLCONV
u16TextClose(UText *ut)
{
    // Most of the work of close is done by the generic UText framework close.
    // All that needs to be done here is delete the string if the UText
    // owns it. This only occurs if the UText was created by u16TextClone().
    if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT)) != 0) {
        UChar* s = (UChar*)ut->context;
        uprv_free(s);
        ut->context = NULL;
        ut->chunkContents = NULL;
    }
}

static const struct UTextFuncs u16Funcs =
{
    sizeof(UTextFuncs),
    0, 0, 0,           // Reserved alignment padding
    u16TextClone,
    u16TextNativeLength,
    u16TextAccess,
    u16TextExtract,
    u16TextReplace,
    u16TextCopy,
    NULL,              // MapOffsetToNative,
    NULL,              // MapIndexToUTF16,
    u16TextClose,
    NULL,              // spare 1
    NULL,              // spare 2
    NULL,              // spare 3
};

U_CDECL_END

static const UChar gEmptyU16String[] = { 0 };

U_CAPI UText * U_EXPORT2
utext_openConstU16(UText *ut,
    const UChar* s, int64_t length, int64_t capacity,
    UErrorCode* pErrorCode)
{
    U_ASSERT(U16_CHUCK_TOLERANCE >= U16_MAX_LENGTH);
    U_ASSERT(U16_TEXT_CHUNK_SIZE - U16_CHUCK_TOLERANCE > U16_CHUCK_TOLERANCE);

    if (U_FAILURE(*pErrorCode)) {
        return NULL;
    }

    if ((s == NULL) && (length == 0)) {
        s = gEmptyU16String;
    }

    if ((s == NULL) || (length < -1) || (capacity < -1)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    ut = utext_setup(ut, 0, pErrorCode);
    if (U_SUCCESS(*pErrorCode)) {
        ut->pFuncs = &u16Funcs;
        if (length == -1) {
            ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);
        }
        if ((length == -1) || (length < U16_TEXT_CHUNK_SIZE)) {
            ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_STABLE_CHUNKS);
        }
        ut->context = (void*)s;
        ut->a = length < 0 ? 0 : length;
        BC_AS_I64(ut) = capacity;
    }
    return ut;
}

U_CAPI UText * U_EXPORT2
utext_openU16(UText *ut,
    UChar* s, int64_t length, int64_t capacity,
    UErrorCode* pErrorCode)
{
    if ((s == NULL) || (length < -1) || (capacity < 0)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    // Length must be known for write operations. Spend the
    // time now to figure it out.
    if (length < 0) {
        length = 0;
        for (; (length < capacity) && (s[length] != 0); length++) {
        }
    }

    ut = utext_openConstU16(ut, s, length, capacity, pErrorCode);
    if (U_SUCCESS(*pErrorCode)) {
        ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_WRITABLE);
    }

    return ut;
}

U_CAPI UText * U_EXPORT2
utext_openUChars(UText *ut, const UChar* s, int64_t length, UErrorCode* pErrorCode)
{
    return utext_openConstU16(ut, s, length, -1, pErrorCode);
}

//------------------------------------------------------------------------------
//
// UText implementation for const char* (read-only)/char* (read/write) strings.
//
// Use of UText data members:
//   context    pointer to const char*/char*
//   a          length of string.
//   d          length of buffer (read/write string only).
//   ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE) != 0
//              length of string is not known yet. ut->a can grow.
//   p          pointer to the active buffer.
//   q          pointer to the alternate buffer.
//
//------------------------------------------------------------------------------

// Sizes are in increments of sizeof(uint8_t).
enum {
    U8_TEXT_CHUNK_SIZE = 32,
    U8_TEXT_CHUNK_SCAN_AHEAD = 32,
    U8_CHUCK_TOLERANCE = U8_MAX_LENGTH
};

struct u8ChunkBuffer {
    // Native index of first UChar in chunk.
    int64_t chunkNativeStart;

    // Native index following last UChar in chunk.
    int64_t chunkNativeLimit;

    // The UChar buffer. Requires extra space to allow for the difference between
    // encodings.
    // Tolerance is to allow growth at the beginning and the end of the
    // chunk to accomodate non-boundary aligned characters.
    UChar chunkContents[U8_TEXT_CHUNK_SIZE + U8_CHUCK_TOLERANCE * 2];

    // Length of the text chunk in UChars.
    int32_t chunkLength;

    // The relative offset mapping from the chunk offset to the chunk native start.
    // Should be the same length as chunkContents.
    int8_t chunkU16ToNative[U8_TEXT_CHUNK_SIZE + U8_CHUCK_TOLERANCE * 2];

    int8_t chunkNativeToU16[U8_TEXT_CHUNK_SIZE + U8_CHUCK_TOLERANCE * 2];

    // The highest chunk offset where native indexing and chunk indexing correspond.
    int32_t nativeIndexingLimit;
};

U_CDECL_BEGIN

static UText * U_CALLCONV
u8TextClone(UText *dest, const UText *src,
    UBool deep, UErrorCode* pErrorCode)
{
    // First, do a generic shallow clone.
    dest = utext_shallowClone(dest, src, pErrorCode);

    if (deep && U_SUCCESS(*pErrorCode)) {
        // Next, for deep clones, make a copy of the string. The copied storage is
        // owned by the newly created clone. UTEXT_PROVIDER_OWNS_TEXT is the flag
        // to know that this needs to be free'd on u8TextClose().
        //
        // If the string is read-only, the cloned string IS going to be NUL
        // terminated, whether or not the original was. If the string is read/write
        // we know the buffer size ahead of time.
        const uint8_t* s = (const uint8_t*)src->context;
        int64_t length64 = 0;
        if ((dest->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE)) != 0)
            length64 = BC_AS_I64(dest);
        else {
            // Avoid using u8TextLength() as this is a non-const function where
            // in cases where the input was NUL terminated and the length has not
            // yet been determined the UText could change.
            length64 = src->a;
            if (BC_AS_I64(src) < 0) {
                for (; (s[length64] != 0); length64++) {
                }
            }
            else {
                for (; (length64 < BC_AS_I64(src)) && (s[length64] != 0); length64++) {
                }
            }
            length64++;
        }

        uint8_t* copyStr = (uint8_t*)uprv_malloc(length64 * sizeof(uint8_t));
        if (copyStr == NULL) {
            *pErrorCode = U_MEMORY_ALLOCATION_ERROR;
        }
        else {
            int64_t i;
            if (BC_AS_I64(src) < 0) {
                for (i = 0; (i < length64); i++) {
                    copyStr[i] = s[i];
                }
            }
            else {
                for (i = 0; (i < BC_AS_I64(src)) && (i < length64); i++) {
                    copyStr[i] = s[i];
                }
            }
            dest->context = (void*)copyStr;
            dest->providerProperties |= I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT);
        }
    }
    return dest;
}

static int64_t U_CALLCONV
u8TextNativeLength(UText *ut)
{
    if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) != 0) {
        // NUL terminated string and we don't yet know the length, so scan for it.
        //
        // Avoid using u8TextAccess() becuase we don't want to change the iteration
        // postion.
        const uint8_t* s = (const uint8_t*)ut->context;
        int64_t length64 = ut->a;
        if (BC_AS_I64(ut) < 0) {
            for (; (s[length64] != 0); length64++) {
            }
        }
        else {
            for (; (length64 < BC_AS_I64(ut)) && (s[length64] != 0); length64++) {
            }
        }
        ut->a = length64;
        ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);
    }
    return ut->a;
}

static int32_t U_CALLCONV
u8TextMapIndexToUTF16(const UText *ut, int64_t nativeIndex);

static int64_t U_CALLCONV
u8SetCodePointStart(const UText *ut, int64_t nativeIndex, UBool safe) {
    const uint8_t* s = (const uint8_t*)ut->context;
    int64_t i = nativeIndex;
    if (U8_IS_TRAIL(s[i])) {
        if (safe) {
            if (U8_IS_TRAIL(s[i])) {
                // Convert to 32-bit for utf8_back1SafeBody() and then back
                // to 64-bit to maintain single code stream.
                int64_t offset64 = i - (i < U8_TEXT_CHUNK_SIZE ? i : U8_TEXT_CHUNK_SIZE);
                int32_t j = (int32_t)(i - offset64);
                i = offset64 + utf8_back1SafeBody(&s[offset64], 0, j);
            }
        }
        else {
            while ((i > 0) && (U8_IS_TRAIL(s[i]))) {
                --i;
            }
        }
    }
    return i;
}

static int64_t U_CALLCONV
u8ScanLength(UText *ut, int64_t nativeLimit)
{
    if (nativeLimit >= ut->a) {
        if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) != 0) {
            const uint8_t* s = (const uint8_t*)ut->context;
            // NUL terminated string and we don't yet know the length. Requested nativeLimit
            // is beyond where we have scanned so far.
            //
            // Scan ahead beyond the requested nativeLimit. Strategy here is to avoid fully
            // scanning a long string when the caller only wants to see a few characters at
            // its beginning.
            int64_t scanLimit64 = nativeLimit + U16_TEXT_CHUNK_SCAN_AHEAD;
            if ((nativeLimit + U16_TEXT_CHUNK_SCAN_AHEAD) < 0) {
                scanLimit64 = INT64_MAX;
            }

            int64_t chunkLimit64 = (int32_t)ut->a;
            if (BC_AS_I64(ut) < 0) {
                for (; (s[chunkLimit64] != 0) && (chunkLimit64 < scanLimit64); chunkLimit64++) {
                }
            }
            else {
                for (; (chunkLimit64 < BC_AS_I64(ut)) && (s[chunkLimit64] != 0) && (chunkLimit64 < scanLimit64); chunkLimit64++) {
                }
            }
            ut->a = chunkLimit64;

            if (chunkLimit64 < scanLimit64) {
                // Found the end of the string. Turn off looking for the end in future calls.
                ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);

                if (nativeLimit > chunkLimit64) {
                    nativeLimit = chunkLimit64;
                }
            }
        }
        else {
            // We know the length of this string, and the user is requesting something
            // at or beyond the length. Pin the requested nativeIndex to the length.
            nativeLimit = ut->a;
        }
    }
    else {
        nativeLimit = u8SetCodePointStart(ut, nativeLimit, true);
    }
    return nativeLimit;
}

static UBool U_CALLCONV
u8TextAccess(UText *ut,
    int64_t nativeIndex, UBool forward)
{
    const uint8_t* s = (const uint8_t*)ut->context;

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pin 'nativeStart' to a positive index, if it came in out-of-bounds.
    // Pin 'nativeStart64' to the adjusted length of the string, if it came in
    // out-of-bounds. We may need to scan ahead if the length is not known.
    // Snap 'nativeStart64' to the beginning of a code point.
    int64_t nativeIndex64 = utext_pinIndex64(nativeIndex, INT64_MAX);
    nativeIndex64 = u8ScanLength(ut, nativeIndex64);

    // Find the next chunk.
    //
    // Consider and determine potential [chunkNativeStart, chunkNativeLimit) to contain
    // the access request. The chunk may or may not become active during this access
    // request. It may wait until the next request and get swapped in.
    //
    // This approch provides a consistent chunking that is not relative to the index
    // but can reliably be arrived at everytime.
    u8ChunkBuffer* alternateBuffer = (u8ChunkBuffer*)ut->q;

    UBool prepareChunk = FALSE;
    int64_t chunkNativeStart64 = 0;
    int64_t chunkNativeLimit64 = 0;
    if ((nativeIndex64 >= ut->chunkNativeStart) && (nativeIndex64 <= ut->chunkNativeLimit)) {
        // Forward iteration request.
        if ((forward) && (nativeIndex64 <= ut->a)) {
            chunkNativeLimit64 = nativeIndex64;
            while ((chunkNativeLimit64 < ut->a) && (U8_IS_TRAIL(s[chunkNativeLimit64]))) { chunkNativeLimit64++; }

            if ((chunkNativeLimit64 == ut->chunkNativeLimit) || 
                (chunkNativeLimit64 >= ut->chunkNativeLimit - U8_CHUCK_TOLERANCE)) {
                int8_t offset = 0;
                if (chunkNativeLimit64 / U8_TEXT_CHUNK_SIZE < ut->a / U8_TEXT_CHUNK_SIZE) {
                    ((chunkNativeLimit64 % U8_TEXT_CHUNK_SIZE) > U8_CHUCK_TOLERANCE) ? 1 : 0;
                }
                chunkNativeStart64 = ((chunkNativeLimit64 / U8_TEXT_CHUNK_SIZE) + offset) * U8_TEXT_CHUNK_SIZE;
                chunkNativeLimit64 = utext_pinIndex64(((chunkNativeLimit64 / U8_TEXT_CHUNK_SIZE) + 1 + offset) * U8_TEXT_CHUNK_SIZE, ut->a);
                prepareChunk = TRUE;
            }
        }
        // Backward iteration request.
        else if ((!forward) && (nativeIndex64 > 0)) {
            chunkNativeStart64 = nativeIndex64;
            while ((chunkNativeStart64 > 0) && (U8_IS_TRAIL(s[chunkNativeStart64]))) { chunkNativeStart64--; }

            if ((chunkNativeStart64 == ut->chunkNativeStart) || 
                (chunkNativeStart64 < ut->chunkNativeStart + U8_CHUCK_TOLERANCE)) {
                int8_t offset = ((chunkNativeStart64 % U8_TEXT_CHUNK_SIZE) > U8_CHUCK_TOLERANCE) ? 1 : 0;
                chunkNativeLimit64 = utext_pinIndex64(((chunkNativeStart64 / U8_TEXT_CHUNK_SIZE) + offset) * U8_TEXT_CHUNK_SIZE, ut->a);
                chunkNativeStart64 = utext_pinIndex64(((chunkNativeStart64 / U8_TEXT_CHUNK_SIZE) - 1 + offset) * U8_TEXT_CHUNK_SIZE, ut->a);
                prepareChunk = TRUE;
            }
        }
    }
    else {
        // Random access request.
        chunkNativeStart64 = (nativeIndex64 / U8_TEXT_CHUNK_SIZE) * U8_TEXT_CHUNK_SIZE;
        chunkNativeLimit64 = utext_pinIndex64(((nativeIndex64 / U8_TEXT_CHUNK_SIZE) + 1) * U8_TEXT_CHUNK_SIZE, ut->a);

        // Special case. If we are moving backwards and our random request places us
        // at the beginning of the chunk boundary, add an extra character so that
        // utext_prev32() does not go past the beginning of the chunk boundary and the
        // next request triggers another utext_access() for the next complete chunk.
        if ((!forward) && (chunkNativeStart64 > 0) && (chunkNativeStart64 == nativeIndex64))
            chunkNativeStart64--;

        prepareChunk = TRUE;
    }

    // Prpeare next chunk.
    // 
    // Given the [chunkNativeStart64, chunkNativeLimit64) fill the alternate buffer with
    // the UChars that this span represents. Always fill forward the chunk regardless of
    // the direction. It makes chunkContents and nativeIndexing easier.
    if (prepareChunk) {
        // The beginning and ending points of a chunk must not be left in the middle of a
        // surrogate pair. Expand the chunk to accomodate.
        //
        // It doesn't matter if the begin/end char happen to be an unpaired surrogate,
        // it's simpler not to worry about it if they are included.
        while ((chunkNativeStart64 > 0) && (U8_IS_TRAIL(s[chunkNativeStart64]))) { chunkNativeStart64--; }
        while ((chunkNativeLimit64 < ut->a) && (U8_IS_TRAIL(s[chunkNativeLimit64]))) { chunkNativeLimit64++; }

        if ((chunkNativeStart64 != alternateBuffer->chunkNativeStart) || (chunkNativeLimit64 != alternateBuffer->chunkNativeLimit)) {
            // Fill the chunk buffer and mapping arrays.
            alternateBuffer->nativeIndexingLimit = -1;

            int64_t si;
            int32_t di;
            UChar32 uchar;
            for (si = chunkNativeStart64, di = 0; si < chunkNativeLimit64; ) {
                uchar = s[si];
                if (U8_IS_SINGLE(uchar)) {
                    if (di < U8_TEXT_CHUNK_SIZE + U8_CHUCK_TOLERANCE * 2) {
                        alternateBuffer->chunkContents[di] = (UChar)uchar;
                        alternateBuffer->chunkU16ToNative[di] = (int8_t)((si - chunkNativeStart64) - di);
                        alternateBuffer->chunkNativeToU16[si - chunkNativeStart64] = (int8_t)(di - (si - chunkNativeStart64));
                    }
                    si++;
                    di++;
                }
                else {
                    if (alternateBuffer->nativeIndexingLimit < 0) {
                        alternateBuffer->nativeIndexingLimit = di;
                    }
                    int64_t savedSi = si;
                    int32_t savedDi = di;

                    // Convert to 32-bit for utf8_nextCharSafeBody() and then back
                    // to 64-bit to maintain single code stream.
                    int32_t limit32 = utext_pinIndex32((int32_t)(ut->a - si < si + U8_TEXT_CHUNK_SIZE ? ut->a - si : U8_TEXT_CHUNK_SIZE), chunkNativeLimit64);
                    int32_t j = 1;
                    uchar = utf8_nextCharSafeBody(&s[si], &j, limit32, uchar, -3);
                    si += j;
                    if (U_IS_BMP(uchar)) {
                        if (di < U8_TEXT_CHUNK_SIZE + U8_CHUCK_TOLERANCE * 2) {
                            alternateBuffer->chunkContents[di] = (UChar)uchar;
                        }
                        di++;
                    }
                    else {
                        if (di < U8_TEXT_CHUNK_SIZE + U8_CHUCK_TOLERANCE * 2) {
                            alternateBuffer->chunkContents[di] = U16_LEAD(uchar);
                        }
                        di++;
                        if (di < U8_TEXT_CHUNK_SIZE + U8_CHUCK_TOLERANCE * 2) {
                            alternateBuffer->chunkContents[di] = U16_TRAIL(uchar);
                        }
                        di++;
                    }

                    // Alternate approach that uses macros, does not account for issues with
                    // malformed sequences.
                    //
                    //U8_NEXT_OR_FFFD(s, si, ut->a, uchar);
                    //if ((uchar == 0) && ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) == 0)) {
                    //	si--;
                    //	break;
                    //}
                    //U16_APPEND_UNSAFE(alternateBuffer->chunkContents, di, uchar);

                    int64_t i;
                    for (i = savedDi; (di < U8_TEXT_CHUNK_SIZE + U8_CHUCK_TOLERANCE * 2) && (i < di); i++) {
                        alternateBuffer->chunkU16ToNative[i] = (int8_t)((savedSi - chunkNativeStart64) - i);
                    }
                    for (i = savedSi; i < si; i++) {
                        alternateBuffer->chunkNativeToU16[i - chunkNativeStart64] = (int8_t)(savedDi - (i - chunkNativeStart64));
                    }
                }
            }

            if (alternateBuffer->nativeIndexingLimit < 0) {
                alternateBuffer->nativeIndexingLimit = di;
            }
            alternateBuffer->chunkU16ToNative[di] = (int8_t)((si - chunkNativeStart64) - di);
            alternateBuffer->chunkNativeToU16[si - chunkNativeStart64] = (int8_t)(di - (si - chunkNativeStart64));

            alternateBuffer->chunkNativeStart = chunkNativeStart64;
            alternateBuffer->chunkNativeLimit = chunkNativeLimit64;
            alternateBuffer->chunkLength = di;

            UErrorCode errorCode = U_ZERO_ERROR;
            u_terminateUChars(alternateBuffer->chunkContents, (U8_TEXT_CHUNK_SIZE * U16_MAX_LENGTH / U8_MAX_LENGTH) + U8_CHUCK_TOLERANCE, di, &errorCode);
        }
    }

    // Check if we need to make a buffer change. Swap to the previosuly prepared buffer if
    // we are no longer in the active buffer.
    if ((nativeIndex64 >= alternateBuffer->chunkNativeStart) && (nativeIndex64 <= alternateBuffer->chunkNativeLimit)) {
        // Swap buffers
        ut->q = ut->p;
        ut->p = alternateBuffer;
        ut->chunkNativeStart = alternateBuffer->chunkNativeStart;
        ut->chunkNativeLimit = alternateBuffer->chunkNativeLimit;
        ut->chunkContents = &(alternateBuffer->chunkContents[0]);
        ut->chunkLength = alternateBuffer->chunkLength;
        ut->nativeIndexingLimit = alternateBuffer->nativeIndexingLimit;
    }

    // Set current iteration position using the code point adjusted one used
    // to figure out chunk boundaries.
    //
    // Convert this from the nativeIndex (u8) to the to the chunk contents index (u16).
    ut->chunkOffset = u8TextMapIndexToUTF16(ut, nativeIndex64);

    // Return whether the request is at the start and or end of the string.
    return ((forward) && (nativeIndex64 < ut->a)) || ((!forward) && (nativeIndex64 > 0));
}

static int32_t U_CALLCONV
u8TextExtract(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    UChar* dest, int32_t destCapacity,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if ((destCapacity < 0) ||
        ((dest == NULL) && (destCapacity > 0)) ||
        (nativeStart > nativeLimit)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    const uint8_t* s = (uint8_t*)ut->context;

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Snaps 'nativeStart64' to the beginning of a code point.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    // Snaps 'nativeLimit64' to the beginning of a code point.
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, ut->a);
    nativeStart64 = u8SetCodePointStart(ut, nativeStart64, true);
    int64_t nativeLimit64 = u8ScanLength(ut, nativeLimit);

    // Because we are moving code points we may go over the requested limit in order
    // to include missing trail bytes.
    //
    // Since the destination is 32-bit, ensure that di never logically exceeds
    // INT32_MAX.
    int64_t si;
    int32_t di;
    UChar32 uchar;
    for (si = nativeStart64, di = 0; (si < nativeLimit64) && (di >= 0); ) {
        uchar = s[si];
        if (U8_IS_SINGLE(uchar)) {
            if (di < destCapacity) {
                dest[di] = (UChar)uchar;
            }
            si++;
            di++;
        }
        else {
            // Convert to 32-bit for utf8_nextCharSafeBody() and then back
            // to 64-bit to maintain single code stream.
            int32_t limit32 = utext_pinIndex32((int32_t)(ut->a - si < si + U8_TEXT_CHUNK_SIZE ? ut->a - si : U8_TEXT_CHUNK_SIZE), nativeLimit64);
            int32_t j = 1;
            uchar = utf8_nextCharSafeBody(&s[si], &j, limit32, uchar, -3);
            si += j;
            if (U_IS_BMP(uchar)) {
                if (di < destCapacity) {
                    dest[di] = (UChar)uchar;
                }
                di++;
            }
            else {
                if (di < destCapacity) {
                    dest[di] = U16_LEAD(uchar);
                }
                di++;
                if (di < destCapacity) {
                    dest[di] = U16_TRAIL(uchar);
                }
                di++;
            }
        }

        // Alternate approach that uses macros, does not account for issues with 
        // malformed sequences.
        //
        //j = si - ut->chunkNativeStart;
        //U8_NEXT_UNSAFE(s, j, uchar);
        //if (di < destCapacity) { // Only store if there is space.
        //	// If we have two UChars but not enought space, take the lead.
        //	if (di + U16_LENGTH(uchar) > destCapacity) {
        //		dest[di] = U16_LEAD(uchar);
        //		di += U16_LENGTH(uchar);
        //	}
        //	else {
        //		U16_APPEND_UNSAFE(dest, di, uchar);
        //	}
        //}
        //else {
        //	di += U16_LENGTH(uchar);
        //}
        //si += j - (si - ut->chunkNativeStart);
    }

    // Put iteration position at the point just following the extracted text.
    u8TextAccess(ut, si, TRUE);

    // Add a terminating NUL if space in the buffer permits, and set the error
    // status as required.
    u_terminateUChars(dest, destCapacity, di, pErrorCode);

    return di;
}

static void U_CALLCONV
u8InvalidateBuffers(UText *ut)
{
    u8ChunkBuffer* activeBuffer = (u8ChunkBuffer*)ut->p;
    activeBuffer->chunkLength = 0;
    activeBuffer->chunkNativeLimit = 0;
    activeBuffer->chunkNativeStart = 0;
    activeBuffer->nativeIndexingLimit = 0;

    u8ChunkBuffer* alternateBuffer = (u8ChunkBuffer*)ut->q;
    alternateBuffer->chunkLength = 0;
    alternateBuffer->chunkNativeLimit = 0;
    alternateBuffer->chunkNativeStart = 0;
    alternateBuffer->nativeIndexingLimit = 0;
}

static int32_t U_CALLCONV
u8TextReplace(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    const UChar* replacementText, int32_t replacementLength,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if ((replacementText == NULL) && (replacementLength != 0)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if (replacementLength < -1) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if (nativeStart > nativeLimit) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    if (replacementLength < 0) {
        replacementLength = u_strlen(replacementText);
    }

    uint8_t* s = (uint8_t*)ut->context;

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    int64_t nativeReplLength64 = 0;
    if (replacementLength > 0) {
        int64_t i;
        UChar32 uchar;
        for (i = 0; i < replacementLength; ) {
            U16_NEXT_UNSAFE(replacementText, i, uchar);
            nativeReplLength64 += U8_LENGTH(uchar);
        }
    }

    int64_t length64 = (int32_t)ut->a;
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, length64);
    int64_t nativeLimit64 = utext_pinIndex64(nativeLimit, length64);
    int64_t diff64 = nativeReplLength64 - (nativeLimit64 - nativeStart64);

    if (length64 + diff64 > BC_AS_I64(ut)) {
        *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        return 0;
    }

    // The algorithm goal is two-fold: first, do not allocate any extra memory
    // to make the replacement; second, do it in a single pass. Depending on
    // the direction we can tackle the replacement and meet these two goals
    // at the same time.
    if (nativeLimit64 - nativeStart64 < nativeReplLength64) {
        int64_t si;
        int32_t di;
        UChar32 uchar;
        for (si = length64 + diff64 - 1; (si >= nativeStart64 + nativeReplLength64); si--)
            s[si] = s[si - diff64];
        for (di = replacementLength; (di > 0) && (si >= nativeStart64); ) {
            U16_PREV(replacementText, 0, di, uchar);
            if (U8_IS_SINGLE(uchar)) {
                s[si--] = (uint8_t)uchar;
            }
            else {
                // Convert to 32-bit for utf8_appendCharSafeBody() and then back
                // to 64-bit to maintain single code stream.
                UBool isError = FALSE;
                int32_t limit32 = (int32_t)(ut->a + diff64 - si < si + U8_TEXT_CHUNK_SIZE ? ut->a + diff64 - si : U8_TEXT_CHUNK_SIZE);
                si += utf8_appendCharSafeBody(&s[si], 0, limit32, uchar, &isError);

                // Alternate approach that uses macros, does not account for issues with
                // malformed sequences.
                //
                //int64_t j = U8_LENGTH(uchar);
                //i -= j;
                //U8_APPEND_UNSAFE(s, i, uchar);
                //i -= j;
            }
        }
    }
    else {
        int64_t si;
        int32_t di;
        UChar32 uchar;
        for (si = (int64_t)nativeStart64, di = 0; (di < replacementLength) && (si < nativeStart64 + nativeReplLength64); ) {
            U16_NEXT(replacementText, di, ut->a, uchar);
            if (U8_IS_SINGLE(uchar)) {
                s[si++] = (uint8_t)uchar;
            }
            else {
                // Convert to 32-bit for utf8_appendCharSafeBody() and then back
                // to 64-bit to maintain single code stream.
                UBool isError = FALSE;
                int32_t limit32 = utext_pinIndex32((int32_t)(si + U8_TEXT_CHUNK_SIZE > ut->a - si ? ut->a - si : U8_TEXT_CHUNK_SIZE), nativeLimit64);
                si += utf8_appendCharSafeBody(&s[si], 0, limit32, uchar, &isError);

                // Alternate approach that uses macros, does not account for issues with
                // malformed sequences.
                //
                //U8_APPEND_UNSAFE(s, i, uchar);
            }
        }
        for (; (si < length64); si++)
            s[si] = s[si - diff64];
    }

    if ((nativeReplLength64 > 0) || (nativeLimit64 - nativeStart64 > 0)) {
        ut->a += diff64;

        utext_terminateChars(s, BC_AS_I64(ut), ut->a, pErrorCode);

        // Set the iteration position to the end of the newly inserted replacement text.
        utext_invalidateAccess(ut);
        u8InvalidateBuffers(ut);
        u8TextAccess(ut, nativeLimit64 + diff64, TRUE);
    }

    return (int32_t)diff64;
}

static void U_CALLCONV
u8TextCopy(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    int64_t nativeDest,
    UBool move,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return;
    }
    if (nativeStart > nativeLimit) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeDest64' to the length of the string, if it came in out-of-bounds.
    uint8_t* s = (uint8_t*)ut->context;
    int64_t length64 = (int64_t)ut->a;
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, length64);
    int64_t nativeLimit64 = utext_pinIndex64(nativeLimit, length64);
    int64_t nativeDest64 = utext_pinIndex64(nativeDest, length64);
    int64_t diff64 = (move ? 0 : nativeLimit64 - nativeStart64);

    // [nativeStart, nativeLimit) cannot overlap [dest, nativeLimit-nativeStart).
    if ((nativeDest64 > nativeStart64) && (nativeDest64 < nativeLimit64)) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }
    if (length64 + diff64 > BC_AS_I64(ut)) {
        *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        return;
    }

    // The algorithm goal is two-fold: first, do not allocate any extra memory
    // to make the replacement; second, do it in a single pass. Depending on
    // the direction we can tackle the replacement and meet these two goals
    // at the same time if the text is being moved. Otherwise, we use the same
    // algorithm for u8TextReplace() but only the backwards case is needed.
    if (move) {
        if (nativeStart64 < nativeDest64) {
            int64_t i, j;
            for (i = nativeStart64; (i < nativeLimit64); i++) {
                uint8_t u8char = s[nativeStart64];
                for (j = nativeStart64; (j < nativeDest64 - 1); j++)
                    s[j] = s[j + 1];
                s[j] = u8char;
            }
        }
        else if (nativeStart64 > nativeDest64) {
            int64_t i, j;
            for (i = nativeLimit64 - 1; (i >= nativeStart64); i--) {
                uint8_t u8char = s[nativeLimit64 - 1];
                for (j = nativeLimit64 - 1; (j > nativeDest64); j--)
                    s[j] = s[j - 1];
                s[j] = u8char;
            }
        }
    }
    else {
        int64_t offset32 = nativeStart64 + (nativeStart64 > nativeDest64 ? diff64 : 0) - nativeDest64;
        int64_t i;
        for (i = length64 + diff64 - 1; (i >= nativeDest64 + diff64); i--)
            s[i] = s[i - diff64];
        for (; (i >= nativeDest64); i--)
            s[i] = s[offset32 + i];

        if (diff64) {
            ut->a += diff64;
        }
    }

    if (diff64) {
        utext_terminateChars(s, BC_AS_I64(ut), ut->a, pErrorCode);
    }

    int64_t nativeIndex64 = nativeDest64 + nativeLimit64 - nativeStart64;

    // Put iteration position at the newly inserted (moved) block.
    if ((move) && (nativeDest64 > nativeStart64)) {
        nativeIndex64 = nativeDest64;
    }

    utext_invalidateAccess(ut);
    u8InvalidateBuffers(ut);
    u8TextAccess(ut, nativeIndex64, TRUE);
}

static int64_t U_CALLCONV
u8TextMapOffsetToNative(const UText *ut)
{
    u8ChunkBuffer* activeBuffer = (u8ChunkBuffer*)ut->p;
    int64_t nativeOffset = ut->chunkNativeStart + (ut->chunkOffset + activeBuffer->chunkU16ToNative[ut->chunkOffset]);
    return nativeOffset;
}

static int32_t U_CALLCONV
u8TextMapIndexToUTF16(const UText *ut, int64_t nativeIndex)
{
    u8ChunkBuffer* activeBuffer = (u8ChunkBuffer*)ut->p;
    int32_t nativeOffset = (int32_t)(nativeIndex - ut->chunkNativeStart);
    int32_t chunkOffset = nativeOffset + activeBuffer->chunkNativeToU16[nativeOffset];
    return chunkOffset;
}

static void U_CALLCONV
u8TextClose(UText *ut)
{
    // Most of the work of close is done by the generic UText framework close.
    // All that needs to be done here is delete the string if the UText
    // owns it. This only occurs if the UText was created by u8TextClone().
    if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT)) != 0) {
        uint8_t* s = (uint8_t*)ut->context;
        uprv_free(s);
        ut->context = NULL;
        ut->chunkContents = NULL;
    }
}

static const struct UTextFuncs u8Funcs =
{
    sizeof(UTextFuncs),
    0, 0, 0,           // Reserved alignment padding
    u8TextClone,
    u8TextNativeLength,
    u8TextAccess,
    u8TextExtract,
    u8TextReplace,
    u8TextCopy,
    u8TextMapOffsetToNative,
    u8TextMapIndexToUTF16,
    u8TextClose,
    NULL,              // spare 1
    NULL,              // spare 2
    NULL,              // spare 3
};

U_CDECL_END

static const uint8_t gEmptyU8String[] = { 0 };

U_CAPI UText *U_EXPORT2
utext_openConstU8(UText *ut,
    const uint8_t* s, int64_t length, int64_t capacity,
    UErrorCode* pErrorCode)
{
    U_ASSERT(U8_CHUCK_TOLERANCE >= U8_MAX_LENGTH);
    U_ASSERT(U8_TEXT_CHUNK_SIZE - U8_CHUCK_TOLERANCE > U8_CHUCK_TOLERANCE);

    if (U_FAILURE(*pErrorCode)) {
        return NULL;
    }

    if ((s == NULL) && (length == 0)) {
        s = gEmptyU8String;
    }

    if ((s == NULL) || (length < -1) || (capacity < -1)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    ut = utext_setup(ut, sizeof(u8ChunkBuffer) * 2, pErrorCode);
    if (U_SUCCESS(*pErrorCode)) {
        ut->pFuncs = &u8Funcs;
        if (length == -1) {
            ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);
        }
        ut->context = (void*)s;
        ut->a = length < 0 ? 0 : length;
        BC_AS_I64(ut) = capacity;

        ut->p = ut->pExtra;
        ut->q = (void*)((uint8_t*)ut->pExtra + sizeof(u8ChunkBuffer));
    }
    return ut;
}

U_CAPI UText *U_EXPORT2
utext_openU8(UText *ut,
    uint8_t* s, int64_t length, int64_t capacity,
    UErrorCode* pErrorCode)
{
    if ((s == NULL) || (length < -1) || (capacity < 0)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    // Length must be known for write operations. Spend the
    // time now to figure it out.
    if (length < 0) {
        length = 0;
        for (; (length < capacity) && (s[length] != 0); length++) {
        }
    }

    ut = utext_openConstU8(ut, s, length, capacity, pErrorCode);
    if (U_SUCCESS(*pErrorCode)) {
        ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_WRITABLE);
    }

    return ut;
}

U_CAPI UText *U_EXPORT2
utext_openUTF8(UText *ut, const char* s, int64_t length, UErrorCode* pErrorCode)
{
    return utext_openConstU8(ut, (const uint8_t*)s, length, -1, pErrorCode);
}

//------------------------------------------------------------------------------
//
// UText implementation for const UChar32* (read-only)/UChar32* (read/write) strings.
//
// Use of UText data members:
//   context    pointer to const char*/char*
//   a          length of string.
//   d          length of buffer (read/write string only).
//   ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE) != 0
//              length of string is not known yet. ut->a can grow.
//   p          pointer to the active buffer.
//   q          pointer to the alternate buffer.
//
//------------------------------------------------------------------------------

// Sizes are in increments of sizeof(UChar32).
enum {
    U32_TEXT_CHUNK_SIZE = 32,
    U32_TEXT_CHUNK_SCAN_AHEAD = 32,
    U32_CHUCK_TOLERANCE = 1
};

struct u32ChunkBuffer {
    // Native index of first UChar in chunk.
    int64_t chunkNativeStart;

    // Native index following last UChar in chunk.
    int64_t chunkNativeLimit;

    // The UChar buffer. Requires extra space to allow for the difference between
    // encodings.
    // Tolerance is to allow growth at the beginning and the end of the
    // chunk to accomodate non-boundary aligned characters.
    UChar chunkContents[U32_TEXT_CHUNK_SIZE * U16_MAX_LENGTH + U32_CHUCK_TOLERANCE * 2];

    // Length of the text chunk in UChars.
    int32_t chunkLength;

    // The relative offset mapping from the chunk offset to the chunk native start.
    // Should be the same length as chunkContents.
    int8_t chunkU16ToNative[U32_TEXT_CHUNK_SIZE * U16_MAX_LENGTH + U32_CHUCK_TOLERANCE * 2];

    int8_t chunkNativeToU16[U32_TEXT_CHUNK_SIZE * U16_MAX_LENGTH + U32_CHUCK_TOLERANCE * 2];

    // The highest chunk offset where native indexing and chunk indexing correspond.
    int32_t nativeIndexingLimit;
};

U_CDECL_BEGIN

static UText * U_CALLCONV
u32TextClone(UText *dest, const UText *src,
    UBool deep, UErrorCode* pErrorCode)
{
    // First, do a generic shallow clone.
    dest = utext_shallowClone(dest, src, pErrorCode);

    if (deep && U_SUCCESS(*pErrorCode)) {
        // Next, for deep clones, make a copy of the string. The copied storage is
        // owned by the newly created clone. UTEXT_PROVIDER_OWNS_TEXT is the flag
        // to know that this needs to be free'd on u32TextClose().
        //
        // If the string is read-only, the cloned string IS going to be NUL
        // terminated, whether or not the original was. If the string is read/write
        // we know the buffer size ahead of time.
        const UChar32* s = (const UChar32*)src->context;
        int64_t length64 = 0;
        if ((dest->providerProperties & I32_FLAG(UTEXT_PROVIDER_WRITABLE)) != 0)
            length64 = BC_AS_I64(dest);
        else {
            // Avoid using u32TextLength() as this is a non-const function where
            // in cases where the input was NUL terminated and the length has not
            // yet been determined the UText could change.
            length64 = src->a;
            if (BC_AS_I64(src) < 0) {
                for (; (s[length64] != 0); length64++) {
                }
            }
            else {
                for (; (length64 < BC_AS_I64(src)) && (s[length64] != 0); length64++) {
                }
            }
            length64++;
        }

        UChar32* copyStr = (UChar32*)uprv_malloc(length64 * sizeof(UChar32));
        if (copyStr == NULL) {
            *pErrorCode = U_MEMORY_ALLOCATION_ERROR;
        }
        else {
            int64_t i;
            if (BC_AS_I64(src) < 0) {
                for (i = 0; (i < length64); i++) {
                    copyStr[i] = s[i];
                }
            }
            else {
                for (i = 0; (i < BC_AS_I64(src)) && (i < length64); i++) {
                    copyStr[i] = s[i];
                }
            }
            dest->context = (void*)copyStr;
            dest->providerProperties |= I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT);
        }
    }
    return dest;
}

static int64_t U_CALLCONV
u32TextNativeLength(UText *ut)
{
    if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) != 0) {
        // NUL terminated string and we don't yet know the length, so scan for it.
        //
        // Avoid using u32TextAccess() becuase we don't want to change the iteration
        // postion.
        const UChar32* s = (const UChar32*)ut->context;
        int64_t length64 = ut->a;
        if (BC_AS_I64(ut) < 0) {
            for (; (s[length64] != 0); length64++) {
            }
        }
        else {
            for (; (length64 < BC_AS_I64(ut)) && (s[length64] != 0); length64++) {
            }
        }
        ut->a = length64;
        ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);
    }
    return ut->a;
}

static int32_t U_CALLCONV
u32TextMapIndexToUTF16(const UText *ut, int64_t nativeIndex);

static int64_t U_CALLCONV
u32ScanLength(UText *ut, int64_t nativeLimit)
{
    if (nativeLimit >= ut->a) {
        if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE)) != 0) {
            const UChar32* s = (const UChar32*)ut->context;
            // NUL terminated string and we don't yet know the length. Requested nativeLimit
            // is beyond where we have scanned so far.
            //
            // Scan ahead beyond the requested nativeLimit. Strategy here is to avoid fully
            // scanning a long string when the caller only wants to see a few characters at
            // its beginning.
            int64_t scanLimit64 = nativeLimit + U16_TEXT_CHUNK_SCAN_AHEAD;
            if ((nativeLimit + U16_TEXT_CHUNK_SCAN_AHEAD) < 0) {
                scanLimit64 = INT64_MAX;
            }

            int64_t chunkLimit64 = (int32_t)ut->a;
            if (BC_AS_I64(ut) < 0) {
                for (; (s[chunkLimit64] != 0) && (chunkLimit64 < scanLimit64); chunkLimit64++) {
                }
            }
            else {
                for (; (chunkLimit64 < BC_AS_I64(ut)) && (s[chunkLimit64] != 0) && (chunkLimit64 < scanLimit64); chunkLimit64++) {
                }
            }
            ut->a = chunkLimit64;

            if (chunkLimit64 < scanLimit64) {
                // Found the end of the string. Turn off looking for the end in future calls.
                ut->providerProperties &= ~I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);

                if (nativeLimit > chunkLimit64) {
                    nativeLimit = chunkLimit64;
                }
            }
        }
        else {
            // We know the length of this string, and the user is requesting something
            // at or beyond the length. Pin the requested nativeIndex to the length.
            nativeLimit = ut->a;
        }
    }
    return nativeLimit;
}

static UBool U_CALLCONV
u32TextAccess(UText *ut,
    int64_t nativeIndex, UBool forward)
{
    const UChar32* s = (const UChar32*)ut->context;

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pin 'nativeStart' to a positive index, if it came in out-of-bounds.
    // Pin 'nativeStart64' to the adjusted length of the string, if it came in
    // out-of-bounds. We may need to scan ahead if the length is not known.
    // Snap 'nativeStart64' to the beginning of a code point.
    int64_t nativeIndex64 = utext_pinIndex64(nativeIndex, INT64_MAX);
    nativeIndex64 = u32ScanLength(ut, nativeIndex64);

    // Find the next chunk.
    //
    // Consider and determine potential [chunkNativeStart, chunkNativeLimit) to contain
    // the access request. The chunk may or may not become active during this access
    // request. It may wait until the next request and get swapped in.
    //
    // This approch provides a consistent chunking that is not relative to the index
    // but can reliably be arrived at everytime.
    u32ChunkBuffer* alternateBuffer = (u32ChunkBuffer*)ut->q;

    UBool prepareChunk = FALSE;
    int64_t chunkNativeStart64 = 0;
    int64_t chunkNativeLimit64 = 0;
    if ((nativeIndex64 >= ut->chunkNativeStart) && (nativeIndex64 <= ut->chunkNativeLimit)) {
        // Forward iteration request.
        if ((forward) && (nativeIndex64 <= ut->a)) {
            chunkNativeLimit64 = nativeIndex64;

            if ((chunkNativeLimit64 == ut->chunkNativeLimit) ||
                (chunkNativeLimit64 >= ut->chunkNativeLimit)) {
                chunkNativeStart64 = (chunkNativeLimit64 / U32_TEXT_CHUNK_SIZE) * U32_TEXT_CHUNK_SIZE;
                chunkNativeLimit64 = utext_pinIndex64(((chunkNativeLimit64 / U32_TEXT_CHUNK_SIZE) + 1) * U32_TEXT_CHUNK_SIZE, ut->a);
                prepareChunk = TRUE;
            }
        }
        // Backward iteration request.
        else if ((!forward) && (nativeIndex64 > 0)) {
            chunkNativeStart64 = nativeIndex64;

            if ((chunkNativeStart64 == ut->chunkNativeStart) ||
                (chunkNativeStart64 < ut->chunkNativeStart)) {
                chunkNativeLimit64 = utext_pinIndex64((chunkNativeStart64 / U32_TEXT_CHUNK_SIZE) * U32_TEXT_CHUNK_SIZE, ut->a);
                chunkNativeStart64 = ((chunkNativeStart64 / U32_TEXT_CHUNK_SIZE) - 1) * U32_TEXT_CHUNK_SIZE;
                prepareChunk = TRUE;
            }
        }
    }
    else {
        // Random access request.
        chunkNativeStart64 = (nativeIndex64 / U32_TEXT_CHUNK_SIZE) * U32_TEXT_CHUNK_SIZE;
        chunkNativeLimit64 = utext_pinIndex64(((nativeIndex64 / U32_TEXT_CHUNK_SIZE) + 1) * U32_TEXT_CHUNK_SIZE, ut->a);
        prepareChunk = TRUE;
    }

    // Prpeare next chunk.
    // 
    // Given the [chunkNativeStart64, chunkNativeLimit64) fill the alternate buffer with
    // the UChars that this span represents. Always fill forward the chunk regardless of
    // the direction. It makes chunkContents and nativeIndexing easier.
    if (prepareChunk) {
        if ((chunkNativeStart64 != alternateBuffer->chunkNativeStart) || (chunkNativeLimit64 != alternateBuffer->chunkNativeLimit)) {
            // Fill the chunk buffer and mapping arrays.
            alternateBuffer->nativeIndexingLimit = -1;

            int64_t si;
            int32_t di;
            UChar32 uchar;
            for (si = chunkNativeStart64, di = 0; si < chunkNativeLimit64; ) {
                uchar = s[si];
                if (U_IS_BMP(uchar)) {
                    if (di < U32_TEXT_CHUNK_SIZE * U16_MAX_LENGTH + U32_CHUCK_TOLERANCE * 2) {
                        alternateBuffer->chunkContents[di] = (UChar)uchar;
                        alternateBuffer->chunkU16ToNative[di] = (int8_t)((si - chunkNativeStart64) - di);
                        alternateBuffer->chunkNativeToU16[si - chunkNativeStart64] = (int8_t)(di - (si - chunkNativeStart64));
                    }
                    si++;
                    di++;
                }
                else {
                    if (alternateBuffer->nativeIndexingLimit < 0) {
                        alternateBuffer->nativeIndexingLimit = di;
                    }
                    int64_t savedDi = di;

                    if (di < U32_TEXT_CHUNK_SIZE * U16_MAX_LENGTH + U32_CHUCK_TOLERANCE * 2) {
                        alternateBuffer->chunkContents[di] = U16_LEAD(uchar);
                    }
                    di++;
                    if (di < U32_TEXT_CHUNK_SIZE * U16_MAX_LENGTH + U32_CHUCK_TOLERANCE * 2) {
                        alternateBuffer->chunkContents[di] = U16_TRAIL(uchar);
                    }
                    di++;

                    int64_t i;
                    for (i = savedDi; (di < U32_TEXT_CHUNK_SIZE * U16_MAX_LENGTH + U32_CHUCK_TOLERANCE * 2) && (i < di); i++) {
                        alternateBuffer->chunkU16ToNative[i] = (int8_t)((si - chunkNativeStart64) - i);
                    }
                    alternateBuffer->chunkNativeToU16[si - chunkNativeStart64] = (int8_t)(savedDi - (si - chunkNativeStart64));
                    si++;
                }
            }

            if (alternateBuffer->nativeIndexingLimit < 0) {
                alternateBuffer->nativeIndexingLimit = di;
            }
            alternateBuffer->chunkU16ToNative[di] = (int8_t)((si - chunkNativeStart64) - di);
            alternateBuffer->chunkNativeToU16[si - chunkNativeStart64] = (int8_t)(di - (si - chunkNativeStart64));

            alternateBuffer->chunkNativeStart = chunkNativeStart64;
            alternateBuffer->chunkNativeLimit = chunkNativeLimit64;
            alternateBuffer->chunkLength = di;

            UErrorCode errorCode = U_ZERO_ERROR;
            u_terminateUChars(alternateBuffer->chunkContents, U32_TEXT_CHUNK_SIZE * U16_MAX_LENGTH, di, &errorCode);
        }
    }

    // Check if we need to make a buffer change. Swap to the previosuly prepared buffer if
    // we are no longer in the active buffer.
    if ((nativeIndex64 >= alternateBuffer->chunkNativeStart) && (nativeIndex64 <= alternateBuffer->chunkNativeLimit)) {
        // Swap buffers
        ut->q = ut->p;
        ut->p = alternateBuffer;
        ut->chunkNativeStart = alternateBuffer->chunkNativeStart;
        ut->chunkNativeLimit = alternateBuffer->chunkNativeLimit;
        ut->chunkContents = &(alternateBuffer->chunkContents[0]);
        ut->chunkLength = alternateBuffer->chunkLength;
        ut->nativeIndexingLimit = alternateBuffer->nativeIndexingLimit;
    }

    // Set current iteration position using the code point adjusted one used
    // to figure out chunk boundaries.
    //
    // Convert this from the nativeIndex (u32) to the to the chunk contents index (u16).
    ut->chunkOffset = u32TextMapIndexToUTF16(ut, nativeIndex64);

    // Return whether the request is at the start and or end of the string.
    return ((forward) && (nativeIndex64 < ut->a)) || ((!forward) && (nativeIndex64 > 0));
}

static int32_t U_CALLCONV
u32TextExtract(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    UChar* dest, int32_t destCapacity,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if ((destCapacity < 0) ||
        ((dest == NULL) && (destCapacity > 0)) ||
        (nativeStart > nativeLimit)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    const UChar32* s = (UChar32*)ut->context;

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Snaps 'nativeStart64' to the beginning of a code point.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    // Snaps 'nativeLimit64' to the beginning of a code point.
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, ut->a);
    int64_t nativeLimit64 = u32ScanLength(ut, nativeLimit);

    // Because we are moving code points we may go over the requested limit in order
    // to include missing trail bytes.
    //
    // Since the destination is 32-bit, ensure that di never logically exceeds
    // INT32_MAX.
    int64_t si;
    int32_t di;
    UChar32 uchar;
    for (si = nativeStart64, di = 0; (si < nativeLimit64) && (di >= 0); ) {
        uchar = s[si++];
        if (U_IS_BMP(uchar)) {
            if (di < destCapacity) {
                dest[di] = (UChar)uchar;
            }
            di++;
        }
        else {
            if (di < destCapacity) {
                dest[di] = U16_LEAD(uchar);
            }
            di++;
            if (di < destCapacity) {
                dest[di] = U16_TRAIL(uchar);
            }
            di++;
        }
    }

    // Put iteration position at the point just following the extracted text.
    u32TextAccess(ut, si, TRUE);

    // Add a terminating NUL if space in the buffer permits, and set the error
    // status as required.
    u_terminateUChars(dest, destCapacity, di, pErrorCode);

    return di;
}

static void U_CALLCONV
u32InvalidateBuffers(UText *ut)
{
    u32ChunkBuffer* activeBuffer = (u32ChunkBuffer*)ut->p;
    activeBuffer->chunkLength = 0;
    activeBuffer->chunkNativeLimit = 0;
    activeBuffer->chunkNativeStart = 0;
    activeBuffer->nativeIndexingLimit = 0;

    u32ChunkBuffer* alternateBuffer = (u32ChunkBuffer*)ut->q;
    alternateBuffer->chunkLength = 0;
    alternateBuffer->chunkNativeLimit = 0;
    alternateBuffer->chunkNativeStart = 0;
    alternateBuffer->nativeIndexingLimit = 0;
}

static int32_t U_CALLCONV
u32TextReplace(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    const UChar* replacementText, int32_t replacementLength,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if ((replacementText == NULL) && (replacementLength != 0)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if (replacementLength < -1) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    if (nativeStart > nativeLimit) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    if (replacementLength < 0) {
        replacementLength = u_strlen(replacementText);
    }

    UChar32* s = (UChar32*)ut->context;

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    int64_t nativeReplLength64 = 0;
    if (replacementLength > 0) {
        int64_t i;
        UChar32 uchar;
        for (i = 0; i < replacementLength; ) {
            U16_NEXT_UNSAFE(replacementText, i, uchar);
            nativeReplLength64++;
        }
    }

    int64_t length64 = (int32_t)ut->a;
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, length64);
    int64_t nativeLimit64 = utext_pinIndex64(nativeLimit, length64);
    int64_t diff64 = nativeReplLength64 - (nativeLimit64 - nativeStart64);

    if (length64 + diff64 > BC_AS_I64(ut)) {
        *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        return 0;
    }

    // The algorithm goal is two-fold: first, do not allocate any extra memory
    // to make the replacement; second, do it in a single pass. Depending on
    // the direction we can tackle the replacement and meet these two goals
    // at the same time.
    if (nativeLimit64 - nativeStart64 < nativeReplLength64) {
        int64_t si;
        int32_t di;
        UChar32 uchar;
        for (si = length64 + diff64 - 1; (si >= nativeStart64 + nativeReplLength64); si--)
            s[si] = s[si - diff64];
        for (di = replacementLength; (di > 0) && (si >= nativeStart64); si--) {
            U16_PREV(replacementText, 0, di, uchar);
            s[si] = uchar;
        }
    }
    else {
        int64_t si;
        int32_t di;
        UChar32 uchar;
        for (si = (int64_t)nativeStart64, di = 0; (di < replacementLength) && (si < nativeStart64 + nativeReplLength64); si++) {
            U16_NEXT(replacementText, di, replacementLength, uchar);
            s[si] = uchar;
        }
        for (; (si < length64); si++)
            s[si] = s[si - diff64];
    }

    if ((nativeReplLength64 > 0) || (nativeLimit64 - nativeStart64 > 0)) {
        ut->a += diff64;

        utext_terminateUChars32(s, BC_AS_I64(ut), ut->a, pErrorCode);

        // Set the iteration position to the end of the newly inserted replacement text.
        utext_invalidateAccess(ut);
        u32InvalidateBuffers(ut);
        u32TextAccess(ut, nativeLimit64 + diff64, TRUE);
    }

    return (int32_t)diff64;
}

static void U_CALLCONV
u32TextCopy(UText *ut,
    int64_t nativeStart, int64_t nativeLimit,
    int64_t nativeDest,
    UBool move,
    UErrorCode* pErrorCode)
{
    if (U_FAILURE(*pErrorCode)) {
        return;
    }
    if (nativeStart > nativeLimit) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }

    // Pin the requested nativeIndex to the bounds of the string (not the chunk).
    // Pins 'nativeStart64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeLimit64' to the length of the string, if it came in out-of-bounds.
    // Pins 'nativeDest64' to the length of the string, if it came in out-of-bounds.
    UChar32* s = (UChar32*)ut->context;
    int64_t length64 = (int64_t)ut->a;
    int64_t nativeStart64 = utext_pinIndex64(nativeStart, length64);
    int64_t nativeLimit64 = utext_pinIndex64(nativeLimit, length64);
    int64_t nativeDest64 = utext_pinIndex64(nativeDest, length64);
    int64_t diff64 = (move ? 0 : nativeLimit64 - nativeStart64);

    // [nativeStart, nativeLimit) cannot overlap [dest, nativeLimit-nativeStart).
    if ((nativeDest64 > nativeStart64) && (nativeDest64 < nativeLimit64)) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }
    if (length64 + diff64 > BC_AS_I64(ut)) {
        *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
        return;
    }

    // The algorithm goal is two-fold: first, do not allocate any extra memory
    // to make the replacement; second, do it in a single pass. Depending on
    // the direction we can tackle the replacement and meet these two goals
    // at the same time if the text is being moved. Otherwise, we use the same
    // algorithm for u32TextReplace() but only the backwards case is needed.
    if (move) {
        if (nativeStart64 < nativeDest64) {
            int64_t i, j;
            for (i = nativeStart64; (i < nativeLimit64); i++) {
                UChar32 u32char = s[nativeStart64];
                for (j = nativeStart64; (j < nativeDest64 - 1); j++)
                    s[j] = s[j + 1];
                s[j] = u32char;
            }
        }
        else if (nativeStart64 > nativeDest64) {
            int64_t i, j;
            for (i = nativeLimit64 - 1; (i >= nativeStart64); i--) {
                UChar32 u32char = s[nativeLimit64 - 1];
                for (j = nativeLimit64 - 1; (j > nativeDest64); j--)
                    s[j] = s[j - 1];
                s[j] = u32char;
            }
        }
    }
    else {
        int64_t offset32 = nativeStart64 + (nativeStart64 > nativeDest64 ? diff64 : 0) - nativeDest64;
        int64_t i;
        for (i = length64 + diff64 - 1; (i >= nativeDest64 + diff64); i--)
            s[i] = s[i - diff64];
        for (; (i >= nativeDest64); i--)
            s[i] = s[offset32 + i];

        if (diff64) {
            ut->a += diff64;
        }
    }

    if (diff64) {
        utext_terminateUChars32(s, BC_AS_I64(ut), ut->a, pErrorCode);
    }

    // Put iteration position at the newly inserted (moved) block.
    int64_t nativeIndex64 = nativeDest64 + nativeLimit64 - nativeStart64;
    if ((move) && (nativeDest64 > nativeStart64)) {
        nativeIndex64 = nativeDest64;
    }

    utext_invalidateAccess(ut);
    u32InvalidateBuffers(ut);
    u32TextAccess(ut, nativeIndex64, TRUE);
}

static int64_t U_CALLCONV
u32TextMapOffsetToNative(const UText *ut)
{
    u32ChunkBuffer* activeBuffer = (u32ChunkBuffer*)ut->p;
    int64_t nativeOffset = ut->chunkNativeStart + (ut->chunkOffset + activeBuffer->chunkU16ToNative[ut->chunkOffset]);
    return nativeOffset;
}

static int32_t U_CALLCONV
u32TextMapIndexToUTF16(const UText *ut, int64_t nativeIndex)
{
    u32ChunkBuffer* activeBuffer = (u32ChunkBuffer*)ut->p;
    int32_t nativeOffset = (int32_t)(nativeIndex - ut->chunkNativeStart);
    int32_t chunkOffset = nativeOffset + activeBuffer->chunkNativeToU16[nativeOffset];
    return chunkOffset;
}

static void U_CALLCONV
u32TextClose(UText *ut)
{
    // Most of the work of close is done by the generic UText framework close.
    // All that needs to be done here is delete the string if the UText
    // owns it. This only occurs if the UText was created by u32TextClone().
    if ((ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT)) != 0) {
        UChar32* s = (UChar32*)ut->context;
        uprv_free(s);
        ut->context = NULL;
        ut->chunkContents = NULL;
    }
}

static const struct UTextFuncs u32Funcs =
{
    sizeof(UTextFuncs),
    0, 0, 0,           // Reserved alignment padding
    u32TextClone,
    u32TextNativeLength,
    u32TextAccess,
    u32TextExtract,
    u32TextReplace,
    u32TextCopy,
    u32TextMapOffsetToNative,
    u32TextMapIndexToUTF16,
    u32TextClose,
    NULL,              // spare 1
    NULL,              // spare 2
    NULL,              // spare 3
};

U_CDECL_END

static const UChar32 gEmptyU32String[] = { 0 };

U_CAPI UText * U_EXPORT2
utext_openConstU32(UText *ut,
    const UChar32* s, int64_t length, int64_t capacity,
    UErrorCode* pErrorCode)
{
    U_ASSERT(U16_CHUCK_TOLERANCE >= U16_MAX_LENGTH);
    U_ASSERT(U16_TEXT_CHUNK_SIZE - U16_CHUCK_TOLERANCE > U16_CHUCK_TOLERANCE);

    if (U_FAILURE(*pErrorCode)) {
        return NULL;
    }

    if ((s == NULL) && (length == 0)) {
        s = gEmptyU32String;
    }

    if ((s == NULL) || (length < -1) || (capacity < -1)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    ut = utext_setup(ut, sizeof(u32ChunkBuffer) * 2, pErrorCode);
    if (U_SUCCESS(*pErrorCode)) {
        ut->pFuncs = &u32Funcs;
        if (length == -1) {
            ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE);
        }
        if ((length == -1) || (length < U16_TEXT_CHUNK_SIZE)) {
            ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_STABLE_CHUNKS);
        }
        ut->context = (void*)s;
        ut->a = length < 0 ? 0 : length;
        BC_AS_I64(ut) = capacity;

        ut->p = ut->pExtra;
        ut->q = (void*)((uint8_t*)ut->pExtra + sizeof(u32ChunkBuffer));
    }
    return ut;
}

U_CAPI UText * U_EXPORT2
utext_openU32(UText *ut,
    UChar32* s, int64_t length, int64_t capacity,
    UErrorCode* pErrorCode)
{
    if ((s == NULL) || (length < -1) || (capacity < 0)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    // Length must be known for write operations. Spend the
    // time now to figure it out.
    if (length < 0) {
        length = 0;
        for (; (length < capacity) && (s[length] != 0); length++) {
        }
    }

    ut = utext_openConstU32(ut, s, length, capacity, pErrorCode);
    if (U_SUCCESS(*pErrorCode)) {
        ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_WRITABLE);
    }

    return ut;
}

//------------------------------------------------------------------------------
//
// UText implementation for UnicodeString (read/write) and
// for const UnicodeString (read only)
// (same implementation, only the flags are different)
//
// Use of UText data members:
//   context    pointer to UnicodeString
//   p          pointer to UnicodeString IF this UText owns the string
//              and it must be deleted on close().  NULL otherwise.
//
//------------------------------------------------------------------------------

U_CDECL_BEGIN

static UText * U_CALLCONV
unistrTextClone(UText *dest, const UText *src, UBool deep, UErrorCode *status) {
    // First do a generic shallow clone.  Does everything needed for the UText struct itself.
    dest = utext_shallowClone(dest, src, status);

    // For deep clones, make a copy of the UnicodeSring.
    //  The copied UnicodeString storage is owned by the newly created UText clone.
    //  A non-NULL pointer in UText.p is the signal to the close() function to delete
    //    the UText.
    //
    if (deep && U_SUCCESS(*status)) {
        const UnicodeString *srcString = (const UnicodeString *)src->context;
        dest->context = new UnicodeString(*srcString);
        dest->providerProperties |= I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT);

        // with deep clone, the copy is writable, even when the source is not.
        dest->providerProperties |= I32_FLAG(UTEXT_PROVIDER_WRITABLE);
    }
    return dest;
}

static void U_CALLCONV
unistrTextClose(UText *ut) {
    // Most of the work of close is done by the generic UText framework close.
    // All that needs to be done here is delete the UnicodeString if the UText
    //  owns it.  This occurs if the UText was created by cloning.
    if (ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT)) {
        UnicodeString *str = (UnicodeString *)ut->context;
        delete str;
        ut->context = NULL;
    }
}

static int64_t U_CALLCONV
unistrTextLength(UText *t) {
    return ((const UnicodeString *)t->context)->length();
}

static UBool U_CALLCONV
unistrTextAccess(UText *ut, int64_t index, UBool  forward) {
    int32_t length = ut->chunkLength;
    ut->chunkOffset = utext_pinIndex32(index, length);

    // Check whether request is at the start or end
    UBool retVal = (forward && index < length) || (!forward && index > 0);
    return retVal;
}

static int32_t U_CALLCONV
unistrTextExtract(UText *t,
    int64_t start, int64_t limit,
    UChar *dest, int32_t destCapacity,
    UErrorCode *pErrorCode) {
    const UnicodeString *us = (const UnicodeString *)t->context;
    int32_t length = us->length();

    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if (destCapacity < 0 || (dest == NULL && destCapacity > 0)) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
    }
    if (start<0 || start>limit) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    int32_t start32 = start < length ? us->getChar32Start((int32_t)start) : length;
    int32_t limit32 = limit < length ? us->getChar32Start((int32_t)limit) : length;

    length = limit32 - start32;
    if (destCapacity > 0 && dest != NULL) {
        int32_t trimmedLength = length;
        if (trimmedLength > destCapacity) {
            trimmedLength = destCapacity;
        }
        us->extract(start32, trimmedLength, dest);
        t->chunkOffset = start32 + trimmedLength;
    }
    else {
        t->chunkOffset = start32;
    }
    u_terminateUChars(dest, destCapacity, length, pErrorCode);
    return length;
}

static int32_t U_CALLCONV
unistrTextReplace(UText *ut,
    int64_t start, int64_t limit,
    const UChar *src, int32_t length,
    UErrorCode *pErrorCode) {
    UnicodeString *us = (UnicodeString *)ut->context;
    int32_t oldLength;

    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if (src == NULL && length != 0) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
    }
    if (start > limit) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }
    oldLength = us->length();
    int32_t start32 = utext_pinIndex32(start, oldLength);
    int32_t limit32 = utext_pinIndex32(limit, oldLength);
    if (start32 < oldLength) {
        start32 = us->getChar32Start(start32);
    }
    if (limit32 < oldLength) {
        limit32 = us->getChar32Start(limit32);
    }

    // replace
    us->replace(start32, limit32 - start32, src, length);
    int32_t newLength = us->length();

    // Update the chunk description.
    ut->chunkContents = us->getBuffer();
    ut->chunkLength = newLength;
    ut->chunkNativeLimit = newLength;
    ut->nativeIndexingLimit = newLength;

    // Set iteration position to the point just following the newly inserted text.
    int32_t lengthDelta = newLength - oldLength;
    ut->chunkOffset = limit32 + lengthDelta;

    return lengthDelta;
}

static void U_CALLCONV
unistrTextCopy(UText *ut,
    int64_t start, int64_t limit,
    int64_t destIndex,
    UBool move,
    UErrorCode *pErrorCode) {
    UnicodeString *us = (UnicodeString *)ut->context;
    int32_t length = us->length();

    if (U_FAILURE(*pErrorCode)) {
        return;
    }
    int32_t start32 = utext_pinIndex32(start, length);
    int32_t limit32 = utext_pinIndex32(limit, length);
    int32_t destIndex32 = utext_pinIndex32(destIndex, length);

    if (start32 > limit32 || (start32 < destIndex32 && destIndex32 < limit32)) {
        *pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }

    if (move) {
        // move: copy to destIndex, then remove original
        int32_t segLength = limit32 - start32;
        us->copy(start32, limit32, destIndex32);
        if (destIndex32 < start32) {
            start32 += segLength;
        }
        us->remove(start32, segLength);
    }
    else {
        // copy
        us->copy(start32, limit32, destIndex32);
    }

    // update chunk description, set iteration position.
    ut->chunkContents = us->getBuffer();
    if (move == FALSE) {
        // copy operation, string length grows
        ut->chunkLength += limit32 - start32;
        ut->chunkNativeLimit = ut->chunkLength;
        ut->nativeIndexingLimit = ut->chunkLength;
    }

    // Iteration position to end of the newly inserted text.
    ut->chunkOffset = destIndex32 + limit32 - start32;
    if (move && destIndex32 > start32) {
        ut->chunkOffset = destIndex32;
    }

}

static const struct UTextFuncs unistrFuncs =
{
    sizeof(UTextFuncs),
    0, 0, 0,             // Reserved alignment padding
    unistrTextClone,
    unistrTextLength,
    unistrTextAccess,
    unistrTextExtract,
    unistrTextReplace,
    unistrTextCopy,
    NULL,                // MapOffsetToNative,
    NULL,                // MapIndexToUTF16,
    unistrTextClose,
    NULL,                // spare 1
    NULL,                // spare 2
    NULL                 // spare 3
};

U_CDECL_END

U_CAPI UText * U_EXPORT2
utext_openUnicodeString(UText *ut, UnicodeString *s, UErrorCode *status) {
    ut = utext_openConstUnicodeString(ut, s, status);
    if (U_SUCCESS(*status)) {
        ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_WRITABLE);
    }
    return ut;
}

U_CAPI UText * U_EXPORT2
utext_openConstUnicodeString(UText *ut, const UnicodeString *s, UErrorCode *status) {
    if (U_SUCCESS(*status) && s->isBogus()) {
        // The UnicodeString is bogus, but we still need to detach the UText
        //   from whatever it was hooked to before, if anything.
        utext_openUChars(ut, NULL, 0, status);
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return ut;
    }
    ut = utext_setup(ut, 0, status);
    //    note:  use the standard (writable) function table for UnicodeString.
    //           The flag settings disable writing, so having the functions in
    //           the table is harmless.
    if (U_SUCCESS(*status)) {
        ut->pFuncs = &unistrFuncs;
        ut->context = s;
        ut->providerProperties = I32_FLAG(UTEXT_PROVIDER_STABLE_CHUNKS);
        ut->chunkContents = s->getBuffer();
        ut->chunkLength = s->length();
        ut->chunkNativeStart = 0;
        ut->chunkNativeLimit = ut->chunkLength;
        ut->nativeIndexingLimit = ut->chunkLength;
    }
    return ut;
}

//------------------------------------------------------------------------------
//
//     UText implementation for text from ICU CharacterIterators
//
//         Use of UText data members:
//            context    pointer to the CharacterIterator
//            a          length of the full text.
//            p          pointer to  buffer 1
//            b          start index of local buffer 1 contents
//            q          pointer to buffer 2
//            c          start index of local buffer 2 contents
//            r          pointer to the character iterator if the UText owns it.
//                       Null otherwise.
//
//------------------------------------------------------------------------------

#define CIBufSize 16

U_CDECL_BEGIN

static void U_CALLCONV
charIterTextClose(UText *ut) {
    // Most of the work of close is done by the generic UText framework close.
    // All that needs to be done here is delete the CharacterIterator if the UText
    //  owns it.  This occurs if the UText was created by cloning.
    CharacterIterator *ci = (CharacterIterator *)ut->r;
    delete ci;
    ut->r = NULL;
}

static int64_t U_CALLCONV
charIterTextLength(UText *ut) {
    return (int32_t)ut->a;
}

static UBool U_CALLCONV
charIterTextAccess(UText *ut, int64_t index, UBool  forward) {
    CharacterIterator *ci = (CharacterIterator *)ut->context;

    int32_t clippedIndex = (int32_t)index;
    if (clippedIndex < 0) {
        clippedIndex = 0;
    }
    else if (clippedIndex >= ut->a) {
        clippedIndex = (int32_t)ut->a;
    }
    int32_t neededIndex = clippedIndex;
    if (!forward && neededIndex > 0) {
        // reverse iteration, want the position just before what was asked for.
        neededIndex--;
    }
    else if (forward && neededIndex == ut->a && neededIndex > 0) {
        // Forward iteration, don't ask for something past the end of the text.
        neededIndex--;
    }

    // Find the native index of the start of the buffer containing what we want.
    neededIndex -= neededIndex % CIBufSize;

    UChar *buf = NULL;
    UBool  needChunkSetup = TRUE;
    int    i;
    if (ut->chunkNativeStart == neededIndex) {
        // The buffer we want is already the current chunk.
        needChunkSetup = FALSE;
    }
    else if (ut->b == neededIndex) {
        // The first buffer (buffer p) has what we need.
        buf = (UChar *)ut->p;
    }
    else if (ut->c == neededIndex) {
        // The second buffer (buffer q) has what we need.
        buf = (UChar *)ut->q;
    }
    else {
        // Neither buffer already has what we need.
        // Load new data from the character iterator.
        // Use the buf that is not the current buffer.
        buf = (UChar *)ut->p;
        if (ut->p == ut->chunkContents) {
            buf = (UChar *)ut->q;
        }
        ci->setIndex(neededIndex);
        for (i = 0; i < CIBufSize; i++) {
            buf[i] = ci->nextPostInc();
            if (i + neededIndex > ut->a) {
                break;
            }
        }
    }

    // We have a buffer with the data we need.
    // Set it up as the current chunk, if it wasn't already.
    if (needChunkSetup) {
        ut->chunkContents = buf;
        ut->chunkLength = CIBufSize;
        ut->chunkNativeStart = neededIndex;
        ut->chunkNativeLimit = neededIndex + CIBufSize;
        if (ut->chunkNativeLimit > ut->a) {
            ut->chunkNativeLimit = ut->a;
            ut->chunkLength = (int32_t)(ut->chunkNativeLimit) - (int32_t)(ut->chunkNativeStart);
        }
        ut->nativeIndexingLimit = ut->chunkLength;
        U_ASSERT(ut->chunkOffset >= 0 && ut->chunkOffset <= CIBufSize);
    }
    ut->chunkOffset = clippedIndex - (int32_t)ut->chunkNativeStart;
    UBool success = (forward ? ut->chunkOffset < ut->chunkLength : ut->chunkOffset>0);
    return success;
}

static UText * U_CALLCONV
charIterTextClone(UText *dest, const UText *src, UBool deep, UErrorCode * status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }

    if (deep) {
        // There is no CharacterIterator API for cloning the underlying text storage.
        *status = U_UNSUPPORTED_ERROR;
        return NULL;
    }
    else {
        CharacterIterator *srcCI = (CharacterIterator *)src->context;
        srcCI = srcCI->clone();
        dest = utext_openCharacterIterator(dest, srcCI, status);
        if (U_FAILURE(*status)) {
            return dest;
        }
        // cast off const on getNativeIndex.
        //   For CharacterIterator based UTexts, this is safe, the operation is const.
        int64_t  ix = utext_getNativeIndex((UText *)src);
        utext_setNativeIndex(dest, ix);
        dest->r = srcCI;    // flags that this UText owns the CharacterIterator
    }
    return dest;
}

static int32_t U_CALLCONV
charIterTextExtract(UText *ut,
    int64_t start, int64_t limit,
    UChar *dest, int32_t destCapacity,
    UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return 0;
    }
    if (destCapacity<0 || (dest == NULL && destCapacity > 0) || start>limit) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    int32_t  length = (int32_t)ut->a;
    int32_t  start32 = utext_pinIndex32(start, length);
    int32_t  limit32 = utext_pinIndex32(limit, length);
    int32_t  desti = 0;
    int32_t  srci;
    int32_t  copyLimit;

    CharacterIterator *ci = (CharacterIterator *)ut->context;
    ci->setIndex32(start32);   // Moves ix to lead of surrogate pair, if needed.
    srci = ci->getIndex();
    copyLimit = srci;
    while (srci < limit32) {
        UChar32 c = ci->next32PostInc();
        int32_t  len = U16_LENGTH(c);
        U_ASSERT(desti + len > 0); /* to ensure desti+len never exceeds MAX_INT32, which must not happen logically */
        if (desti + len <= destCapacity) {
            U16_APPEND_UNSAFE(dest, desti, c);
            copyLimit = srci + len;
        }
        else {
            desti += len;
            *status = U_BUFFER_OVERFLOW_ERROR;
        }
        srci += len;
    }

    charIterTextAccess(ut, copyLimit, TRUE);

    u_terminateUChars(dest, destCapacity, desti, status);
    return desti;
}

static const struct UTextFuncs charIterFuncs =
{
    sizeof(UTextFuncs),
    0, 0, 0,             // Reserved alignment padding
    charIterTextClone,
    charIterTextLength,
    charIterTextAccess,
    charIterTextExtract,
    NULL,                // Replace
    NULL,                // Copy
    NULL,                // MapOffsetToNative,
    NULL,                // MapIndexToUTF16,
    charIterTextClose,
    NULL,                // spare 1
    NULL,                // spare 2
    NULL                 // spare 3
};

U_CDECL_END

U_CAPI UText * U_EXPORT2
utext_openCharacterIterator(UText *ut, CharacterIterator *ci, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }

    if (ci->startIndex() > 0) {
        // No support for CharacterIterators that do not start indexing from zero.
        *status = U_UNSUPPORTED_ERROR;
        return NULL;
    }

    // Extra space in UText for 2 buffers of CIBufSize UChars each.
    int32_t  extraSpace = 2 * CIBufSize * sizeof(UChar);
    ut = utext_setup(ut, extraSpace, status);
    if (U_SUCCESS(*status)) {
        ut->pFuncs = &charIterFuncs;
        ut->context = ci;
        ut->providerProperties = 0;
        ut->a = ci->endIndex();        // Length of text
        ut->p = ut->pExtra;            // First buffer
        ut->b = -1;                    // Native index of first buffer contents
        ut->q = (UChar*)ut->pExtra + CIBufSize;  // Second buffer
        ut->c = -1;                    // Native index of second buffer contents

        // Initialize current chunk contents to be empty.
        //   First access will fault something in.
        //   Note:  The initial nativeStart and chunkOffset must sum to zero
        //          so that getNativeIndex() will correctly compute to zero
        //          if no call to Access() has ever been made.  They can't be both
        //          zero without Access() thinking that the chunk is valid.
        ut->chunkContents = (UChar *)ut->p;
        ut->chunkNativeStart = -1;
        ut->chunkOffset = 1;
        ut->chunkNativeLimit = 0;
        ut->chunkLength = 0;
        ut->nativeIndexingLimit = ut->chunkOffset;  // enables native indexing
    }
    return ut;
}

//------------------------------------------------------------------------------
//
//     UText implementation wrapper for Replaceable (read/write)
//
//         Use of UText data members:
//            context    pointer to Replaceable.
//            p          pointer to Replaceable if it is owned by the UText.
//
//------------------------------------------------------------------------------

// minimum chunk size for this implementation: 3
// to allow for possible trimming for code point boundaries
enum { REP_TEXT_CHUNK_SIZE = 10 };

struct ReplExtra {
    /*
     * Chunk UChars.
     * +1 to simplify filling with surrogate pair at the end.
     */
    UChar s[REP_TEXT_CHUNK_SIZE + 1];
};

U_CDECL_BEGIN

static UText * U_CALLCONV
repTextClone(UText *dest, const UText *src, UBool deep, UErrorCode *status) {
    // First do a generic shallow clone.  Does everything needed for the UText struct itself.
    dest = utext_shallowClone(dest, src, status);

    // For deep clones, make a copy of the Replaceable.
    //  The copied Replaceable storage is owned by the newly created UText clone.
    //  A non-NULL pointer in UText.p is the signal to the close() function to delete
    //    it.
    //
    if (deep && U_SUCCESS(*status)) {
        const Replaceable *replSrc = (const Replaceable *)src->context;
        dest->context = replSrc->clone();
        dest->providerProperties |= I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT);

        // with deep clone, the copy is writable, even when the source is not.
        dest->providerProperties |= I32_FLAG(UTEXT_PROVIDER_WRITABLE);
    }
    return dest;
}

static void U_CALLCONV
repTextClose(UText *ut) {
    // Most of the work of close is done by the generic UText framework close.
    // All that needs to be done here is delete the Replaceable if the UText
    //  owns it.  This occurs if the UText was created by cloning.
    if (ut->providerProperties & I32_FLAG(UTEXT_PROVIDER_OWNS_TEXT)) {
        Replaceable *rep = (Replaceable *)ut->context;
        delete rep;
        ut->context = NULL;
    }
}

static int64_t U_CALLCONV
repTextLength(UText *ut) {
    const Replaceable *replSrc = (const Replaceable *)ut->context;
    int32_t  len = replSrc->length();
    return len;
}

static UBool U_CALLCONV
repTextAccess(UText *ut, int64_t index, UBool forward) {
    const Replaceable *rep = (const Replaceable *)ut->context;
    int32_t length = rep->length();   // Full length of the input text (bigger than a chunk)

    // clip the requested index to the limits of the text.
    int32_t index32 = utext_pinIndex32(index, length);
    U_ASSERT(index <= INT32_MAX);


    /*
     * Compute start/limit boundaries around index, for a segment of text
     * to be extracted.
     * To allow for the possibility that our user gave an index to the trailing
     * half of a surrogate pair, we must request one extra preceding UChar when
     * going in the forward direction.  This will ensure that the buffer has the
     * entire code point at the specified index.
     */
    if (forward) {

        if (index32 >= ut->chunkNativeStart && index32 < ut->chunkNativeLimit) {
            // Buffer already contains the requested position.
            ut->chunkOffset = (int32_t)(index - ut->chunkNativeStart);
            return TRUE;
        }
        if (index32 >= length && ut->chunkNativeLimit == length) {
            // Request for end of string, and buffer already extends up to it.
            // Can't get the data, but don't change the buffer.
            ut->chunkOffset = length - (int32_t)ut->chunkNativeStart;
            return FALSE;
        }

        ut->chunkNativeLimit = index + REP_TEXT_CHUNK_SIZE - 1;
        // Going forward, so we want to have the buffer with stuff at and beyond
        //   the requested index.  The -1 gets us one code point before the
        //   requested index also, to handle the case of the index being on
        //   a trail surrogate of a surrogate pair.
        if (ut->chunkNativeLimit > length) {
            ut->chunkNativeLimit = length;
        }
        // unless buffer ran off end, start is index-1.
        ut->chunkNativeStart = ut->chunkNativeLimit - REP_TEXT_CHUNK_SIZE;
        if (ut->chunkNativeStart < 0) {
            ut->chunkNativeStart = 0;
        }
    }
    else {
        // Reverse iteration.  Fill buffer with data preceding the requested index.
        if (index32 > ut->chunkNativeStart && index32 <= ut->chunkNativeLimit) {
            // Requested position already in buffer.
            ut->chunkOffset = index32 - (int32_t)ut->chunkNativeStart;
            return TRUE;
        }
        if (index32 == 0 && ut->chunkNativeStart == 0) {
            // Request for start, buffer already begins at start.
            //  No data, but keep the buffer as is.
            ut->chunkOffset = 0;
            return FALSE;
        }

        // Figure out the bounds of the chunk to extract for reverse iteration.
        // Need to worry about chunk not splitting surrogate pairs, and while still
        // containing the data we need.
        // Fix by requesting a chunk that includes an extra UChar at the end.
        // If this turns out to be a lead surrogate, we can lop it off and still have
        //   the data we wanted.
        ut->chunkNativeStart = index32 + 1 - REP_TEXT_CHUNK_SIZE;
        if (ut->chunkNativeStart < 0) {
            ut->chunkNativeStart = 0;
        }

        ut->chunkNativeLimit = index32 + 1;
        if (ut->chunkNativeLimit > length) {
            ut->chunkNativeLimit = length;
        }
    }

    // Extract the new chunk of text from the Replaceable source.
    ReplExtra *ex = (ReplExtra *)ut->pExtra;
    // UnicodeString with its buffer a writable alias to the chunk buffer
    UnicodeString buffer(ex->s, 0 /*buffer length*/, REP_TEXT_CHUNK_SIZE /*buffer capacity*/);
    rep->extractBetween((int32_t)ut->chunkNativeStart, (int32_t)ut->chunkNativeLimit, buffer);

    ut->chunkContents = ex->s;
    ut->chunkLength = (int32_t)(ut->chunkNativeLimit - ut->chunkNativeStart);
    ut->chunkOffset = (int32_t)(index32 - ut->chunkNativeStart);

    // Surrogate pairs from the input text must not span chunk boundaries.
    // If end of chunk could be the start of a surrogate, trim it off.
    if (ut->chunkNativeLimit < length &&
        U16_IS_LEAD(ex->s[ut->chunkLength - 1])) {
        ut->chunkLength--;
        ut->chunkNativeLimit--;
        if (ut->chunkOffset > ut->chunkLength) {
            ut->chunkOffset = ut->chunkLength;
        }
    }

    // if the first UChar in the chunk could be the trailing half of a surrogate pair,
    // trim it off.
    if (ut->chunkNativeStart > 0 && U16_IS_TRAIL(ex->s[0])) {
        ++(ut->chunkContents);
        ++(ut->chunkNativeStart);
        --(ut->chunkLength);
        --(ut->chunkOffset);
    }

    // adjust the index/chunkOffset to a code point boundary
    U16_SET_CP_START(ut->chunkContents, 0, ut->chunkOffset);

    // Use fast indexing for get/setNativeIndex()
    ut->nativeIndexingLimit = ut->chunkLength;

    return TRUE;
}

static int32_t U_CALLCONV
repTextExtract(UText *ut,
    int64_t start, int64_t limit,
    UChar *dest, int32_t destCapacity,
    UErrorCode *status) {
    const Replaceable *rep = (const Replaceable *)ut->context;
    int32_t  length = rep->length();

    if (U_FAILURE(*status)) {
        return 0;
    }
    if (destCapacity < 0 || (dest == NULL && destCapacity > 0)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    if (start > limit) {
        *status = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    int32_t  start32 = utext_pinIndex32(start, length);
    int32_t  limit32 = utext_pinIndex32(limit, length);

    // adjust start, limit if they point to trail half of surrogates
    if (start32 < length && U16_IS_TRAIL(rep->charAt(start32)) &&
        U_IS_SUPPLEMENTARY(rep->char32At(start32))) {
        start32--;
    }
    if (limit32 < length && U16_IS_TRAIL(rep->charAt(limit32)) &&
        U_IS_SUPPLEMENTARY(rep->char32At(limit32))) {
        limit32--;
    }

    length = limit32 - start32;
    if (length > destCapacity) {
        limit32 = start32 + destCapacity;
    }
    UnicodeString buffer(dest, 0, destCapacity); // writable alias
    rep->extractBetween(start32, limit32, buffer);
    repTextAccess(ut, limit32, TRUE);

    return u_terminateUChars(dest, destCapacity, length, status);
}

static int32_t U_CALLCONV
repTextReplace(UText *ut,
    int64_t start, int64_t limit,
    const UChar *src, int32_t length,
    UErrorCode *status) {
    Replaceable *rep = (Replaceable *)ut->context;
    int32_t oldLength;

    if (U_FAILURE(*status)) {
        return 0;
    }
    if (src == NULL && length != 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    oldLength = rep->length(); // will subtract from new length
    if (start > limit) {
        *status = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }

    int32_t start32 = utext_pinIndex32(start, oldLength);
    int32_t limit32 = utext_pinIndex32(limit, oldLength);

    // Snap start & limit to code point boundaries.
    if (start32 < oldLength && U16_IS_TRAIL(rep->charAt(start32)) &&
        start32>0 && U16_IS_LEAD(rep->charAt(start32 - 1)))
    {
        start32--;
    }
    if (limit32 < oldLength && U16_IS_LEAD(rep->charAt(limit32 - 1)) &&
        U16_IS_TRAIL(rep->charAt(limit32)))
    {
        limit32++;
    }

    // Do the actual replace operation using methods of the Replaceable class
    UnicodeString replStr((UBool)(length < 0), src, length); // read-only alias
    rep->handleReplaceBetween(start32, limit32, replStr);
    int32_t newLength = rep->length();
    int32_t lengthDelta = newLength - oldLength;

    // Is the UText chunk buffer OK?
    if (ut->chunkNativeLimit > start32) {
        // this replace operation may have impacted the current chunk.
        // invalidate it, which will force a reload on the next access.
        utext_invalidateAccess(ut);
    }

    // set the iteration position to the end of the newly inserted replacement text.
    int32_t newIndexPos = limit32 + lengthDelta;
    repTextAccess(ut, newIndexPos, TRUE);

    return lengthDelta;
}

static void U_CALLCONV
repTextCopy(UText *ut,
    int64_t start, int64_t limit,
    int64_t destIndex,
    UBool move,
    UErrorCode *status)
{
    Replaceable *rep = (Replaceable *)ut->context;
    int32_t length = rep->length();

    if (U_FAILURE(*status)) {
        return;
    }
    if (start > limit || (start < destIndex && destIndex < limit))
    {
        *status = U_INDEX_OUTOFBOUNDS_ERROR;
        return;
    }

    int32_t start32 = utext_pinIndex32(start, length);
    int32_t limit32 = utext_pinIndex32(limit, length);
    int32_t destIndex32 = utext_pinIndex32(destIndex, length);

    // TODO:  snap input parameters to code point boundaries.

    if (move) {
        // move: copy to destIndex, then replace original with nothing
        int32_t segLength = limit32 - start32;
        rep->copy(start32, limit32, destIndex32);
        if (destIndex32 < start32) {
            start32 += segLength;
            limit32 += segLength;
        }
        rep->handleReplaceBetween(start32, limit32, UnicodeString());
    }
    else {
        // copy
        rep->copy(start32, limit32, destIndex32);
    }

    // If the change to the text touched the region in the chunk buffer,
    //  invalidate the buffer.
    int32_t firstAffectedIndex = destIndex32;
    if (move && start32 < firstAffectedIndex) {
        firstAffectedIndex = start32;
    }
    if (firstAffectedIndex < ut->chunkNativeLimit) {
        // changes may have affected range covered by the chunk
        utext_invalidateAccess(ut);
    }

    // Put iteration position at the newly inserted (moved) block,
    int32_t  nativeIterIndex = destIndex32 + limit32 - start32;
    if (move && destIndex32 > start32) {
        // moved a block of text towards the end of the string.
        nativeIterIndex = destIndex32;
    }

    // Set position, reload chunk if needed.
    repTextAccess(ut, nativeIterIndex, TRUE);
}

static const struct UTextFuncs repFuncs =
{
    sizeof(UTextFuncs),
    0, 0, 0,           // Reserved alignment padding
    repTextClone,
    repTextLength,
    repTextAccess,
    repTextExtract,
    repTextReplace,
    repTextCopy,
    NULL,              // MapOffsetToNative,
    NULL,              // MapIndexToUTF16,
    repTextClose,
    NULL,              // spare 1
    NULL,              // spare 2
    NULL               // spare 3
};

U_CAPI UText * U_EXPORT2
utext_openReplaceable(UText *ut, Replaceable *rep, UErrorCode *status)
{
    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (rep == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    ut = utext_setup(ut, sizeof(ReplExtra), status);
    if (U_FAILURE(*status)) {
        return ut;
    }

    ut->providerProperties = I32_FLAG(UTEXT_PROVIDER_WRITABLE);
    if (rep->hasMetaData()) {
        ut->providerProperties |= I32_FLAG(UTEXT_PROVIDER_HAS_META_DATA);
    }

    ut->pFuncs = &repFuncs;
    ut->context = rep;
    return ut;
}

U_CDECL_END
