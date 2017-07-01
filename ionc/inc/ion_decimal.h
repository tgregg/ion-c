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

#ifdef __cplusplus
extern "C" {
#endif

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
     * The _ion_decimal holds a decNumber.
     */
    ION_DECIMAL_TYPE_NUMBER = 2,

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
 * NOTE: this function should not be called on values that do not need to remain in scope after the reader is closed;
 * doing so may force an unnecessary copy.
 *
 * @param value - The value to claim.
 */
ION_API_EXPORT iERR ion_decimal_claim(ION_DECIMAL *value);

/**
 * Frees any memory that was allocated during the corresponding call to `ion_decimal_claim` on this value.
 *
 * NOTE: it is an error to call this function on a value that has not been claimed; values that haven't been claimed do
 * not need to be released manually.
 *
 * @param value - The value to release.
 */
ION_API_EXPORT iERR ion_decimal_release(ION_DECIMAL *value);

/**
 * Compares decQuads for equivalence under the Ion data model. That is, the sign, coefficient, and exponent must be
 * equivalent for the normalized values (even for zero).
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
 */
ION_API_EXPORT iERR ion_decimal_to_string(const ION_DECIMAL *value, char *p_string);

#ifdef __cplusplus
}
#endif

#endif /* ION_DECIMAL_H_ */
