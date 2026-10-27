# protoc 代码生成辅助函数
#
# 作用：把一组 .proto 编译成 C++ 源码，并打成一个可供多个 target 链接的库。
# 生成的 .pb.cc/.pb.h 放在当前子目录的 build 目录下（generated/），
# 不污染源码目录；.proto 变更时会自动重新生成。
#
# 用法：
#   im_add_proto_lib(im_proto
#       PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto
#       PROTOS base.proto user.proto)
#
# 参数：
#   target      —— 生成的静态库名
#   PROTO_DIR   —— .proto 所在目录（同时作为 protoc 的 -I 路径）
#   PROTOS      —— 相对 PROTO_DIR 的文件名列表
function(im_add_proto_lib target)
    cmake_parse_arguments(ARG "" "PROTO_DIR" "PROTOS" ${ARGN})

    if(NOT ARG_PROTO_DIR)
        set(ARG_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
    endif()

    set(gen_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
    file(MAKE_DIRECTORY ${gen_dir})

    set(srcs)
    foreach(p ${ARG_PROTOS})
        get_filename_component(abs "${ARG_PROTO_DIR}/${p}" ABSOLUTE)
        get_filename_component(base "${p}" NAME_WE)
        set(cc ${gen_dir}/${base}.pb.cc)

        # --experimental_allow_proto3_optional：兼容 proto3 的 optional 字段
        add_custom_command(
            OUTPUT ${cc}
            COMMAND protobuf::protoc
            ARGS --cpp_out=${gen_dir}
                 -I ${ARG_PROTO_DIR}
                 --experimental_allow_proto3_optional
                 ${abs}
            DEPENDS ${abs}
            COMMENT "protoc ${p}"
            VERBATIM
        )
        list(APPEND srcs ${cc})
    endforeach()

    add_library(${target} ${srcs})
    target_include_directories(${target} PUBLIC ${gen_dir})
    target_link_libraries(${target} PUBLIC protobuf::libprotobuf)
endfunction()
