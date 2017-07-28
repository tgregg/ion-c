/*
 * Copyright 2009-2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

TEST(IonTextDecimal, ReaderPreservesFullFidelityDecNumber) {
    const char *text_decimal = "1.1999999999999999555910790149937383830547332763671875\n1.1999999999999999555910790149937383830547332763671875";
    hREADER reader;
    ION_TYPE type;
    ION_DECIMAL ion_decimal_1, ion_decimal_2;

    hWRITER writer = NULL;
    ION_STREAM *ion_stream = NULL;
    BYTE *result;
    SIZE result_len;

    BOOL is_equal;

    ION_ASSERT_OK(ion_test_new_text_reader(text_decimal, &reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal_1));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal_2));

    ION_ASSERT_OK(ion_decimal_equals(&ion_decimal_1, &ion_decimal_2, &g_TestDecimalContext, &is_equal));
    ASSERT_TRUE(is_equal);

    ION_ASSERT_OK(ion_test_new_writer(&writer, &ion_stream, FALSE));
    ION_ASSERT_OK(ion_writer_write_ion_decimal(writer, &ion_decimal_1));
    ION_ASSERT_OK(ion_writer_write_ion_decimal(writer, &ion_decimal_2));
    ION_ASSERT_OK(ion_test_writer_get_bytes(writer, ion_stream, &result, &result_len));

    ION_ASSERT_OK(ion_reader_close(reader));
    ASSERT_EQ(strlen(text_decimal), result_len) << text_decimal << " vs. " << std::endl
                                                << std::string((char *)result, result_len);
    assertStringsEqual(text_decimal, (char *)result, result_len);
    free(result);
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal_1));
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal_2));
}

TEST(IonTextDecimal, ReaderFailsUponLossOfPrecisionDecNumber) {
    // This test asserts that an error is raised when decimal precision would be lost. From the
    // `ion_reader_read_ion_decimal` API, this only occurs when the input has more digits of precision than would fit in
    // a decQuad , and the precision exceeds the context's max digits.
    decContext context = {
            DECQUAD_Pmax,                   // max digits
            DEC_MAX_MATH,                   // max exponent
            -DEC_MAX_MATH,                  // min exponent
            DEC_ROUND_HALF_EVEN,            // rounding mode
            DEC_Errors,                     // trap conditions
            0,                              // status flags
            0                               // apply exponent clamp?
    };
    const char *text_decimal = "1.1999999999999999555910790149937383830547332763671875";
    hREADER reader;
    ION_READER_OPTIONS options;
    ION_TYPE type;
    ION_DECIMAL ion_decimal;

    ion_test_initialize_reader_options(&options);
    options.decimal_context = &context;
    ION_ASSERT_OK(ion_reader_open_buffer(&reader, (BYTE *)text_decimal, strlen(text_decimal), &options));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ASSERT_EQ(IERR_NUMERIC_OVERFLOW, ion_reader_read_ion_decimal(reader, &ion_decimal));
    ION_ASSERT_OK(ion_reader_close(reader));
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal));
}

TEST(IonTextDecimal, ReaderFailsUponLossOfPrecisionDecQuad) {
    // This test asserts that an error is raised when decimal precision would be lost. From the
    // `ion_reader_read_decimal` API, This always occurs when the input has more digits of precision than would fit in a
    // decQuad.
    decContext context = {
            DECQUAD_Pmax,                   // max digits
            DEC_MAX_MATH,                   // max exponent
            -DEC_MAX_MATH,                  // min exponent
            DEC_ROUND_HALF_EVEN,            // rounding mode
            DEC_Errors,                     // trap conditions
            0,                              // status flags
            0                               // apply exponent clamp?
    };
    const char *text_decimal = "1.1999999999999999555910790149937383830547332763671875";
    hREADER reader;
    ION_READER_OPTIONS options;
    ION_TYPE type;
    decQuad quad;

    ion_test_initialize_reader_options(&options);
    options.decimal_context = &context;
    ION_ASSERT_OK(ion_reader_open_buffer(&reader, (BYTE *)text_decimal, strlen(text_decimal), &options));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ASSERT_EQ(IERR_NUMERIC_OVERFLOW, ion_reader_read_decimal(reader, &quad));
    ION_ASSERT_OK(ion_reader_close(reader));
}

TEST(IonTextDecimal, ReaderAlwaysPreservesUpTo34Digits) {
    // Because decQuads are statically sized, decimals with <= DECQUAD_Pmax digits of precision never need to overflow;
    // they ca always be accommodated in a decQuad. This test asserts that precision is preserved even when the context
    // is configured with fewer digits than DECQUAD_Pmax.
    decContext context = {
            3,                              // max digits
            DEC_MAX_MATH,                   // max exponent
            -DEC_MAX_MATH,                  // min exponent
            DEC_ROUND_HALF_EVEN,            // rounding mode
            DEC_Errors,                     // trap conditions
            0,                              // status flags
            0                               // apply exponent clamp?
    };
    const char *text_decimal = "1.234\n5.678";
    hREADER reader;
    ION_READER_OPTIONS options;
    ION_TYPE type;
    ION_DECIMAL ion_decimal;
    decQuad quad;

    hWRITER writer = NULL;
    ION_STREAM *ion_stream = NULL;
    BYTE *result;
    SIZE result_len;

    ion_test_initialize_reader_options(&options);
    options.decimal_context = &context;
    ION_ASSERT_OK(ion_reader_open_buffer(&reader, (BYTE *)text_decimal, strlen(text_decimal), &options));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_decimal(reader, &quad));

    ION_ASSERT_OK(ion_test_new_writer(&writer, &ion_stream, FALSE));
    ION_ASSERT_OK(ion_writer_write_ion_decimal(writer, &ion_decimal));
    ION_ASSERT_OK(ion_writer_write_decimal(writer, &quad));
    ION_ASSERT_OK(ion_test_writer_get_bytes(writer, ion_stream, &result, &result_len));

    ION_ASSERT_OK(ion_reader_close(reader));
    ASSERT_EQ(strlen(text_decimal), result_len) << text_decimal << " vs. " << std::endl
                                                << std::string((char *)result, result_len);
    assertStringsEqual(text_decimal, (char *)result, result_len);
    free(result);
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal));
}

TEST(IonDecimal, WriteAllValues) {
    const char *text_decimals = "1.1999999999999999555910790149937383830547332763671875\n-1d+123";
    hREADER reader;
    ION_READER_OPTIONS options;
    ION_DECIMAL ion_decimal;

    hWRITER writer = NULL;
    ION_STREAM *ion_stream = NULL;
    BYTE *result;
    SIZE result_len;

    ion_test_initialize_reader_options(&options);
    ION_ASSERT_OK(ion_reader_open_buffer(&reader, (BYTE *)text_decimals, strlen(text_decimals), &options));
    ION_ASSERT_OK(ion_test_new_writer(&writer, &ion_stream, FALSE));
    ION_ASSERT_OK(ion_writer_write_all_values(writer, reader));
    ION_ASSERT_OK(ion_test_writer_get_bytes(writer, ion_stream, &result, &result_len));

    ION_ASSERT_OK(ion_reader_close(reader));
    assertStringsEqual(text_decimals, (char *)result, result_len);
    free(result);
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal));
}

TEST(IonBinaryDecimal, RoundtripPreservesFullFidelityDecNumber) {
    const char *text_decimal = "1.1999999999999999555910790149937383830547332763671875";
    hREADER reader;
    ION_TYPE type;
    ION_DECIMAL ion_decimal_before, ion_decimal_after;
    BOOL equals;

    hWRITER writer = NULL;
    ION_STREAM *ion_stream = NULL;
    BYTE *result;
    SIZE result_len;

    ION_ASSERT_OK(ion_test_new_text_reader(text_decimal, &reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal_before));
    // Make sure we start with a full-fidelity decimal, otherwise the test would incorrectly succeed.
    ASSERT_TRUE(ION_DECIMAL_IS_NUMBER((&ion_decimal_before)));
    ASSERT_EQ(53, ion_decimal_before.value.num_value->digits);

    ION_ASSERT_OK(ion_test_new_writer(&writer, &ion_stream, TRUE));
    ION_ASSERT_OK(ion_writer_write_ion_decimal(writer, &ion_decimal_before));
    ION_ASSERT_OK(ion_test_writer_get_bytes(writer, ion_stream, &result, &result_len));

    ION_ASSERT_OK(ion_reader_close(reader));
    ION_ASSERT_OK(ion_test_new_reader(result, (SIZE)result_len, &reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal_after));
    ION_ASSERT_OK(ion_decimal_equals(&ion_decimal_before, &ion_decimal_after, &((ION_READER *)reader)->_deccontext, &equals));
    ION_ASSERT_OK(ion_reader_close(reader));

    ASSERT_TRUE(equals);
    free(result);
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal_before));
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal_after));
}

TEST(IonBinaryDecimal, ReaderFailsUponLossOfPrecisionDecNumber) {
    // This test asserts that an error is raised when decimal precision would be lost. From the
    // `ion_reader_read_ion_decimal` API, this only occurs when the input has more digits of precision than would fit in
    // a decQuad , and the precision exceeds the context's max digits.
    decContext context = {
            DECQUAD_Pmax,                   // max digits
            DEC_MAX_MATH,                   // max exponent
            -DEC_MAX_MATH,                  // min exponent
            DEC_ROUND_HALF_EVEN,            // rounding mode
            DEC_Errors,                     // trap conditions
            0,                              // status flags
            0                               // apply exponent clamp?
    };
    const char *text_decimal = "1.1999999999999999555910790149937383830547332763671875";
    hREADER reader;
    ION_READER_OPTIONS options;
    ION_TYPE type;
    ION_DECIMAL ion_decimal;

    hWRITER writer = NULL;
    ION_STREAM *ion_stream = NULL;
    BYTE *result;
    SIZE result_len;

    // This reader supports arbitrarily-high decimal precision.
    ION_ASSERT_OK(ion_test_new_text_reader(text_decimal, &reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal));
    ASSERT_TRUE(ION_DECIMAL_IS_NUMBER((&ion_decimal)));
    ASSERT_EQ(53, ion_decimal.value.num_value->digits);

    ION_ASSERT_OK(ion_test_new_writer(&writer, &ion_stream, TRUE));
    ION_ASSERT_OK(ion_writer_write_ion_decimal(writer, &ion_decimal));
    ION_ASSERT_OK(ion_test_writer_get_bytes(writer, ion_stream, &result, &result_len));

    ION_ASSERT_OK(ion_reader_close(reader));
    ion_test_initialize_reader_options(&options);
    options.decimal_context = &context;
    // This reader only supports decQuad precision, which the input exceeds.
    ION_ASSERT_OK(ion_reader_open_buffer(&reader, (BYTE *)result, result_len, &options));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ASSERT_EQ(IERR_NUMERIC_OVERFLOW, ion_reader_read_ion_decimal(reader, &ion_decimal));
    ION_ASSERT_OK(ion_reader_close(reader));

    free(result);
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal));
}

TEST(IonBinaryDecimal, ReaderFailsUponLossOfPrecisionDecQuad) {
    // This test asserts that an error is raised when decimal precision would be lost. From the
    // `ion_reader_read_decimal` API, This always occurs when the input has more digits of precision than would fit in a
    // decQuad.
    decContext context = {
            DECQUAD_Pmax,                   // max digits
            DEC_MAX_MATH,                   // max exponent
            -DEC_MAX_MATH,                  // min exponent
            DEC_ROUND_HALF_EVEN,            // rounding mode
            DEC_Errors,                     // trap conditions
            0,                              // status flags
            0                               // apply exponent clamp?
    };
    const char *text_decimal = "1.1999999999999999555910790149937383830547332763671875";
    hREADER reader;
    ION_READER_OPTIONS options;
    ION_TYPE type;
    ION_DECIMAL ion_decimal;
    decQuad quad;

    hWRITER writer = NULL;
    ION_STREAM *ion_stream = NULL;
    BYTE *result;
    SIZE result_len;

    // This reader supports arbitrarily-high decimal precision.
    ION_ASSERT_OK(ion_test_new_text_reader(text_decimal, &reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal));
    ASSERT_TRUE(ION_DECIMAL_IS_NUMBER((&ion_decimal)));
    ASSERT_EQ(53, ion_decimal.value.num_value->digits);

    ION_ASSERT_OK(ion_test_new_writer(&writer, &ion_stream, TRUE));
    ION_ASSERT_OK(ion_writer_write_ion_decimal(writer, &ion_decimal));
    ION_ASSERT_OK(ion_test_writer_get_bytes(writer, ion_stream, &result, &result_len));

    ION_ASSERT_OK(ion_reader_close(reader));
    ion_test_initialize_reader_options(&options);
    options.decimal_context = &context;
    // This reader only supports decQuad precision, which the input exceeds.
    ION_ASSERT_OK(ion_reader_open_buffer(&reader, (BYTE *)result, result_len, &options));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ASSERT_EQ(IERR_NUMERIC_OVERFLOW, ion_reader_read_decimal(reader, &quad));
    ION_ASSERT_OK(ion_reader_close(reader));

    free(result);
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal));
}

TEST(IonBinaryDecimal, ReaderAlwaysPreservesUpTo34Digits) {
    // Because decQuads are statically sized, decimals with <= DECQUAD_Pmax digits of precision never need to overflow;
    // they ca always be accommodated in a decQuad. This test asserts that precision is preserved even when the context
    // is configured with fewer digits than DECQUAD_Pmax.
    decContext context = {
            3,                              // max digits
            DEC_MAX_MATH,                   // max exponent
            -DEC_MAX_MATH,                  // min exponent
            DEC_ROUND_HALF_EVEN,            // rounding mode
            DEC_Errors,                     // trap conditions
            0,                              // status flags
            0                               // apply exponent clamp?
    };
    const char *text_decimal = "1.234\n5.678";
    hREADER reader;
    ION_READER_OPTIONS options;
    ION_TYPE type;
    ION_DECIMAL ion_decimal_before, ion_decimal_after;
    decQuad quad_before, quad_after;

    hWRITER writer = NULL;
    ION_STREAM *ion_stream = NULL;
    BYTE *result;
    SIZE result_len;

    BOOL decimal_equals, quad_equals;

    // This reader supports arbitrarily-high decimal precision.
    ION_ASSERT_OK(ion_test_new_text_reader(text_decimal, &reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal_before));
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, ion_decimal_before.type);
    ASSERT_EQ(4, decQuadDigits(&ion_decimal_before.value.quad_value));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_decimal(reader, &quad_before));

    ION_ASSERT_OK(ion_test_new_writer(&writer, &ion_stream, TRUE));
    ION_ASSERT_OK(ion_writer_write_ion_decimal(writer, &ion_decimal_before));
    ION_ASSERT_OK(ion_writer_write_decimal(writer, &quad_before));
    ION_ASSERT_OK(ion_test_writer_get_bytes(writer, ion_stream, &result, &result_len));

    ion_test_initialize_reader_options(&options);
    options.decimal_context = &context;
    ION_ASSERT_OK(ion_reader_open_buffer(&reader, (BYTE *)text_decimal, strlen(text_decimal), &options));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_ion_decimal(reader, &ion_decimal_after));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_DECIMAL, type);
    ION_ASSERT_OK(ion_reader_read_decimal(reader, &quad_after));

    ION_ASSERT_OK(ion_decimal_equals(&ion_decimal_before, &ion_decimal_after, &((ION_READER *)reader)->_deccontext, &decimal_equals));
    ION_ASSERT_OK(ion_decimal_equals_quad(&quad_before, &quad_after, &((ION_READER *)reader)->_deccontext, &quad_equals));

    ION_ASSERT_OK(ion_reader_close(reader));

    ASSERT_TRUE(decimal_equals);
    ASSERT_TRUE(quad_equals);
    free(result);
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal_before));
    ION_ASSERT_OK(ion_decimal_free(&ion_decimal_after));
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

    ION_ASSERT_OK(ion_decimal_free(&result));
    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&fhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
}

TEST(IonDecimal, FMADecQuadInPlaceAllOperandsSame) {
    ION_DECIMAL lhs, expected;
    // The operands are all backed by decQuads.
    ION_ASSERT_OK(ion_decimal_from_int32(&lhs, 10));
    ION_ASSERT_OK(ion_decimal_fma(&lhs, &lhs, &lhs, &lhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&expected, 110));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &lhs));

    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&result));
    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&fhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&result));
    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&fhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&fhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&fhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&result));
    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&fhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&fhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
}

TEST(IonDecimal, AddDecQuad) {
    ION_DECIMAL result, lhs, rhs, expected;
    // The operands are all backed by decQuads.
    ION_ASSERT_OK(ion_decimal_from_int32(&lhs, 9));
    ION_ASSERT_OK(ion_decimal_from_int32(&rhs, 1));
    ION_ASSERT_OK(ion_decimal_add(&result, &lhs, &rhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&expected, 10));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    ION_ASSERT_OK(ion_decimal_free(&result));
    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
}

TEST(IonDecimal, AddDecNumber) {
    ION_DECIMAL result, lhs, rhs, expected;
    // Because these decimals have more than DECQUAD_Pmax digits, they will be backed by decNumbers.
    ION_ASSERT_OK(ion_decimal_from_string(&lhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&rhs, "100000000000000000000000000000000000001.", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_add(&result, &lhs, &rhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&expected, "200000000000000000000000000000000000002.", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &result));

    ION_ASSERT_OK(ion_decimal_free(&result));
    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&result));
    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&result));
    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
}

TEST(IonDecimal, AddDecQuadInPlaceAllOperandsSame) {
    ION_DECIMAL lhs, expected;
    ION_ASSERT_OK(ion_decimal_from_int32(&lhs, 1));
    ION_ASSERT_OK(ion_decimal_add(&lhs, &lhs, &lhs, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&expected, 2));
    ASSERT_TRUE(assertIonDecimalEq(&expected, &lhs));
    ASSERT_EQ(ION_DECIMAL_TYPE_QUAD, lhs.type);

    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&expected));
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

    ION_ASSERT_OK(ion_decimal_free(&lhs));
    ION_ASSERT_OK(ion_decimal_free(&rhs));
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

    ION_ASSERT_OK(ion_decimal_free(&ion_number_positive));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_negative));
    ION_ASSERT_OK(ion_decimal_free(&ion_quad_positive));
    ION_ASSERT_OK(ion_decimal_free(&ion_quad_negative));
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

    ION_ASSERT_OK(ion_decimal_free(&ion_quad_positive));
    ION_ASSERT_OK(ion_decimal_free(&ion_quad_negative));
    ION_ASSERT_OK(ion_decimal_free(&ion_quad_positive_result));
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

    ION_ASSERT_OK(ion_decimal_free(&ion_number_positive));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_negative));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_positive_result));
}

TEST(IonDecimal, CopySign) {
    ION_DECIMAL ion_number_positive, ion_quad_negative, ion_number_result;
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number_positive, "999999999999999999999999999999999999999999", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_int32(&ion_quad_negative, -1));
    ASSERT_FALSE(ion_decimal_is_negative(&ion_number_positive));
    ION_ASSERT_OK(ion_decimal_copy_sign(&ion_number_result, &ion_number_positive, &ion_quad_negative, &g_TestDecimalContext));
    ASSERT_TRUE(ion_decimal_is_negative(&ion_number_result));
    ION_ASSERT_OK(ion_decimal_minus(&ion_number_result, &ion_number_result, &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&ion_number_positive, &ion_number_result));

    ION_ASSERT_OK(ion_decimal_free(&ion_number_positive));
    ION_ASSERT_OK(ion_decimal_free(&ion_quad_negative));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_result));
}

TEST(IonDecimal, ToIntegralValue) {
    ION_DECIMAL ion_quad, ion_quad_expected, ion_number, ion_number_expected, ion_number_result;
    ION_ASSERT_OK(ion_decimal_from_string(&ion_quad, "9999.999e3", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number, "999999999999999999999999999999999999999999.999e3", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_to_integral_value(&ion_quad, &ion_quad, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_to_integral_value(&ion_number_result, &ion_number, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_quad_expected, "9999999", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number_expected, "999999999999999999999999999999999999999999999", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&ion_quad_expected, &ion_quad));
    ASSERT_TRUE(assertIonDecimalEq(&ion_number_expected, &ion_number_result));
    ASSERT_TRUE(assertIonDecimalEq(&ion_number_expected, &ion_number));

    ION_ASSERT_OK(ion_decimal_free(&ion_quad));
    ION_ASSERT_OK(ion_decimal_free(&ion_quad_expected));
    ION_ASSERT_OK(ion_decimal_free(&ion_number));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_expected));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_result));
}

TEST(IonDecimal, ToIntegralValueRounded) {
    ION_DECIMAL ion_quad, ion_quad_expected, ion_number, ion_number_expected, ion_number_result;
    ION_ASSERT_OK(ion_decimal_from_string(&ion_quad, "9998.999", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number, "999999999999999999999999999999999999999998.999", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_to_integral_value(&ion_quad, &ion_quad, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_to_integral_value(&ion_number_result, &ion_number, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_quad_expected, "9999", &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number_expected, "999999999999999999999999999999999999999999", &g_TestDecimalContext));
    ASSERT_TRUE(assertIonDecimalEq(&ion_quad_expected, &ion_quad));
    ASSERT_TRUE(assertIonDecimalEq(&ion_number_expected, &ion_number_result));
    ASSERT_FALSE(assertIonDecimalEq(&ion_number_expected, &ion_number));

    ION_ASSERT_OK(ion_decimal_free(&ion_quad));
    ION_ASSERT_OK(ion_decimal_free(&ion_quad_expected));
    ION_ASSERT_OK(ion_decimal_free(&ion_number));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_expected));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_result));
}

TEST(IonDecimal, ToAndFromString) {
    ION_DECIMAL ion_quad, ion_number_small, ion_number_large;
    ION_DECIMAL ion_quad_after, ion_number_small_after, ion_number_large_after;
    char *quad_str, *small_str, *large_str;
    decQuad quad;
    decNumber number_small;
    decQuadZero(&quad);
    ION_ASSERT_OK(ion_decimal_from_quad(&ion_quad, &quad));
    decNumberZero(&number_small);
    ION_ASSERT_OK(ion_decimal_from_number(&ion_number_small, &number_small));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number_large, "-999999999999999999999999999999999999999999.999d-3", &g_TestDecimalContext));

    ASSERT_EQ(DECQUAD_String, ION_DECIMAL_STRLEN(&ion_quad));
    ASSERT_EQ(1 + 14, ION_DECIMAL_STRLEN(&ion_number_small));
    ASSERT_EQ(45 + 14, ION_DECIMAL_STRLEN(&ion_number_large));

    quad_str = (char *)malloc(ION_DECIMAL_STRLEN(&ion_quad));
    small_str = (char *)malloc(ION_DECIMAL_STRLEN(&ion_number_small));
    large_str = (char *)malloc(ION_DECIMAL_STRLEN(&ion_number_large));

    ION_ASSERT_OK(ion_decimal_to_string(&ion_quad, quad_str));
    ION_ASSERT_OK(ion_decimal_to_string(&ion_number_small, small_str));
    ION_ASSERT_OK(ion_decimal_to_string(&ion_number_large, large_str));

    ION_ASSERT_OK(ion_decimal_from_string(&ion_quad_after, quad_str, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number_small_after, small_str, &g_TestDecimalContext));
    ION_ASSERT_OK(ion_decimal_from_string(&ion_number_large_after, large_str, &g_TestDecimalContext));

    ASSERT_TRUE(assertIonDecimalEq(&ion_quad, &ion_quad_after));
    ASSERT_TRUE(assertIonDecimalEq(&ion_number_small, &ion_number_small_after));
    ASSERT_TRUE(assertIonDecimalEq(&ion_number_large, &ion_number_large_after));

    free(quad_str);
    free(small_str);
    free(large_str);
    ION_ASSERT_OK(ion_decimal_free(&ion_quad));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_small));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_large));
    ION_ASSERT_OK(ion_decimal_free(&ion_quad_after));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_small_after));
    ION_ASSERT_OK(ion_decimal_free(&ion_number_large_after));
}
