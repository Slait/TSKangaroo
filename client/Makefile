# Makefile for RCKangaroo Distributed Client

CUDA_PATH = /usr/local/cuda
NVCC = $(CUDA_PATH)/bin/nvcc
CXX = g++

# CUDA architectures
CUDA_ARCHS = -gencode arch=compute_61,code=sm_61 \
             -gencode arch=compute_75,code=sm_75 \
             -gencode arch=compute_86,code=sm_86 \
             -gencode arch=compute_89,code=sm_89

# Compiler flags
NVCCFLAGS = $(CUDA_ARCHS) -O3 -Xptxas -O3 -Xcompiler -O3 -std=c++14
CXXFLAGS = -O3 -std=c++14 -fPIC

# Include paths
INCLUDES = -I$(CUDA_PATH)/include -I/usr/include/jsoncpp

# Libraries
LIBS = -L$(CUDA_PATH)/lib64 -lcudart -lcuda -ljsoncpp -lcurl

# Source files
CUDA_SOURCES = ../RCGpuCore.cu
CPP_SOURCES = ../utils.cpp ../Ec.cpp ../GpuKang.cpp ServerClient.cpp RCKangarooClient.cpp

# Object files
CUDA_OBJECTS = $(CUDA_SOURCES:.cu=.o)
CPP_OBJECTS = $(CPP_SOURCES:.cpp=.o)

# Target executable
TARGET = RCKangarooClient

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(CUDA_OBJECTS) $(CPP_OBJECTS)
	$(NVCC) $(NVCCFLAGS) -o $@ $^ $(LIBS)

%.o: %.cu
	$(NVCC) $(NVCCFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(CUDA_OBJECTS) $(CPP_OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Dependencies check
deps:
	@echo "Checking dependencies..."
	@which nvcc > /dev/null || (echo "CUDA not found. Please install CUDA toolkit." && exit 1)
	@pkg-config --exists jsoncpp || (echo "libjsoncpp-dev not found. Please install: sudo apt-get install libjsoncpp-dev" && exit 1)
	@pkg-config --exists libcurl || (echo "libcurl-dev not found. Please install: sudo apt-get install libcurl4-openssl-dev" && exit 1)
	@echo "All dependencies satisfied."

# Build instructions
help:
	@echo "RCKangaroo Distributed Client Build System"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - CUDA Toolkit (11.x or later)"
	@echo "  - libjsoncpp-dev: sudo apt-get install libjsoncpp-dev"
	@echo "  - libcurl-dev: sudo apt-get install libcurl4-openssl-dev"
	@echo ""
	@echo "Targets:"
	@echo "  all     - Build the client"
	@echo "  clean   - Clean build files"
	@echo "  deps    - Check dependencies"
	@echo "  install - Install to /usr/local/bin"
	@echo "  help    - Show this help"