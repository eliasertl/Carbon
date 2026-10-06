# A settings window generated from one struct with CarbonReflection, on the first backend that is built.
list(GET CARBON_EXAMPLE_BACKENDS 0 backend)
add_executable(ReflectionExample Main.cpp)
# The executable is called Reflection, like the folder; the target name keeps it apart from the library.
set_target_properties(ReflectionExample PROPERTIES OUTPUT_NAME Reflection)
target_link_libraries(ReflectionExample PRIVATE CarbonExample${backend} Carbon::Reflection)
carbon_configure_example(ReflectionExample ${backend})
