# ESP-IDF component registration for the RadiaCode library's ESP_PLATFORM/NimBLE-cpp port.
idf_component_register(
    SRCS
        "src/BluetoothTransport.cpp"
        "src/BytesBuffer.cpp"
        "src/Decoders.cpp"
        "src/RadiaCode.cpp"
        "src/RadiaCodeTypes.cpp"
    INCLUDE_DIRS
        "src"
    REQUIRES
        esp-nimble-cpp
)
