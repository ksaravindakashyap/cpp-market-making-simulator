add_library(mmsim_core STATIC
    src/event_bus.cpp
    src/order_book.cpp
    src/matching_engine.cpp
    src/volatility.cpp
    src/avellaneda_stoikov.cpp
    src/risk_manager.cpp
    src/market_data_feed.cpp
    src/order_manager.cpp
)
target_include_directories(mmsim_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
