#---------------------------------------------------------------------
# Makefile for MolFlow
#

#---------------------------------------------------------------------
# All source files needed for the application
#

SRC = 	Facet.cpp File.cpp Simulation.cpp  std_molflow.cpp \
	Surface.cpp Utils.cpp Random.cpp

OBJ = 	Facet.o File.o Simulation.o  std_molflow.o \
	Surface.o Utils.o Random.o


#---------------------------------------------------------------------

CXX		= g++


ifdef _profile

CXXFLAGS =     -D_DEBUG -Dlinux  -c -pg -I.

LFLAGS   =      -pg -lpthread -lm -ldl 

else

ifdef _release

CXXFLAGS =     -O2 -Dlinux  -c -I.

LFLAGS   =      -lpthread -lm -ldl

else

CXXFLAGS =     -g -D_DEBUG -Dlinux  -c -I.

LFLAGS   =      -lpthread -lm -ldl

endif

endif

#--------------------------------------------------------------------

all: molflow

molflow: $(OBJ)
	@echo Linking...
	@rm -f molflow
	$(CXX) $(OBJ) $(LFLAGS) -o molflow
	@echo Linking OK...

clean:
	@echo Cleaning...
	@rm -f *.o
	@rm -f molflow

