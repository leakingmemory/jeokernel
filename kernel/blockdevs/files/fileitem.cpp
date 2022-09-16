//
// Created by sigsegv on 9/16/22.
//

#include <files/fileitem.h>

std::string text(fileitem_status status) {
    switch (status) {
        case fileitem_status::SUCCESS:
            return "Success";
        case fileitem_status::IO_ERROR:
            return "Input/output error";
        case fileitem_status::INTEGRITY_ERROR:
            return "Filesystem itegrity error";
        case fileitem_status::NOT_SUPPORTED_FS_FEATURE:
            return "Feature not supported";
        case fileitem_status::INVALID_REQUEST:
            return "Invalid request to filesystem";
        default:
            return "Invalid status code";
    }
}
