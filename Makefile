all: gtkicon

clean:
	rm -f gtkicon

gtkicon: gtkicon.cpp
	$(CXX) -W -Wall -o $@ $< `pkg-config --cflags --libs gtkmm-2.4 dbus-1`

.PHONY: all clean
