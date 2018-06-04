TARGET = ../esysinfo

OBJECTS = system.o process.o ui.o main.o

default: $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) $(OBJECTS) $(shell pkg-config --libs $(PKGS)) $(LIBS) $(LDFLAGS) -o $@

main.o: main.c
	$(CC) -c $(CFLAGS) $(shell pkg-config --cflags $(PKGS)) main.c -o $@

system.o: system.c
	$(CC) -c $(CFLAGS) system.c -o $@

process.o: process.c
	$(CC) -c $(CFLAGS) $(shell pkg-config --cflags $(PKGS)) process.c -o $@

ui.o: ui.c
	$(CC) -c $(CFLAGS) $(shell pkg-config --cflags $(PKGS)) ui.c -o $@

clean:
	-rm $(OBJECTS)
	-rm $(TARGET)
