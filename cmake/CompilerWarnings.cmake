# Applies a consistent set of warnings to the given target.
#
# Usage:
#   zest_set_warnings(my_target)
#
# Honors the ZEST_WARNINGS_AS_ERRORS option to promote warnings to errors.
function(zest_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  endif()

  if(ZEST_WARNINGS_AS_ERRORS)
    if(MSVC)
      target_compile_options(${target} PRIVATE /WX)
    else()
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
