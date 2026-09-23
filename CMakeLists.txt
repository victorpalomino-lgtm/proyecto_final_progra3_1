cmake_minimum_required(VERSION 4.0)
project(proyecto_progra3_1)

set(CMAKE_CXX_STANDARD 20)
find_package(Threads REQUIRED)

# Cada carpeta corresponde a una responsabilidad de la entrega.
set(LIMPIEZA src/02_limpieza_normalizacion/DataCleaner.cpp)
set(EXPORTACION src/02_limpieza_normalizacion/CSVWriter.cpp)
set(ARBOL "src/03_arbol_insercion/MovieTrie(arbol).cpp")
set(BUSQUEDA src/04_busqueda/Busqueda.cpp)
set(INTERFAZ src/05_interfaz_integracion/Interfaz.cpp)
set(UTILIDADES src/comun/Utilidades.cpp)

add_executable(proyecto_progra3_1 main.cpp
        ${LIMPIEZA} ${EXPORTACION} ${ARBOL} ${BUSQUEDA} ${INTERFAZ} ${UTILIDADES})
target_include_directories(proyecto_progra3_1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(proyecto_progra3_1 PRIVATE Threads::Threads)

include(CTest)
if(BUILD_TESTING)
    add_executable(csv_reader_tests tests/CSVReaderTests.cpp ${LIMPIEZA} ${EXPORTACION})
    add_executable(data_cleaner_tests tests/DataCleanerTests.cpp ${LIMPIEZA})
    add_executable(csv_writer_tests tests/CSVWriterTests.cpp ${EXPORTACION})
    add_executable(interfaz_tests tests/InterfazTests.cpp
            ${BUSQUEDA} ${UTILIDADES} ${LIMPIEZA} ${ARBOL})

    foreach(test_target csv_reader_tests data_cleaner_tests csv_writer_tests interfaz_tests)
        target_include_directories(${test_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME ${test_target} COMMAND ${test_target})
    endforeach()
endif()
