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

#include "gather_vectors.h"
#include "value_stream.h"
#include "ion_assert.h"

// NOTE: custom parameterized test names are not supported well by some IDEs (e.g. CLion). They will still run,
// but they don't integrate well with the GUI. Hence, it is best to disable then when debugging within an IDE.
// When better support comes to IDEs, these conditionals should be removed.
#ifndef ION_TEST_VECTOR_VERBOSE_NAMES
#define ION_TEST_VECTOR_VERBOSE_NAMES 1
#endif

#define ION_TEST_VECTOR_CLEANUP() { \
    if (initial_stream) { \
        delete initial_stream; \
        initial_stream = NULL; \
    } \
    if (roundtrip_stream) { \
        delete roundtrip_stream; \
        roundtrip_stream = NULL; \
    } \
    if (catalog) { \
        ion_catalog_close(catalog); \
        catalog = NULL; \
    } \
}

#define ION_TEST_VECTOR_INIT() { \
    g_TimestampEquals = ion_timestamp_equals; \
    g_CurrentTest = test_name; \
    initial_stream = new IonEventStream(); \
    roundtrip_stream = NULL; \
    catalog = NULL; \
    EXPECT_EQ(IERR_OK, ion_catalog_open(&catalog)); \
}

TEST(TestVectors, HasRequiredDependencies) {
    // If this flag is false, these tests can't run.
    ASSERT_TRUE(GTEST_HAS_PARAM_TEST);
    ASSERT_TRUE(GTEST_HAS_TR1_TUPLE);
}

std::string simplifyFilename(std::string filename) {
    // Because google test requires parameterized test names to be ASCII alphanumeric + underscore...
    std::string copy(filename);
    std::replace(copy.begin(), copy.end(), ION_TEST_PATH_SEPARATOR_CHAR, '_');
    std::replace(copy.begin(), copy.end(), '.', '_');
    std::replace(copy.begin(), copy.end(), '-', '_');
    return copy;
}

std::string getTestName(std::string filename, VECTOR_TEST_TYPE *test_type, READER_INPUT_TYPE input_type) {
    std::string test_name = simplifyFilename(filename) + "_";
    if (test_type) {
        switch (*test_type) {
            case READ:
                test_name += "READ";
                break;
            case ROUNDTRIP_TEXT:
                test_name += "ROUNDTRIP_TEXT";
                break;
            default:
                test_name += "ROUNDTRIP_BINARY";
                break;
        }
        test_name += "_";
    }
    switch (input_type) {
        case STREAM:
            test_name += "STREAM";
            break;
        case BUFFER:
            test_name += "BUFFER";
            break;
    }
    return test_name;
}

class FilenameParameter
        : public ::testing::TestWithParam< ::testing::tuple<std::string, VECTOR_TEST_TYPE, READER_INPUT_TYPE> > {
public:
    virtual ~FilenameParameter() {
        ION_TEST_VECTOR_CLEANUP();
    }

    virtual void SetUp() {
        filename = ::testing::get<0>(GetParam());
        test_type = ::testing::get<1>(GetParam());
        input_type = ::testing::get<2>(GetParam());
        test_name = getTestName(filename, &test_type, input_type);
        ION_TEST_VECTOR_INIT();
    }

    virtual void TearDown() {
        ION_TEST_VECTOR_CLEANUP();
    }

    std::string test_name;
protected:
    std::string filename;
    VECTOR_TEST_TYPE test_type;
    READER_INPUT_TYPE input_type;
    IonEventStream *initial_stream;
    IonEventStream *roundtrip_stream;
    ION_CATALOG *catalog;
};

struct FilenameParameterToString {
    template <class ParamType>
    std::string operator()(const ::testing::TestParamInfo<ParamType>& info) const {
        std::string filename = ::testing::get<0>(info.param);
        VECTOR_TEST_TYPE test_type = ::testing::get<1>(info.param);
        READER_INPUT_TYPE input_type = ::testing::get<2>(info.param);
        return getTestName(filename, &test_type, input_type);
    }
};

class GoodBasicFilenameParameter : public FilenameParameter {};
class GoodEquivsFilenameParameter : public FilenameParameter {};
class GoodTimestampEquivTimelineParameter : public FilenameParameter {};
class GoodNonequivsFilenameParameter : public FilenameParameter {};

class BadFilenameParameter
        : public ::testing::TestWithParam< ::testing::tuple<std::string, READER_INPUT_TYPE> > {
public:
    virtual ~BadFilenameParameter() {
        ION_TEST_VECTOR_CLEANUP();
    }

    virtual void SetUp() {
        filename = ::testing::get<0>(GetParam());
        input_type = ::testing::get<1>(GetParam());
        test_name = getTestName(filename, NULL, input_type);
        ION_TEST_VECTOR_INIT();
    }

    virtual void TearDown() {
        ION_TEST_VECTOR_CLEANUP();
    }

    std::string test_name;
protected:
    std::string filename;
    READER_INPUT_TYPE input_type;
    IonEventStream *initial_stream;
    IonEventStream *roundtrip_stream;
    ION_CATALOG *catalog;
};

struct BadFilenameParameterToString {
    template <class ParamType>
    std::string operator()(const ::testing::TestParamInfo<ParamType>& info) const {
        std::string filename = ::testing::get<0>(info.param);
        READER_INPUT_TYPE input_type = ::testing::get<1>(info.param);
        return getTestName(filename, NULL, input_type);
    }
};

std::vector<std::string> gather(TEST_FILE_TYPE filetype, TEST_FILE_CLASSIFICATION classification) {
    std::vector<std::string> files;
    gather_files(filetype, classification, &files);
    return files;
}

BOOL assertIonScalarEq(IonEventBase *expected, IonEventBase *actual, ASSERTION_TYPE assertion_type) {
    ION_ENTER_ASSERTIONS;
    void *expected_value = expected->value;
    void *actual_value = actual->value;
    ION_EXPECT_FALSE((expected_value == NULL) ^ (actual_value == NULL));
    if (expected_value == NULL) {
        // Equivalence of ion types has already been tested.
        return TRUE;
    }
    switch (ION_TYPE_INT(expected->ion_type)) {
        case tid_BOOL_INT:
            ION_EXPECT_EQ(*(BOOL *) expected_value, *(BOOL *) actual_value);
            break;
        case tid_INT_INT:
            ION_EXPECT_INT_EQ((ION_INT *) expected_value, (ION_INT *) actual_value);
            break;
        case tid_FLOAT_INT:
            ION_EXPECT_DOUBLE_EQ(*(double *) expected_value, *(double *) actual_value);
            break;
        case tid_DECIMAL_INT:
            ION_EXPECT_DECIMAL_EQ((decQuad *) expected_value, (decQuad *) actual_value);
            break;
        case tid_TIMESTAMP_INT:
            ION_EXPECT_TIMESTAMP_EQ((ION_TIMESTAMP *) expected_value, (ION_TIMESTAMP *) actual_value);
            break;
        case tid_SYMBOL_INT:
        case tid_STRING_INT:
        case tid_CLOB_INT:
        case tid_BLOB_INT: // Clobs and blobs are stored in ION_STRINGs too...
            ION_EXPECT_STRING_EQ((ION_STRING *) expected_value, (ION_STRING *) actual_value);
            break;
        default:
            EXPECT_FALSE("Illegal state: unknown ion type.");
    }
    ION_EXIT_ASSERTIONS;
}

BOOL assertIonEventsEq(IonEventStream *stream_expected, size_t index_expected, IonEventStream *stream_actual,
                       size_t index_actual, ASSERTION_TYPE assertion_type);

/* Returns the length of the current value, in number of events. Scalars will always return 1. */
size_t valueEventLength(IonEventStream *stream, size_t start_index) {
    IonEventBase *start = stream->at(start_index);
    if (CONTAINER_START == start->event_type) {
        size_t length;
        size_t i = start_index;
        while (TRUE) {
            IonEventBase *curr = stream->at(++i);
            if (curr->event_type == CONTAINER_END && curr->depth == start->depth) {
                length = ++i - start_index;
                break;
            }
        }
        return length;
    }
    return 1;
}

/* Assert that the struct starting at index_expected is a subset of the struct starting at index_actual. */
BOOL assertIonStructIsSubset(IonEventStream *stream_expected, size_t index_expected, IonEventStream *stream_actual,
                             size_t index_actual, ASSERTION_TYPE assertion_type) {
    ION_ENTER_ASSERTIONS;
    int target_depth = stream_expected->at(index_expected)->depth;
    index_expected++; // Move past the CONTAINER_START events
    index_actual++;
    size_t index_actual_start = index_actual;
    std::set<size_t> skips;
    while (TRUE) {
        index_actual = index_actual_start;
        IonEventBase *expected = stream_expected->at(index_expected);
        if (expected->event_type == CONTAINER_END && expected->depth == target_depth) {
            break;
        }
        ION_STRING *expected_field_name = expected->field_name;
        EXPECT_TRUE(expected_field_name != NULL);
        while (TRUE) {
            if (skips.count(index_actual) == 0) {
                IonEventBase *actual = stream_actual->at(index_actual);
                ION_ASSERT(!(actual->event_type == CONTAINER_END && actual->depth == target_depth),
                           "Reached end of struct before finding matching field.");
                if (ionStringEq(expected_field_name, actual->field_name)
                    && assertIonEventsEq(stream_expected, index_expected, stream_actual, index_actual,
                                         ASSERTION_TYPE_SET_FLAG)) {
                    // Skip indices that have already matched. Ensures that structs with different numbers of the same
                    // key:value mapping are not equal.
                    skips.insert(index_actual);
                    break;
                }
            }
            index_actual += valueEventLength(stream_actual, index_actual);
        }
        index_expected += valueEventLength(stream_expected, index_expected);
    }
    ION_EXIT_ASSERTIONS;
}

BOOL assertIonStructEq(IonEventStream *stream_expected, size_t index_expected, IonEventStream *stream_actual,
                       size_t index_actual, ASSERTION_TYPE assertion_type) {
    ION_ENTER_ASSERTIONS;
    // By asserting that 'expected' and 'actual' are bidirectional subsets, we are asserting they are equivalent.
    ION_ACCUMULATE_ASSERTION(
            assertIonStructIsSubset(stream_expected, index_expected, stream_actual, index_actual, assertion_type));
    ION_ACCUMULATE_ASSERTION(
            assertIonStructIsSubset(stream_actual, index_actual, stream_expected, index_expected, assertion_type));
    ION_EXIT_ASSERTIONS;
}

BOOL assertIonSequenceEq(IonEventStream *stream_expected, size_t index_expected, IonEventStream *stream_actual,
                         size_t index_actual, ASSERTION_TYPE assertion_type) {
    ION_ENTER_ASSERTIONS;
    int target_depth = stream_expected->at(index_expected)->depth;
    index_expected++; // Move past the CONTAINER_START events
    index_actual++;
    while (TRUE) {
        ION_ACCUMULATE_ASSERTION(
                assertIonEventsEq(stream_expected, index_expected, stream_actual, index_actual, assertion_type));
        if (stream_expected->at(index_expected)->event_type == CONTAINER_END &&
            stream_expected->at(index_expected)->depth == target_depth) {
            ION_EXPECT_TRUE(stream_actual->at(index_actual)->event_type == CONTAINER_END &&
                            stream_actual->at(index_actual)->depth == target_depth);
            break;
        }
        index_expected += valueEventLength(stream_expected, index_expected);
        index_actual += valueEventLength(stream_actual, index_actual);
    }
    ION_EXIT_ASSERTIONS;
}

BOOL assertIonEventsEq(IonEventStream *stream_expected, size_t index_expected, IonEventStream *stream_actual,
                       size_t index_actual, ASSERTION_TYPE assertion_type) {
    ION_ENTER_ASSERTIONS;
    IonEventBase *expected = stream_expected->at(index_expected);
    IonEventBase *actual = stream_actual->at(index_actual);
    ION_EXPECT_EQ(expected->event_type, actual->event_type);
    ION_EXPECT_EQ(expected->ion_type, actual->ion_type);
    ION_EXPECT_EQ(expected->depth, actual->depth);
    ION_EXPECT_STRING_EQ(expected->field_name, actual->field_name);
    ION_EXPECT_EQ(expected->num_annotations, actual->num_annotations);
    for (size_t i = 0; i < expected->num_annotations; i++) {
        ION_EXPECT_STRING_EQ(expected->annotations[i], actual->annotations[i]);
    }
    switch (expected->event_type) {
        case STREAM_END:
        case CONTAINER_END:
            break;
        case CONTAINER_START:
            switch (ION_TYPE_INT(expected->ion_type)) {
                case tid_STRUCT_INT:
                    ION_ACCUMULATE_ASSERTION(
                            assertIonStructEq(stream_expected, index_expected, stream_actual, index_actual,
                                              assertion_type));
                    break;
                case tid_SEXP_INT: // intentional fall-through
                case tid_LIST_INT:
                    ION_ACCUMULATE_ASSERTION(
                            assertIonSequenceEq(stream_expected, index_expected, stream_actual, index_actual,
                                                assertion_type));
                    break;
                default:
                    EXPECT_FALSE("Illegal state: container start event with non-container type.");
            }
            break;
        case SCALAR:
            ION_ACCUMULATE_ASSERTION(assertIonScalarEq(expected, actual, assertion_type));
            break;
        default:
            EXPECT_FALSE("Illegal state: unknown event type.");
    }
    ION_EXIT_ASSERTIONS;
}

BOOL assertIonEventStreamEq(IonEventStream *expected, IonEventStream *actual, ASSERTION_TYPE assertion_type) {
    ION_ENTER_ASSERTIONS;
    size_t index_expected = 0;
    size_t index_actual = 0;
    while (index_expected < expected->size() && index_actual < actual->size()) {
        ION_ACCUMULATE_ASSERTION(assertIonEventsEq(expected, index_expected, actual, index_actual, assertion_type));
        index_expected += valueEventLength(expected, index_expected);
        index_actual += valueEventLength(actual, index_actual);
    }
    ION_ASSERT(expected->size() == index_expected, "Expected stream did not reach its end.");
    ION_ASSERT(actual->size() == index_actual, "Actual stream did not reach its end.");
    ION_EXIT_ASSERTIONS;
}

iERR ionTestRoundtrip(IonEventStream *initial_stream, IonEventStream **roundtrip_stream, ION_CATALOG *catalog,
                      std::string test_name, std::string filename, READER_INPUT_TYPE input_type,
                      VECTOR_TEST_TYPE test_type) {
    iERR status = IERR_OK;
    if (test_type > READ) {
        BYTE *written = NULL;
        status = write_value_stream(initial_stream, test_type, catalog, &written);
        EXPECT_EQ(IERR_OK, status) << test_name << " FAILED ON WRITE" << std::endl;
        if (IERR_OK != status) goto finish;
        *roundtrip_stream = new IonEventStream();
        status = read_value_stream(*roundtrip_stream, input_type, filename, read_all, test_name, catalog);
        EXPECT_EQ(IERR_OK, status) << test_name << " FAILED ON ROUNDTRIP READ" << std::endl;
        if (IERR_OK != status) goto finish;
        status = assertIonEventStreamEq(initial_stream, *roundtrip_stream, ASSERTION_TYPE_NORMAL) ? IERR_OK
                                                                                                  : IERR_INVALID_STATE;
        finish:
        if (written) {
            free(written);
        }
    }
    return status;
}

TEST_P(GoodBasicFilenameParameter, good_basic) {
    iERR status = read_value_stream(initial_stream, input_type, filename, read_all, test_name, catalog);
    EXPECT_EQ(IERR_OK, status) << test_name << " Error: " << ion_error_to_str(status) << std::endl;
    if (IERR_OK == status) {
        ionTestRoundtrip(initial_stream, &roundtrip_stream, catalog, test_name, filename, input_type, test_type);
    }
}

INSTANTIATE_TEST_CASE_P(
        TestVectors,
        GoodBasicFilenameParameter,
        ::testing::Combine(
                ::testing::ValuesIn(gather(FILETYPE_ALL, CLASSIFICATION_GOOD_BASIC)),
                ::testing::Values(READ, ROUNDTRIP_TEXT, ROUNDTRIP_BINARY),
                ::testing::Values(STREAM, BUFFER)
        )
#if ION_TEST_VECTOR_VERBOSE_NAMES
        , FilenameParameterToString()
#endif
);


typedef void (*COMPARISON_FN)(IonEventStream *stream, size_t index_expected, size_t index_actual);

void comparisonEquivs(IonEventStream *stream, size_t index_expected, size_t index_actual) {
    EXPECT_TRUE(assertIonEventsEq(stream, index_expected, stream, index_actual, ASSERTION_TYPE_NORMAL))
                        << std::string("Test: ") << g_CurrentTest
                        << " comparing events at index " << index_expected << " and " << index_actual;
}

void comparisonNonequivs(IonEventStream *stream, size_t index_expected, size_t index_actual) {
    EXPECT_FALSE(assertIonEventsEq(stream, index_expected, stream, index_actual, ASSERTION_TYPE_SET_FLAG))
                        << std::string("Test: ") << g_CurrentTest
                        << " comparing events at index " << index_expected << " and " << index_actual;
}

void testEquivsSet(IonEventStream *stream, size_t index, int target_depth, COMPARISON_TYPE comparison_type) {
    // TODO might as well compare each element to itself too (for equivs only). This isn't done currently.
    COMPARISON_FN comparison_fn = (comparison_type == COMPARISON_TYPE_EQUIVS) ? comparisonEquivs
                                                                              : comparisonNonequivs;
    size_t i = index;
    size_t j = index;
    size_t step = 1;
    BOOL are_containers = stream->at(i)->event_type == CONTAINER_START;
    while (TRUE) {
        if (are_containers) {
            // Find the start of the next container to compare its events for equivalence with this one.
            step = valueEventLength(stream, j);
        }
        j += step;
        if (stream->at(j)->event_type == CONTAINER_END && stream->at(j)->depth == target_depth) {
            i += valueEventLength(stream, i);
            j = i;
        } else {
            (*comparison_fn)(stream, i, j);
        }
        if (stream->at(i)->event_type == CONTAINER_END && stream->at(i)->depth == target_depth) {
            break;
        }
    }
}

BOOL testEmbeddedDocumentSet(IonEventStream *stream, size_t index, int target_depth, COMPARISON_TYPE comparison_type) {
    // TODO could roundtrip the embedded event streams instead of the strings representing them
    ION_ENTER_ASSERTIONS;
    ASSERTION_TYPE assertion_type = (comparison_type == COMPARISON_TYPE_EQUIVS) ? ASSERTION_TYPE_NORMAL
                                                                                : ASSERTION_TYPE_SET_FLAG;
    size_t i = index;
    size_t j = index;
    while (TRUE) {
        j += 1;
        if (stream->at(j)->event_type == CONTAINER_END && stream->at(j)->depth == target_depth) {
            i += 1;
            j = i;
        } else {
            IonEventBase *expected_event = stream->at(i);
            IonEventBase *actual_event = stream->at(j);
            ION_ASSERT(tid_STRING == expected_event->ion_type, "Embedded documents must be strings.");
            ION_ASSERT(tid_STRING == actual_event->ion_type, "Embedded documents must be strings.");
            char *expected_ion_string = ion_string_strdup((ION_STRING *) expected_event->value);
            char *actual_ion_string = ion_string_strdup((ION_STRING *) actual_event->value);
            IonEventStream expected_stream, actual_stream;
            ION_ASSERT(IERR_OK == read_value_stream_from_string(expected_ion_string, &expected_stream),
                       "Embedded document failed to parse");
            ION_ASSERT(IERR_OK == read_value_stream_from_string(actual_ion_string, &actual_stream),
                       "Embedded document failed to parse");
            ION_EXPECT_TRUE_MSG(assertIonEventStreamEq(&expected_stream, &actual_stream, assertion_type),
                                std::string("Error comparing streams \"") << expected_ion_string << "\" and \""
                                                                          << actual_ion_string << "\".");
        }
        if (stream->at(i)->event_type == CONTAINER_END && stream->at(i)->depth == target_depth) {
            break;
        }
    }
    ION_EXIT_ASSERTIONS;
}

const char *embeddedDocumentsAnnotation = "embedded_documents";

void testComparisonSets(IonEventStream *stream, COMPARISON_TYPE comparison_type) {
    size_t i = 0;
    while (i < stream->size()) {
        IonEventBase *event = stream->at(i);
        if (i == stream->size() - 1) {
            ASSERT_EQ(STREAM_END, event->event_type);
            i++;
        } else {
            ASSERT_EQ(CONTAINER_START, event->event_type);
            ASSERT_TRUE((tid_SEXP == event->ion_type) || (tid_LIST == event->ion_type));
            size_t step = valueEventLength(stream, i);
            if (event->num_annotations == 1
                && !strcmp(ion_string_strdup(event->annotations[0]), embeddedDocumentsAnnotation)) {
                testEmbeddedDocumentSet(stream, i + 1, 0, comparison_type);
            } else {
                testEquivsSet(stream, i + 1, 0, comparison_type);
            }
            i += step;
        }
    }
}

TEST_P(GoodEquivsFilenameParameter, good_equivs) {
    iERR status = read_value_stream(initial_stream, input_type, filename, read_all, test_name, catalog);
    EXPECT_EQ(IERR_OK, status) << test_name << " Error: " << ion_error_to_str(status) << std::endl;
    if (IERR_OK == status) {
        testComparisonSets(initial_stream, COMPARISON_TYPE_EQUIVS);
        if (test_type > READ) {
            status = ionTestRoundtrip(initial_stream, &roundtrip_stream, catalog, test_name, filename, input_type,
                                      test_type);
            EXPECT_EQ(IERR_OK, status) << test_name << " Error: roundtrip failed." << std::endl;
            testComparisonSets(roundtrip_stream, COMPARISON_TYPE_EQUIVS);
        }
    }
}

INSTANTIATE_TEST_CASE_P(
        TestVectors,
        GoodEquivsFilenameParameter,
        ::testing::Combine(
                ::testing::ValuesIn(gather(FILETYPE_ALL, CLASSIFICATION_GOOD_EQUIVS)),
                ::testing::Values(READ, ROUNDTRIP_TEXT, ROUNDTRIP_BINARY),
                ::testing::Values(STREAM, BUFFER)
        )
#if ION_TEST_VECTOR_VERBOSE_NAMES
        , FilenameParameterToString()
#endif
);

TEST_P(GoodTimestampEquivTimelineParameter, good_timestamp_equivtimeline) {
    g_TimestampEquals = ion_timestamp_instant_equals;
    iERR status = read_value_stream(initial_stream, input_type, filename, read_all, test_name, catalog);
    EXPECT_EQ(IERR_OK, status) << test_name << " Error: " << ion_error_to_str(status) << std::endl;
    if (IERR_OK == status) {
        testComparisonSets(initial_stream, COMPARISON_TYPE_EQUIVS);
        if (test_type > READ) {
            status = ionTestRoundtrip(initial_stream, &roundtrip_stream, catalog, test_name, filename, input_type,
                                      test_type);
            EXPECT_EQ(IERR_OK, status) << test_name << " Error: roundtrip failed." << std::endl;
            testComparisonSets(roundtrip_stream, COMPARISON_TYPE_EQUIVS);
        }
    }
}

INSTANTIATE_TEST_CASE_P(
        TestVectors,
        GoodTimestampEquivTimelineParameter,
        ::testing::Combine(
                ::testing::ValuesIn(gather(FILETYPE_ALL, CLASSIFICATION_GOOD_TIMESTAMP_EQUIVTIMELINE)),
                ::testing::Values(READ, ROUNDTRIP_TEXT, ROUNDTRIP_BINARY),
                ::testing::Values(STREAM, BUFFER)
        )
#if ION_TEST_VECTOR_VERBOSE_NAMES
        , FilenameParameterToString()
#endif
);

TEST_P(GoodNonequivsFilenameParameter, good_nonequivs) {
    iERR status = read_value_stream(initial_stream, input_type, filename, read_all, test_name, catalog);
    EXPECT_EQ(IERR_OK, status) << test_name << " Error: " << ion_error_to_str(status) << std::endl;
    if (IERR_OK == status) {
        testComparisonSets(initial_stream, COMPARISON_TYPE_NONEQUIVS);
        if (test_type > READ) {
            status = ionTestRoundtrip(initial_stream, &roundtrip_stream, catalog, test_name, filename, input_type,
                                      test_type);
            EXPECT_EQ(IERR_OK, status) << test_name << " Error: roundtrip failed." << std::endl;
            testComparisonSets(roundtrip_stream, COMPARISON_TYPE_NONEQUIVS);
        }
    }
}

INSTANTIATE_TEST_CASE_P(
        TestVectors,
        GoodNonequivsFilenameParameter,
        ::testing::Combine(
                ::testing::ValuesIn(gather(FILETYPE_ALL, CLASSIFICATION_GOOD_NONEQUIVS)),
                ::testing::Values(READ, ROUNDTRIP_TEXT, ROUNDTRIP_BINARY),
                ::testing::Values(STREAM, BUFFER)
        )
#if ION_TEST_VECTOR_VERBOSE_NAMES
        , FilenameParameterToString()
#endif
);

TEST_P(BadFilenameParameter, bad_basic) {
    iERR status = read_value_stream(initial_stream, input_type, filename, read_all, test_name, catalog);
    EXPECT_NE(IERR_OK, status) << test_name << " FAILED" << std::endl;
}

INSTANTIATE_TEST_CASE_P(
        TestVectors,
        BadFilenameParameter,
        ::testing::Combine(
                ::testing::ValuesIn(gather(FILETYPE_ALL, CLASSIFICATION_BAD)),
                ::testing::Values(STREAM, BUFFER)
        )
#if ION_TEST_VECTOR_VERBOSE_NAMES
        , BadFilenameParameterToString()
#endif
);
