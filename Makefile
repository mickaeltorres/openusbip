NAME=openusbipd
SRC=main.c net.c process.c
OBJ=$(SRC:.c=.o)
CFLAGS=-Wall -Werror
LDFLAGS=

LD=$(CC)
RM=rm -fr

all: $(NAME)

$(NAME): $(OBJ)
	$(LD) -o $(NAME) $(LDFLAGS) $(OBJ)

clean:
	$(RM) $(OBJ) $(NAME)

re: clean all
