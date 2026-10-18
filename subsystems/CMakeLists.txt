
add_subdirectory(csr)
add_subdirectory(win)
if(ENABLE_NTVDM)
    add_subdirectory(mvdm)
endif()
