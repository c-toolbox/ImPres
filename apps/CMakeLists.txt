##OpenVR section start####################
option(OPENVR_SUPPORT "Build ImPres application with OpenVR support" OFF)

# RGBEasy Specific
option(RGBEASY_ENABLE "Use RGBEASY to capture high-res material" OFF)

# Zxing Specific
option(ZXING_ENABLE "Use ZXING to enable QR decoding and more" OFF)

add_subdirectory(CaptureTester)
add_subdirectory(DomePres)