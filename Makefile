BUNDLE = zamcomp.lv2
INSTALL_DIR = /usr/lib/lv2

$(BUNDLE): manifest.ttl zamcomp.ttl zamcomp.so zamcomp_gui.so
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	cp manifest.ttl zamcomp.ttl zamcomp.so $(BUNDLE)

zamcomp.so: zamcomp.cpp
	g++ -shared -fPIC -DPIC zamcomp.cpp `pkg-config --cflags --libs lv2-plugin` -o zamcomp.so

zamcomp_gui.so: zamcomp_gui.cpp zamcomp.peg
	g++ -shared -fPIC -DPIC zamcomp_gui.cpp `pkg-config --cflags --libs lv2-gui` -o zamcomp_gui.so

zamcomp.peg: zamcomp.ttl
	lv2peg zamcomp.ttl zamcomp.peg

install: $(BUNDLE)
	mkdir -p $(INSTALL_DIR)
	rm -rf $(INSTALL_DIR)/$(BUNDLE)
	cp -R $(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) zamcomp.so zamcomp_gui.so zamcomp.peg
