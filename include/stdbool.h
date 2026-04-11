//
// Created by sigsegv on 4/28/22.
//

#ifndef JEOKERNEL_STDBOOL_H
#define JEOKERNEL_STDBOOL_H

#ifndef __cplusplus
#if !defined(__bool_true_false_are_defined) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
typedef int bool;
#define true 1
#define false 0
#define __bool_true_false_are_defined 1
#endif
#endif


#endif //JEOKERNEL_STDBOOL_H
