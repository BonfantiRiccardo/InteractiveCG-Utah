# Builds LodePNG once as a static library, so projects only link it instead of
# recompiling lodepng.cpp on every build. Rebuilt only when the sources change.
CXX = C:/msys64/ucrt64/bin/g++.exe
AR  = C:/msys64/ucrt64/bin/ar.exe
CXXFLAGS = -std=c++17 -O2

liblodepng.a: lodepng.o
	$(AR) rcs $@ $^

lodepng.o: lodepng.cpp lodepng.h
	$(CXX) $(CXXFLAGS) -c lodepng.cpp -o lodepng.o
