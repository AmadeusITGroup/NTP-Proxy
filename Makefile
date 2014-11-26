BINARIES=sls ntpproxy

.PHONY: clean

all: $(BINARIES)

clean:
	$(RM) $(BINARIES)
