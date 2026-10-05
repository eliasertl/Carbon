# A settings window generated from one struct with CarbonReflection.
add_executable(ReflectionExample Main.cpp)
# The executable is called Reflection, like the folder; the target name keeps it apart from the library.
set_target_properties(ReflectionExample PROPERTIES OUTPUT_NAME Reflection)
target_link_libraries(ReflectionExample PRIVATE CarbonExampleCommon Carbon::Reflection)
carbon_configure_target(ReflectionExample)
carbon_copy_dawn_runtime(ReflectionExample)
