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

#ifndef ION_EXTRACTOR_IMPL_H
#define ION_EXTRACTOR_IMPL_H

#include "ion_internal.h"
#include "ion_extractor.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A path for the extractor to match.
 */
struct _ion_extractor_path_descriptor {
    /**
     * A unique identifier for this path.
     */
    ION_EXTRACTOR_SIZE path_id;

    /**
     * The number of components in the path, i.e. the length of `path_components`.
     */
    ION_EXTRACTOR_SIZE path_length;

    ION_EXTRACTOR *extractor;

};

/**
 * A path component type.
 */
typedef enum _ion_extractor_path_component_type {
    /**
     * This component provides text which must exactly match a field name within the struct at this component's
     * depth in the path.
     */
    FIELD       = 0,

    /**
     * This component provides an ordinal, which must exactly match an index in the collection at this component's
     * depth in the path.
     */
    ORDINAL     = 1,

    /**
     * This component provides text which matches any value at this component's depth in the path.
     */
    WILDCARD    = 2,

} ION_EXTRACTOR_PATH_COMPONENT_TYPE;

/**
 * A path component, which can represent a particular field, ordinal, or wildcard.
 */
typedef struct _ion_extractor_path_component {
    /**
     * Zero (FALSE) if there are more components in the path; non-zero (TRUE) if this is the last component in the path.
     * If this component is terminal and it matches the current value, the matcher's callback should be
     * invoked. If it is not terminal, but it matches the current element, then the matcher should remain active
     * at this depth. If this component doesn't match the current element, it should be marked inactive.
     *
     * NOTE: it is possible to calculate whether a component is terminal (TRUE if the component's depth is equal to the
     * matcher's path's length), but storing it may be cheaper, as calculating it would require accessing the matcher's
     * path's length in a disparate memory location each time a component is accessed.
     */
    bool is_terminal;

    /**
     * The type of the component: FIELD, ORDINAL, or WILDCARD.
     */
    ION_EXTRACTOR_PATH_COMPONENT_TYPE type;

    /**
     * The value of the component. If the component's type is FIELD or WILDCARD, `text` must be valid.
     * If the component's type is ORDINAL, `ordinal` must be valid.
     */
    union {
        ION_STRING  text;
        POSITION    ordinal;
    } value;

} ION_EXTRACTOR_PATH_COMPONENT;

/**
 * Stores the data needed to convey a match to the user. One ION_EXTRACTOR_MATCHER is created per path.
 *
 * NOTE: the user retains memory ownership of `callback` and `user_context`.
 */
typedef struct _ion_extractor_matcher {
    /**
     * The path to match.
     */
    ION_EXTRACTOR_PATH_DESCRIPTOR *path;

    /**
     * The callback to invoke when the path matches.
     */
    ION_EXTRACTOR_CALLBACK callback;

    /**
     * The opaque user context to provide to the callback upon match.
     */
    void *user_context;

} ION_EXTRACTOR_MATCHER;

struct _ion_extractor {

    /**
     * The configuration options.
     */
    ION_EXTRACTOR_OPTIONS options;

    /**
     * TRUE if the user has started, but not finished, a path. When TRUE, the user cannot start another path or start
     * matching.
     */
    bool _path_in_progress;

    /**
     * The length of the current path. Only valid when `_path_in_progress` is TRUE.
     */
    ION_EXTRACTOR_SIZE _cur_path_len;

    /**
     * Path components from all registered paths organized by depth. Components at depth 1 begin at index 0, components
     * at depth 2 begin at index ION_EXTRACTOR_MAX_NUM_PATHS, and so on. There is a maximum of
     * ION_EXTRACTOR_MAX_PATH_LENGTH depths. This organization mimics access order: when determining matches
     * at depth N, all partial paths that matched at depth N - 1 must have their components at depth N accessed and
     * tested.
     */
    ION_EXTRACTOR_PATH_COMPONENT path_components[ION_EXTRACTOR_MAX_PATH_LENGTH * ION_EXTRACTOR_MAX_NUM_PATHS];

    /**
     * The number of valid elements in `matchers`.
     */
    ION_EXTRACTOR_SIZE matchers_length;

    /**
     * A matcher for a particular path, indexed by path ID.
     */
    ION_EXTRACTOR_MATCHER matchers[ION_EXTRACTOR_MAX_NUM_PATHS];

};

#ifdef __cplusplus
}
#endif

#endif //ION_EXTRACTOR_IMPL_H
