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
 * Compares decQuads for equivalence under the Ion data model. That is, the sign, coefficient, and exponent must be
 * equivalent for the normalized values (even for zero).
 */
// TODO get rid of this in favor of JUST the ION_DECIMAL version
ION_API_EXPORT iERR ion_decimal_equals(const decQuad *left, const decQuad *right, decContext *context, BOOL *is_equal);
// TODO rename to ion_decimal_equals
ION_API_EXPORT iERR ion_decimal_equals_iondec(const ION_DECIMAL *left, const ION_DECIMAL *right, decContext *context, BOOL *is_equal);

#ifdef __cplusplus
}
#endif

#endif /* ION_DECIMAL_H_ */
