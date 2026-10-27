cmake_minimum_required(VERSION 3.16)
project(chml C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_library(chml STATIC src/document.c src/style.c src/chml_io.c src/builder.c src/chml_policy.c)
target_include_directories(chml PUBLIC include)

add_library(chml_components STATIC examples/components/components.c)
target_link_libraries(chml_components PUBLIC chml)
target_include_directories(chml_components PUBLIC examples/components)

add_executable(chml_hello examples/hello-world/main.c)
target_link_libraries(chml_hello PRIVATE chml)

add_executable(chml_components_example examples/components/main.c)
target_link_libraries(chml_components_example PRIVATE chml_components)

add_executable(chml_ergonomics_test examples/ergonomics/main.c)
target_link_libraries(chml_ergonomics_test PRIVATE chml)
target_include_directories(chml_ergonomics_test PRIVATE src)
add_test(NAME chml_ergonomics_test COMMAND chml_ergonomics_test)
enable_testing()
add_executable(chml_style_test tests/style_test.c)
target_link_libraries(chml_style_test PRIVATE chml)
add_test(NAME chml_style_test COMMAND chml_style_test)

add_executable(chml_document_test tests/document_test.c)
target_link_libraries(chml_document_test PRIVATE chml)
add_test(NAME chml_document_test COMMAND chml_document_test)

add_executable(chml_serialization_test tests/serialization_test.c)
target_link_libraries(chml_serialization_test PRIVATE chml)
add_test(NAME chml_serialization_test COMMAND chml_serialization_test)

add_executable(chml_policy_test tests/policy_test.c)
target_link_libraries(chml_policy_test PRIVATE chml)
add_test(NAME chml_policy_test COMMAND chml_policy_test)

add_executable(chml_depth_test tests/depth_test.c)
target_link_libraries(chml_depth_test PRIVATE chml)
add_test(NAME chml_depth_test COMMAND chml_depth_test)

add_executable(chml_builder_test tests/builder_test.c)
target_link_libraries(chml_builder_test PRIVATE chml_components)
target_include_directories(chml_builder_test PRIVATE src)
add_test(NAME chml_builder_test COMMAND chml_builder_test)

add_executable(chml_atomic_write_test tests/atomic_write_test.c)
target_link_libraries(chml_atomic_write_test PRIVATE chml)
add_test(NAME chml_atomic_write_test COMMAND chml_atomic_write_test)

# Proves the replacement mechanism executed, not merely compiled.
add_executable(chml_atomic_identity_test tests/atomic_identity_test.c)
target_link_libraries(chml_atomic_identity_test PRIVATE chml)
target_include_directories(chml_atomic_identity_test PRIVATE src)
add_test(NAME chml_atomic_identity_test COMMAND chml_atomic_identity_test)

# Negative control: the same test linked against a deliberately non-atomic
# writer must fail. It compiles the needed sources directly so the broken
# implementation is the only definition of chml_write_file_atomic in the image.
add_executable(chml_atomic_identity_negative
    tests/atomic_identity_test.c
    src/document.c
    src/style.c
    src/chml_policy.c
    tests/support/chml_io_inplace.c)
target_include_directories(chml_atomic_identity_negative PRIVATE include src)
add_test(NAME chml_atomic_identity_negative COMMAND chml_atomic_identity_negative)
set_tests_properties(chml_atomic_identity_negative PROPERTIES WILL_FAIL TRUE)
