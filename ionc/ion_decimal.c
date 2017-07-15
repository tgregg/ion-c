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
#include "decimal128.h"

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


iERR ion_decimal_to_string(const ION_DECIMAL *value, char *p_string) {
    ION_DECIMAL_API(value, decQuadToString(quad_value, p_string), decNumberToString(num_value, p_string));
}

#define ION_DECIMAL_UTILITY_API(name, if_quad, if_number) \
    uint32_t name(const ION_DECIMAL *value) { \
        ION_DECIMAL_API(value, return if_quad(value), return if_number(num_value)); \
    }

ION_DECIMAL_UTILITY_API(ion_decimal_is_finite, decQuadIsFinite, decNumberIsFinite);
ION_DECIMAL_UTILITY_API(ion_decimal_is_infinite, decQuadIsInfinite, decNumberIsInfinite);
ION_DECIMAL_UTILITY_API(ion_decimal_is_nan, decQuadIsNaN, decNumberIsNaN);
ION_DECIMAL_UTILITY_API(ion_decimal_is_negative, decQuadIsNegative, decNumberIsNegative);
ION_DECIMAL_UTILITY_API(ion_decimal_is_zero, decQuadIsZero, decNumberIsZero);
ION_DECIMAL_UTILITY_API(ion_decimal_is_canonical, decQuadIsCanonical, decNumberIsCanonical);
ION_DECIMAL_UTILITY_API(ion_decimal_radix, decQuadRadix, decNumberRadix);

uint32_t ion_decimal_is_normal(const ION_DECIMAL *value, decContext *context) {
    ION_DECIMAL_API(value, return decQuadIsNormal(quad_value), return decNumberIsNormal(num_value, context));
}

uint32_t ion_decimal_is_subnormal(const ION_DECIMAL *value, decContext *context) {
    ION_DECIMAL_API(value, return decQuadIsSubnormal(quad_value), return decNumberIsSubnormal(num_value, context));
}

int32_t ion_decimal_get_exponent(const ION_DECIMAL *value) {
    ION_DECIMAL_API(value, return decQuadGetExponent(quad_value), return num_value->exponent);
}

uint32_t ion_decimal_digits(const ION_DECIMAL *value) {
    ION_DECIMAL_API(value, return decQuadDigits(quad_value), return num_value->digits);
}

uint32_t ion_decimal_same_quantum(const ION_DECIMAL *lhs, const ION_DECIMAL *rhs) {
    return (uint32_t)(ion_decimal_get_exponent(lhs) == ion_decimal_get_exponent(rhs));
}

uint32_t ion_decimal_is_integer(const ION_DECIMAL *value) {
    ION_DECIMAL_API(value, return decQuadIsInteger(quad_value), return num_value->exponent == 0);
}

iERR ion_decimal_zero(ION_DECIMAL *value) {
    ION_DECIMAL_API(value, decQuadZero(quad_value), decNumberZero(num_value));
}

#define ION_DECIMAL_CONVERSION_API(name, type, if_quad, if_number) \
iERR name(const ION_DECIMAL *value, decContext *context, type *p_out) { \
    iENTER; \
    uint32_t status; \
    \
    ASSERT(p_out); \
    \
    status = decContextSaveStatus(context, DEC_Inexact | DEC_Invalid_operation); \
    decContextClearStatus(context, DEC_Inexact | DEC_Invalid_operation); \
    ION_DECIMAL_IF_QUAD(value) { \
        *p_out = if_quad(ION_DECIMAL_AS_QUAD(value), context, context->round); \
    } \
    ION_DECIMAL_ELSE_IF_NUMBER(value) { \
        *p_out = if_number(ION_DECIMAL_AS_NUMBER(value), context); \
    } \
    ION_DECIMAL_ENDIF; \
    if (decContextTestStatus(context, DEC_Inexact)) { \
        err = IERR_NUMERIC_OVERFLOW; \
    } \
    if (decContextTestStatus(context, DEC_Invalid_operation)) { \
        err = IERR_INVALID_ARG; \
    } \
    decContextRestoreStatus(context, status, DEC_Inexact | DEC_Invalid_operation); \
    iRETURN; \
}

ION_DECIMAL_CONVERSION_API(ion_decimal_to_int32, int32_t, decQuadToInt32Exact, decNumberToInt32);
ION_DECIMAL_CONVERSION_API(ion_decimal_to_uint32, uint32_t, decQuadToUInt32Exact, decNumberToUInt32);

iERR ion_decimal_to_ion_int(const ION_DECIMAL *value, decContext *context, ION_INT *p_int) {
    iENTER;
    if (!ion_decimal_is_integer(value)) {
        FAILWITH(IERR_INVALID_ARG);
    }
    ION_DECIMAL_IF_QUAD(value) {
        IONCHECK(ion_int_from_decimal(p_int, quad_value, context))
    } \
    ION_DECIMAL_ELSE_IF_NUMBER(value) {
        IONCHECK(_ion_int_from_decimal_number(p_int, num_value, context))
    }
    ION_DECIMAL_ENDIF;
    iRETURN;
}

iERR ion_decimal_from_ion_int(const ION_DECIMAL *value, decContext *context, ION_INT *p_int) {
    ION_DECIMAL_API (
        value,
        IONCHECK(ion_int_to_decimal(p_int, quad_value, context)),
        IONCHECK(_ion_int_to_decimal_number(p_int, num_value, context))
    );
}

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

#define ION_DECIMAL_CONVERT_THREE_OPERAND \
    BYTE operands_buffer[3 * ION_DECNUMBER_DECQUAD_SIZE]; \
    size_t offset; \
    decNumber *op1, *op2, *op3; \
    memset(operands_buffer, 0, 3 * ION_DECNUMBER_DECQUAD_SIZE); \
    ION_DECIMAL_QUAD_TO_NUM(lhs, op1, 0); \
    ION_DECIMAL_QUAD_TO_NUM(rhs, op2, 1); \
    ION_DECIMAL_QUAD_TO_NUM(fhs, op3, 2);

#define ION_DECIMAL_CONVERT_TWO_OPERAND \
    BYTE operands_buffer[2 * ION_DECNUMBER_DECQUAD_SIZE]; \
    size_t offset; \
    decNumber *op1, *op2; \
    memset(operands_buffer, 0, 2 * ION_DECNUMBER_DECQUAD_SIZE); \
    ION_DECIMAL_QUAD_TO_NUM(lhs, op1, 0); \
    ION_DECIMAL_QUAD_TO_NUM(rhs, op2, 1); \

#define ION_DECIMAL_OVERFLOW_API_BUILDER(name, num_ops, api_args, if_quad, if_quad_args, decnum_mask_calc, if_number, if_number_args, helper_args, convert_ops, converted_args, standardized_args) \
iERR _##name##_standardized helper_args { \
    iENTER; \
    convert_ops; \
    IONCHECK(_ion_decimal_number_alloc(NULL, context->digits, &ION_DECIMAL_AS_NUMBER(value))); \
    if_number converted_args; \
    value->type = ION_DECIMAL_TYPE_NUMBER; \
    iRETURN; \
} \
iERR name api_args { \
    iENTER; \
    uint32_t status; \
    int decnum_mask = decnum_mask_calc; \
    if (decnum_mask > 0 && decnum_mask < ((1 << num_ops) - 1)) { \
        /* Not all operands have the same type. Convert all non-decNumbers to decNumbers. */ \
        IONCHECK(_##name##_standardized standardized_args); \
    } \
    else if (decnum_mask){ \
        ASSERT(decnum_mask == ((1 << num_ops) - 1)); \
        /* All operands are decNumbers. */ \
        IONCHECK(_ion_decimal_number_alloc(NULL, context->digits, &ION_DECIMAL_AS_NUMBER(value))); \
        if_number if_number_args; \
        value->type = ION_DECIMAL_TYPE_NUMBER; \
    } \
    else { \
        ASSERT(decnum_mask == 0); \
        /* All operands are decQuads. */ \
        status = decContextSaveStatus(context, DEC_Inexact); \
        decContextClearStatus(context, DEC_Inexact); \
        if_quad if_quad_args; \
        value->type = ION_DECIMAL_TYPE_QUAD; \
        if (decContextTestStatus(context, DEC_Inexact)) { \
            /* The operation overflowed the maximum decQuad precision. Convert operands and result to decNumbers. */ \
            IONCHECK(_##name##_standardized standardized_args); \
        } \
        decContextRestoreStatus(context, status, DEC_Inexact); \
    } \
    iRETURN; \
}

#define ION_DECIMAL_CALC_THREE_OP(name, if_quad, if_number) \
    ION_DECIMAL_OVERFLOW_API_BUILDER ( \
        name, \
        /*num_ops=*/3, \
        /*api_args=*/(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, const ION_DECIMAL *fhs, decContext *context), \
        if_quad, \
        /*if_quad_args=*/(ION_DECIMAL_AS_QUAD(value), ION_DECIMAL_AS_QUAD(lhs), ION_DECIMAL_AS_QUAD(rhs), ION_DECIMAL_AS_QUAD(fhs), context), \
        /*decnum_mask_calc=*/ ((lhs->type == ION_DECIMAL_TYPE_NUMBER ? 1 : 0) | (rhs->type == ION_DECIMAL_TYPE_NUMBER ? 2 : 0) | (fhs->type == ION_DECIMAL_TYPE_NUMBER ? 4 : 0)), \
        if_number, \
        /*if_number_args=*/(ION_DECIMAL_AS_NUMBER(value), ION_DECIMAL_AS_NUMBER(lhs), ION_DECIMAL_AS_NUMBER(rhs), ION_DECIMAL_AS_NUMBER(fhs), context), \
        /*helper_args=*/(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, const ION_DECIMAL *fhs, decContext *context, int decnum_mask), \
        /*convert_ops=*/ION_DECIMAL_CONVERT_THREE_OPERAND, \
        /*converted_args=*/(ION_DECIMAL_AS_NUMBER(value), op1, op2, op3, context), \
        /*standardized_args=*/(value, lhs, rhs, fhs, context, decnum_mask) \
    ) \

#define ION_DECIMAL_CALC_TWO_OP(name, if_quad, if_number) \
    ION_DECIMAL_OVERFLOW_API_BUILDER ( \
        name, \
        /*num_ops=*/2, \
        /*api_args=*/(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context), \
        if_quad, \
        /*if_quad_args=*/(ION_DECIMAL_AS_QUAD(value), ION_DECIMAL_AS_QUAD(lhs), ION_DECIMAL_AS_QUAD(rhs), context), \
        /*decnum_mask_calc=*/ ((lhs->type == ION_DECIMAL_TYPE_NUMBER ? 1 : 0) | (rhs->type == ION_DECIMAL_TYPE_NUMBER ? 2 : 0)), \
        if_number, \
        /*if_number_args=*/(ION_DECIMAL_AS_NUMBER(value), ION_DECIMAL_AS_NUMBER(lhs), ION_DECIMAL_AS_NUMBER(rhs), context), \
        /*helper_args=*/(ION_DECIMAL *value, const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context, int decnum_mask), \
        /*convert_ops=*/ION_DECIMAL_CONVERT_TWO_OPERAND, \
        /*converted_args=*/(ION_DECIMAL_AS_NUMBER(value), op1, op2, context), \
        /*standardized_args=*/(value, lhs, rhs, context, decnum_mask) \
    ) \

ION_DECIMAL_CALC_THREE_OP(ion_decimal_fma, decQuadFMA, decNumberFMA);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_add, decQuadAdd, decNumberAdd);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_and, decQuadAnd, decNumberAnd);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_divide, decQuadDivide, decNumberDivide);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_divide_integer, decQuadDivideInteger, decNumberDivideInteger);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_max, decQuadMax, decNumberMax);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_max_mag, decQuadMaxMag, decNumberMaxMag);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_min, decQuadMin, decNumberMin);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_min_mag, decQuadMinMag, decNumberMinMag);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_multiply, decQuadMultiply, decNumberMultiply);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_or, decQuadOr, decNumberOr);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_quantize, decQuadQuantize, decNumberQuantize);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_remainder, decQuadRemainder, decNumberRemainder);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_remainder_near, decQuadRemainderNear, decNumberRemainderNear);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_rotate, decQuadRotate, decNumberRotate);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_scaleb, decQuadScaleB, decNumberScaleB);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_shift, decQuadShift, decNumberShift);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_subtract, decQuadSubtract, decNumberSubtract);
ION_DECIMAL_CALC_TWO_OP(ion_decimal_xor, decQuadXor, decNumberXor);

iERR _ion_decimal_equals_helper(const ION_DECIMAL *lhs, const ION_DECIMAL *rhs, decContext *context, BOOL *is_equal) {
    iENTER;
    int decnum_mask = (lhs->type == ION_DECIMAL_TYPE_NUMBER ? 1 : 0) | (rhs->type == ION_DECIMAL_TYPE_NUMBER ? 2 : 0);
    ION_DECIMAL_CONVERT_TWO_OPERAND;
    IONCHECK(_ion_decimal_equals_number(op1, op2, context, is_equal));
    iRETURN;
}

iERR ion_decimal_equals(const ION_DECIMAL *left, const ION_DECIMAL *right, decContext *context, BOOL *is_equal) {
    iENTER;
    ASSERT(is_equal);
    if (left->type != right->type) {
        IONCHECK(_ion_decimal_equals_helper(left, right, context, is_equal));
        SUCCEED();
    }
    switch (left->type) {
        case ION_DECIMAL_TYPE_QUAD:
            IONCHECK(ion_decimal_equals_quad(ION_DECIMAL_AS_QUAD(left), ION_DECIMAL_AS_QUAD(right), context, is_equal));
            break;
        case ION_DECIMAL_TYPE_NUMBER:
            IONCHECK(_ion_decimal_equals_number(ION_DECIMAL_AS_NUMBER(left), ION_DECIMAL_AS_NUMBER(right), context, is_equal));
            break;
        default:
            FAILWITH(IERR_INVALID_ARG);
    }
    iRETURN;
}