include_guard(GLOBAL)

function(aetheris_configure_target target_name)
  target_compile_features(${target_name} PUBLIC cxx_std_23)
  set_target_properties(
    ${target_name}
    PROPERTIES CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)

  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive- /EHsc)
  else()
    target_compile_options(
      ${target_name}
      PRIVATE -Wall
              -Wextra
              -Wpedantic
              -Wconversion
              -Wsign-conversion
              -Wshadow
              -Wold-style-cast
              -Wnon-virtual-dtor)
  endif()

  if(AETHERIS_ENABLE_STATIC_ANALYSIS)
    find_program(AETHERIS_CLANG_TIDY_EXE NAMES clang-tidy)
    if(AETHERIS_CLANG_TIDY_EXE)
      set_target_properties(${target_name} PROPERTIES CXX_CLANG_TIDY
                                                       "${AETHERIS_CLANG_TIDY_EXE}")
    endif()
  endif()
endfunction()
