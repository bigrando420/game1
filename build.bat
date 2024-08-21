@echo off


:: Get start time:
for /F "tokens=1-4 delims=:.," %%a in ("%time%") do (
   set /A "start=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)


if not exist build (
   mkdir build
)

pushd build

clang -g -fuse-ld=lld  -o cgame.exe ../build.c -O0 -std=c11 -D_CRT_SECURE_NO_WARNINGS -Wextra -Wno-incompatible-library-redeclaration -Wno-sign-compare -Wno-unused-parameter -Wno-builtin-requires-header -lkernel32 -lgdi32 -luser32 -lruntimeobject -lwinmm -ld3d11 -ldxguid -ld3dcompiler -lshlwapi -lole32 -lshcore -lavrt -lksuser -ldbghelp -femit-all-decls 

popd



:: Get end time:
for /F "tokens=1-4 delims=:.," %%a in ("%time%") do (
   set /A "end=(((%%a*60)+1%%b %% 100)*60+1%%c %% 100)*100+1%%d %% 100"
)

:: Get elapsed time:
set /A elapsed=end-start

:: Show elapsed time:
set /A hh=elapsed/(60*60*100), rest=elapsed%%(60*60*100), mm=rest/(60*100), rest%%=60*100, ss=rest/100, cc=rest%%100
if %mm% lss 10 set mm=0%mm%
if %ss% lss 10 set ss=0%ss%
if %cc% lss 10 set cc=0%cc%
echo Build time: %ss%.%cc% seconds