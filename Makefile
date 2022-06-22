TARGET=uxndump
CFLAGS=-O2 -Wall

all: $(TARGET)

$(TARGET): $(TARGET).c

clean:
	rm -f $(TARGET)
