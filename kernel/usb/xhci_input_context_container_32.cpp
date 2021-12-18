//
// Created by sigsegv on 12/18/21.
//

#include "xhci_input_context_container_32.h"

xhci_input_context_container_32::xhci_input_context_container_32() :
    page(sizeof(*inputctx)),
    inputctx(new (page.Pointer()) xhci_input_context())
{
}

xhci_input_context_container_32::~xhci_input_context_container_32() {
    inputctx->~xhci_input_context();
}

xhci_input_context *xhci_input_context_container_32::InputContext() const {
    return inputctx;
}

uint64_t xhci_input_context_container_32::InputContextPhys() const {
    return page.PhysAddr();
}
