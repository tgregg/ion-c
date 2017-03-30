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

TIMESTAMP_COMPARISON_FN g_TimestampEquals = ion_timestamp_equals;
std::string g_CurrentTest = "NONE";

char *ionIntToString(ION_INT *value) {
    SIZE len, written;
    ion_int_char_length(value, &len);
    char *int_str = (char *)malloc(len * sizeof(char));
    ion_int_to_char(value, (BYTE *)int_str, len, &written);
    return int_str;
}

char *ionStringToString(ION_STRING *value) {
    if (!value) {
        return (char *)"NULL";
    }
    return ion_string_strdup(value);
}

::testing::AssertionResult assertIonStringEq(ION_STRING *expected, ION_STRING *actual) {
    if (!(expected == NULL ^ actual == NULL)) {
        if (expected == NULL || ion_string_is_equal(expected, actual)) {
            return ::testing::AssertionSuccess();
        }
    }
    return ::testing::AssertionFailure()
            << std::string("") << ionStringToString(expected) << "  vs. " << ionStringToString(actual);
}

::testing::AssertionResult assertIonIntEq(ION_INT *expected, ION_INT *actual) {
    int int_comparison = 0;
    char *expected_str = NULL;
    char *actual_str = NULL;
    EXPECT_EQ(IERR_OK, ion_int_compare(expected, actual, &int_comparison));
    if (int_comparison == 0) {
        return ::testing::AssertionSuccess();
    }
    expected_str = ionIntToString(expected);
    actual_str = ionIntToString(actual);
    ::testing::AssertionResult result = ::testing::AssertionFailure()
            << std::string("") << expected_str << " vs. " << actual_str;
    free(expected_str);
    free(actual_str);
    return result;
}

::testing::AssertionResult assertIonDecimalEq(decQuad *expected, decQuad *actual) {
    BOOL decimal_equals;
    EXPECT_EQ(IERR_OK, ion_decimal_equals(expected, actual, &g_Context, &decimal_equals));
    if (decimal_equals) {
        return ::testing::AssertionSuccess();
    }
    char expected_str[DECQUAD_String];
    char actual_str[DECQUAD_String];
    decQuadToString(expected, expected_str);
    decQuadToString(actual, actual_str);
    return ::testing::AssertionFailure()
            << std::string("") << expected_str << " vs. " << actual_str;
}

::testing::AssertionResult assertIonTimestampEq(ION_TIMESTAMP *expected, ION_TIMESTAMP *actual) {
    BOOL timestamps_equal;
    EXPECT_EQ(IERR_OK, g_TimestampEquals(expected, actual, &timestamps_equal, &g_Context));
    if (timestamps_equal) {
        return ::testing::AssertionSuccess();
    }
    char expected_str[ION_MAX_TIMESTAMP_STRING];
    char actual_str[ION_MAX_TIMESTAMP_STRING];
    SIZE expected_str_len, actual_str_len;
    EXPECT_EQ(IERR_OK, ion_timestamp_to_string(expected, expected_str, ION_MAX_TIMESTAMP_STRING, &expected_str_len, &g_Context));
    EXPECT_EQ(IERR_OK, ion_timestamp_to_string(actual, actual_str, ION_MAX_TIMESTAMP_STRING, &actual_str_len, &g_Context));
    return ::testing::AssertionFailure()
            << std::string(expected_str, (size_t)expected_str_len) << " vs. " << std::string(actual_str, (size_t)actual_str_len);
}

BOOL ionStringEq(ION_STRING *expected, ION_STRING *actual) {
    return assertIonStringEq(expected, actual) == ::testing::AssertionSuccess();
}

