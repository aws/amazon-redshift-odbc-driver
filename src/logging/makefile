STATIC=librslog.a

CXX := g++
CXXFLAGS := -Wall -I. -I${AWS_SDK_INC_DIR} -std=c++17  -fPIC

LDFLAGS =
LIBS  = $(AWS_SDK_LIB_DIR)/libaws-cpp-sdk-core.a $(AWS_SDK_LIB_DIR)/libaws-c-io.a $(AWS_SDK_LIB_DIR)/libaws-c-common.a

SRC = rslog.cpp

OBJ = $(SRC:.cpp=.o)

$(STATIC): $(OBJ)
	@echo "[Link (Static)]"
	@ar rcs $@ $^

.cpp.o:
	@echo [Compile] $<
	@$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	@echo [clean] $(OBJ)
	rm -f $(OBJ)

all: $(STATIC)
	$(CXX) $(CXXFLAGS) -o rslog $(SRC) $(LDFLAGS) $(LIBS)
