
#include <ion_extractor_impl.h>
#include <sys/time.h>
#include "ion_assert.h"
#include "ion_helpers.h"
#include "ion_test_util.h"
#include "ion_extractor.h"

#define ION_READ_BENCHMARK_START(filename, iterations) \
    struct timeval t0, t1; \
    int i, result = 0; \
    long size, actual_size; \
    char *buffer = NULL; \
    FILE *fstream; \
    fstream = fopen(filename, "rb"); \
    fseek(fstream, 0, SEEK_END); \
    size = ftell(fstream); \
    rewind(fstream); \
    buffer = (char *) malloc(size); \
    actual_size = fread(buffer, 1, size, fstream); \
    fclose(fstream); \
    gettimeofday(&t0, NULL); \
    for (i = 0; i < (iterations); i++)

#define ION_READ_BENCHMARK_FINISH \
    gettimeofday(&t1, NULL); \
    free(buffer); \
    printf("\nDid %u calls in %.2g seconds with result %u\n", i, t1.tv_sec - t0.tv_sec + 1E-6 * (t1.tv_usec - t0.tv_usec), result);

#define ION_EXTRACTOR_BENCHMARK(filename, path_registration_func, iterations) \
    ION_READ_BENCHMARK_START(filename, iterations) { \
        ION_ASSERT_OK(test_benchmark_match(&result, (BYTE *)buffer, actual_size, path_registration_func)); \
    } \
    ION_READ_BENCHMARK_FINISH;

typedef iERR (*TEST_BENCHMARK_REGISTER)(hEXTRACTOR, int *);

iERR test_sum_ints(hREADER reader, ION_EXTRACTOR_PATH *matched_path, void *user_context, ION_EXTRACTOR_CONTROL *control) {
    iENTER;
    ION_TYPE type;
    int age;
    int *sum = (int *)user_context;
    ion_reader_get_type(reader, &type);
    if (type == tid_INT) {
        ion_reader_read_int(reader, &age);
        *sum += age;
    }
    else {
        FAILWITH(IERR_INVALID_STATE);
    }
    *control = ION_EXTRACTOR_CONTROL_NEXT();
    iRETURN;
}

iERR test_benchmark_register_sum_age(hEXTRACTOR extractor, int *result) {
    iENTER;
    hPATH path;
    ION_STRING field;
    //const char *path_ion = "($ion_wildcard::'*' age)";
    //ion_extractor_register_path_from_ion(extractor, &test_sum_ages, &ages_sum, (BYTE *)path_ion, (SIZE)strlen(path_ion), &path);
    IONCHECK(ion_extractor_register_path_start(extractor, &test_sum_ints, result));
    IONCHECK(ion_extractor_register_path_append_wildcard(extractor));
    IONCHECK(ion_string_from_cstr("age", &field));
    IONCHECK(ion_extractor_register_path_append_field(extractor, &field));
    IONCHECK(ion_extractor_register_path_finish(extractor, &path));
    iRETURN;
}

iERR test_benchmark_register_sum_foo(hEXTRACTOR extractor, int *result) {
    iENTER;
    hPATH path;
    ION_STRING field;

    IONCHECK(ion_extractor_register_path_start(extractor, &test_sum_ints, result));
    IONCHECK(ion_string_from_cstr("foo", &field));
    IONCHECK(ion_extractor_register_path_append_field(extractor, &field));
    IONCHECK(ion_extractor_register_path_finish(extractor, &path));
    iRETURN;
}

iERR test_benchmark_match(int *result, BYTE *buffer, SIZE length, TEST_BENCHMARK_REGISTER register_func) {
    iENTER;
    ION_EXTRACTOR_OPTIONS extractor_options;

    hEXTRACTOR extractor;
    hREADER reader;

    memset(&extractor_options, 0, sizeof(ION_EXTRACTOR_OPTIONS));
    extractor_options.max_path_length = 2;
    extractor_options.max_num_paths = 1;

    IONCHECK(ion_reader_open_buffer(&reader, buffer, length, NULL));
    IONCHECK(ion_extractor_open(&extractor, &extractor_options));
    IONCHECK(register_func(extractor, result));
    IONCHECK(ion_extractor_match(extractor, reader));
    IONCHECK(ion_extractor_close(extractor));
    IONCHECK(ion_reader_close(reader));
    iRETURN;
}

iERR test_benchmark_deep_iterate_recursive(int *result, hREADER reader) {
    iENTER;
    ION_TYPE t;
    ION_STRING field_name;
    BOOL in_struct;
    int value;
    for (;;) {
        IONCHECK(ion_reader_next(reader, &t));
        if (t == tid_EOF) {
            break;
        }
        switch(ION_TYPE_INT(t)) {
            case tid_INT_INT:
                IONCHECK(ion_reader_is_in_struct(reader, &in_struct));
                if (in_struct) {
                    IONCHECK(ion_reader_get_field_name(reader, &field_name));
                    if (field_name.length == 3 && !strncmp((char *)field_name.value, "foo", 3)) {
                        IONCHECK(ion_reader_read_int(reader, &value));
                        *result += value;
                    }
                }
            case tid_NULL_INT:
            case tid_BOOL_INT:
            case tid_FLOAT_INT:
            case tid_DECIMAL_INT:
            case tid_TIMESTAMP_INT:
            case tid_SYMBOL_INT:
            case tid_STRING_INT:
            case tid_CLOB_INT:
            case tid_BLOB_INT:
                continue;
            case tid_LIST_INT:
            case tid_SEXP_INT:
            case tid_STRUCT_INT:
                IONCHECK(ion_reader_step_in(reader));
                IONCHECK(test_benchmark_deep_iterate_recursive(result, reader));
                IONCHECK(ion_reader_step_out(reader));
                continue;
            default:
            FAILWITH(IERR_INVALID_STATE);
        }
    }
    iRETURN;
}

iERR test_benchmark_deep_iterate(int *result, BYTE *buffer, SIZE length) {
    iENTER;
    hREADER reader;
    IONCHECK(ion_reader_open_buffer(&reader, buffer, length, NULL));
    IONCHECK(test_benchmark_deep_iterate_recursive(result, reader));
    IONCHECK(ion_reader_close(reader));
    iRETURN;
}

TEST(IonExtractorBenchmark, SumAge) {
    ION_EXTRACTOR_BENCHMARK("/Users/greggt/Desktop/generated_short.10n", &test_benchmark_register_sum_age, 100000);
}

TEST(IonExtractorBenchmark, CountFooShort) {
    ION_EXTRACTOR_BENCHMARK("/Users/greggt/Desktop/generated_short_nested.10n", &test_benchmark_register_sum_foo, 100000);
}

TEST(IonExtractorBenchmark, DeepIterateShort) {
    ION_READ_BENCHMARK_START("/Users/greggt/Desktop/generated_short_nested.10n", 100000) {
        ION_ASSERT_OK(test_benchmark_deep_iterate(&result, (BYTE *)buffer, actual_size));
    }
    ION_READ_BENCHMARK_FINISH;
}


iERR test_benchmark_register_sum_foo_long(hEXTRACTOR extractor, int *result) {
    iENTER;
    hPATH path;
    ION_STRING field;

    IONCHECK(ion_extractor_register_path_start(extractor, &test_sum_ints, result));
    IONCHECK(ion_extractor_register_path_append_wildcard(extractor));
    IONCHECK(ion_string_from_cstr("foo", &field));
    IONCHECK(ion_extractor_register_path_append_field(extractor, &field));
    IONCHECK(ion_extractor_register_path_finish(extractor, &path));
    iRETURN;
}

TEST(IonExtractorBenchmark, CountFooLong) {
    // Expect: ~7 seconds
    ION_EXTRACTOR_BENCHMARK("/Users/greggt/Desktop/generated_nested.10n", &test_benchmark_register_sum_foo_long, 100000);
}

TEST(IonExtractorBenchmark, DeepIterateLong) {
    // Expect: ~40 seconds. NOTE: Java DOM full materialization: ~80 seconds.
    ION_READ_BENCHMARK_START("/Users/greggt/Desktop/generated_nested.10n", 100000) {
        ION_ASSERT_OK(test_benchmark_deep_iterate(&result, (BYTE *)buffer, actual_size));
    }
    ION_READ_BENCHMARK_FINISH;
}
