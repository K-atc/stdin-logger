BIN := stdin-logger
TEST := test/argv-check test/alarm test/sigsegv
TARGET := $(BIN) $(TEST)
INSTALL_DIR := /usr/local/bin

CXXFLAGS = -O2 -Wall
LDFLAGS = 

all: $(TARGET)
	@### Do Nothing

%:%.c
	gcc -o $@ $^

%:%.cpp
	g++ $(CXX_OPTIONS) -o $@ $^

install: $(BIN)
	for f in $^; do \
		install -m 755 -o root $$f $(INSTALL_DIR); \
	done

# uninstall: $(BIN)
# 	for f in $^; do \
# 		rm $(INSTALL_DIR)/$$f; \
# 	done

test: $(TARGET)
	rm -f *.stdin.log
	./stdin-logger
	@echo "----"
	@# echo "test\x00test\n" | ./stdin-logger cat
	echo -e "test\x00test\ntest" | ./stdin-logger test/argv-check 9
	@# echo "----"
	@# (sleep 3; echo -e "test") | ./stdin-logger test/alarm
	@# (sleep 3; echo -e "test") | ENABLE_ALARM=on ./stdin-logger test/alarm
	@echo "----"
	echo -e "test\x00test\ntest" | ./stdin-logger test/sigsegv; xxd sigsegv*.log

clean:
	rm -f $(BIN)