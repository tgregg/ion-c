/*
 * Copyright 2012-2017 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

/**
 * A path extractor that operates on top of the ION_READER interface.
 */

#ifndef ION_EXTRACTOR_H
#define ION_EXTRACTOR_H

#include "ion.h"

#ifdef ION_EXTRACTOR_MAX_PATH_LENGTH_LIMIT
    #undef ION_EXTRACTOR_MAX_PATH_LENGTH_LIMIT
#endif

#ifdef ION_EXTRACTOR_MAX_NUM_PATHS_LIMIT
    #undef ION_EXTRACTOR_MAX_NUM_PATHS_LIMIT
#endif

/**
 * Limit on the maximum length of any path. Sizes and indices are stored in an unsigned 8-bit integer, so this is the
 * max value that can be represented.
 * NOTE: this is a constant that may not be redefined by the user.
 */
#define ION_EXTRACTOR_MAX_PATH_LENGTH_LIMIT UINT8_MAX

/**
 * Limit on the maximum number of paths that can be registered to any extractor. Registered paths are assigned a bit
 * index in a length-64 bitmap.
 * NOTE: this is a constant that may not be redefined by the user.
 */
#define ION_EXTRACTOR_MAX_NUM_PATHS_LIMIT (sizeof(uint_fast64_t))

#ifdef ION_EXTRACTOR_MAX_PATH_LENGTH_DEFAULT
    #undef ION_EXTRACTOR_MAX_PATH_LENGTH_DEFAULT
#endif

#ifdef ION_EXTRACTOR_MAX_NUM_PATHS_DEFAULT
    #undef ION_EXTRACTOR_MAX_NUM_PATHS_DEFAULT
#endif

/**
 * Default maximum length of any path. DEFAULT_WRITER_STACK_DEPTH is chosen as the default because this is the default
 * maximum depth to which readers can descend. Unless the reader is configured with a higher `max_container_depth`,
 * attempting to match paths longer than this will fail.
 * NOTE: this is a constant that may not be redefined by the user.
 */
#define ION_EXTRACTOR_MAX_PATH_LENGTH_DEFAULT DEFAULT_WRITER_STACK_DEPTH

/**
 * Default maximum number of paths that can be registered to any extractor.
 * NOTE: this is a constant that may not be redefined by the user.
 */
#define ION_EXTRACTOR_MAX_NUM_PATHS_DEFAULT 16

/**
 * The maximum length of paths that can be registered to any extractor. The value of this constant may be defined by the
 * user as necessary. Its defined value may not be greater than ION_EXTRACTOR_MAX_PATH_LENGTH_LIMIT nor less than 1, and
 * will be defaulted to DEFAULT_WRITER_STACK_DEPTH if missing or less than 1 and clamped at
 * ION_EXTRACTOR_MAX_PATH_LENGTH_LIMIT if necessary.
 *
 * Defining this value to be as small as possible while remaining larger than any extractor's `max_path_length` will
 * increase the memory density of each extractor, which may improve performance.
 */
#ifndef ION_EXTRACTOR_MAX_PATH_LENGTH
    #define ION_EXTRACTOR_MAX_PATH_LENGTH ION_EXTRACTOR_MAX_PATH_LENGTH_DEFAULT
#else
    #if ION_EXTRACTOR_MAX_PATH_LENGTH > ION_EXTRACTOR_MAX_PATH_LENGTH_LIMIT
        #undef ION_EXTRACTOR_MAX_PATH_LENGTH
        #define ION_EXTRACTOR_MAX_PATH_LENGTH ION_EXTRACTOR_MAX_PATH_LENGTH_LIMIT
    #elif ION_EXTRACTOR_MAX_PATH_LENGTH < 1
        #undef ION_EXTRACTOR_MAX_PATH_LENGTH
        #define ION_EXTRACTOR_MAX_PATH_LENGTH ION_EXTRACTOR_MAX_PATH_LENGTH_DEFAULT
    #endif
#endif

/**
 * The maximum number of paths that can be registered to any extractor. The value of this constant may be defined by the
 * user as necessary. Its defined value may not be greater than ION_EXTRACTOR_MAX_NUM_PATHS_LIMIT nor less than 1, and
 * will be defaulted to ION_EXTRACTOR_MAX_NUM_PATHS_DEFAULT if missing or less than 1 and clamped at
 * ION_EXTRACTOR_MAX_NUM_PATHS_LIMIT if necessary.
 *
 * Defining this value to be as small as possible while remaining larger than any extractor's `max_num_paths` will
 * increase the memory density of each extractor, which may improve performance.
 */
#ifndef ION_EXTRACTOR_MAX_NUM_PATHS
    #define ION_EXTRACTOR_MAX_NUM_PATHS ION_EXTRACTOR_MAX_NUM_PATHS_DEFAULT
#else
    #if ION_EXTRACTOR_MAX_NUM_PATHS > ION_EXTRACTOR_MAX_NUM_PATHS_LIMIT
        #undef ION_EXTRACTOR_MAX_NUM_PATHS
        #define ION_EXTRACTOR_MAX_NUM_PATHS ION_EXTRACTOR_MAX_NUM_PATHS_LIMIT
    #elif  ION_EXTRACTOR_MAX_NUM_PATHS < 1
        #undef ION_EXTRACTOR_MAX_NUM_PATHS
        #define ION_EXTRACTOR_MAX_NUM_PATHS ION_EXTRACTOR_MAX_NUM_PATHS_DEFAULT
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extractor configuration to be supplied by the user when opening a new extractor.
 */
typedef struct _ion_extractor_options {
    /**
     * The maximum length of paths that can be registered to this extractor. Defaults to ION_EXTRACTOR_MAX_PATH_LENGTH,
     * and may not be greater than ION_EXTRACTOR_MAX_PATH_LENGTH.
     *
     * The closer this value is to the length of the longest path registered to the extractor, the denser the paths can
     * be organized, which may improve performance.
     */
    uint8_t max_path_length;

    /**
     * The maximum number of paths that can be registered to this extractor. Defaults to ION_EXTRACTOR_MAX_NUM_PATHS,
     * and may not be greater than ION_EXTRACTOR_MAX_NUM_PATHS.
     *
     * The closer this value is to the actual number of paths provided to the extractor, the denser the paths can
     * be organized, which may improve performance.
     */
    uint8_t max_num_paths;

} ION_EXTRACTOR_OPTIONS;

/**
 * The internal extractor structure.
 */
typedef struct _ion_extractor ION_EXTRACTOR;

/**
 * Handle to an ION_EXTRACTOR.
 */
typedef ION_EXTRACTOR *hEXTRACTOR;

/**
 * The internal path structure.
 */
typedef struct _ion_extractor_path_descriptor ION_EXTRACTOR_PATH_DESCRIPTOR;

/**
 * Handle to an ION_EXTRACTOR_PATH_DESCRIPTOR.
 */
typedef ION_EXTRACTOR_PATH_DESCRIPTOR *hPATH;

/**
 * An instruction used by callback implementations to control execution of the extractor after a match. In general,
 * these instructions tell the extractor to "step-out-N", meaning that the extractor should continue processing from
 * N levels up from the length of the matched path.
 *
 * The following macros are provided for convenience:
 *  `ION_EXTRACTOR_CONTROL_STEP_OUT(n)` - Creates a control instruction that tells the extractor to step out N.
 *  `ION_EXTRACTOR_CONTROL_NEXT()` - Creates a control instruction that tells the extractor to continue normally,
 *      without stepping out (i.e. step out 0).
 */
typedef int_fast16_t ION_EXTRACTOR_CONTROL;

/**
 * Signals the extractor to step out N before resuming processing of paths.
 */
static inline ION_EXTRACTOR_CONTROL
ion_extractor_control_step_out(int n)
{
    return (ION_EXTRACTOR_CONTROL) n;
}

/**
 * Signals the extractor to continue processing of paths without stepping out.
 */
static inline ION_EXTRACTOR_CONTROL
ion_extractor_control_next()
{
    return ion_extractor_control_step_out(0);
}

/**
 * Callback function to be invoked when the extractor matches a path.
 *
 * @param reader - The reader provided to the matching extractor through `ion_extractor_match`, positioned on the
 *  matching value. This allows the callback implementation to query the value's ion_type and to use the appropriate
 *  ion_reader function to read the matched value.
 * @param matched_path - The path that was matched. This will be reference-equivalent to one of the extractor's
 *  registered paths.
 * @param user_context - User context data, reference-equivalent to the user_context provided for matched_path during
 *  path registration.
 * @param control - A control instruction to be conveyed back to the extractor (output parameter).
 */
typedef iERR (*ION_EXTRACTOR_CALLBACK)(hREADER reader, hPATH matched_path, void *user_context,
                                       ION_EXTRACTOR_CONTROL *control);

/**
 * Allocates a new extractor and prepares it to process the given reader.
 *
 * @param extractor - A non-null extractor pointer. The caller is responsible for freeing the resulting extractor using
 *  `ion_extractor_close`.
 * @param options - Configuration options. May be null. If null, defaults will be used. The caller owns the memory, but
 *  is not required to keep it accessible after this call.
 * @return a non-zero error code in the case of failure, otherwise IERR_OK.
 */
ION_API_EXPORT iERR ion_extractor_open(hEXTRACTOR *extractor, ION_EXTRACTOR_OPTIONS *options);

/**
 * Registers the given callback and user context to a new empty path. To finish constructing the path, the user
 * must append at least one path component and must then call `ion_extractor_path_finish` before calling
 * `ion_extractor_match`. Calling this function will fail unless `ion_extractor_register_path_start` and
 * `ion_extractor_register_path_finish` have been called the same number of times.
 *
 * @param extractor - An extractor which has already been opened by calling `ion_extractor_open`.
 * @param callback - The callback function to be invoked by the extractor when the given path is matched.
 * @param user_context - User context data to be passed as an argument to `callback`. This data remains opaque to the
 *  extractor, and may be null.
 * @return a non-zero error code in the case of failure, otherwise IERR_OK.
 *
 * Ownership: the caller owns the callback and the user context.
 */
ION_API_EXPORT iERR ion_extractor_register_path_start(hEXTRACTOR extractor, ION_EXTRACTOR_CALLBACK callback, void *user_context);

/**
 * Appends a path component representing a field name to the current path. Calling this function will fail unless there
 * has been exactly one more call to `ion_extractor_register_path_start` than to `ion_extractor_register_path_finish`.
 *
 * @param extractor - An extractor which has already been opened by calling `ion_extractor_open` and has an active path.
 * @param value - The field name to add to the path.
 * @return a non-zero error code in the case of failure, otherwise IERR_OK.
 *
 * Ownership: the caller owns the value, but is not required to keep it accessible after this call.
 */
ION_API_EXPORT iERR ion_extractor_register_path_append_field(hEXTRACTOR extractor, ION_STRING *value);

/**
 * Appends a path component representing an ordinal (e.g. a collection index) to the current path. Calling this function
 * will fail unless there has been exactly one more call to `ion_extractor_register_path_start` than to
 * `ion_extractor_register_path_finish`.
 *
 * @param extractor - An extractor which has already been opened by calling `ion_extractor_open` and has an active path.
 * @param value - The ordinal to add to the path.
 * @return a non-zero error code in the case of failure, otherwise IERR_OK.
 */
ION_API_EXPORT iERR ion_extractor_register_path_append_ordinal(hEXTRACTOR extractor, POSITION value);

/**
 * Appends a path component representing a wildcard to the current path.  Calling this function will fail unless there
 * has been exactly one more call to `ion_extractor_register_path_start` than to `ion_extractor_register_path_finish`.
 *
 * @param extractor - An extractor which has already been opened by calling `ion_extractor_open` and has an active path.
 * @return a non-zero error code in the case of failure, otherwise IERR_OK.
 */
ION_API_EXPORT iERR ion_extractor_register_path_append_wildcard(hEXTRACTOR extractor);

/**
 * Finishes the current path and provides a handle to it. Calling this function will fail unless there has been exactly
 * one more call to `ion_extractor_register_path_start` than to `ion_extractor_register_path_finish` AND the current
 * path has at least one component.
 * @param extractor - An extractor which has already been opened by calling `ion_extractor_open` and has an active path.
 * @param path - A handle to the resulting path.
 * @return a non-zero error code in the case of failure, otherwise IERR_OK.
 *
 * Ownership: the extractor retains ownership of the underlying path.
 */
ION_API_EXPORT iERR ion_extractor_register_path_finish(hEXTRACTOR extractor, hPATH *path);

/**
 * Registers a path from text or binary Ion data. The data must contain exactly one top-level value: an ordered sequence
 * (list or sexp) containing a number of elements less than or equal to the extractor's `max_path_length`. The elements
 * must be either text types (string or symbol), representing fields or wildcards, or integers, representing ordinals.
 * In order for a text value to represent a wildcard, it must be annotated with the special annotation `$ion_wildcard`
 * as its first annotation, and must have text corresponding to one of the supported wildcards (currently only `*`).
 * This is to allow paths to match fields with the same text as a wildcard.
 * For example,
 *  <pre>
 *    (abc $ion_wildcard::'*' def 2)
 *  </pre>
 * represents a path of length 4 consisting of a field, wildcard, field, and ordinal.
 * NOTE: this is a standalone function that does not require calls to `ion_extractor_register_path_start` nor
 * `ion_extractor_register_path_finish`. However, other paths registered to the same extractor may be constructed
 * using those functions.
 *
 * @param extractor - An extractor which has already been opened by calling `ion_extractor_open`.
 * @param callback - The callback function to be invoked by the extractor when the given path is matched.
 * @param user_context - User context data to be passed as an argument to `callback`. This data remains opaque to the
 *  extractor, and may be null.
 * @param ion_data - The ion data representing the path, which follows the schema described above.
 * @param ion_data_length - The length, in bytes, of `ion_data`.
 * @param path - A handle to the resulting path.
 * @return a non-zero error code in the case of failure, otherwise IERR_OK.
 *
 * Ownership: the caller owns the callback and the user context.
 */
ION_API_EXPORT iERR ion_extractor_register_path_from_ion(hEXTRACTOR  extractor, ION_EXTRACTOR_CALLBACK callback,
                                                         void *user_context, BYTE *ion_data, SIZE ion_data_length,
                                                         hPATH  *path);

/**
 * Extracts matches within the data read by the extractor's reader using the extractor's registered paths.
 * @param extractor - The extractor to match.
 * @param reader - A pre-allocated reader, which has already been opened by calling `ion_reader_open_*`.
 * @return a non-zero error code in the case of failure (including any underlying parsing failures), otherwise IERR_OK.
 *
 * Ownership: the caller owns the reader and its associated resources, and guarantees that that they remain accessible
 * at least until this call terminates. To free the reader, the caller must eventually call `ion_reader_close` as usual.
 */
ION_API_EXPORT iERR ion_extractor_match(hEXTRACTOR extractor, hREADER reader);

/**
 * Deallocates the given extractor.
 * @param extractor - The extractor to deallocate.
 */
ION_API_EXPORT iERR ion_extractor_close(hEXTRACTOR extractor);

#ifdef __cplusplus
}
#endif

#endif //ION_EXTRACTOR_H
