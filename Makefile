# Name of the executable
TARGET = Kinux

# Source files
SRC = main.c command_parser.c built_in_commands.c external_commands.c

# Object files
OBJ = $(SRC:.c=.o)

# Compiler and compilation flags
CC = gcc

# They tell GCC how to compile your C program and what kinds of warnings to report.
CFLAGS = -Wall -Wextra -Werror 

# Default target
all: $(TARGET)

# Link object files to generate the executable
#  gcc -Wall -Wextra -Werror -o Kinux main.c command_parser.c helpers.c
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) 

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Remove object files
clean:
	rm -f *.o

# Remove object files and executable
fclean: clean
	rm -f $(TARGET)
	
# Rebuild the project
re: fclean all
.PHONY: all clean fclean re