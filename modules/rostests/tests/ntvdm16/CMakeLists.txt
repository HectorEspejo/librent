include(${REACTOS_SOURCE_DIR}/subsystems/mvdm/asm16.cmake)

add_asm16_bin(hello ${CMAKE_CURRENT_BINARY_DIR}/hello.com 0x0100 hello.S)
add_asm16_bin(oobread ${CMAKE_CURRENT_BINARY_DIR}/oobread.com 0x0100 oobread.S)
add_asm16_bin(keytest ${CMAKE_CURRENT_BINARY_DIR}/keytest.com 0x0100 keytest.S)
add_asm16_bin(textmodes ${CMAKE_CURRENT_BINARY_DIR}/textmodes.com 0x0100 textmodes.S)

add_executable(vmtestmark vmtestmark.c)
set_module_type(vmtestmark win32cui UNICODE)
add_importlibs(vmtestmark msvcrt kernel32)

if(ENABLE_NTVDM_BOOT_RUN)
    add_cd_file(TARGET vmtestmark DESTINATION reactos/system32 FOR all)
    add_cd_file(FILE ${REACTOS_SOURCE_DIR}/scripts/librent/librent-vmtest.cmd DESTINATION reactos/system32 FOR all)
endif()
