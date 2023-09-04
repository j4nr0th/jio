add_executable(jio_test_csv
#        ../iobase.c ../iocsv.c ../ioerr.c
        csv/simple_csv_test.c)
configure_file(csv/simple.csv "${CMAKE_BINARY_DIR}/csv_test_simple.csv" COPYONLY)
target_link_libraries(jio_test_csv PRIVATE jio)
add_test(NAME csv_test COMMAND jio_test_csv WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(jio_test_edit_csv
#        ../iobase.c ../iocsv.c ../ioerr.c
        csv/edit_csv_test.c)
configure_file(csv/simple.csv "${CMAKE_BINARY_DIR}/csv_test_simple.csv" COPYONLY)
target_link_libraries(jio_test_edit_csv PRIVATE jio)
add_test(NAME csv_edit_test COMMAND jio_test_edit_csv WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(jio_test_ugly_csv
#        ../iobase.c ../iocsv.c ../ioerr.c
        csv/ugly_csv_test.c)
configure_file(csv/ugly.csv "${CMAKE_BINARY_DIR}/csv_test_ugly.csv" COPYONLY)
configure_file(csv/ugly2.csv "${CMAKE_BINARY_DIR}/csv_test_ugly2.csv" COPYONLY)
target_link_libraries(jio_test_ugly_csv PRIVATE jio)
add_test(NAME csv_ugly_test COMMAND jio_test_ugly_csv WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(jio_test_simple_cfg
#        ../iobase.c ../ioerr.c ../iocfg.c
        cfg/simple_cfg.c)
configure_file(cfg/simple.ini "${CMAKE_BINARY_DIR}/ini_test_simple.ini" COPYONLY)
target_link_libraries(jio_test_simple_cfg PRIVATE jio)
add_test(NAME cfg_simple_test COMMAND jio_test_simple_cfg WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(jio_test_line_counting
#        ../iobase.c ../ioerr.c
        base/line_counting.c)
configure_file(base/full_lines.txt "${CMAKE_BINARY_DIR}/full_lines.txt" COPYONLY)
configure_file(base/with_spaces.txt "${CMAKE_BINARY_DIR}/with_spaces.txt" COPYONLY)
target_link_libraries(jio_test_line_counting PRIVATE jio)
add_test(NAME base_line_counting COMMAND jio_test_line_counting WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
