/*
*******************************************************************************
*
*   Copyright (C) 2009, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  filterednormalizer2.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009dec10
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/normalizer2.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/unorm.h"

U_NAMESPACE_BEGIN

// TODO: duplicate from normalizer2impl.h; move to a lower-level header file
/**
 * Check that the string is readable and writable.
 * Sets U_ILLEGAL_ARGUMENT_ERROR if the string isBogus() or has an open getBuffer().
 */
inline void checkCanGetBuffer(const UnicodeString &s, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode) && s.isBogus()) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
    }
}

// TODO: use UnicodeSet::span(UnicodeString &s, ...) and UnicodeString::tempSubString()

static int32_t setSpan(const UnicodeSet &set,
                       const UnicodeString &s, int32_t start,
                       USetSpanCondition spanCondition) {
    return start+set.span(s.getBuffer()+start, s.length()-start, spanCondition);
}

static int32_t setSpanBack(const UnicodeSet &set,
                           const UnicodeString &s, int32_t start,
                           USetSpanCondition spanCondition) {
    int32_t sLength=s.length();
    if(start>sLength) {
        start=sLength;
    }
    return set.span(s.getBuffer(), start, spanCondition);
}

UnicodeString &
FilteredNormalizer2::normalize(const UnicodeString &src,
                               UnicodeString &dest,
                               UErrorCode &errorCode) const {
    checkCanGetBuffer(src, errorCode);
    if(U_FAILURE(errorCode)) {
        dest.setToBogus();
        return dest;
    }
    if(&dest==&src) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return dest;
    }
    dest.remove();
    return normalize(src, dest, USET_SPAN_SIMPLE, errorCode);
}

// Internal: No argument checking, and appends to dest.
// Pass as input spanCondition the one that is likely to yield a non-zero
// span length at the start of src.
// For set=[:age=3.2:], since almost all common characters were in Unicode 3.2,
// USET_SPAN_SIMPLE should be passed in for the start of src
// and USET_SPAN_NOT_CONTAINED should be passed in if we continue after
// an in-filter prefix.
UnicodeString &
FilteredNormalizer2::normalize(const UnicodeString &src,
                               UnicodeString &dest,
                               USetSpanCondition spanCondition,
                               UErrorCode &errorCode) const {
    UnicodeString tempDest;  // Don't throw away destination buffer between iterations.
    for(int32_t prevSpanLimit=0; prevSpanLimit<src.length();) {
        int32_t spanLimit=setSpan(set, src, prevSpanLimit, spanCondition);
        int32_t spanLength=spanLimit-prevSpanLimit;
        if(spanCondition==USET_SPAN_NOT_CONTAINED) {
            if(spanLength!=0) {
                dest.append(src, prevSpanLimit, spanLength);
            }
            spanCondition=USET_SPAN_SIMPLE;
        } else {
            if(spanLength!=0) {
                // Not norm2.normalizeSecondAndAppend() because we do not want
                // to modify the non-filter part of dest.
                dest.append(norm2.normalize(src.tempSubStringBetween(prevSpanLimit, spanLimit),
                                            tempDest, errorCode));
                if(U_FAILURE(errorCode)) {
                    break;
                }
            }
            spanCondition=USET_SPAN_NOT_CONTAINED;
        }
        prevSpanLimit=spanLimit;
    }
    return dest;
}

UnicodeString &
FilteredNormalizer2::normalizeSecondAndAppend(UnicodeString &first,
                                              const UnicodeString &second,
                                              UErrorCode &errorCode) const {
    return normalizeSecondAndAppend(first, second, TRUE, errorCode);
}

UnicodeString &
FilteredNormalizer2::append(UnicodeString &first,
                            const UnicodeString &second,
                            UErrorCode &errorCode) const {
    return normalizeSecondAndAppend(first, second, FALSE, errorCode);
}

UnicodeString &
FilteredNormalizer2::normalizeSecondAndAppend(UnicodeString &first,
                                              const UnicodeString &second,
                                              UBool doNormalize,
                                              UErrorCode &errorCode) const {
    checkCanGetBuffer(first, errorCode);
    checkCanGetBuffer(second, errorCode);
    if(U_FAILURE(errorCode)) {
        return first;
    }
    if(&first==&second) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return first;
    }
    if(first.isEmpty()) {
        if(doNormalize) {
            return normalize(second, first, errorCode);
        } else {
            return first=second;
        }
    }
    // merge the in-filter suffix of the first string with the in-filter prefix of the second
    int32_t prefixLimit=setSpan(set, second, 0, USET_SPAN_SIMPLE);
    if(prefixLimit!=0) {
        UnicodeString prefix(second.tempSubString(0, prefixLimit));
        int32_t suffixStart=setSpanBack(set, first, INT32_MAX, USET_SPAN_SIMPLE);
        if(suffixStart==0) {
            if(doNormalize) {
                norm2.normalizeSecondAndAppend(first, prefix, errorCode);
            } else {
                norm2.append(first, prefix, errorCode);
            }
        } else {
            UnicodeString middle(first, suffixStart, INT32_MAX);
            if(doNormalize) {
                norm2.normalizeSecondAndAppend(middle, prefix, errorCode);
            } else {
                norm2.append(middle, prefix, errorCode);
            }
            first.replace(suffixStart, INT32_MAX, middle);
        }
    }
    if(prefixLimit<second.length()) {
        UnicodeString rest(second.tempSubString(prefixLimit, INT32_MAX));
        if(doNormalize) {
            normalize(rest, first, USET_SPAN_NOT_CONTAINED, errorCode);
        } else {
            first.append(rest);
        }
    }
    return first;
}

UBool
FilteredNormalizer2::isNormalized(const UnicodeString &s, UErrorCode &errorCode) const {
    checkCanGetBuffer(s, errorCode);
    if(U_FAILURE(errorCode)) {
        return FALSE;
    }
    USetSpanCondition spanCondition=USET_SPAN_SIMPLE;
    for(int32_t prevSpanLimit=0; prevSpanLimit<s.length();) {
        int32_t spanLimit=setSpan(set, s, prevSpanLimit, spanCondition);
        if(spanCondition==USET_SPAN_NOT_CONTAINED) {
            spanCondition=USET_SPAN_SIMPLE;
        } else {
            if( !norm2.isNormalized(s.tempSubStringBetween(prevSpanLimit, spanLimit), errorCode) ||
                U_FAILURE(errorCode)
            ) {
                return FALSE;
            }
            spanCondition=USET_SPAN_NOT_CONTAINED;
        }
        prevSpanLimit=spanLimit;
    }
    return TRUE;
}

UNormalizationCheckResult
FilteredNormalizer2::quickCheck(const UnicodeString &s, UErrorCode &errorCode) const {
    checkCanGetBuffer(s, errorCode);
    if(U_FAILURE(errorCode)) {
        return UNORM_MAYBE;
    }
    USetSpanCondition spanCondition=USET_SPAN_SIMPLE;
    for(int32_t prevSpanLimit=0; prevSpanLimit<s.length();) {
        int32_t spanLimit=setSpan(set, s, prevSpanLimit, spanCondition);
        if(spanCondition==USET_SPAN_NOT_CONTAINED) {
            spanCondition=USET_SPAN_SIMPLE;
        } else {
            UNormalizationCheckResult qcResult=
                norm2.quickCheck(s.tempSubStringBetween(prevSpanLimit, spanLimit), errorCode);
            if(U_FAILURE(errorCode) || qcResult!=UNORM_YES) {
                return qcResult;
            }
            spanCondition=USET_SPAN_NOT_CONTAINED;
        }
        prevSpanLimit=spanLimit;
    }
    return UNORM_YES;
}

int32_t
FilteredNormalizer2::spanQuickCheckYes(const UnicodeString &s, UErrorCode &errorCode) const {
    checkCanGetBuffer(s, errorCode);
    if(U_FAILURE(errorCode)) {
        return 0;
    }
    USetSpanCondition spanCondition=USET_SPAN_SIMPLE;
    for(int32_t prevSpanLimit=0; prevSpanLimit<s.length();) {
        int32_t spanLimit=setSpan(set, s, prevSpanLimit, spanCondition);
        if(spanCondition==USET_SPAN_NOT_CONTAINED) {
            spanCondition=USET_SPAN_SIMPLE;
        } else {
            int32_t yesLimit=
                prevSpanLimit+
                norm2.spanQuickCheckYes(
                    s.tempSubStringBetween(prevSpanLimit, spanLimit), errorCode);
            if(U_FAILURE(errorCode) || yesLimit<spanLimit) {
                return yesLimit;
            }
            spanCondition=USET_SPAN_NOT_CONTAINED;
        }
        prevSpanLimit=spanLimit;
    }
    return s.length();
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(FilteredNormalizer2)

U_NAMESPACE_END

#endif  // !UCONFIG_NO_NORMALIZATION
