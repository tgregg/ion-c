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

#ifndef IONC_VALUE_STREAM_H
#define IONC_VALUE_STREAM_H

#include <vector>
#include <string>
#include "ion.h"

typedef ION_STRING ION_LOB;

typedef enum _reader_input_type {
    STREAM = 0,
    BUFFER
} READER_INPUT_TYPE;

typedef enum _ion_event_type {
    SCALAR = 0,
    CONTAINER_START,
    CONTAINER_END,
    STREAM_END
} ION_EVENT_TYPE;

typedef enum _vector_test_type {
    READ = 0,
    ROUNDTRIP_TEXT,
    ROUNDTRIP_BINARY
} VECTOR_TEST_TYPE;

class IonEventBase {
public:
    ION_EVENT_TYPE event_type;
    ION_TYPE ion_type;
    ION_STRING *field_name;
    ION_STRING **annotations;
    SIZE num_annotations;
    int depth;
    void *value;

    IonEventBase(ION_EVENT_TYPE event_type, ION_TYPE ion_type, ION_STRING *field_name, ION_STRING **annotations, SIZE num_annotations, int depth) {
        this->event_type = event_type;
        this->ion_type = ion_type;
        this->field_name = field_name;
        this->annotations = annotations;
        this->num_annotations = num_annotations;
        this->depth = depth;
        value = NULL;
    }
};

class IonEventStream {
    std::vector<IonEventBase*> *event_stream;
public:
    IonEventStream();
    ~IonEventStream();
    IonEventBase *append_new(ION_EVENT_TYPE event_type, ION_TYPE ion_type, ION_STRING *field_name,
                             ION_STRING **annotations, SIZE num_annotations, int depth);

    size_t size() {
        return event_stream->size();
    }

    IonEventBase *at(size_t index) {
        return event_stream->at(index);
    }
};


typedef iERR (*LOOP_FN)(hREADER r, IonEventStream *s);

iERR read_all(hREADER hreader, IonEventStream *stream);
iERR read_value_stream_from_string(const char *ion_string, IonEventStream *stream);
iERR read_value_stream(IonEventStream *stream, READER_INPUT_TYPE input_type, std::string pathname, LOOP_FN fn, std::string fn_name, ION_CATALOG *catalog);
iERR write_value_stream(IonEventStream *stream, VECTOR_TEST_TYPE test_type, ION_CATALOG *catalog, BYTE **out);

#endif //IONC_VALUE_STREAM_H
