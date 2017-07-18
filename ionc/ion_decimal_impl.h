/*
 * Copyright 2011-2016 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef IONC_ION_DECIMAL_IMPL_H
#define IONC_ION_DECIMAL_IMPL_H

#include "ion_types.h"

#ifdef DECNUMDIGITS
    #undef DECNUMDIGITS
#endif
#define DECNUMDIGITS DECQUAD_Pmax

#define ION_DECNUMBER_UNITS_SIZE(decimal_digits) \
     (sizeof(decNumberUnit) * (((decimal_digits / DECDPUN) + ((decimal_digits % DECDPUN) ? 1 : 0))))

#define ION_DECNUMBER_SIZE(decimal_digits) (sizeof(decNumber) + ION_DECNUMBER_UNITS_SIZE(decimal_digits))

#ifdef __cplusplus
extern "C" {
#endif

iERR _ion_decimal_number_alloc(void *owner, SIZE decimal_digits, decNumber **p_number);
iERR _ion_decimal_from_string_helper(char *str, decContext *context, hOWNER owner, decQuad *p_quad, decNumber **p_num);

#ifdef __cplusplus
}
#endif

#endif //IONC_ION_DECIMAL_IMPL_H
