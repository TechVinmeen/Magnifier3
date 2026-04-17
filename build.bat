@echo off
setlocal

set MSBUILD="C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
set PROJECT="C:\Users\CMCEML-W54105\Desktop\Personal\Code\BlockView2022\BlockView.vcxproj"

echo Building BlockView2022 (Debug2022 x64)...
%MSBUILD% %PROJECT% /p:Configuration=Debug2022 /p:Platform=x64 /m /verbosity:normal
echo.
echo Exit code: %ERRORLEVEL%
endlocal
