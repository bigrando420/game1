@echo off
if not exist "build_release" (
	mkdir build_release
)

pushd build_release

clang -o cgame.exe ../build.c -Ofast -DNDEBUG -std=c11 -D_CRT_SECURE_NO_WARNINGS -Wextra -Wno-incompatible-library-redeclaration -Wno-sign-compare -Wno-unused-parameter -Wno-builtin-requires-header -Wno-deprecated-declarations -lkernel32 -lgdi32 -luser32 -lruntimeobject -lwinmm -ld3d11 -ldxguid -ld3dcompiler -lshlwapi -lole32 -lshcore -lavrt -lksuser -finline-functions -finline-hint-functions -ffast-math -fno-math-errno -funsafe-math-optimizations -freciprocal-math -ffinite-math-only -fassociative-math -fno-signed-zeros -fno-trapping-math -ftree-vectorize  -fomit-frame-pointer -funroll-loops -fno-rtti -fno-exceptions -L../fmod -lfmod -lfmodstudio -g

copy "..\fmod.dll" "."
copy "..\fmodstudio.dll" "."
xcopy "..\res" "res" /E /I /Y

popd