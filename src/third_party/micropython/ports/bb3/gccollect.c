/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <setjmp.h>

#include "py/mpstate.h"
#include "py/gc.h"

#if MICROPY_ENABLE_GC

void gc_collect(void) {
  // void * python_stack_top = MP_STATE_THREAD(stack_top);
  // assert(python_stack_top != NULL);

  gc_collect_start();

  // /* get the registers.
  //  * regs is the also the last object on the stack so the stack is bound by
  //  * &regs and python_stack_top. */
  // jmp_buf regs;
  // setjmp(regs);

  // void **regs_ptr = (void**)&regs;

  // /* On the device, the stack is stored in reverse order, but it might not be
  //  * the case on a computer. We thus have to take the absolute value of the
  //  * addresses difference. */
  // size_t stackLengthInByte;
  // void ** scanStart;
  // if ((uintptr_t)python_stack_top > (uintptr_t)regs_ptr) {

  //   /* To compute the stack length:
  //    *                                  regs
  //    *                             <----------->
  //    * STACK ->  ...|  |  |  |  |  |--|--|--|--|  |  |  |  |  |  |
  //    *                             ^&regs                        ^python_stack_top
  //    * */

  //   stackLengthInByte = (uintptr_t)python_stack_top - (uintptr_t)regs_ptr;
  //   scanStart = regs_ptr;

  // } else {

  //   /* When computing the stack length, take into account regs' size.
  //    *                                                 regs
  //    *                                            <----------->
  //    * STACK ->  |  |  |  |  |  |  |  |  |  |  |  |--|--|--|--|  |  |  |...
  //    *           ^python_stack_top                ^&regs
  //    * */

  //   stackLengthInByte = (uintptr_t)regs_ptr - (uintptr_t)python_stack_top + sizeof(regs);
  //   scanStart = (void **)python_stack_top;

  // }
  // /* Memory error detectors might find an error here as they might split regs
  //  * and stack memory zones. */
  // gc_collect_root(scanStart, stackLengthInByte/sizeof(void *));

  gc_collect_end();
}

#endif //MICROPY_ENABLE_GC
