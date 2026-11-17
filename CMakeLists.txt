cmake_minimum_required(VERSION 4.0)
project(proyecto_progra3_1)

set(CMAKE_CXX_STANDARD 20)
find_package(Threads REQUIRED)
# MovieTrie y las busquedas usan std::thread: todos los ejecutables enlazan Threads.
link_libraries(Threads::Threads)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()
# Cada carpeta corresponde a una responsabilidad de la entrega.
set(LIMPIEZA src/02_limpieza_normalizacion/DataCleaner.cpp)
set(EXPORTACION src/02_limpieza_normalizacion/CSVWriter.cpp)
set(ARBOL "src/03_arbol_insercion/MovieTrie(arbol).cpp"
        "src/03_arbol_insercion/IndiceGlobal.cpp")
set(BUSQUEDA src/04_busqueda/Busqueda.cpp src/04_busqueda/BusquedaComun.cpp
        src/04_busqueda/BuscadorPeliculas.cpp src/04_busqueda/DecoradorBusqueda.cpp)
set(INTERFAZ src/05_interfaz_integracion/Interfaz.cpp src/05_interfaz_integracion/ListasUsuario.cpp
        src/05_interfaz_integracion/Recomendador.cpp)
set(UTILIDADES src/comun/Utilidades.cpp)

add_executable(proyecto_progra3_1 main.cpp
        ${LIMPIEZA} ${EXPORTACION} ${ARBOL} ${BUSQUEDA} ${INTERFAZ} ${UTILIDADES})
target_include_directories(proyecto_progra3_1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Benchmark de indexacion y busqueda (no es una prueba de ctest).
add_executable(benchmark bench/Benchmark.cpp ${LIMPIEZA} ${ARBOL} ${BUSQUEDA} ${UTILIDADES})
target_include_directories(benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

include(CTest)
if(BUILD_TESTING)
    add_executable(csv_reader_tests tests/CSVReaderTests.cpp ${LIMPIEZA} ${EXPORTACION})
    add_executable(data_cleaner_tests tests/DataCleanerTests.cpp ${LIMPIEZA})
    add_executable(csv_writer_tests tests/CSVWriterTests.cpp ${EXPORTACION})
    add_executable(interfaz_tests tests/InterfazTests.cpp
            ${BUSQUEDA} ${UTILIDADES} ${LIMPIEZA} ${ARBOL})
    add_executable(indice_global_tests tests/IndiceGlobalTests.cpp ${LIMPIEZA} ${ARBOL})
    add_executable(listas_usuario_tests tests/ListasUsuarioTests.cpp
            src/05_interfaz_integracion/ListasUsuario.cpp src/05_interfaz_integracion/Recomendador.cpp
            ${BUSQUEDA} ${UTILIDADES} ${LIMPIEZA} ${ARBOL})
    add_executable(patrones_3_4_tests tests/Patrones34Tests.cpp
            src/05_interfaz_integracion/ListasUsuario.cpp
            ${BUSQUEDA} ${UTILIDADES} ${LIMPIEZA} ${ARBOL})
    add_executable(paralelo_tests tests/ParaleloTests.cpp ${BUSQUEDA} ${UTILIDADES} ${LIMPIEZA} ${ARBOL})

    foreach(test_target csv_reader_tests data_cleaner_tests csv_writer_tests interfaz_tests
            indice_global_tests listas_usuario_tests patrones_3_4_tests paralelo_tests)
        target_include_directories(${test_target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME ${test_target} COMMAND ${test_target})
    endforeach()
    # Solo lee data/movies_clean.csv (si no existe, omite esa parte): se corre desde la raiz.
    set_tests_properties(paralelo_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()