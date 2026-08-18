foreach(example IN ITEMS basic async custom_sink)
  add_executable(zest_${example} ${example}.cpp)
  target_link_libraries(zest_${example} PRIVATE zest::zest)
  zest_set_warnings(zest_${example})
endforeach()

# C++20 module consumer (`import zest;`) — only when the module target is on.
if(ZEST_BUILD_MODULE)
  add_executable(zest_module_import module_import.cpp)
  target_link_libraries(zest_module_import PRIVATE zest::module)
  target_compile_features(zest_module_import PRIVATE cxx_std_20)
  zest_set_warnings(zest_module_import)
endif()
