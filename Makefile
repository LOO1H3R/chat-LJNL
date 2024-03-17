CC ?= gcc
CXX ?= g++
CPP ?= g++

APP_NAME_B = sideB
OBJFILES_B = sideB.o

APP_NAME_A = sideA
OBJFILES_A = sideA.o

LIB_DIRS = .
LIBS = -lasound
LIB = pthread

all: $(APP_NAME_B) $(APP_NAME_A)

$(APP_NAME_B): $(OBJFILES_B)
	$(CC) $^ -o $@ -L$(LIB_DIRS) $(LIBS) -l$(LIB)

$(APP_NAME_A): $(OBJFILES_A)
	$(CC) $^ -o $@ -L$(LIB_DIRS) $(LIBS) -l$(LIB)

%.o: %.c
	$(CC) -c $^ -o $@

clean:
	rm *.o  $(APP_NAME_B) $(APP_NAME_A)

fresh:
	make clean
	make all 