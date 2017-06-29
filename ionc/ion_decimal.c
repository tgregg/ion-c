/*
 * Copyright 2009-2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include "ion.h"
#include "ion_helpers.h"
#include "ion_decimal_impl.h"

iERR _ion_decimal_number_alloc(void *owner, SIZE decimal_digits, decNumber **p_number) {
    iENTER;
    ASSERT(p_number);
    *p_number = ion_alloc_with_owner(owner, sizeof(decNumber) + ((decimal_digits / DECDPUN) + ((decimal_digits % DECDPUN) ? 1 : 0)));
    if (*p_number == NULL) {
        FAILWITH(IERR_NO_MEMORY);
    }
    iRETURN;
}

iERR _ion_decimal_equals_quad(const decQuad *left, const decQuad *right, decContext *context, BOOL *is_equal) {
    iENTER;
    decQuad left_canonical, right_canonical;
    uint8_t left_coefficient[DECQUAD_Pmax], right_coefficient[DECQUAD_Pmax];

    if (!left || !right || !context || !is_equal) FAILWITH(IERR_INVALID_ARG);

    if (!decQuadIsCanonical(left)) {
        decQuadCanonical(&left_canonical, left);
    }
    else {
        left_canonical = *left;
    }
    if (!decQuadIsCanonical(right)) {
        decQuadCanonical(&right_canonical, right);
    }
    else {
        right_canonical = *right;
    }

    if (decQuadGetExponent(&left_canonical) != decQuadGetExponent(&right_canonical)) {
        goto not_equal;
    }
    if (decQuadGetCoefficient(&left_canonical, left_coefficient)
            != decQuadGetCoefficient(&right_canonical, right_coefficient)) {
        goto not_equal;
    }
    if (memcmp(left_coefficient, right_coefficient, DECQUAD_Pmax)) {
        goto not_equal;
    }
    *is_equal = TRUE;
    SUCCEED();
not_equal:
    *is_equal = FALSE;
    iRETURN;
}

iERR ion_decimal_equals(const decQuad *left, const decQuad *right, decContext *context, BOOL *is_equal) {
    iENTER;
    ASSERT(is_equal);
    IONCHECK(_ion_decimal_equals_quad(left, right, context, is_equal));
    iRETURN;
}

iERR _ion_decimal_equals_number(const decNumber *left, const decNumber *right, decContext *context, BOOL *is_equal) {
    iENTER;
    // Note: all decNumbers are canonical.
    if (!left || !right || !context || !is_equal) FAILWITH(IERR_INVALID_ARG);

    if (left->exponent != right->exponent) {
        goto not_equal;
    }

    if (left->digits != right->digits) {
        goto not_equal;
    }

    if (left->bits != right->bits) {
        goto not_equal;
    }

    if (memcmp(left->lsu, right->lsu, (size_t)((right->digits / DECDPUN)) + ((right->digits % DECDPUN) ? 1 : 0))) {
        goto not_equal;
    }

    *is_equal = TRUE;
    SUCCEED();
not_equal:
    *is_equal = FALSE;
    iRETURN;
}

iERR ion_decimal_equals_iondec(const ION_DECIMAL *left, const ION_DECIMAL *right, decContext *context, BOOL *is_equal) {
    iENTER;
    ASSERT(is_equal);
    if (left->type != right->type) {
        *is_equal = FALSE;
        SUCCEED();
    }
    switch (left->type) {
        case ION_DECIMAL_TYPE_QUAD:
            IONCHECK(_ion_decimal_equals_quad(&left->value.quad_value, &right->value.quad_value, context, is_equal));
            break;
        case ION_DECIMAL_TYPE_NUMBER:
            IONCHECK(_ion_decimal_equals_number(left->value.num_value, right->value.num_value, context, is_equal));
            break;
        default:
            FAILWITH(IERR_INVALID_ARG);
    }
    iRETURN;
}

