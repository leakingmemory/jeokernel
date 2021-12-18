//
// Created by sigsegv on 12/18/21.
//

#ifndef JEOKERNEL_XHCI_INPUT_CONTEXT_CONTAINER_32_H
#define JEOKERNEL_XHCI_INPUT_CONTEXT_CONTAINER_32_H

#include "Phys32Page.h"
#include "xhci.h"

class xhci_input_context_container_32 : public xhci_input_context_container {
private:
    Phys32Page page;
    xhci_input_context *inputctx;
public:
    xhci_input_context_container_32();
    ~xhci_input_context_container_32() override;
    xhci_input_context *InputContext() const override;
    uint64_t InputContextPhys() const override;
};


#endif //JEOKERNEL_XHCI_INPUT_CONTEXT_CONTAINER_32_H
