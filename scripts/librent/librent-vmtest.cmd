@echo off
rem librent NTVDM test harness entry point. It looks for the test disk and
rem runs the unattended test script installed by vmtest.py payload.
for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if exist "%%D:\LIBRENT\RUNTESTS.CMD" (
        call "%%D:\LIBRENT\RUNTESTS.CMD"
        goto :done
    )
)
:done
