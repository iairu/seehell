# Adresar kam budu presunute .o objektove subory po builde
OBJDIR = obj
# Vystupna cesta binarky
EXE = build/main
# Vsetky .c zdrojove subory potrebne pre binarku
SOURCES = main.c syscall.S
# Kompilator
CXX = gcc
# Kompilator flagy (aj vsetky includes -I... sem)
CXXFLAGS = 
# CXXFLAGS = -g
# Kompilator kniznice (vsetky libs -l... sem)
LIBS =


UNAME_S := $(shell uname -s)
ifneq ($(UNAME_S), Linux) #LINUX
	@echo Fatal: Only Linux is currently supported.
else
	CFLAGS = $(CXXFLAGS)
	OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))

%.o: %.c
	$(CXX) -Wall $(CXXFLAGS) -c -o $@ $<

all: $(EXE)
	mv *.o $(OBJDIR)
	@echo Build complete.

$(EXE): $(OBJS)
	$(CXX) -Wall -o $@ $^ $(CXXFLAGS) $(LIBS)
	

clean:
	rm -f $(EXE) 
	cd obj && rm -f $(OBJS)

endif
