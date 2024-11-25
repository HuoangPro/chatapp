
chat: chat.cpp
	g++ -o chat chat.cpp -pthread

all: chat

clean:
	rm -rf chat

.PHONY: all clean chat