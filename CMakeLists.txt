cmake_minimum_required(VERSION 4.0)
project(proyecto_progra3_1)

set(CMAKE_CXX_STANDARD 20)


find_package(Threads REQUIRED)


add_executable(proyecto_progra3_1 main.cpp DataCleaner.cpp CSVWriter.cpp
        "MovieTrie(arbol).cpp"
        "MovieTrie(arbol).h"
        Interfaz.cpp
        Busqueda.cpp
        Utilidades.cpp)

target_link_libraries(proyecto_progra3_1 PRIVATE Threads::Threads)


include(CTest)
if(BUILD_TESTING)
    add_executable(data_cleaner_tests tests/DataCleanerTests.cpp DataCleaner.cpp)
    target_include_directories(data_cleaner_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME data_cleaner_tests COMMAND data_cleaner_tests)
    add_executable(csv_writer_tests tests/CSVWriterTests.cpp CSVWriter.cpp
            CSVWriter.cpp)
    target_include_directories(csv_writer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME csv_writer_tests COMMAND csv_writer_tests)
    add_executable(interfaz_tests tests/InterfazTests.cpp Busqueda.cpp Utilidades.cpp
            DataCleaner.cpp "MovieTrie(arbol).cpp")
    target_include_directories(interfaz_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME interfaz_tests COMMAND interfaz_tests)
endif()
