APP_NAME = app
BUILD_DIR = ./build
SRC_FOLDER = ./src
OBJS_FOLDER = $(BUILD_DIR)/objs
BIN_PATH = $(BUILD_DIR)/$(APP_NAME)
SIM_METALLIB = $(BUILD_DIR)/sim.metallib

CXX = clang++
CXXFLAGS = -std=c++17 -I. -I$(SRC_FOLDER) -I/opt/homebrew/include -MMD -MP
SDL2_CFLAGS := $(shell sdl2-config --cflags)
SDL2_LDFLAGS := $(shell sdl2-config --libs)
FRAMEWORKS = -framework Metal -framework Foundation -framework QuartzCore -framework CoreGraphics

SRCS := $(wildcard $(SRC_FOLDER)/*.cpp)
OBJS := $(patsubst $(SRC_FOLDER)/%.cpp, $(OBJS_FOLDER)/%.o, $(SRCS))

DEPS := $(OBJS:.o=.d)

.PHONY: all clean run brain


all: $(BIN_PATH) $(METALLIB) $(SIM_METALLIB)

$(BIN_PATH): $(OBJS)
	@echo "Linkage de l'exécutable..."
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) $(FRAMEWORKS) $(SDL2_LDFLAGS) -o $@
	@echo "Succès ! Exécutable créé ici : $@"

$(OBJS_FOLDER)/%.o: $(SRC_FOLDER)/%.cpp
	@echo "Compilation de $< ..."
	@mkdir -p $(OBJS_FOLDER)
	$(CXX) $(CXXFLAGS) $(SDL2_CFLAGS) -c $< -o $@


$(SIM_METALLIB): shaders/sim_shaders.metal $(SRC_FOLDER)/CreatureRenderData.h
	@echo "Compilation des Shaders de simulation..."
	@mkdir -p $(BUILD_DIR)
	xcrun -sdk macosx metal -I$(SRC_FOLDER) -c shaders/sim_shaders.metal -o $(BUILD_DIR)/sim_shaders.air
	xcrun -sdk macosx metallib $(BUILD_DIR)/sim_shaders.air -o $(SIM_METALLIB)
	@rm $(BUILD_DIR)/sim_shaders.air
	@echo "Shaders sim compilés dans $(SIM_METALLIB)"

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)
	@echo "Dossier build nettoyé."

brain:
	python3 src/main.py

run: all
	@trap 'kill 0' EXIT; $(BIN_PATH) & python3 src/main.py

bear:
	make clean
	bear -- make

setup:
	xcode-select --install