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

#include <ion_extractor_impl.h>
#include "ion_assert.h"
#include "ion_helpers.h"
#include "ion_test_util.h"
#include "ion_extractor.h"

/**
 * Max number of paths and path lengths used in these tests. If more are needed, just increase these limits. Having
 * them be as small as possible makes debugging easier.
 */
#define ION_EXTRACTOR_TEST_MAX_PATHS 5
#define ION_EXTRACTOR_TEST_PATH_LENGTH 5

/**
 * Initializes an extractor test with the given options.
 */
#define ION_EXTRACTOR_TEST_INIT_OPTIONS(options) \
    hREADER reader; \
    hEXTRACTOR extractor; \
    hPATH path; \
    int num_paths = 0; \
    ASSERTION_CONTEXT assertion_contexts[ION_EXTRACTOR_TEST_MAX_PATHS]; \
    ASSERTION_CONTEXT *assertion_context; \
    ASSERT_MATCHES assertion; \
    ION_ASSERT_OK(ion_extractor_open(&extractor, &options));

/**
 * Initializes an extractor test with the default options.
 */
#define ION_EXTRACTOR_TEST_INIT \
    ION_EXTRACTOR_OPTIONS options; \
    options.max_path_length = ION_EXTRACTOR_TEST_PATH_LENGTH; \
    options.max_num_paths = ION_EXTRACTOR_TEST_MAX_PATHS; \
    options.match_relative_paths = false; \
    ION_EXTRACTOR_TEST_INIT_OPTIONS(options);

/**
 * Prepares the next assertion context.
 */
#define ION_EXTRACTOR_TEST_NEXT_CONTEXT \
    assertion_context = &assertion_contexts[num_paths++]; \
    assertion_context->assertion = assertion; \
    assertion_context->num_matches = 0;

/**
 * Starts a path and initializes its test assertion context, which will be passed through the extractor as user context.
 * `assert_matches` must first be set to the ASSERT_MATCHES that should be called when this path matches.
 */
#define ION_EXTRACTOR_TEST_PATH_START(path_length) \
    ION_EXTRACTOR_TEST_NEXT_CONTEXT; \
    ION_ASSERT_OK(ion_extractor_path_create(extractor, path_length, &testCallback, assertion_context, &path));

/**
 * Finishes the current path.
 */
#define ION_EXTRACTOR_TEST_PATH_END \
    assertion_context->path = path;

/**
 * Registers a path from the given Ion text.
 */
#define ION_EXTRACTOR_TEST_PATH_FROM_TEXT(text) \
    ION_EXTRACTOR_TEST_NEXT_CONTEXT; \
    ION_ASSERT_OK(ion_extractor_path_create_from_ion(extractor, &testCallback, assertion_context, (BYTE *)text, (SIZE)strlen(text), &path)); \
    ION_EXTRACTOR_TEST_PATH_END;

/**
 * Generic execution of an extractor for extractor tests. A const char * named `ion_text` must be declared first.
 */
#define ION_EXTRACTOR_TEST_MATCH \
    ION_ASSERT_OK(ion_test_new_text_reader(ion_text, &reader)); \
    ION_ASSERT_OK(ion_extractor_match(extractor, reader)); \
    ION_ASSERT_OK(ion_extractor_close(extractor)); \
    ION_ASSERT_OK(ion_reader_close(reader));

/**
 * Asserts that the path at the given index matched n times. Paths are indexed in the order they are added, starting
 * at 0.
 */
#define ION_EXTRACTOR_TEST_ASSERT_MATCHED(index, n) ASSERT_EQ((n), assertion_contexts[index].num_matches)

/**
 * Test-specific assertion function to be provided to the extractor within the user context. This can't be done directly
 * in the callback because gtest's assertions need to be invoked from a void function.
 */
typedef void (*ASSERT_MATCHES)(ION_READER *reader, ION_EXTRACTOR_PATH_DESCRIPTOR *, ION_EXTRACTOR_PATH_DESCRIPTOR *, ION_EXTRACTOR_CONTROL *);

/**
 * Test-specific state to be provided to the extractor as user context. In addition to verifying that user context is
 * passed through correctly, this gives the test the ability to perform assertions on the callback results.
 */
typedef struct _assertion_context {
    ASSERT_MATCHES assertion;
    hPATH path;
    int num_matches;
} ASSERTION_CONTEXT;

/**
 * Callback that can be used for path extractor tests. Treats user_context as an ASSERT_MATCHES function pointer,
 * and invokes that function.
 */
iERR testCallback(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, void *user_context, ION_EXTRACTOR_CONTROL *control) {
    iENTER;
    ASSERTION_CONTEXT *assertion_context = (ASSERTION_CONTEXT *)user_context;
    assertion_context->assertion(reader, matched_path, assertion_context->path, control);
    assertion_context->num_matches++;
    iRETURN;
}

/**
 * Callback that can be used in tests that don't need to make assertions in the callback.
 */
iERR testCallbackBasic(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, void *user_context, ION_EXTRACTOR_CONTROL *control) {
    iENTER;
    iRETURN;
}


/**
 * Callback that can be used in tests that expect a callback to never be invoked. Simply raises an error.
 */
iERR testCallbackNeverInvoked(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, void *user_context, ION_EXTRACTOR_CONTROL *control) {
    iENTER;
    FAILWITH(IERR_INVALID_STATE);
    iRETURN;
}

void assertMatchesByFieldAtDepth1(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    ION_STRING value;
    ION_TYPE type;

    ASSERT_TRUE(matched_path == original_path);
    ION_ASSERT_OK(ion_reader_get_type(reader, &type));
    ION_ASSERT_OK(ion_reader_read_string(reader, &value));
    ASSERT_EQ(tid_SYMBOL, type);
    assertStringsEqual("def", (char *)value.value, value.length);
}

TEST(IonExtractor, MatchesByFieldAtDepth1) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";
    ION_STRING value;
    ION_ASSERT_OK(ion_string_from_cstr("abc", &value));

    assertion = &assertMatchesByFieldAtDepth1;
    ION_EXTRACTOR_TEST_PATH_START(1);
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &value));
    ION_EXTRACTOR_TEST_PATH_END;

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
}

TEST(IonExtractor, MatchesByFieldAtDepth1FromIon) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertMatchesByFieldAtDepth1;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(abc)");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
}

void assertMatchesByOrdinalAtDepth1(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    // It happens to match the same field.
    assertMatchesByFieldAtDepth1(reader, matched_path, original_path, control);
}

TEST(IonExtractor, MatchesByOrdinalAtDepth1) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertMatchesByOrdinalAtDepth1;
    ION_EXTRACTOR_TEST_PATH_START(1);
    ION_ASSERT_OK(ion_extractor_path_append_ordinal(path, 0));
    ION_EXTRACTOR_TEST_PATH_END;

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
}

TEST(IonExtractor, MatchesByOrdinalAtDepth1FromIon) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertMatchesByOrdinalAtDepth1;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(0)");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
}

void assertMatchesByFieldAndOrdinalAtDepth3(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    int value;
    ION_TYPE type;

    ASSERT_TRUE(matched_path == original_path);
    ION_ASSERT_OK(ion_reader_get_type(reader, &type));
    ION_ASSERT_OK(ion_reader_read_int(reader, &value));
    ASSERT_EQ(tid_INT, type);
    ASSERT_EQ(3, value);
}

TEST(IonExtractor, MatchesByFieldAndOrdinalAtDepth3) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";
    ION_STRING foo_field, bar_field;
    ION_ASSERT_OK(ion_string_from_cstr("foo", &foo_field));
    ION_ASSERT_OK(ion_string_from_cstr("bar", &bar_field));

    assertion = &assertMatchesByFieldAndOrdinalAtDepth3;
    ION_EXTRACTOR_TEST_PATH_START(3);
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &foo_field));
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &bar_field));
    ION_ASSERT_OK(ion_extractor_path_append_ordinal(path, 2));
    ION_EXTRACTOR_TEST_PATH_END;

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
}

TEST(IonExtractor, MatchesByFieldAndOrdinalAtDepth3FromIon) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertMatchesByFieldAndOrdinalAtDepth3;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(foo bar 2)");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
}

TEST(IonExtractor, MatchesByFieldAndOrdinalAtDepth3FromIonAlternate) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertMatchesByFieldAndOrdinalAtDepth3;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("['foo', \"bar\", abc::2]");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
}

void assertMatchesByWildcard(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    int value;
    ION_TYPE type;

    ASSERT_TRUE(matched_path == original_path);
    ION_ASSERT_OK(ion_reader_get_type(reader, &type));
    ION_ASSERT_OK(ion_reader_read_int(reader, &value));
    ASSERT_EQ(tid_INT, type);
    ASSERT_TRUE(value == 1 || value == 2 || value == 3);
}

TEST(IonExtractor, MatchesByWildcard) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";
    ION_STRING foo_field, bar_field;
    ION_ASSERT_OK(ion_string_from_cstr("foo", &foo_field));
    ION_ASSERT_OK(ion_string_from_cstr("bar", &bar_field));

    assertion = &assertMatchesByWildcard;
    ION_EXTRACTOR_TEST_PATH_START(3);
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &foo_field));
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &bar_field));
    ION_ASSERT_OK(ion_extractor_path_append_wildcard(path));
    ION_EXTRACTOR_TEST_PATH_END;

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 3);
}

TEST(IonExtractor, MatchesByWildcardFromIon) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertMatchesByWildcard;
    // TODO leaving the * unquoted here should be legal, but it fails to parse. This needs to be fixed in the core
    // library.
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(foo bar $ion_wildcard::'*')");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 3);
}

TEST(IonExtractor, MatchesByWildcardWithFieldNameStar) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {'*':[1, 2, 3]}}";

    assertion = &assertMatchesByWildcard;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(foo '*' $ion_wildcard::'*')");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 3);
}

void assertMatchesByNonTerminalWildcard(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    int value;
    ION_TYPE type;

    ASSERT_TRUE(matched_path == original_path);
    ION_ASSERT_OK(ion_reader_get_type(reader, &type));
    ION_ASSERT_OK(ion_reader_read_int(reader, &value));
    ASSERT_EQ(tid_INT, type);
    ASSERT_TRUE(value == 1 || value == 3);
}

TEST(IonExtractor, MatchesByNonTerminalWildcard) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[{baz:1}, {zar:2}, {baz:3}]}}";

    assertion = &assertMatchesByNonTerminalWildcard;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(foo bar $ion_wildcard::'*' baz)");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 2);
}

TEST(IonExtractor, MatchesMultiplePaths) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";
    ION_STRING abc_field, foo_field, bar_field;
    ION_ASSERT_OK(ion_string_from_cstr("abc", &abc_field));
    ION_ASSERT_OK(ion_string_from_cstr("foo", &foo_field));
    ION_ASSERT_OK(ion_string_from_cstr("bar", &bar_field));

    assertion = &assertMatchesByFieldAndOrdinalAtDepth3;
    ION_EXTRACTOR_TEST_PATH_START(3);
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &foo_field));
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &bar_field));
    ION_ASSERT_OK(ion_extractor_path_append_ordinal(path, 2));
    ION_EXTRACTOR_TEST_PATH_END;

    assertion = &assertMatchesByFieldAtDepth1;
    ION_EXTRACTOR_TEST_PATH_START(1);
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &abc_field));
    ION_EXTRACTOR_TEST_PATH_END;

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(1, 1);
}

TEST(IonExtractor, MatchesMultiplePathsFromIon) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertMatchesByFieldAndOrdinalAtDepth3;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(foo bar 2)");

    assertion = &assertMatchesByFieldAtDepth1;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(abc)");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(1, 1);
}

TEST(IonExtractor, MatchesMultiplePathsCreatedUpFront) {
    ION_EXTRACTOR_TEST_INIT;
    hPATH path2, path3;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}, baz:def}";
    ION_STRING abc_field, foo_field, bar_field;

    ION_ASSERT_OK(ion_string_from_cstr("abc", &abc_field));
    ION_ASSERT_OK(ion_string_from_cstr("foo", &foo_field));
    ION_ASSERT_OK(ion_string_from_cstr("bar", &bar_field));

    // Create paths up front.
    assertion = &assertMatchesByFieldAndOrdinalAtDepth3;
    ION_EXTRACTOR_TEST_NEXT_CONTEXT;
    ION_ASSERT_OK(ion_extractor_path_create(extractor, 3, &testCallback, assertion_context, &path));
    assertion = &assertMatchesByFieldAtDepth1;
    ION_EXTRACTOR_TEST_NEXT_CONTEXT;
    ION_ASSERT_OK(ion_extractor_path_create(extractor, 1, &testCallback, assertion_context, &path2));

    // Interleave path component appending between the paths.
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &foo_field));
    ION_ASSERT_OK(ion_extractor_path_append_field(path2, &abc_field));
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &bar_field));
    assertion = &assertMatchesByFieldAtDepth1;
    ION_EXTRACTOR_TEST_NEXT_CONTEXT;
    ION_ASSERT_OK(ion_extractor_path_create_from_ion(extractor, &testCallback, assertion_context, (BYTE *)"(baz)", 5, &path3));
    ION_ASSERT_OK(ion_extractor_path_append_ordinal(path, 2));

    assertion_contexts[path->path_id].path = path;
    assertion_contexts[path2->path_id].path = path2;
    assertion_contexts[path3->path_id].path = path3;

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(path->path_id, 1);
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(path2->path_id, 1);
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(path3->path_id, 1);

}

void assertMatchesSamePathMultipleTimes(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    ION_STRING str_value;
    int int_value;
    ION_TYPE type;

    ASSERT_TRUE(matched_path == original_path);
    ION_ASSERT_OK(ion_reader_get_type(reader, &type));
    ASSERT_TRUE(tid_INT == type || tid_SYMBOL == type);
    if (tid_INT == type) {
        ION_ASSERT_OK(ion_reader_read_int(reader, &int_value));
        ASSERT_EQ(123, int_value);
    }
    else {
        ION_ASSERT_OK(ion_reader_read_string(reader, &str_value));
        assertStringsEqual("def", (char *)str_value.value, str_value.length);
    }
}

TEST(IonExtractor, MatchesSamePathMultipleTimes) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc:def}{abc:123}";
    ION_STRING abc_field;
    ION_ASSERT_OK(ion_string_from_cstr("abc", &abc_field));

    assertion = &assertMatchesSamePathMultipleTimes;
    ION_EXTRACTOR_TEST_PATH_START(1);
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &abc_field));
    ION_EXTRACTOR_TEST_PATH_END;

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 2);
}

TEST(IonExtractor, MatchesSamePathMultipleTimesFromIon) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc:def}{abc:123}";

    assertion = &assertMatchesSamePathMultipleTimes;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(abc)")

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 2);
}

void assertDoesNotMatchPath(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    ASSERT_FALSE(TRUE) << "Path with ID " << matched_path->path_id << " matched when it should not have.";
}

TEST(IonExtractor, DoesNotMatchPath) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";
    ION_STRING foo_field, bar_field;
    ION_ASSERT_OK(ion_string_from_cstr("foo", &foo_field));
    ION_ASSERT_OK(ion_string_from_cstr("bar", &bar_field));

    assertion = &assertDoesNotMatchPath;
    ION_EXTRACTOR_TEST_PATH_START(3);
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &foo_field));
    ION_ASSERT_OK(ion_extractor_path_append_field(path, &bar_field));
    ION_ASSERT_OK(ion_extractor_path_append_ordinal(path, 3)); // This ordinal is out of range of the data.
    ION_EXTRACTOR_TEST_PATH_END;

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 0); // Matched zero times.
}

TEST(IonExtractor, DoesNotMatchPathFromIon) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertDoesNotMatchPath;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(foo bar 3)");

    ION_EXTRACTOR_TEST_MATCH;
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 0); // Matched zero times.
}

TEST(IonExtractorFailsOn, TooLargeMaxPathLength) {
    hEXTRACTOR extractor;
    ION_EXTRACTOR_OPTIONS options;
    options.max_path_length = ION_EXTRACTOR_MAX_PATH_LENGTH + 1;
    options.max_num_paths = ION_EXTRACTOR_MAX_NUM_PATHS;
    ION_ASSERT_FAIL(ion_extractor_open(&extractor, &options));
}

TEST(IonExtractorFailsOn, TooLargeMaxNumPaths) {
    hEXTRACTOR extractor;
    ION_EXTRACTOR_OPTIONS options;
    options.max_path_length = ION_EXTRACTOR_MAX_PATH_LENGTH;
    options.max_num_paths = ION_EXTRACTOR_MAX_NUM_PATHS + 1;
    ION_ASSERT_FAIL(ion_extractor_open(&extractor, &options));
}

TEST(IonExtractorFailsOn, TooSmallMaxPathLength) {
    hEXTRACTOR extractor;
    ION_EXTRACTOR_OPTIONS options;
    options.max_path_length = 0;
    options.max_num_paths = ION_EXTRACTOR_MAX_NUM_PATHS;
    ION_ASSERT_FAIL(ion_extractor_open(&extractor, &options));
}

TEST(IonExtractorFailsOn, TooSmallMaxNumPaths) {
    hEXTRACTOR extractor;
    ION_EXTRACTOR_OPTIONS options;
    options.max_path_length = ION_EXTRACTOR_MAX_PATH_LENGTH;
    options.max_num_paths = 0;
    ION_ASSERT_FAIL(ion_extractor_open(&extractor, &options));
}

TEST(IonExtractorFailsOn, PathSizeZero) {
    hEXTRACTOR extractor;
    hPATH path;
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    ION_ASSERT_FAIL(ion_extractor_path_create(extractor, 0, &testCallbackNeverInvoked, NULL, &path));
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, PathExceedsDeclaredLength) {
    hEXTRACTOR extractor;
    hPATH path;
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    ION_ASSERT_OK(ion_extractor_path_create(extractor, 1, &testCallbackBasic, NULL, &path));
    ION_ASSERT_OK(ion_extractor_path_append_ordinal(path, 1));
    ION_ASSERT_FAIL(ion_extractor_path_append_ordinal(path, 0));
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, PathExceedsMaxLength) {
    hEXTRACTOR extractor;
    hPATH path;
    ION_EXTRACTOR_OPTIONS options;
    options.max_path_length = 1;
    options.max_num_paths = ION_EXTRACTOR_MAX_NUM_PATHS;
    ION_ASSERT_OK(ion_extractor_open(&extractor, &options));
    ION_ASSERT_FAIL(ion_extractor_path_create(extractor, 2, &testCallbackBasic, NULL, &path));
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, IncompletePath) {
    hEXTRACTOR extractor;
    hPATH path;
    hREADER reader;
    ION_ASSERT_OK(ion_test_new_text_reader("[1, [1, 2], 3]", &reader));
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    ION_ASSERT_OK(ion_extractor_path_create(extractor, 3, &testCallbackBasic, NULL, &path));
    ION_ASSERT_OK(ion_extractor_path_append_ordinal(path, 1));
    ION_ASSERT_OK(ion_extractor_path_append_ordinal(path, 0));
    ION_ASSERT_FAIL(ion_extractor_match(extractor, reader));
    ION_ASSERT_OK(ion_extractor_close(extractor));
    ION_ASSERT_OK(ion_reader_close(reader));
}

TEST(IonExtractorFailsOn, TooManyRegisteredPaths) {
    hEXTRACTOR extractor;
    hPATH path;
    ION_EXTRACTOR_OPTIONS options;
    options.max_path_length = ION_EXTRACTOR_MAX_PATH_LENGTH;
    options.max_num_paths = 1;
    ION_ASSERT_OK(ion_extractor_open(&extractor, &options));
    ION_ASSERT_OK(ion_extractor_path_create(extractor, 1, &testCallbackBasic, NULL, &path));
    ION_ASSERT_FAIL(ion_extractor_path_create(extractor, 1, &testCallbackNeverInvoked, NULL, &path));
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, PathAppendWithoutCreate) {
    hEXTRACTOR extractor;
    ION_EXTRACTOR_PATH_DESCRIPTOR *path = (ION_EXTRACTOR_PATH_DESCRIPTOR *)malloc(sizeof(ION_EXTRACTOR_PATH_DESCRIPTOR));
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    path->path_id = 0;
    path->_current_length = 0;
    path->path_length = 1;
    path->extractor = extractor;
    ION_ASSERT_FAIL(ion_extractor_path_append_ordinal(path, 2));
    free(path);
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, CreatePathFromIonWithMoreThanOneTopLevelValue) {
    hEXTRACTOR extractor;
    hPATH path;
    const char *data = "(foo) 123";
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    ION_ASSERT_FAIL(ion_extractor_path_create_from_ion(extractor, &testCallbackNeverInvoked, NULL, (BYTE *)data, strlen(data), &path));
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, CreatePathFromIonWithZeroPathComponents) {
    hEXTRACTOR extractor;
    hPATH path;
    const char *data = "()";
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    ION_ASSERT_FAIL(ion_extractor_path_create_from_ion(extractor, &testCallbackNeverInvoked, NULL, (BYTE *)data, strlen(data), &path));
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, CreatePathFromIonNotSequence) {
    hEXTRACTOR extractor;
    hPATH path;
    const char *data = "abc";
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    ION_ASSERT_FAIL(ion_extractor_path_create_from_ion(extractor, &testCallbackNeverInvoked, NULL, (BYTE *)data, strlen(data), &path));
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, CreatePathFromIonPathExceedsMaxLength) {
    hEXTRACTOR extractor;
    hPATH path;
    const char *data = "(foo bar)";
    ION_EXTRACTOR_OPTIONS options;
    options.max_path_length = 1;
    options.max_num_paths = ION_EXTRACTOR_MAX_NUM_PATHS;
    ION_ASSERT_OK(ion_extractor_open(&extractor, &options));
    ION_ASSERT_FAIL(ion_extractor_path_create_from_ion(extractor, &testCallbackNeverInvoked, NULL, (BYTE *)data, strlen(data), &path));
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, AppendAfterCreatePathFromIon) {
    hEXTRACTOR extractor;
    hPATH path;
    const char *data = "(foo bar)";
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    ION_ASSERT_OK(ion_extractor_path_create_from_ion(extractor, &testCallbackNeverInvoked, NULL, (BYTE *)data, strlen(data), &path));
    ION_ASSERT_FAIL(ion_extractor_path_append_ordinal(path, 2)); // This will exceed the path's declared length.
    ION_ASSERT_OK(ion_extractor_close(extractor));
}

TEST(IonExtractorFailsOn, ReaderStartingAtDepthOtherThanZero) {
    hEXTRACTOR extractor;
    hREADER reader;
    hPATH path;
    ION_TYPE type;
    const char *ion_text = "{foo:{bar:{baz:123}}}";
    ION_ASSERT_OK(ion_test_new_text_reader(ion_text, &reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_STRUCT, type);
    ION_ASSERT_OK(ion_reader_step_in(reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_STRUCT, type);
    ION_ASSERT_OK(ion_reader_step_in(reader));
    ION_ASSERT_OK(ion_extractor_open(&extractor, NULL));
    ION_ASSERT_OK(ion_extractor_path_create_from_ion(extractor, &testCallbackNeverInvoked, NULL, (BYTE *)"(baz)", 5, &path));
    ION_ASSERT_FAIL(ion_extractor_match(extractor, reader));
    ION_ASSERT_OK(ion_extractor_close(extractor));
    ION_ASSERT_OK(ion_reader_close(reader));
}

void assertMatchesRelativePath(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path,
                               ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    ION_TYPE type;
    int value;
    ASSERT_TRUE(original_path == matched_path);
    ION_ASSERT_OK(ion_reader_get_type(reader, &type));
    ASSERT_EQ(tid_INT, type);
    ION_ASSERT_OK(ion_reader_read_int(reader, &value));
    ASSERT_TRUE((value == 123) || (value == 456));
}

TEST(IonExtractor, MatchesRelativePath) {
    ION_EXTRACTOR_OPTIONS options;
    options.max_path_length = ION_EXTRACTOR_TEST_PATH_LENGTH;
    options.max_num_paths = ION_EXTRACTOR_TEST_MAX_PATHS;
    options.match_relative_paths = true;
    ION_EXTRACTOR_TEST_INIT_OPTIONS(options);
    ION_TYPE type;
    // Step in the reader to point to the first 'bar' at depth 2. The extractor will process all siblings of depth 2,
    // but will not step out past depth 2.
    const char *ion_text = "{foo:{bar:{baz:123}, bar:{baz:456}}, foo{bar:{baz:789}}}";
    ION_ASSERT_OK(ion_test_new_text_reader(ion_text, &reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_STRUCT, type);
    ION_ASSERT_OK(ion_reader_step_in(reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_STRUCT, type);
    ION_ASSERT_OK(ion_reader_step_in(reader));
    ION_ASSERT_OK(ion_extractor_open(&extractor, &options));

    assertion = &assertMatchesRelativePath;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(baz)"); // Matches 123 and 456
    assertion = &assertDoesNotMatchPath;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(foo bar baz)"); // Never matches because the extractor is scoped at depth 2.
    ION_ASSERT_OK(ion_extractor_match(extractor, reader));
    ION_ASSERT_OK(ion_extractor_close(extractor));
    ION_ASSERT_OK(ion_reader_close(reader));
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 2);
    ION_EXTRACTOR_TEST_ASSERT_MATCHED(1, 0);
}

void assertStructAndStepInButNotOut(hREADER reader, ION_EXTRACTOR_PATH_DESCRIPTOR *matched_path, ION_EXTRACTOR_PATH_DESCRIPTOR *original_path, ION_EXTRACTOR_CONTROL *control) {
    ION_TYPE type;
    ION_STRING field_name;
    ASSERT_TRUE(matched_path == original_path);
    ION_ASSERT_OK(ion_reader_get_type(reader, &type));
    ASSERT_EQ(tid_STRUCT, type);
    ION_ASSERT_OK(ion_reader_step_in(reader));
    ION_ASSERT_OK(ion_reader_next(reader, &type));
    ASSERT_EQ(tid_LIST, type);
    ION_ASSERT_OK(ion_reader_get_field_name(reader, &field_name));
    assertStringsEqual("bar", (char *)field_name.value, field_name.length);
    // No matching call to ion_reader_step_out. This will cause the extractor to raise an error.
}

TEST(IonExtractorFailsOn, ReturningFromCallbackAtDifferentDepth) {
    ION_EXTRACTOR_TEST_INIT;
    const char *ion_text = "{abc: def, foo: {bar:[1, 2, 3]}}";

    assertion = &assertStructAndStepInButNotOut;
    ION_EXTRACTOR_TEST_PATH_FROM_TEXT("(foo)");
    ION_ASSERT_OK(ion_test_new_text_reader(ion_text, &reader));
    ION_ASSERT_FAIL(ion_extractor_match(extractor, reader));
    ION_ASSERT_OK(ion_extractor_close(extractor));
    ION_ASSERT_OK(ion_reader_close(reader));

    ION_EXTRACTOR_TEST_ASSERT_MATCHED(0, 1);
}
