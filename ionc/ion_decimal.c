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

#include <inc/ion_decimal.h>
#include "ion.h"
#include "ion_helpers.h"
#include "ion_decimal_impl.h"
#include "decimal128.h"

#define ION_DECNUMBER_UNITS_SIZE(decimal_digits) \
     (sizeof(decNumberUnit) * (((decimal_digits / DECDPUN) + ((decimal_digits % DECDPUN) ? 1 : 0))))

#define ION_DECNUMBER_SIZE(decimal_digits) (sizeof(decNumber) + ION_DECNUMBER_UNITS_SIZE(decimal_digits))
#define ION_DECNUMBER_DECQUAD_SIZE ION_DECNUMBER_SIZE(DECQUAD_Pmax)

iERR _ion_decimal_number_alloc(void *owner, SIZE decimal_digits, decNumber **p_number) {
    iENTER;
    ASSERT(p_number);
    if (!owner) {
        *p_number = ion_xalloc(ION_DECNUMBER_SIZE(decimal_digits));
    }
    else {
        *p_number = ion_alloc_with_owner(owner, ION_DECNUMBER_SIZE(decimal_digits));
    }
    if (*p_number == NULL) {
        FAILWITH(IERR_NO_MEMORY);
    }
    iRETURN;
}

iERR ion_decimal_claim(ION_DECIMAL *value) {
    iENTER;
    decNumber *copy;

    switch (value->type) {
        case ION_DECIMAL_TYPE_QUAD:
            // Nothing needs to be done; the decQuad lives within the ION_DECIMAL.
            break;
        case ION_DECIMAL_TYPE_NUMBER:
            // The decNumber may have been allocated with an owner, meaning its memory will go out of scope when that
            // owner is closed. This copy extends that scope until ion_decimal_release.
            copy = ion_xalloc((size_t)ION_DECNUMBER_SIZE(value->value.num_value->digits));
            decNumberCopy(copy, value->value.num_value);
            value->value.num_value = copy;
            break;
        default:
            FAILWITH(IERR_INVALID_ARG);
    }
    iRETURN;
}

iERR ion_decimal_release(ION_DECIMAL *value) {
    iENTER;
    switch (value->type) {
        case ION_DECIMAL_TYPE_QUAD:
            // Nothing needs to be done; the decQuad lives within the ION_DECIMAL.
            break;
        case ION_DECIMAL_TYPE_NUMBER:
            ion_xfree(value->value.num_value);
            break;
        default:
            FAILWITH(IERR_INVALID_ARG);
    }
    iRETURN;
}

iERR ion_decimal_equals_quad(const decQuad *left, const decQuad *right, decContext *context, BOOL *is_equal) {
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

    if (memcmp(left->lsu, right->lsu, ION_DECNUMBER_UNITS_SIZE(right->digits))) {
        goto not_equal;
    }

    *is_equal = TRUE;
    SUCCEED();
not_equal:
    *is_equal = FALSE;
    iRETURN;
}

iERR ion_decimal_equals(const ION_DECIMAL *left, const ION_DECIMAL *right, decContext *context, BOOL *is_equal) {
    iENTER;
    ASSERT(is_equal);
    if (left->type != right->type) {
        // TODO support this, otherwise the user has to care what type backs his/her ION_DECIMAL.
        *is_equal = FALSE;
        SUCCEED();
    }
    switch (left->type) {
        case ION_DECIMAL_TYPE_QUAD:
            IONCHECK(ion_decimal_equals_quad(&left->value.quad_value, &right->value.quad_value, context, is_equal));
            break;
        case ION_DECIMAL_TYPE_NUMBER:
            IONCHECK(_ion_decimal_equals_number(left->value.num_value, right->value.num_value, context, is_equal));
            break;
        default:
            FAILWITH(IERR_INVALID_ARG);
    }
    iRETURN;
}

#define ION_DECIMAL_AS_QUAD(ion_decimal) &ion_decimal->value.quad_value
#define ION_DECIMAL_AS_NUMBER(ion_decimal) ion_decimal->value.num_value

iERR _ion_decimal_from_string_helper(char *str, decContext *context, hOWNER owner, decQuad *p_quad, decNumber **p_num) {
    iENTER;
    char            *cp, c_save = 0;
    uint32_t        saved_status;
    SIZE            decimal_digits = 0;

    for (cp = str; *cp; cp++) {
        if (*cp == 'd' || *cp == 'D') {
            c_save = *cp;
            *cp = 'e';
            break;
        }
        if (*cp != '.') {
            decimal_digits++;
        }
    }
    // Replace 'd' with 'e' to satisfy the decNumber APIs.
    if (*cp) {
        c_save = *cp;
        *cp = 'e';
    }
    saved_status = decContextSaveStatus(context, DEC_Inexact);
    decContextClearStatus(context, DEC_Inexact);
    decQuadFromString(p_quad, str, context);
    if (decContextTestStatus(context, DEC_Inexact)) {
        if (p_num) {
            decContextClearStatus(context, DEC_Inexact);
            IONCHECK(_ion_decimal_number_alloc(owner, decimal_digits, p_num));
            decNumberFromString(*p_num, str, context);
            if (decContextTestStatus(context, DEC_Inexact)) {
                // The value is too large to fit in any decimal representation. Rather than silently losing precision,
                // fail.
                FAILWITH(IERR_NUMERIC_OVERFLOW);
            }
        }
        else {
            // The value is too large to fit in a decQuad. Rather than silently losing precision, fail.
            FAILWITH(IERR_NUMERIC_OVERFLOW);
        }
    }
    decContextRestoreStatus(context, saved_status, DEC_Inexact);
    // restore the string is we munged it, just in case someone else wants to use if later
    if (*cp) {
        *cp = c_save;
    }

    iRETURN;
}

iERR ion_decimal_from_string(ION_DECIMAL *value, char *str, decContext *context, hOWNER owner) {
    iENTER;
    decNumber *num_value = NULL;
    IONCHECK(_ion_decimal_from_string_helper(str, context, owner, ION_DECIMAL_AS_QUAD(value), &num_value));
    if (num_value) {
        value->type = ION_DECIMAL_TYPE_NUMBER;
        value->value.num_value = num_value;
    }
    else {
        value->type = ION_DECIMAL_TYPE_QUAD;
    }
    iRETURN;
}

iERR ion_decimal_from_uint32(ION_DECIMAL *value, uint32_t num) {
    iENTER;
    decQuadFromUInt32(ION_DECIMAL_AS_QUAD(value), num);
    value->type = ION_DECIMAL_TYPE_QUAD;
    iRETURN;
}

iERR ion_decimal_from_int32(ION_DECIMAL *value, int32_t num) {
    iENTER;
    decQuadFromInt32(ION_DECIMAL_AS_QUAD(value), num);
    value->type = ION_DECIMAL_TYPE_QUAD;
    iRETURN;
}

iERR ion_decimal_from_quad(ION_DECIMAL *value, decQuad *quad) {
    iENTER;
    decQuadCopy(&value->value.quad_value, quad);
    value->type = ION_DECIMAL_TYPE_QUAD;
    iRETURN;
}

iERR ion_decimal_from_number(ION_DECIMAL *value, decNumber *number) {
    iENTER;
    value->value.num_value = number;
    value->type = ION_DECIMAL_TYPE_NUMBER;
    iRETURN;
}

#define ION_DECIMAL_IF_QUAD(ion_decimal) \
decQuad *quad_value; \
decNumber *num_value; \
switch(ion_decimal->type) { \
    case ION_DECIMAL_TYPE_QUAD: \
        quad_value = ION_DECIMAL_AS_QUAD(ion_decimal);

#define ION_DECIMAL_ELSE_IF_NUMBER(ion_decimal) \
        break; \
    case ION_DECIMAL_TYPE_NUMBER: \
        num_value = ION_DECIMAL_AS_NUMBER(ion_decimal);

#define ION_DECIMAL_ENDIF \
        break; \
    default: \
        FAILWITH(IERR_INVALID_ARG); \
}

#define ION_DECIMAL_API(value, if_quad, if_number) \
iENTER; \
ION_DECIMAL_IF_QUAD(value) { \
    if_quad; \
} \
ION_DECIMAL_ELSE_IF_NUMBER(value) { \
    if_number; \
} \
ION_DECIMAL_ENDIF; \
iRETURN;

#define ION_DECIMAL_CALCULATION_API(value, if_quad, if_overflow, if_number, context) \
iENTER; \
uint32_t saved_status; \
ION_DECIMAL_IF_QUAD(value) { \
    saved_status = decContextSaveStatus(context, DEC_Inexact); \
    decContextClearStatus(context, DEC_Inexact); \
    if_quad; \
    value->type = ION_DECIMAL_TYPE_QUAD; \
    if (decContextTestStatus(context, DEC_Inexact)) { /*IONCHECK(_ion_decimal_number_alloc(NULL, context->digits, ION_DECIMAL_AS_NUMBER(value)));*/ /*decQuadToNumber(quad_value, ION_DECIMAL_AS_NUMBER(value));*/ \
        if_overflow; \
    } \
    decContextRestoreStatus(context, saved_status, DEC_Inexact); \
} \
ION_DECIMAL_ELSE_IF_NUMBER(value) { \
    if_number; \
    value->type = ION_DECIMAL_TYPE_NUMBER; \
} \
ION_DECIMAL_ENDIF; \
iRETURN;


iERR ion_decimal_to_string(const ION_DECIMAL *value, char *p_string) {
    ION_DECIMAL_API(value, decQuadToString(quad_value, p_string), decNumberToString(num_value, p_string));
}

iERR _ion_decimal_fma_standardized(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, const ION_DECIMAL *fhs, decContext *context, int decnum_mask) {
    iENTER;
    BYTE operands_buffer[3 * ION_DECNUMBER_DECQUAD_SIZE];
    size_t offset;
    decNumber *op1, *op2, *op3;

    memset(operands_buffer, 0, 3 * ION_DECNUMBER_DECQUAD_SIZE);
    if ((decnum_mask & 1) == 0) {
        offset = 0;
        op1 = (decNumber *)(operands_buffer + offset);
        decQuadToNumber(ION_DECIMAL_AS_QUAD(lhs), op1);
    }
    else {
        ASSERT(ION_DECIMAL_TYPE_NUMBER == lhs->type);
        op1 = lhs->value.num_value;
    }
    if ((decnum_mask & 2) == 0) {
        offset = 1 * ION_DECNUMBER_DECQUAD_SIZE;
        op2 = (decNumber *)(operands_buffer + offset); // This makes sure the num_value is active in the union.
        decQuadToNumber(ION_DECIMAL_AS_QUAD(rhs), op2);
    }
    else {
        ASSERT(ION_DECIMAL_TYPE_NUMBER == rhs->type);
        op2 = rhs->value.num_value;
    }
    if ((decnum_mask & 4) == 0) {
        offset = 2 * ION_DECNUMBER_DECQUAD_SIZE;
        op3 = (decNumber *)(operands_buffer + offset); // This makes sure the num_value is active in the union.
        decQuadToNumber(ION_DECIMAL_AS_QUAD(fhs), op3);
    }
    else {
        ASSERT(ION_DECIMAL_TYPE_NUMBER == fhs->type);
        op3 = fhs->value.num_value;
    }

    IONCHECK(_ion_decimal_number_alloc(NULL, context->digits, &ION_DECIMAL_AS_NUMBER(value)));
    decNumberFMA(ION_DECIMAL_AS_NUMBER(value), op1, op2, op3, context);
    value->type = ION_DECIMAL_TYPE_NUMBER;
    iRETURN;
}
/*
iERR _ion_decimal_fma_helper(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, const ION_DECIMAL *fhs, decContext *context, int decnum_mask) {
    ION_DECIMAL_CALCULATION_API(value, decQuadFMA(ION_DECIMAL_AS_QUAD(value), ION_DECIMAL_AS_QUAD(lhs), ION_DECIMAL_AS_QUAD(rhs), ION_DECIMAL_AS_QUAD(fhs), context),
                                IONCHECK(_ion_decimal_fma_standardized(value, lhs, rhs, fhs, context, decnum_mask)),
                                decNumberFMA(ION_DECIMAL_AS_NUMBER(value), ION_DECIMAL_AS_NUMBER(lhs), ION_DECIMAL_AS_NUMBER(rhs), ION_DECIMAL_AS_NUMBER(fhs), context), context);
}
*/
iERR ion_decimal_fma(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, const ION_DECIMAL *fhs, decContext *context) {
    iENTER;
    uint32_t saved_status; \
    int num_decnums =   (lhs->type == ION_DECIMAL_TYPE_NUMBER ? 1 : 0)
                      | (rhs->type == ION_DECIMAL_TYPE_NUMBER ? 2 : 0)
                      | (fhs->type == ION_DECIMAL_TYPE_NUMBER ? 4 : 0);
    if (num_decnums > 0 && num_decnums < 7) {

        // All operands must have the same type. Convert all non-decNumbers to decNumbers.
        IONCHECK(_ion_decimal_fma_standardized(value, lhs, rhs, fhs, context, num_decnums));
    }
    else {
        value->type = (num_decnums == 0) ? ION_DECIMAL_TYPE_QUAD : ION_DECIMAL_TYPE_NUMBER;
        ION_DECIMAL_IF_QUAD(value) { \
            saved_status = decContextSaveStatus(context, DEC_Inexact);
            decContextClearStatus(context, DEC_Inexact);
            decQuadFMA(ION_DECIMAL_AS_QUAD(value), ION_DECIMAL_AS_QUAD(lhs), ION_DECIMAL_AS_QUAD(rhs), ION_DECIMAL_AS_QUAD(fhs), context);
            value->type = ION_DECIMAL_TYPE_QUAD;
            if (decContextTestStatus(context, DEC_Inexact)) {
                IONCHECK(_ion_decimal_fma_standardized(value, lhs, rhs, fhs, context, num_decnums));
            }
            decContextRestoreStatus(context, saved_status, DEC_Inexact);
        }
        ION_DECIMAL_ELSE_IF_NUMBER(value) {
            IONCHECK(_ion_decimal_number_alloc(NULL, context->digits, &ION_DECIMAL_AS_NUMBER(value)));
            decNumberFMA(ION_DECIMAL_AS_NUMBER(value), ION_DECIMAL_AS_NUMBER(lhs), ION_DECIMAL_AS_NUMBER(rhs), ION_DECIMAL_AS_NUMBER(fhs), context);
            value->type = ION_DECIMAL_TYPE_NUMBER;
        }
        ION_DECIMAL_ENDIF;
    }
    iRETURN;
}

#define ION_DECIMAL_OVERFLOW_API_HELPER(value, if_quad, if_overflow, if_number, context) \
uint32_t saved_status; \
ION_DECIMAL_IF_QUAD(value) { \
    saved_status = decContextSaveStatus(context, DEC_Inexact); \
    decContextClearStatus(context, DEC_Inexact); \
    if_quad(ION_DECIMAL_AS_QUAD(value), ION_DECIMAL_AS_QUAD(lhs), ION_DECIMAL_AS_QUAD(rhs), ION_DECIMAL_AS_QUAD(fhs), context); \
    value->type = ION_DECIMAL_TYPE_QUAD; \
    if (decContextTestStatus(context, DEC_Inexact)) { /*IONCHECK(_ion_decimal_number_alloc(NULL, context->digits, ION_DECIMAL_AS_NUMBER(value)));*/ /*decQuadToNumber(quad_value, ION_DECIMAL_AS_NUMBER(value));*/ \
        IONCHECK(if_overflow(value, lhs, rhs, fhs, context, decnum_mask)); \
    } \
    decContextRestoreStatus(context, saved_status, DEC_Inexact); \
} \
ION_DECIMAL_ELSE_IF_NUMBER(value) { \
    if_number(ION_DECIMAL_AS_NUMBER(value), ION_DECIMAL_AS_NUMBER(lhs), ION_DECIMAL_AS_NUMBER(rhs), ION_DECIMAL_AS_NUMBER(fhs), context); \
    value->type = ION_DECIMAL_TYPE_NUMBER; \
} \
ION_DECIMAL_ENDIF; \

#define ION_DECIMAL_QUAD_TO_NUM(dec, dn, ord) \
    if ((decnum_mask & (1 << ord)) == 0) { \
        offset = ord * ION_DECNUMBER_DECQUAD_SIZE; \
        dn = (decNumber *)(operands_buffer + offset); \
        decQuadToNumber(ION_DECIMAL_AS_QUAD(dec), dn); \
    } \
    else { \
        ASSERT(ION_DECIMAL_TYPE_NUMBER == dec->type); \
        dn = dec->value.num_value; \
    } \

#define ION_DECIMAL_OVERFLOW_API_THREE_OPERAND(name, if_quad, if_number) \
iERR _##name##_standardized(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, const ION_DECIMAL *fhs, decContext *context, int decnum_mask) { \
    iENTER; \
    BYTE operands_buffer[3 * ION_DECNUMBER_DECQUAD_SIZE]; \
    size_t offset; \
    decNumber *op1, *op2, *op3; \
    memset(operands_buffer, 0, 3 * ION_DECNUMBER_DECQUAD_SIZE); \
    ION_DECIMAL_QUAD_TO_NUM(lhs, op1, 0); \
    ION_DECIMAL_QUAD_TO_NUM(rhs, op2, 1); \
    ION_DECIMAL_QUAD_TO_NUM(fhs, op3, 2); \
    IONCHECK(_ion_decimal_number_alloc(NULL, context->digits, &ION_DECIMAL_AS_NUMBER(value))); \
    if_number(ION_DECIMAL_AS_NUMBER(value), op1, op2, op3, context); \
    value->type = ION_DECIMAL_TYPE_NUMBER; \
    iRETURN; \
} \
iERR name(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, const ION_DECIMAL *fhs, decContext *context) { \
    iENTER; \
    uint32_t status; \
    int decnum_mask =     (lhs->type == ION_DECIMAL_TYPE_NUMBER ? 1 : 0) \
                        | (rhs->type == ION_DECIMAL_TYPE_NUMBER ? 2 : 0) \
                        | (fhs->type == ION_DECIMAL_TYPE_NUMBER ? 4 : 0); \
    if (decnum_mask > 0 && decnum_mask < 7) { \
        /* Not all operands have the same type. Convert all non-decNumbers to decNumbers. */ \
        IONCHECK(_##name##_standardized(value, lhs, rhs, fhs, context, decnum_mask)); \
    } \
    else if (decnum_mask){ \
        ASSERT(decnum_mask == 7); \
        /* All operands are decNumbers. */ \
        IONCHECK(_ion_decimal_number_alloc(NULL, context->digits, &ION_DECIMAL_AS_NUMBER(value))); \
        if_number(ION_DECIMAL_AS_NUMBER(value), ION_DECIMAL_AS_NUMBER(lhs), ION_DECIMAL_AS_NUMBER(rhs), ION_DECIMAL_AS_NUMBER(fhs), context); \
        value->type = ION_DECIMAL_TYPE_NUMBER; \
    } \
    else { \
        ASSERT(decnum_mask == 0); \
        /* All operands are decQuads. */ \
        status = decContextSaveStatus(context, DEC_Inexact); \
        decContextClearStatus(context, DEC_Inexact); \
        if_quad(ION_DECIMAL_AS_QUAD(value), ION_DECIMAL_AS_QUAD(lhs), ION_DECIMAL_AS_QUAD(rhs), ION_DECIMAL_AS_QUAD(fhs), context); \
        value->type = ION_DECIMAL_TYPE_QUAD; \
        if (decContextTestStatus(context, DEC_Inexact)) { \
            /* The operation overflowed the maximum decQuad precision. Convert operands and result to decNumbers. */ \
            IONCHECK(_##name##_standardized(value, lhs, rhs, fhs, context, decnum_mask)); \
        } \
        decContextRestoreStatus(context, status, DEC_Inexact); \
    } \
    iRETURN; \
}

ION_DECIMAL_OVERFLOW_API_THREE_OPERAND(ion_decimal_fma_macro, decQuadFMA, decNumberFMA);


