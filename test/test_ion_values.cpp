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

#include "ion_assert.h"
#include "ion_helpers.h"
#include "ion_test_util.h"
#include "ion_decimal_impl.h"


TEST(IonTimestamp, IgnoresSuperfluousOffset) {
    ION_TIMESTAMP expected1, expected2, actual;
    BOOL has_local_offset;
    int local_offset;

    ION_ASSERT_OK(ion_timestamp_for_year(&expected1, 1));

    ION_ASSERT_OK(ion_timestamp_for_year(&expected2, 1));
    SET_FLAG_ON(expected2.precision, ION_TT_BIT_TZ);
    expected2.tz_offset = 1;

    ION_ASSERT_OK(ion_timestamp_for_year(&actual, 1));
    ION_ASSERT_OK(ion_timestamp_set_local_offset(&actual, 1));
    ION_ASSERT_OK(ion_timestamp_has_local_offset(&actual, &has_local_offset));
    ION_ASSERT_OK(ion_timestamp_get_local_offset(&actual, &local_offset));

    ASSERT_FALSE(has_local_offset);
    ASSERT_EQ(0, actual.tz_offset);
    ASSERT_EQ(0, local_offset);
    ASSERT_TRUE(assertIonTimestampEq(&expected1, &actual));
    ASSERT_TRUE(assertIonTimestampEq(&expected2, &actual)); // Equivalence ignores the superfluous offset as well.
}

TEST(IonDecimal, FMADecQuad) {
    ION_DECIMAL result, lhs, rhs, fhs, expected;
    // The operands are all backed by decQuads.
    ION_ASSERT_OK(ion_decimal_from_int32(&lhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    ION_ASSERT_OK(ion_decimal_fma(&result, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&expected, 101));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    ION_ASSERT_OK(ion_decimal_release(&result));
    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&fhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, FMADecQuadInPlaceAllOperandsSame) {
    ION_DECIMAL lhs, expected;
    // The operands are all backed by decQuads.
    ION_ASSERT_OK(ion_decimal_from_int32(&lhs, 10));
    ION_ASSERT_OK(ion_decimal_fma(&lhs, &lhs, &lhs, &lhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&expected, 110));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &lhs));

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, FMADecNumber) {
    ION_DECIMAL result, lhs, rhs, fhs, expected;
    // Because these decimals have more than DECQUAD_Pmax digits, they will be backed by decNumbers.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&rhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&fhs, "-100000000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_fma(&result, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "10000000000000000000000000000000000000100000000000000000000000000000000000000.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    ION_ASSERT_OK(ion_decimal_release(&result));
    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&fhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, FMAMixed) {
    ION_DECIMAL result, lhs, rhs, fhs, expected;
    // Because this decimal has more than DECQUAD_Pmax digits, it will be backed by a decNumber.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    // These operands are backed by decQuads. They will be temporarily converted to decNumbers to perform the calculation.
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    ION_ASSERT_OK(ion_decimal_fma(&result, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "1000000000000000000000000000000000000011.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, fhs.type);

    ION_ASSERT_OK(ion_decimal_release(&result));
    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&fhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, FMAMixedInPlaceNumber) {
    ION_DECIMAL lhs, rhs, fhs, expected;
    // Because this decimal has more than DECQUAD_Pmax digits, it will be backed by a decNumber.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    // These operands are backed by decQuads. They will be temporarily converted to decNumbers to perform the calculation.
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    ION_ASSERT_OK(ion_decimal_fma(&lhs, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "1000000000000000000000000000000000000011.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &lhs));

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, fhs.type);

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&fhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, FMAMixedInPlaceQuad) {
    ION_DECIMAL lhs, rhs, fhs, expected;
    // Because this decimal has more than DECQUAD_Pmax digits, it will be backed by a decNumber.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    // These operands are backed by decQuads. They will be temporarily converted to decNumbers to perform the calculation.
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    ION_ASSERT_OK(ion_decimal_fma(&fhs, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "1000000000000000000000000000000000000011.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &fhs));

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_NUMBER, lhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&fhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, FMADecQuadOverflows) {
    ION_DECIMAL result, lhs, rhs, fhs, expected;
    // This decimal has exactly DECQUAD_Pmax digits, so it fits into a decQuad.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "1000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    // The operation will try to keep this in decQuads, but detects overflow and upgrades them to decNumbers.
    ION_ASSERT_OK(ion_decimal_fma(&result, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "10000000000000000000000000000000011.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    // Asserts that the operation results in a decNumber.
    ASSERT_EQ(ION_DECIMAL_TYPE_NUMBER, result.type);

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, lhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, fhs.type);

    ION_ASSERT_OK(ion_decimal_release(&result));
    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&fhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, FMADecQuadOverflowsInPlace) {
    ION_DECIMAL lhs, rhs, fhs, expected;
    // This decimal has exactly DECQUAD_Pmax digits, so it fits into a decQuad.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "1000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    // The operation will try to keep this in decQuads, but detects overflow and upgrades them to decNumbers.
    ION_ASSERT_OK(ion_decimal_fma(&lhs, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "10000000000000000000000000000000011.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &lhs));

    // Asserts that the operation results in a decNumber.
    ASSERT_EQ(ION_DECIMAL_TYPE_NUMBER, lhs.type);

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, fhs.type);

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&fhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, FMADecQuadOverflowsTwoOperandsSameAsOutput) {
    ION_DECIMAL lhs, rhs, expected;
    // This decimal has exactly DECQUAD_Pmax digits, so it fits into a decQuad.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "1000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 11));
    // The operation will try to keep this in decQuads, but detects overflow and upgrades them to decNumbers.
    ION_ASSERT_OK(ion_decimal_fma(&rhs, &lhs, &rhs, &rhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "11000000000000000000000000000000022.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &rhs));

    // Asserts that the operation results in a decNumber.
    ASSERT_EQ(ION_DECIMAL_TYPE_NUMBER, rhs.type);

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, lhs.type);

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, AddDecQuad) {
    ION_DECIMAL result, lhs, rhs, expected;
    // The operands are all backed by decQuads.
    ION_ASSERT_OK(ion_decimal_from_int32(&lhs, 9));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 1));
    ION_ASSERT_OK(ion_decimal_add(&result, &lhs, &rhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&expected, 10));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    ION_ASSERT_OK(ion_decimal_release(&result));
    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, AddDecNumber) {
    ION_DECIMAL result, lhs, rhs, expected;
    // Because these decimals have more than DECQUAD_Pmax digits, they will be backed by decNumbers.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&rhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_add(&result, &lhs, &rhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "200000000000000000000000000000000000002.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    ION_ASSERT_OK(ion_decimal_release(&result));
    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, AddMixed) {
    ION_DECIMAL result, lhs, rhs, expected;
    // Because this decimal has more than DECQUAD_Pmax digits, it will be backed by a decNumber.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "100000000000000000000000000000000000002.", &g_TestDecimalContext));

    // These operands are backed by decQuads. They will be temporarily converted to decNumbers to perform the calculation.
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, -1));
    ION_ASSERT_OK(ion_decimal_add(&result, &lhs, &rhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);

    ION_ASSERT_OK(ion_decimal_release(&result));
    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, AddDecQuadOverflows) {
    ION_DECIMAL result, lhs, rhs, expected;
    // This decimal has exactly DECQUAD_Pmax digits, so it fits into a decQuad.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "9999999999999999999999999999999999.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 2));
    // The operation will try to keep this in decQuads, but detects overflow and upgrades them to decNumbers.
    ION_ASSERT_OK(ion_decimal_add(&result, &lhs, &rhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "10000000000000000000000000000000001.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    // Asserts that the operation results in a decNumber.
    ASSERT_EQ(ION_DECIMAL_TYPE_NUMBER, result.type);

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, lhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);

    ION_ASSERT_OK(ion_decimal_release(&result));
    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, AddDecQuadOverflowsInPlace) {
    ION_DECIMAL lhs, rhs, expected;
    // This decimal has exactly DECQUAD_Pmax digits, so it fits into a decQuad.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "9999999999999999999999999999999999.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 2));
    // The operation will try to keep this in decQuads, but detects overflow and upgrades them to decNumbers.
    ION_ASSERT_OK(ion_decimal_add(&lhs, &lhs, &rhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "10000000000000000000000000000000001.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &lhs));

    // Asserts that the operation results in a decNumber.
    ASSERT_EQ(ION_DECIMAL_TYPE_NUMBER, lhs.type);

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, AddDecQuadInPlaceAllOperandsSame) {
    ION_DECIMAL lhs, expected;
    ION_ASSERT_OK(ion_decimal_from_int32(&lhs, 1));
    ION_ASSERT_OK(ion_decimal_add(&lhs, &lhs, &lhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&expected, 2));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &lhs));
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, lhs.type);

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&expected));
}

TEST(IonDecimal, EqualsWithMixedOperands) {
    decNumber number;
    decQuad quad;
    ION_DECIMAL lhs, rhs;

    // Note: no need to allocate extra space for this decNumber because it always has at least one decimal unit
    // available (and 7 fits in one decimal unit).
    decNumberFromInt32(&number, 7);
    decQuadFromInt32(&quad, 7);
    ION_ASSERT_OK(ion_decimal_from_number(&lhs, &number));
    ION_ASSERT_OK(ion_decimal_from_quad(&rhs, &quad));

    ASSERT_TRUE(assertIonDecimalEq(&lhs, &rhs));
    ASSERT_TRUE(assertIonDecimalEq(&rhs, &lhs));
    ASSERT_TRUE(assertIonDecimalEq(&rhs, &rhs));
    ASSERT_TRUE(assertIonDecimalEq(&lhs, &lhs));

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&rhs));
}

TEST(IonDecimal, IsNegative) {
    ION_DECIMAL ion_number_positive, ion_number_negative, ion_quad_positive, ion_quad_negative;
    decNumber number_positive, number_negative;
    decQuad quad_positive, quad_negative;

    decNumberFromInt32(&number_positive, 1);
    decNumberFromInt32(&number_negative, -1);
    decQuadFromInt32(&quad_positive, 1);
    decQuadFromInt32(&quad_negative, -1);

    ION_ASSERT_OK(ion_decimal_from_number(&ion_number_positive, &number_positive));
    ION_ASSERT_OK(ion_decimal_from_number(&ion_number_negative, &number_negative));
    ION_ASSERT_OK(ion_decimal_from_quad(&ion_quad_positive, &quad_positive));
    ION_ASSERT_OK(ion_decimal_from_quad(&ion_quad_negative, &quad_negative));

    ASSERT_TRUE(ion_decimal_is_negative(&ion_number_negative));
    ASSERT_TRUE(ion_decimal_is_negative(&ion_quad_negative));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_number_positive));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_quad_positive));

    ION_ASSERT_OK(ion_decimal_release(&ion_number_positive));
    ION_ASSERT_OK(ion_decimal_release(&ion_number_negative));
    ION_ASSERT_OK(ion_decimal_release(&ion_quad_positive));
    ION_ASSERT_OK(ion_decimal_release(&ion_quad_negative));
}

TEST(IonDecimal, AbsQuad) {
    ION_DECIMAL ion_quad_negative, ion_quad_positive, ion_quad_positive_result;
    ION_ASSERT_OK(ion_decimal_from_int32(&ion_quad_negative, -999999));
    ION_ASSERT_OK(ion_decimal_from_int32(&ion_quad_positive, 999999));
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, ion_quad_negative.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, ion_quad_positive.type);
    ASSERT_TRUE(ion_decimal_is_negative(&ion_quad_negative));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_quad_positive));
    ION_ASSERT_OK(ion_decimal_abs(&ion_quad_negative, &ion_quad_negative, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_abs(&ion_quad_positive_result, &ion_quad_positive, &g_TestDecimalContext));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_quad_negative));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_quad_positive));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_quad_positive_result));
    ASSERT_TRUE(assertIonDecimalEq(&ion_quad_positive, &ion_quad_negative));
    ASSERT_TRUE(assertIonDecimalEq(&ion_quad_positive, &ion_quad_positive_result));

    ION_ASSERT_OK(ion_decimal_release(&ion_quad_positive));
    ION_ASSERT_OK(ion_decimal_release(&ion_quad_negative));
    ION_ASSERT_OK(ion_decimal_release(&ion_quad_positive_result));
}

TEST(IonDecimal, AbsNumber) {
    ION_DECIMAL ion_number_negative, ion_number_positive, ion_number_positive_result;
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number_negative, "-999999999999999999999999999999999999999999", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number_positive, "999999999999999999999999999999999999999999", &g_TestDecimalContext));
    ASSERT_EQ(ION_DECIMAL_TYPE_NUMBER, ion_number_negative.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_NUMBER, ion_number_positive.type);
    ASSERT_TRUE(ion_decimal_is_negative(&ion_number_negative));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_number_positive));
    ION_ASSERT_OK(ion_decimal_abs(&ion_number_negative, &ion_number_negative, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_abs(&ion_number_positive_result, &ion_number_positive, &g_TestDecimalContext));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_number_negative));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_number_positive));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_number_positive_result));
    ASSERT_TRUE(assertIonDecimalEq(&ion_number_positive, &ion_number_negative));
    ASSERT_TRUE(assertIonDecimalEq(&ion_number_positive, &ion_number_positive_result));

    ION_ASSERT_OK(ion_decimal_release(&ion_number_positive));
    ION_ASSERT_OK(ion_decimal_release(&ion_number_negative));
    ION_ASSERT_OK(ion_decimal_release(&ion_number_positive_result));
}
