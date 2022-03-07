//
// Created by sigsegv on 3/6/22.
//

#ifndef JEOKERNEL_SCSIERROR_H
#define JEOKERNEL_SCSIERROR_H

enum class ScsiCmdNonSuccessfulStatus {
    UNSPECIFIED,
    SIGNATURE_CHECK,
    COMMAND_FAILED,
    PHASE_ERROR,
    OTHER_ERROR
};

#endif //JEOKERNEL_SCSIERROR_H
