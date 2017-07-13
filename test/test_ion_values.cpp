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
    ION_DECIMAL result1, result2, lhs, rhs, fhs, expected;
    // The operands are all backed by decQuads.
    ION_ASSERT_OK(ion_decimal_from_int32(&lhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    ION_ASSERT_OK(ion_decimal_fma(&result1, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_fma_macro(&result2, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&expected, 101));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result1));
    ASSERT_TRUE(assertIonDecimalEq(&result1, &result2));

    /*
    char expected_str[ION_TEST_DECIMAL_MAX_STRLEN];
    ION_ASSERT_OK(ion_decimal_to_string(&result2, expected_str));
    std::cout << expected_str << std::endl;
     */
}

TEST(IonDecimal, FMADecNumber) {
    ION_DECIMAL result1, result2, lhs, rhs, fhs, expected;
    // Because this decimal has more than DECQUAD_Pmax digits, it will be backed by a decNumber.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext, NULL));
    /*
    char lhs_str[ION_TEST_DECIMAL_MAX_STRLEN];
    ION_ASSERT_OK(ion_decimal_to_string(&lhs, lhs_str));
    std::cout << lhs_str << std::endl;
    */
    // These operands are backed by decQuads. They will be temporarily converted to decNumbers to perform the calculation.
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    ION_ASSERT_OK(ion_decimal_fma(&result1, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_fma_macro(&result2, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "1000000000000000000000000000000000000011.", &g_TestDecimalContext, NULL));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result1));
    ASSERT_TRUE(assertIonDecimalEq(&result1, &result2));

    /*
    char expected_str[ION_TEST_DECIMAL_MAX_STRLEN];
    ION_ASSERT_OK(ion_decimal_to_string(&result2, expected_str));
    std::cout << expected_str << std::endl;
    */
    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, fhs.type);

    ION_ASSERT_OK(ion_decimal_release(&lhs));
    ION_ASSERT_OK(ion_decimal_release(&result1));
    ION_ASSERT_OK(ion_decimal_release(&result2));
}

TEST(IonDecimal, FMADecQuadOverflows) {
    ION_DECIMAL result1, result2, lhs, rhs, fhs, expected;
    // This decimal has exactly DECQUAD_Pmax digits, so it fits into a decQuad.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "1000000000000000000000000000000001.", &g_TestDecimalContext, NULL));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 10));
    ION_ASSERT_OK(ion_decimal_from_int32(&fhs, 1));
    // The operation will try to keep this in decQuads, but detects overflow and upgrades them to decNumbers.
    ION_ASSERT_OK(ion_decimal_fma(&result1, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_fma_macro(&result2, &lhs, &rhs, &fhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "10000000000000000000000000000000011.", &g_TestDecimalContext, NULL));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result1));
    ASSERT_TRUE(assertIonDecimalEq(&result1, &result2));

    // Asserts that the operation did not change the operands.
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, lhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, rhs.type);
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, fhs.type);

    ION_ASSERT_OK(ion_decimal_release(&result1));
    ION_ASSERT_OK(ion_decimal_release(&result2));
}
