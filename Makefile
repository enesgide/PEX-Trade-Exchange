CC=gcc
CFLAGS=-Wall -Werror -Wvla -O0 -std=c11 -g -fsanitize=address,leak
LDFLAGS=-lm
BINARIES=pe_exchange pe_trader

all: $(BINARIES)

pe_exchange: pe_exchange.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

pe_trader: pe_trader.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

tests:
	@echo Building tests...
	$(CC) test_files/unit_tests.c products_reader.c orderbook_handler.c $(CFLAGS) -o test_files/unit_tests test_files/libcmocka-static.a $(LDFLAGS)
	@ echo Finished building tests!

run_tests: $(TARGET)
	@echo Running tests...
	./test_files/unit_tests
	@echo Finished running tests!

.PHONY: clean
clean:
	rm -f $(BINARIES)

