mkdir build

cmake -G "Ninja" -H. -Bbuild ^
    -DCMAKE_CXX_COMPILER="%CXX%" ^
    -DCMAKE_C_COMPILER="%CC%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%LIBRARY_PREFIX%" ^
    -DPython_ROOT_DIR="%PREFIX%" ^
    -DPython_EXECUTABLE="%PYTHON%" ^
    -DCLINGO_BUILD_WITH_PYTHON=ON ^
    -DCLINGO_BUILD_WITH_LUA=OFF ^
    -DCLINGO_MANAGE_RPATH=OFF ^
    -DPYCLINGO_INSTALL="system"

cmake --build build
cmake --build build --target install
