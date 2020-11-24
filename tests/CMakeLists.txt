
ADD_EXECUTABLE(net_test net_test.cpp)
target_link_libraries(net_test $<TARGET_OBJECTS:NET_TEST_OBJECTS>)

ADD_EXECUTABLE(rx_drop rx_drop.cpp)
target_link_libraries(rx_drop $<TARGET_OBJECTS:NET_TEST_OBJECTS>)

ADD_EXECUTABLE(clock_test clock_test.cpp)
target_link_libraries(clock_test $<TARGET_OBJECTS:NET_TEST_OBJECTS>)
