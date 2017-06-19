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

#include <inc/ion_extractor.h>
#include "ion_extractor_impl.h"

#define ION_EXTRACTOR_ALL_PATHS_ACTIVE (ION_EXTRACTOR_ACTIVE_PATH_MAP)0xFFFFFFFFFFFFFFFF

#define ION_EXTRACTOR_GET_COMPONENT(extractor, path_depth, path_index) \
    &extractor->path_components[(path_depth) * extractor->options.max_num_paths + path_index]

#define ION_EXTRACTOR_WILDCARD_ANNOTATION "$ion_wildcard"

/**
 * A bit map representing matching paths at a particular path depth. If the bit at index N is set, it means the path
 * with ID = N is active. If zero, there are no paths active at this depth, and the extractor is free to skip and step
 * out.
 * NOTE: This is coupled to the ION_EXTRACTOR_DEFAULT_MAX_NUM_PATHS_LIMIT: one bit for each possible
 * path. Raising that limit (or making it configurable) will require a different strategy for tracking active paths.
 */
typedef uint_fast64_t ION_EXTRACTOR_ACTIVE_PATH_MAP;

iERR ion_extractor_open(hEXTRACTOR *extractor, ION_EXTRACTOR_OPTIONS *options) {
    iENTER;
    ION_EXTRACTOR *pextractor = NULL;
    SIZE len;
    ASSERT(extractor);

    len = sizeof(ION_EXTRACTOR);
    pextractor = (ION_EXTRACTOR *)ion_alloc_owner(len);
    *extractor = pextractor;
    if (!pextractor) {
        FAILWITH(IERR_NO_MEMORY);
    }
    memset(pextractor, 0, len);

    if (options) {
        if (options->max_num_paths > ION_EXTRACTOR_MAX_NUM_PATHS || options->max_num_paths < 1) {
            FAILWITHMSG(IERR_INVALID_ARG, "Extractor's max_num_paths must be in [1, ION_EXTRACTOR_MAX_NUM_PATHS].");
        }
        if (options->max_path_length > ION_EXTRACTOR_MAX_PATH_LENGTH || options->max_path_length < 1) {
            FAILWITHMSG(IERR_INVALID_ARG, "Extractor's max_path_length must be in [1, ION_EXTRACTOR_MAX_PATH_LENGTH].");
        }
    }
    pextractor->options.max_num_paths = (options) ? options->max_num_paths : (int8_t)ION_EXTRACTOR_MAX_NUM_PATHS;
    pextractor->options.max_path_length = (options) ? options->max_path_length : (int8_t)ION_EXTRACTOR_MAX_PATH_LENGTH;

    iRETURN;
}

iERR ion_extractor_close(hEXTRACTOR extractor) {
    iENTER;
    // Frees associated resources (path descriptors, copied field strings), then frees the extractor
    // itself.
    ion_free_owner(extractor);
    iRETURN;
}

iERR ion_extractor_path_create(hEXTRACTOR extractor, ION_EXTRACTOR_SIZE path_length, ION_EXTRACTOR_CALLBACK callback,
                               void *user_context, ION_EXTRACTOR_PATH_DESCRIPTOR **p_path) {
    iENTER;
    ION_EXTRACTOR_MATCHER *matcher;
    ION_EXTRACTOR_PATH_DESCRIPTOR *path;

    ASSERT(p_path);

    if (extractor->matchers_length >= extractor->options.max_num_paths) {
        FAILWITHMSG(IERR_NO_MEMORY, "Too many registered paths.");
    }
    if (extractor->_path_in_progress || extractor->_cur_path_len != 0) {
        FAILWITHMSG(IERR_INVALID_STATE, "Cannot start new path before finishing the previous one.");
    }
    if (path_length > extractor->options.max_path_length || path_length <= 0) {
        FAILWITHMSG(IERR_INVALID_ARG, "Illegal number of path components.");
    }
    // This will be freed by ion_free_owner during ion_extractor_close.
    path = ion_alloc_with_owner(extractor, sizeof(ION_EXTRACTOR_PATH_DESCRIPTOR));
    extractor->_path_in_progress = true;
    path->path_length = path_length;
    path->path_id = extractor->matchers_length;
    path->extractor = extractor;
    matcher = &extractor->matchers[path->path_id];
    matcher->callback = callback;
    matcher->user_context = user_context;
    matcher->path = path;
    *p_path = path;

    iRETURN;
}

iERR _ion_extractor_path_append_helper(ION_EXTRACTOR_PATH_DESCRIPTOR *path, ION_EXTRACTOR_PATH_COMPONENT **p_component) {
    iENTER;
    ION_EXTRACTOR_PATH_COMPONENT *component;
    ION_EXTRACTOR *extractor = path->extractor;

    ASSERT(p_component);
    ASSERT(extractor->matchers_length >= 0);

    if (!extractor->_path_in_progress) {
        FAILWITHMSG(IERR_INVALID_STATE, "No path is in progress.");
    }

    if (extractor->_cur_path_len >= extractor->options.max_path_length || extractor->_cur_path_len >= path->path_length) {
        FAILWITHMSG(IERR_INVALID_STATE, "Path is too long.");
    }

    component = ION_EXTRACTOR_GET_COMPONENT(extractor, extractor->_cur_path_len, extractor->matchers_length);
    component->is_terminal = (++extractor->_cur_path_len == path->path_length);
    if (component->is_terminal) {
        extractor->_path_in_progress = false;
        extractor->_cur_path_len = 0;
        extractor->matchers_length++;
    }
    *p_component = component;
    iRETURN;
}

iERR ion_extractor_path_append_field(ION_EXTRACTOR_PATH_DESCRIPTOR *path, ION_STRING *value) {
    iENTER;
    ION_EXTRACTOR_PATH_COMPONENT *component;
    IONCHECK(_ion_extractor_path_append_helper(path, &component));
    // Note: this is an allocation, with extractor as the memory owner. This allocated memory is freed by
    // `ion_free_owner` during `ion_extractor_close`. Each of these occupies space in a contiguous block assigned
    // to the extractor.
    IONCHECK(ion_string_copy_to_owner(path->extractor, &component->value.text, value));
    component->type = FIELD;
    iRETURN;
}

iERR ion_extractor_path_append_ordinal(ION_EXTRACTOR_PATH_DESCRIPTOR *path, POSITION value) {
    iENTER;
    ION_EXTRACTOR_PATH_COMPONENT *component;
    IONCHECK(_ion_extractor_path_append_helper(path, &component));
    component->value.ordinal = value;
    component->type = ORDINAL;
    iRETURN;
}

iERR ion_extractor_path_append_wildcard(ION_EXTRACTOR_PATH_DESCRIPTOR *path) {
    iENTER;
    ION_EXTRACTOR_PATH_COMPONENT *component;
    IONCHECK(_ion_extractor_path_append_helper(path, &component));
    component->type = WILDCARD;
    iRETURN;
}

iERR ion_extractor_path_create_from_ion(hEXTRACTOR extractor, ION_EXTRACTOR_CALLBACK callback,
                                        void *user_context, BYTE *ion_data, SIZE ion_data_length,
                                        ION_EXTRACTOR_PATH_DESCRIPTOR **p_path) {
    iENTER;
    ION_READER *reader = NULL;
    ION_READER_OPTIONS options;
    ION_TYPE type;
    ION_STRING text, wildcard_annotation;
    BOOL has_annotations;
    ION_EXTRACTOR_PATH_COMPONENT components[ION_EXTRACTOR_MAX_PATH_LENGTH], *component;
    ION_EXTRACTOR_SIZE path_length = 0, i;
    ION_EXTRACTOR_PATH_DESCRIPTOR *path;

    ASSERT(p_path);

    wildcard_annotation.value = (BYTE *)ION_EXTRACTOR_WILDCARD_ANNOTATION;
    wildcard_annotation.length = (int32_t)strlen(ION_EXTRACTOR_WILDCARD_ANNOTATION);

    memset(&options, 0, sizeof(ION_READER_OPTIONS));
    options.max_container_depth = (extractor->options.max_path_length < MIN_WRITER_STACK_DEPTH)
                                  ? MIN_WRITER_STACK_DEPTH : extractor->options.max_path_length;
    IONCHECK(ion_reader_open_buffer(&reader, ion_data, ion_data_length, &options));
    IONCHECK(ion_reader_next(reader, &type));
    if (type != tid_SEXP && type != tid_LIST) {
        FAILWITHMSG(IERR_INVALID_ARG, "Improper path format.");
    }
    IONCHECK(ion_reader_step_in(reader));
    for (;;) {
        component = &components[path_length];
        IONCHECK(ion_reader_next(reader, &type));
        if (type == tid_EOF) {
            break;
        }
        path_length++;
        switch(ION_TYPE_INT(type)) {
            case tid_INT_INT:
                component->type = ORDINAL;
                IONCHECK(ion_reader_read_int64(reader, &component->value.ordinal));
                break;
            case tid_SYMBOL_INT:
            case tid_STRING_INT:
                IONCHECK(ion_reader_has_any_annotations(reader, &has_annotations));
                if (has_annotations) {
                    IONCHECK(ion_reader_get_an_annotation(reader, 0, &text));
                    if (ION_STRING_EQUALS(&wildcard_annotation, &text)) {
                        component->type = WILDCARD;
                        break;
                    }
                }
                component->type = FIELD;
                IONCHECK(ion_reader_read_string(reader, &text));
                IONCHECK(ion_string_copy_to_owner(extractor, &component->value.text, &text));
                break;
            default:
                FAILWITHMSG(IERR_INVALID_ARG, "Improper path format.");
        }
    }
    IONCHECK(ion_reader_step_out(reader));
    IONCHECK(ion_extractor_path_create(extractor, path_length, callback, user_context, &path));
    for (i = 0; i < path_length; i++) {
        component = &components[i];
        switch (component->type) {
            case ORDINAL:
                IONCHECK(ion_extractor_path_append_ordinal(path, component->value.ordinal));
                break;
            case FIELD:
                IONCHECK(ion_extractor_path_append_field(path, &component->value.text));
                break;
            case WILDCARD:
                IONCHECK(ion_extractor_path_append_wildcard(path));
                break;
            default:
                FAILWITH(IERR_INVALID_STATE);
        }
    }

    *p_path = path;
    iRETURN;
}

iERR _ion_extractor_evaluate_field_predicate(ION_READER *reader, ION_EXTRACTOR_PATH_COMPONENT *path_component, bool *matches) {
    iENTER;
    ION_STRING field_name;

    ASSERT(path_component->type == FIELD);

    IONCHECK(ion_reader_get_field_name(reader, &field_name));
    *matches = ION_STRING_EQUALS(&field_name, &path_component->value.text);
    iRETURN;
}

iERR _ion_extractor_evaluate_predicate(ION_READER *reader, ION_EXTRACTOR_PATH_COMPONENT *path_component, POSITION ordinal, bool *matches) {
    iENTER;

    ASSERT(reader);
    ASSERT(path_component);
    ASSERT(matches);

    *matches = false;

    switch (path_component->type) {
        case FIELD:
            IONCHECK(_ion_extractor_evaluate_field_predicate(reader, path_component, matches));
            break;
        case ORDINAL:
            *matches = ordinal == path_component->value.ordinal;
            break;
        case WILDCARD:
            // TODO different types of wildcards?
            *matches = true;
            break;
        default:
            FAILWITH(IERR_INVALID_STATE);
    }
    iRETURN;
}

iERR _ion_extractor_evaluate_predicates(ION_EXTRACTOR *extractor, ION_READER *reader, SIZE depth, POSITION ordinal,
                                        ION_EXTRACTOR_CONTROL *control,
                                        ION_EXTRACTOR_ACTIVE_PATH_MAP previous_depth_actives,
                                        ION_EXTRACTOR_ACTIVE_PATH_MAP *current_depth_actives) {
    iENTER;
    int8_t i;
    bool matches;
    ION_EXTRACTOR_PATH_COMPONENT *path_component;
    ION_EXTRACTOR_MATCHER *matcher;

    ASSERT(extractor);
    ASSERT(reader);
    ASSERT(current_depth_actives);
    // This depth should not have been stepped into if nothing matched at the previous depth.
    ASSERT(previous_depth_actives != 0);
    ASSERT(depth > 0);
    // NOTE: The following is not a user error because reaching this point requires an active path at this depth and
    // depths above the max path length are rejected at construction.
    ASSERT(depth <= extractor->options.max_path_length);

    for (i = 0; i < extractor->matchers_length; i++) {
        if (previous_depth_actives & (1 << i)) {
            path_component = ION_EXTRACTOR_GET_COMPONENT(extractor, depth - 1, i);
            IONCHECK(_ion_extractor_evaluate_predicate(reader, path_component, ordinal, &matches));
            if (matches) {
                if (path_component->is_terminal) {
                    matcher = &extractor->matchers[i];
                    matcher->callback(reader, matcher->path, matcher->user_context, control);
                    if (*control) {
                        SUCCEED(); // TODO do what? There are still more predicates to match for this value.
                    }
                }
                *current_depth_actives |= (1 << i);
            }
        }
    }

    iRETURN;
}

iERR _ion_extractor_match_helper(hEXTRACTOR extractor, ION_READER *reader, SIZE depth,
                                 ION_EXTRACTOR_ACTIVE_PATH_MAP previous_depth_actives) {
    iENTER;
    ION_TYPE t;
    POSITION ordinal = 0;
    ION_EXTRACTOR_CONTROL control = ion_extractor_control_next();
    ION_EXTRACTOR_ACTIVE_PATH_MAP current_depth_actives;

    for (;;) {
        IONCHECK(ion_reader_next(reader, &t));
        if (t == tid_EOF) {
            break;
        }
        // Each value at depth N can match any active partial path from depth N - 1.
        current_depth_actives = 0;
        if (depth > 0) {
            IONCHECK(_ion_extractor_evaluate_predicates(extractor, reader, depth, ordinal, &control,
                                                        previous_depth_actives, &current_depth_actives));
        }
        else {
            ASSERT(depth == 0);
            // Everything matches at depth 0.
            current_depth_actives = ION_EXTRACTOR_ALL_PATHS_ACTIVE;
        }
        // TODO do what with control?
        ordinal++;
        switch(ION_TYPE_INT(t)) {
            case tid_NULL_INT:
            case tid_BOOL_INT:
            case tid_INT_INT:
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
                if (current_depth_actives != 0) {
                    IONCHECK(ion_reader_step_in(reader));
                    IONCHECK(_ion_extractor_match_helper(extractor, reader, depth + 1, current_depth_actives));
                    IONCHECK(ion_reader_step_out(reader));
                }
                continue;
            default:
                FAILWITH(IERR_INVALID_STATE);
        }
    }
    iRETURN;
}


iERR ion_extractor_match(hEXTRACTOR extractor, hREADER reader) {
    iENTER;
    SIZE depth;

    if (extractor->_path_in_progress) {
        FAILWITHMSG(IERR_INVALID_STATE, "Cannot start matching with a path in progress.");
    }

    IONCHECK(ion_reader_get_depth(reader, &depth));
    if (depth != 0) {
        FAILWITHMSG(IERR_INVALID_STATE, "Reader must be at depth 0 to start matching.");
    }

    if (extractor->matchers_length) {
        IONCHECK(_ion_extractor_match_helper(extractor, reader, 0, 0));
    }
    iRETURN;
}
