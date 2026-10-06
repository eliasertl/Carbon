# A settings window generated from one struct with CarbonReflection, on the first backend that is built, as
# Examples/<Backend>/Reflection. The target name keeps it apart from the library.
list(GET CARBON_EXAMPLE_BACKENDS 0 backend)
add_executable(ReflectionExample Main.cpp)
target_link_libraries(ReflectionExample PRIVATE CarbonExample${backend} Carbon::Reflection)
carbon_configure_example(ReflectionExample ${backend} Reflection)
