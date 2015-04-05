all: benchmap

#-----------------------------------------------------------------------------

OBJDIR = obj/
INCDIR = include/ parparser/
SRCDIR = src/ parparser/
BINDIR = bin/

#-----------------------------------------------------------------------------

CC     = mpicxx
CFLAG  = -I$(INCDIR)

#-----------------------------------------------------------------------------

BINFILE = benchmap.a

#-----------------------------------------------------------------------------

FILES = main pugixml parparser

#-----------------------------------------------------------------------------

OBJECTS = $(addprefix $(OBJDIR), $(addsuffix .o, $(FILES)))
CFILES =  $(addprefix $(SRCDIR), $(addsuffix .cpp, $(FILES)))

#-----------------------------------------------------------------------------

benchmap: $(OBJECTS)
	@mkdir -p bin
	@echo "\033[30;1;41m "bin" dir was created \033[0m"

	@$(CC) $(OBJECTS) -o $(BINDIR)$(BINFILE)

	@echo "\033[30;1;41m benchmap builded successfully! \033[0m"
	@echo "\033[30;1;41m --> $(BINDIR)$(BINFILE) \033[0m"

$(OBJDIR)%.o: $(SRCDIR)%.cpp
	@mkdir -p $(OBJDIR)
	@$(CC) -c $(DFLAG) $(CFLAG) $(addprefix -I, $(INCDIR)) $^ -o $@
	@echo "\033[30;1;46m $@ - done \033[0m\n"

clean:
	rm -r -f bin
	rm -r -f obj

#-----------------------------------------------------------------------------
