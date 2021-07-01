include(../../../../../toolchain.cmake)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DACPI_MACHINE_WIDTH=64 -D_LINUX -DACPI_USE_SYSTEM_CLIBRARY -DACPI_DISASSEMBLER -DACPI_ASL_COMPILER")

#_d=`mktemp -d $(OBJDIR)/$(1).XXXXXX`
function(yacc_invoke inputfile inputpath output destination)
    execute_process(COMMAND mktemp -d "${CMAKE_CURRENT_BINARY_DIR}/${inputfile}.XXXXXX" OUTPUT_VARIABLE yaccdir)
    string(REGEX REPLACE "\n$" "" yaccdir "${yaccdir}")
    execute_process(COMMAND /bin/sh -c "cd \"${yaccdir}\"; bison -y -v -d -pAslCompiler \"${inputpath}/${inputfile}\"; ls -lah" OUTPUT_VARIABLE result)
    message("${result}")
    execute_process(COMMAND mv -v "${yaccdir}/${output}" "${destination}" OUTPUT_VARIABLE result)
    message("${result}")
    execute_process(COMMAND rm -rf "${yaccdir}")
endfunction()

execute_process(COMMAND /bin/sh -c "m4 -P \"-I${CMAKE_CURRENT_SOURCE_DIR}\" \"${CMAKE_CURRENT_SOURCE_DIR}/aslparser.y\" > \"${CMAKE_CURRENT_BINARY_DIR}/aslcompiler.y\"")

set(CMAKE_CXX_STANDARD 20)

AUX_SOURCE_DIRECTORY(. C_SOURCES)

yacc_invoke(aslcompiler.y "${CMAKE_CURRENT_BINARY_DIR}" "y.tab.h" "${CMAKE_CURRENT_BINARY_DIR}/aslcompiler.y.h")
yacc_invoke(dtparser.y "${CMAKE_CURRENT_SOURCE_DIR}" "y.tab.h" "${CMAKE_CURRENT_BINARY_DIR}/dtparser.y.h")

add_library(acpica-compiler OBJECT ${C_SOURCES})

target_include_directories(acpica-compiler PRIVATE ../include)
target_include_directories(acpica-compiler PRIVATE ../../../../../../include)
target_include_directories(acpica-compiler PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
