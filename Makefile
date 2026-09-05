# compiler
CXX ?= clang++

# use pkg-config to get correct flags
XCB_CFLAGS := $(shell pkg-config --cflags xcb xcb-keysyms xcb-util 2>/dev/null)
XCB_LIBS := $(shell pkg-config --libs xcb xcb-keysyms xcb-util 2>/dev/null)

# fallback if pkg-config fails (for systems without pkg-config)
ifeq ($(XCB_CFLAGS),)
    XCB_CFLAGS := -I/usr/local/include
    XCB_LIBS := -lxcb -lxcb-keysyms -lxcb-util -lpthread
endif

# compiler flags
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -pipe $(XCB_CFLAGS)
LIBS := $(XCB_LIBS)
LDFLAGS := 

# target
TARGET := xiri
SOURCES := xiri.cpp
OBJECTS := $(SOURCES:.cpp=.o)

# Xephyr settings
XEPHYR_DISPLAY := :2
XEPHYR_SIZE := 1280x720
XEPHYR_CMD := Xephyr $(XEPHYR_DISPLAY) -screen $(XEPHYR_SIZE) -ac -reset -terminate

# default target
all: $(TARGET)

# link
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)

# compile
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# clean
clean:
	rm -f $(TARGET) $(OBJECTS)

# install
install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

# uninstall
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

# run directly on current X server
run: all
	./$(TARGET)

# run inside Xephyr
xephyr: all
	@echo "Starting Xephyr on $(XEPHYR_DISPLAY)..."
	@echo ""
	@$(XEPHYR_CMD) &
	@echo "Running Xiri inside Xephyr..."
	@sleep 2
	@DISPLAY=$(XEPHYR_DISPLAY) ./$(TARGET) &
	@echo "Xiri is running on $(XEPHYR_DISPLAY)"
	@wait

# check dependencies
check-deps:
	@echo "Checking dependencies..."
	@command -v $(CXX) >/dev/null 2>&1 || { echo "ERROR: $(CXX) not found"; exit 1; }
	@pkg-config --exists xcb 2>/dev/null || { echo "ERROR: xcb not found"; exit 1; }
	@pkg-config --exists xcb-keysyms 2>/dev/null || { echo "ERROR: xcb-keysyms not found"; exit 1; }
	@pkg-config --exists xcb-util 2>/dev/null || { echo "ERROR: xcb-util not found"; exit 1; }
	@echo "All dependencies found!"
	@echo "CFLAGS: $(XCB_CFLAGS)"
	@echo "LIBS: $(XCB_LIBS)"

# help
help:
	@echo "Xiri Makefile commands:"
	@echo "  make            - Build Xiri (default)"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make install    - Install Xiri to /usr/local/bin"
	@echo "  make uninstall  - Remove Xiri from /usr/local/bin"
	@echo "  make run        - Build and run Xiri directly"
	@echo "  make xephyr     - Build and run Xiri inside Xephyr (isolated)"
	@echo "  make check-deps - Check if dependencies are installed"
	@echo "  make help       - Show this help"
	@echo "  make sddm       - Add .desktop file to run xiri session from sddm"``
	@echo "  make config     - Creates config folder at ~/.config/xiri and copies the default config file there"


# sddm
sddm:
	@echo "Adding xiri to sddm as option... (super user required)"
	@touch /usr/share/xsessions/xiri.desktop
	@mv  xiri.desktop /usr/share/xsessions/xiri.desktop
	@echo "Done! Now type sudo make install, to add xiri binary to /usr/local/bin and you will be able to boot into xiri."


config:
	@echo "Creating config folder at ~/.config/"
	@mkdir ~/.config/xiri
	@echo "Done! Now copying the config file to the config folder..."
	@cp ~/xiri/config.ini ~/.config/xiri/config.ini
	@echo "Done! Now you can edit the config file and don't forget to reinstall xiri to apply changes." 
# phony targets
.PHONY: all clean install uninstall run xephyr check-deps help
