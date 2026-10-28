# ODB 代码生成辅助函数
#
# 作用：把 ODB 实体头文件编译成 user-odb.cxx/.hxx/.ixx 与建表 SQL，
# 生成目录放在当前子目录的 build 目录（generated/），不污染源码目录。
#
# 用法：
#   im_add_odb_lib(im_odb
#       ODB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/odb
#       FILES user.hxx)
#
# 参数：
#   target    —— 生成的静态库名
#   ODB_DIR   —— 实体头文件目录（同时作为 odb 的 -I 路径）
#   FILES     —— 相对 ODB_DIR 的实体文件列表
function(im_add_odb_lib target)
    cmake_parse_arguments(ARG "" "ODB_DIR" "FILES" ${ARGN})

    if(NOT ARG_ODB_DIR)
        set(ARG_ODB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/odb)
    endif()

    find_program(ODB_COMPILER odb REQUIRED)

    set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
    file(MAKE_DIRECTORY ${gen_dir})

    set(srcs)
    foreach(f ${ARG_FILES})
        get_filename_component(base "${f}" NAME_WE)
        set(cxx ${gen_dir}/${base}-odb.cxx)

        # -o 指定输出目录，避免生成文件落回源码目录
        # --profile boost/date-time 用于 boost::posix_time::ptime 的映射
        add_custom_command(
            OUTPUT ${cxx}
            COMMAND ${ODB_COMPILER}
            ARGS -d mysql
                 --std c++11
                 --generate-query
                 --generate-schema
                 --profile boost/date-time
                 -I ${ARG_ODB_DIR}
                 -o ${gen_dir}
                 ${ARG_ODB_DIR}/${f}
            DEPENDS ${ARG_ODB_DIR}/${f}
            COMMENT "odb ${f}"
            VERBATIM
        )
        list(APPEND srcs ${cxx})
    endforeach()

    add_library(${target} ${srcs})
    target_include_directories(${target} PUBLIC ${gen_dir} ${ARG_ODB_DIR})
    target_link_libraries(${target} PUBLIC
        PkgConfig::ODB_MYSQL
        PkgConfig::ODB_BOOST
        Boost::date_time
    )
endfunction()
