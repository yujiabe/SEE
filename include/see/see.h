#ifndef _SEE_h_see_
#define _SEE_h_see_

/*
 * Although every header file is autonomous (or handles its own dependencies),
 * this header file conveniently includes them all.
 */

#ifdef __cplusplus
extern "C" {
#endif /* C++ */

#include "config.h"

#include "type.h"
#include "value.h"
#include "object.h"
#include "native.h"
#include "cfunction.h"
#include "debug.h"
#include "eval.h"
#include "error.h"
#include "input.h"
#include "intern.h"
#include "interpreter.h"
#include "mem.h"
#include "no.h"
#include "string.h"
#include "try.h"

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _SEE_h_see_ */
