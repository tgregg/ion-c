#include <ion_debug.h>
#include <ion_helpers.h>
#include <ion_test_utils.h>
#include <ion_types.h>

#define ION_ASSERT_BUFFER_SIZE 1024

static char ion_assert_buffer[ION_ASSERT_BUFFER_SIZE];

#define ASSERT_ERROR(message, file, line) fprintf(stderr, "%s (%s:%d)", (message), ion_helper_short_filename((file)), (line))

#define ASSERT_EQUALS_INT(expected, actual, message) IONCHECK(_assert_equals_int((expected), (actual), (message), __FILE__, __LINE__))

static inline iERR _assert_equals_int(int expected, int actual, char* message, char* file, int line) {
    if (expected == actual) {
        return IERR_OK;
    } else {
        if (message != NULL) {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "%s: Expected %i but was %i", message, expected, actual);
        } else {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "Expected %i but was %i", expected, actual);
        }
        ASSERT_ERROR(ion_assert_buffer, file, line);
        return IERR_INVALID_ARG;
    }
}

#define ASSERT_EQUALS_ION_TYPE(expected, actual, message) IONCHECK(_assert_equals_ion_type((expected), (actual), (message), __FILE__, __LINE__))

static inline iERR _assert_equals_ion_type(ION_TYPE expected, ION_TYPE actual, char* message, char* file, int line) {
    if (expected == actual) {
        return IERR_OK;
    } else {
        if (message != NULL) {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "%s: Expected %s but was %s", message, ion_test_get_type_name(expected), ion_test_get_type_name(actual));
        } else {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "Expected %s but was %s", ion_test_get_type_name(expected), ion_test_get_type_name(actual));
        }
        ASSERT_ERROR(ion_assert_buffer, file, line);
        return IERR_INVALID_ARG;
    }
}

#define ASSERT_EQUALS_C_STRING_ION_STRING(expected, actual, message) IONCHECK(_assert_equals_c_string_ion_string((expected), (actual), (message), __FILE__, __LINE__))

static inline iERR _assert_equals_c_string_ion_string(char* expected, ION_STRING* actual, char* message, char* file, int line) {
    char* actual_c_string = calloc(actual->length + 1, sizeof(char));
    memcpy(actual_c_string, actual->value, actual->length);

    BOOL error = FALSE;
    if (strlen(expected) != actual->length) {
        error = TRUE;
    } else if (strncmp(expected, actual_c_string, actual->length) != 0) {
        error = TRUE;
    }

    if (error) {
        if (message != NULL) {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "%s: Expected %s but was %s", message, expected, actual_c_string);
        } else {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "Expected %s but was %s", expected, actual_c_string);
        }
        ASSERT_ERROR(ion_assert_buffer, file, line);
    }

    free(actual_c_string);

    return error ? IERR_INVALID_ARG : IERR_OK;
}

#define ASSERT_EQUALS_C_STRING(expected, actual, message) IONCHECK(_assert_equals_c_string((expected), (actual), (message), __FILE__, __LINE__))

static inline iERR _assert_equals_c_string(char* expected, char* actual, char* message, char* file, int line) {
    if (strcmp(expected, actual) == 0) {
        return IERR_OK;
    } else {
        if (message != NULL) {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "%s: Expected %s but was %s", message, expected, actual);
        } else {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "Expected %s but was %s", expected, actual);
        }
        ASSERT_ERROR(ion_assert_buffer, file, line);
        return IERR_INVALID_ARG;
    }
}

#define ASSERT_EQUALS_BYTES(expected, expected_length, actual, actual_length, message) IONCHECK(_assert_equals_bytes((expected), (expected_length), (actual), (actual_length), (message), __FILE__, __LINE__))

static inline iERR _assert_equals_bytes(BYTE* expected, size_t expected_length, BYTE* actual, size_t actual_length, char* message, char* file, int line) {
    BOOL error = FALSE;

    if (expected_length != actual_length) {
        error = TRUE;
    } else if (strncmp((char*) expected, (char*) actual, expected_length) != 0) {
        error = TRUE;
    }

    if (error) {
        char* expected_c_string = bytes_to_hex_string(expected, expected_length);
        char* actual_c_string = bytes_to_hex_string(actual, actual_length);
        if (message != NULL) {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "%s: Expected %s but was %s", message, expected_c_string, actual_c_string);
        } else {
            snprintf(ion_assert_buffer, ION_ASSERT_BUFFER_SIZE, "Expected %s but was %s", expected_c_string, actual_c_string);
        }
        ASSERT_ERROR(ion_assert_buffer, file, line);

        free(expected_c_string);
        free(actual_c_string);
    }

    return error ? IERR_INVALID_ARG : IERR_OK;
}