include(../../../../../toolchain.cmake)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DACPI_MACHINE_WIDTH=64 -D_LINUX -DACPI_USE_SYSTEM_CLIBRARY -DACPI_DISASSEMBLER -DACPI_ASL_COMPILER")

#_d=`mktemp -d $(OBJDIR)/$(1).XXXXXX`
function(yacc_invoke inputfile inputpath output destination prefix)
    execute_process(COMMAND mktemp -d "${CMAKE_CURRENT_BINARY_DIR}/${inputfile}.XXXXXX" OUTPUT_VARIABLE yaccdir)
    string(REGEX REPLACE "\n$" "" yaccdir "${yaccdir}")
    execute_process(COMMAND /bin/sh -c "cd \"${yaccdir}\"; bison -y -v -d -p${prefix} \"${inputpath}/${inputfile}\"; ls -lah" OUTPUT_VARIABLE result)
    message("${result}")
    execute_process(COMMAND mv -v "${yaccdir}/${output}" "${destination}" OUTPUT_VARIABLE result)
    message("${result}")
    execute_process(COMMAND rm -rf "${yaccdir}")
endfunction()

function(flex_invoke inputfile destination prefix)
    execute_process(COMMAND flex -i -s "-P${prefix}" "-o${destination}" "${CMAKE_CURRENT_SOURCE_DIR}/${inputfile}" OUTPUT_VARIABLE result)
    message("${result}")
endfunction()

execute_process(COMMAND /bin/sh -c "m4 -P \"-I${CMAKE_CURRENT_SOURCE_DIR}\" \"${CMAKE_CURRENT_SOURCE_DIR}/aslparser.y\" > \"${CMAKE_CURRENT_BINARY_DIR}/aslcompiler.y\"")

set(CMAKE_CXX_STANDARD 20)

AUX_SOURCE_DIRECTORY(. C_SOURCES)

list(REMOVE_ITEM C_SOURCES ./aslstubs.c)

yacc_invoke(aslcompiler.y "${CMAKE_CURRENT_BINARY_DIR}" "y.tab.h" "${CMAKE_CURRENT_BINARY_DIR}/aslcompiler.y.h" "AslCompiler")
yacc_invoke(aslcompiler.y "${CMAKE_CURRENT_BINARY_DIR}" "y.tab.c" "${CMAKE_CURRENT_BINARY_DIR}/aslcompilerparse.c" "AslCompiler")
yacc_invoke(dtparser.y "${CMAKE_CURRENT_SOURCE_DIR}" "y.tab.h" "${CMAKE_CURRENT_BINARY_DIR}/dtparser.y.h" "DtParser")
yacc_invoke(dtparser.y "${CMAKE_CURRENT_SOURCE_DIR}" "y.tab.c" "${CMAKE_CURRENT_BINARY_DIR}/dtparserparse.c" "DtParser")
yacc_invoke(dtcompilerparser.y "${CMAKE_CURRENT_SOURCE_DIR}" "y.tab.h" "${CMAKE_CURRENT_BINARY_DIR}/dtcompilerparser.y.h" "DtCompilerParser")
yacc_invoke(dtcompilerparser.y "${CMAKE_CURRENT_SOURCE_DIR}" "y.tab.c" "${CMAKE_CURRENT_BINARY_DIR}/dtcompilerparserparse.c" "DtCompilerParser")
yacc_invoke(prparser.y "${CMAKE_CURRENT_SOURCE_DIR}" "y.tab.h" "${CMAKE_CURRENT_BINARY_DIR}/prparser.y.h" "PrParser")
yacc_invoke(prparser.y "${CMAKE_CURRENT_SOURCE_DIR}" "y.tab.c" "${CMAKE_CURRENT_BINARY_DIR}/prparserparse.c" "PrParser")

flex_invoke(aslcompiler.l "${CMAKE_CURRENT_BINARY_DIR}/aslcompilerlex.c" "AslCompiler")
flex_invoke(dtparser.l "${CMAKE_CURRENT_BINARY_DIR}/dtparserlex.c" "DtParser")
flex_invoke(dtcompilerparser.l "${CMAKE_CURRENT_BINARY_DIR}/dtcompilerparserlex.c" "DtCompilerParser")
flex_invoke(prparser.l "${CMAKE_CURRENT_BINARY_DIR}/prparserlex.c" "PrParser")

add_library(acpica-compiler OBJECT ${C_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/aslcompilerparse.c ${CMAKE_CURRENT_BINARY_DIR}/dtparserparse.c ${CMAKE_CURRENT_BINARY_DIR}/dtcompilerparserparse.c ${CMAKE_CURRENT_BINARY_DIR}/prparserparse.c ${CMAKE_CURRENT_BINARY_DIR}/aslcompilerlex.c ${CMAKE_CURRENT_BINARY_DIR}/dtparserlex.c ${CMAKE_CURRENT_BINARY_DIR}/dtcompilerparserlex.c ${CMAKE_CURRENT_BINARY_DIR}/prparserlex.c)

target_include_directories(acpica-compiler PRIVATE ../include)
target_include_directories(acpica-compiler PRIVATE ../../../../../../include)
target_include_directories(acpica-compiler PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_include_directories(acpica-compiler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
