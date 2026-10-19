
## Experimental 16-bit ASM compilation support
include(asm16.cmake)

add_subdirectory(config)
add_subdirectory(dos)
add_subdirectory(ntvdm)
add_subdirectory(pifmgr)
add_subdirectory(vdmdbg)

if(ENABLE_NTVDM_BOOT_RUN)
    add_subdirectory(${REACTOS_SOURCE_DIR}/modules/rostests/tests/ntvdm16 ${CMAKE_CURRENT_BINARY_DIR}/ntvdm16)
endif()
