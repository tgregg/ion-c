/*
 * Copyright 2011-2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at:
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 */

#ifndef ION_DECIMAL_H_
#define ION_DECIMAL_H_

#include "ion_types.h"
#include "ion_platform_config.h"

#ifndef DECNUMDIGITS
    #error DECNUMDIGITS must be defined to be >= DECQUAD_Pmax
#elif DECNUMDIGITS < DECQUAD_Pmax
    #error DECNUMDIGITS must be defined to be >= DECQUAD_Pmax
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TODO expand this general info section
 * For calculation APIs, the output parameter (usually the first) is considered to be an uninitialized value (i.e.
 * no effort is made to free its associated memory before overwriting it) UNLESS it is the same reference as one of
 * the operands. If it is the same reference as one of its operands, the operation will be performed in-place if
 * possible. If not possible, its memory will be freed and replaced with newly allocated memory to hold the result.
 * To avoid memory leaks, it is important not to reuse ION_DECIMAL values as output parameters unless it is an in-place
 * operation or the reused value has been uninitialized first using `ion_decimal_release`.
 */

/**
 * Determines which value of the _ion_decimal's `value` field is active.
 */
typedef enum {
    ION_DECIMAL_TYPE_UNKNOWN = 0,
    /**
     * The _ion_decimal holds a decQuad.
     */
    ION_DECIMAL_TYPE_QUAD = 1,
    /**
     * The _ion_decimal holds an unowned decNumber.
     */
    ION_DECIMAL_TYPE_NUMBER = 2,
    /**
     * The _ion_decimal holds a decNumber whose memory is managed by an owner.
     */
    ION_DECIMAL_TYPE_NUMBER_OWNED = 3,

} ION_DECIMAL_TYPE;

struct _ion_decimal {

    ION_DECIMAL_TYPE type;

    union {
        decQuad quad_value;
        decNumber *num_value;
    } value;
};

//
// support routines for decimal and timestamp values
//
ION_API_EXPORT iERR ion_decimal_set_to_double_value(decQuad *dec, double value, int32_t sig_digits);
ION_API_EXPORT iERR ion_decimal_get_double_value   (decQuad *dec, double *p_value);

/**
 * If necessary, copies the given decimal's internal data so that owner of that data may be closed. This is useful,
 * for example, when it is necessary to keep the value in scope after the reader that produced it is closed. Once this
 * function has been called on a value, `ion_decimal_release` must be called on the same value to free the copied
 * memory.
 *
 * @param value - The value to claim.
 */
ION_API_EXPORT iERR ion_decimal_claim(ION_DECIMAL *value);

/**
 * Frees any memory that was allocated when constructing this value. This should be called to clean up all ION_DECIMALs.
 *
 * @param value - The value to release.
 */
ION_API_EXPORT iERR ion_decimal_release(ION_DECIMAL *value);

/**
 * Compares decQuads for equivalence under the Ion data model. That is, the sign, coefficient, and exponent must be
 * equivalent for the normalized values (even for zero).
 *
 * @deprecated - use of decQuads directly is deprecated. ION_DECIMAL should be used. See `ion_decimal_equals`.
 */
ION_API_EXPORT iERR ion_decimal_equals_quad(const decQuad *left, const decQuad *right, decContext *context, BOOL *is_equal);

/**
 * Compares ION_DECIMALs for equivalence under the Ion data model. That is, the sign, coefficient, and exponent must be
 * equivalent for the normalized values (even for zero).
 */
ION_API_EXPORT iERR ion_decimal_equals(const ION_DECIMAL *left, const ION_DECIMAL *right, decContext *context, BOOL *is_equal);

/**
 * Converts the given ION_DECIMAL to a string. If the value has N decimal digits, the given string must have at least
 * N + 14 available bytes.
 *
 * @return IERR_OK (no errors are possible).
 */
ION_API_EXPORT iERR ion_decimal_to_string(const ION_DECIMAL *value, char *p_string);

/**
 * Converts the given string to its ION_DECIMAL representation.
 *
 * @param value - a non-null pointer to the resulting decimal.
 * @param str - a null-terminated string representing a decimal. Exponents (if any) may be indicated using either 'd'
 *   or 'e'.
 * @param context - the context to use for the conversion. If the decimal lies outside of the context's limits, an error
 *   is raised.
 * @return IERR_NUMERIC_OVERFLOW if the decimal lies outside of the context's limits, otherwise IERR_OK.
 */
ION_API_EXPORT iERR ion_decimal_from_string(ION_DECIMAL *value, char *str, decContext *context);

/**
 * Represents the given uint32 as an ION_DECIMAL. This function has no side effects that cause the resulting ION_DECIMAL
 * to need to be claimed or released.
 *
 * @return IERR_OK (no errors are possible).
 */
ION_API_EXPORT iERR ion_decimal_from_uint32(ION_DECIMAL *value, uint32_t num);

/**
 * Represents the given int32 as an ION_DECIMAL. This function has no side effects that cause the resulting ION_DECIMAL
 * to need to be claimed or released.
 *
 * @return IERR_OK (no errors are possible).
 */
ION_API_EXPORT iERR ion_decimal_from_int32(ION_DECIMAL *value, int32_t num);

/**
 * Represents the given decQuad as an ION_DECIMAL. The caller IS NOT required to keep the given decQuad in scope for the
 * lifetime of the resulting ION_DECIMAL. This function has no side effects that cause the resulting ION_DECIMAL
 * to need to be claimed or released.
 *
 * @return IERR_OK (no errors are possible).
 */
ION_API_EXPORT iERR ion_decimal_from_quad(ION_DECIMAL *value, decQuad *quad);

/**
 *
 * Represents the given decNumber as an ION_DECIMAL. This function does not allocate or copy any memory, so the caller
 * IS required to keep the given decNumber in scope for the lifetime of the resulting ION_DECIMAL. If desired, the
 * caller can alleviate this requirement by calling `ion_decimal_claim` on the resulting ION_DECIMAL (note that this
 * forces a copy). It is the caller's responsibility to eventually free any dynamically allocated memory used by the
 * given decNumber (calling `ion_decimal_release` will not free this memory).
 *
 * @return IERR_OK (no errors are possible).
 */
ION_API_EXPORT iERR ion_decimal_from_number(ION_DECIMAL *value, decNumber *number);

ION_API_EXPORT iERR ion_decimal_from_ion_int(const ION_DECIMAL *value, decContext *context, ION_INT *p_int);

ION_API_EXPORT iERR ion_decimal_to_int32(const ION_DECIMAL *value, decContext *context, int32_t *p_int);
ION_API_EXPORT iERR ion_decimal_to_uint32(const ION_DECIMAL *value, decContext *context, uint32_t *p_int);
ION_API_EXPORT iERR ion_decimal_to_ion_int(const ION_DECIMAL *value, decContext *context, ION_INT *p_int);

ION_API_EXPORT iERR ion_decimal_fma(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, const ION_DECIMAL *fhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_add(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_and(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_divide(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_divide_integer(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_max(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_max_mag(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_min(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_min_mag(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_multiply(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_or(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_quantize(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_remainder(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_remainder_near(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_rotate(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_scaleb(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_shift(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_subtract(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_xor(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_zero(ION_DECIMAL *value);

ION_API_EXPORT iERR ion_decimal_abs(ION_DECIMAL *value, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_invert(ION_DECIMAL *value, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_logb(ION_DECIMAL *value, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_minus(ION_DECIMAL *value, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_plus(ION_DECIMAL *value, const ION_DECIMAL *rhs, decContext *context);
ION_API_EXPORT iERR ion_decimal_reduce(ION_DECIMAL *value, const ION_DECIMAL *rhs, decContext *context);

ION_API_EXPORT uint32_t ion_decimal_digits(const ION_DECIMAL *value);
ION_API_EXPORT int32_t ion_decimal_get_exponent(const ION_DECIMAL *value);
ION_API_EXPORT uint32_t ion_decimal_radix(const ION_DECIMAL *value);

ION_API_EXPORT uint32_t ion_decimal_same_quantum(const ION_DECIMAL *lhs, const ION_DECIMAL *rhs);
ION_API_EXPORT uint32_t ion_decimal_is_integer(const ION_DECIMAL *value);
ION_API_EXPORT uint32_t ion_decimal_is_subnormal(const ION_DECIMAL *value, decContext *context);
ION_API_EXPORT uint32_t ion_decimal_is_normal(const ION_DECIMAL *value, decContext *context);
ION_API_EXPORT uint32_t ion_decimal_is_finite(const ION_DECIMAL *value);
ION_API_EXPORT uint32_t ion_decimal_is_infinite(const ION_DECIMAL *value);
ION_API_EXPORT uint32_t ion_decimal_is_nan(const ION_DECIMAL *value);
ION_API_EXPORT uint32_t ion_decimal_is_negative(const ION_DECIMAL *value);
ION_API_EXPORT uint32_t ion_decimal_is_zero(const ION_DECIMAL *value);
ION_API_EXPORT uint32_t ion_decimal_is_canonical(const ION_DECIMAL *value);


#ifdef __cplusplus
}
#endif

#endif /* ION_DECIMAL_H_ */
