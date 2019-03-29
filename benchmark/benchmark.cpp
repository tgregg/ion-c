
#include <inc/ion_errors.h>
#include <inc/ion.h>
#include <cstdlib>
#include <ion_helpers.h>

iERR read_all(hREADER reader, long *count) {
    iENTER;
    BOOL in_struct = FALSE;
    int depth;
    ION_TYPE t;
    BOOL is_null;
    SIZE annotation_count;
    ION_STRING field_name;
    ION_STRING annotations[10];
    BOOL bool_value;
    int64_t int_value;
    double float_value;
    ION_DECIMAL decimal_value;
    ION_TIMESTAMP timestamp_value;
    ION_STRING string_value;

    for (;;) {
        IONCHECK(ion_reader_next(reader, &t));
        if (t == tid_EOF) {
            IONCHECK(ion_reader_get_depth(reader, &depth));
            if (depth == 0) {
                break;
            }
            IONCHECK(ion_reader_step_out(reader));
            continue;
        }
        *count += 1;
        IONCHECK(ion_reader_is_in_struct(reader, &in_struct));
        if (in_struct) {
            IONCHECK(ion_reader_get_field_name(reader, &field_name));
        }

        IONCHECK(ion_reader_get_annotation_count(reader, &annotation_count));
        if (annotation_count > 0) {
            IONCHECK(ion_reader_get_annotations(reader, annotations, annotation_count, &annotation_count));
        }

        IONCHECK(ion_reader_is_null(reader, &is_null));
        if (is_null) {
            IONCHECK(ion_reader_read_null(reader, &t));
            continue;
        }
        switch (ION_TYPE_INT(t)) {
            case tid_BOOL_INT:
                IONCHECK(ion_reader_read_bool(reader, &bool_value));
                break;
            case tid_INT_INT:
                IONCHECK(ion_reader_read_int64(reader, &int_value));
                break;
            case tid_FLOAT_INT:
                IONCHECK(ion_reader_read_double(reader, &float_value));
                break;
            case tid_DECIMAL_INT:
                IONCHECK(ion_reader_read_ion_decimal(reader, &decimal_value));
                break;
            case tid_TIMESTAMP_INT:
                IONCHECK(ion_reader_read_timestamp(reader, &timestamp_value));
                break;
            case tid_SYMBOL_INT: // intentional fall-through
            case tid_STRING_INT:
                IONCHECK(ion_reader_read_string(reader, &string_value));
                break;
            case tid_CLOB_INT: // intentional fall-through
            case tid_BLOB_INT: {
                //Skipping. Files being tested have no blobs or clobs. Don't want to set up byte buffers.
                break;
            }
            case tid_SEXP_INT: // intentional fall-through
            case tid_LIST_INT: // intentional fall-through
            case tid_STRUCT_INT:
                IONCHECK(ion_reader_step_in(reader));
                break;
            default: FAILWITHMSG(IERR_INVALID_STATE, "Unknown Ion type.");
        }
    }
    iRETURN;
}

int main(int argc, char **argv)
{
    iENTER;
    hREADER reader;
    ION_STREAM *stream;
    ION_READER_OPTIONS options;
    FILE *file;
    long count = 0;
    if (argc != 2) {
        FAILWITHMSG(IERR_INVALID_ARG, "Filename required.");
    }
    file = fopen(argv[1], "rb");
    memset(&options, 0, sizeof(ION_READER_OPTIONS));
    IONCHECK(ion_stream_open_file_in(file, &stream));
    IONCHECK(ion_reader_open(&reader, stream, &options));
    IONCHECK(read_all(reader, &count));
    IONCHECK(ion_reader_close(reader));
    IONCHECK(ion_stream_close(stream));
    printf("%ld\n", count);
    iRETURN;
}
