include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(mmsim_tests
    test_event_bus.cpp
    test_order_book.cpp
    test_matching_engine.cpp
    test_volatility.cpp
    test_avellaneda_stoikov.cpp
    test_risk_manager.cpp
    test_market_data_feed.cpp
)
target_link_libraries(mmsim_tests PRIVATE mmsim_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mmsim_tests)

add_executable(test_event_bus_manual test_event_bus_manual.cpp)
target_link_libraries(test_event_bus_manual PRIVATE mmsim_core)
target_compile_features(test_event_bus_manual PRIVATE cxx_std_20)

add_executable(test_order_book_manual test_order_book_manual.cpp)
target_link_libraries(test_order_book_manual PRIVATE mmsim_core)
target_compile_features(test_order_book_manual PRIVATE cxx_std_20)

add_executable(test_order_book_price_time_manual test_order_book_price_time_manual.cpp)
target_link_libraries(test_order_book_price_time_manual PRIVATE mmsim_core)
target_compile_features(test_order_book_price_time_manual PRIVATE cxx_std_20)

add_executable(test_matching_edge_cases_manual test_matching_edge_cases_manual.cpp)
target_link_libraries(test_matching_edge_cases_manual PRIVATE mmsim_core)
target_compile_features(test_matching_edge_cases_manual PRIVATE cxx_std_20)

add_executable(test_volatility_estimators_manual test_volatility_estimators_manual.cpp)
target_link_libraries(test_volatility_estimators_manual PRIVATE mmsim_core)
target_compile_features(test_volatility_estimators_manual PRIVATE cxx_std_20)

add_executable(test_avellaneda_stoikov_manual test_avellaneda_stoikov_manual.cpp)
target_link_libraries(test_avellaneda_stoikov_manual PRIVATE mmsim_core)
target_compile_features(test_avellaneda_stoikov_manual PRIVATE cxx_std_20)
