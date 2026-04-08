CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGETS = sensor_analyzer_seq sensor_analyzer_par sensor_analyzer_optimized

all: $(TARGETS)

sensor_analyzer_seq: sensor_analyzer_seq.c
	$(CC) $(CFLAGS) -o $@ $< -lm

sensor_analyzer_par: sensor_analyzer_par.c
	$(CC) $(CFLAGS) -o $@ $< -pthread -lm

sensor_analyzer_optimized: sensor_analyzer_optimized.c
	$(CC) $(CFLAGS) -o $@ $< -pthread -lm

clean:
	rm -f $(TARGETS)