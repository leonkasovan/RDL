#
# Cross Platform Makefile
# Compatible with MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X
#
# You will need SDL2 (http://www.libsdl.org):
# Linux:
#   apt-get install libsdl2-dev
# Mac OS X:
#   brew install sdl2
# MSYS2:
#   pacman -S mingw-w64-i686-SDL2
#

#CXX = g++
#CXX = clang++

EXE = roms_downloader
IMGUI_DIR = imgui
SOURCES = main.cpp
# SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp $(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp
OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))
UNAME_S := $(shell uname -s)

CXXFLAGS = -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
# CXXFLAGS = -std=c++11 -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
# CXXFLAGS += -g -Wall -Wformat
CXXFLAGS += -Wall -Wformat
LIBS =

##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
#	LIBS += -lGL -ldl `sdl2-config --libs`
	LIBS += -ldl `sdl2-config --libs` -lcurl -s

	CXXFLAGS += `sdl2-config --cflags`
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo `sdl2-config --libs`
	LIBS += -L/usr/local/lib -L/opt/local/lib

	CXXFLAGS += `sdl2-config --cflags`
	CXXFLAGS += -I/usr/local/include -I/opt/local/include
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "MinGW"
	LIBS += -lgdi32 -lopengl32 -limm32 `pkg-config --static --libs sdl2`

	CXXFLAGS += `pkg-config --cflags sdl2`
	CFLAGS = $(CXXFLAGS)
endif

##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------
all: $(EXE)
	@echo Build complete for $(ECHO_MESSAGE)

system_scrape_id.h: gen_system_scrape_id systemesListe.json
	./gen_system_scrape_id > system_scrape_id.h

%.o:%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/backends/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

gen_system_scrape_id: gen_system_scrape_id.cpp
	g++ gen_system_scrape_id.cpp -o gen_system_scrape_id

systemesListe.json:
	wget "https://api.screenscraper.fr/api2/systemesListe.php?output=json&devid=recalbox&devpassword=C3KbyjX8PKsUgm2tu53y&softname=Emulationstation-Recalbox-9.1&ssid=test&sspassword=test" -O systemesListe.json
	
$(EXE): system_scrape_id.h $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

db:
	wget https://github.com/leonkasovan/RDL/releases/download/v1.0.0/db.zip
	unzip db.zip
# wget -P db https://nopaystation.com/tsv/PSV_GAMES.tsv
# wget -P db https://nopaystation.com/tsv/PSP_GAMES.tsv
# wget -P db https://nopaystation.com/tsv/PSX_GAMES.tsv

# Generate FBNeo Gamelist
# git clone https://github.com/finalburnneo/FBNeo.git
# perl gen_fbneo_gamelist.pl FBNeo/src/burn/drv/ > db/fbneo.gamelist.txt

# Generate MAME Gamelist
# wget https://github.com/mamedev/mame/releases/download/mame0267/mame0267lx.zip
# unzip mame0267lx.zip
# lua5.1 gen_mame_gamelist.lua > db/mame.gamelist.txt

db.zip:
	zip -r db.zip db/
	
clean:
	rm -f $(EXE) $(OBJS)
