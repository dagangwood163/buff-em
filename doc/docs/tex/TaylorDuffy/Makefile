##################################################
##################################################
##################################################
SCUFFEM  = /home/homer/work/scuff-em-installation
#SCUFFEM  = /home/homer/work/scuff-em-debug
LIBDIR   = $(SCUFFEM)/lib

##################################################
##################################################
##################################################
CPPFLAGS += -I$(SCUFFEM)/include/scuff-em -I$(HOME)/include -I. -fopenmp
CPPFLAGS += -I/home/homer/work/scuff-em/src/libs/libscuff
LDFLAGS += -L$(LIBDIR) -Wl,-rpath,$(LIBDIR)
LDFLAGS += -L$(HOME)/codes/lib -L$(HOME)/lib 
CXXFLAGS += -O3 -DHAVE_CONFIG_H
#CXXFLAGS = -ggdb -O0

HR_LIBS=-lscuff

RDL_LIBS=-lreadline -lncurses
HDF5_LIBS= -lhdf5_hl -lhdf5 
LB_LIBS=-llapack -lopenblas -lgomp -lgfortran 
OTHER_LIBS = $(RDL_LIBS) $(HDF5_LIBS) $(LB_LIBS)

LIBS = $(HR_LIBS) $(OTHER_LIBS)

##################################################
##################################################
##################################################
TestRegions:    TestRegions.o
		$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
		/bin/rm *.o *.a
