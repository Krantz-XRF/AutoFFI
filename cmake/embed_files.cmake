function(target_embed_files targ)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "HEADER;NAMESPACE" "INPUT")
  # Generate Header
  set(result_contents "namespace ${ARG_NAMESPACE} {\n")
  foreach(file_name IN LISTS ARG_INPUT)
    string(MAKE_C_IDENTIFIER ${file_name} file_id)
    string(APPEND result_contents "extern const char ${file_id}[]\;\n")
  endforeach()
  string(APPEND result_contents "}\n")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${ARG_HEADER}" ${result_contents})
  # Generate Sources
  foreach(file_name IN LISTS ARG_INPUT)
    file(READ ${file_name} file_contents)
    string(MAKE_C_IDENTIFIER ${file_name} file_id)
    string(PREPEND file_contents
      "#include \"${ARG_HEADER}\"\n"
      "const char ${ARG_NAMESPACE}::${file_id}[] = R\"~raw_contents~(")
    string(APPEND file_contents ")~raw_contents~\"\;\n")
    set(output_path "${CMAKE_CURRENT_BINARY_DIR}/${file_name}.cpp")
    file(WRITE ${output_path} ${file_contents})
    target_sources(${targ} PRIVATE ${output_path})
    target_include_directories(${targ} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  endforeach()
endfunction()
