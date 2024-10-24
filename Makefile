BITLAB = bitlab

BITLAB_BIN = $(BITLAB)/build/bin/$(BITLAB)

.PHONY: all bitlab
all: bitlab
debug: bitlab-debug

bitlab:
	$(MAKE) -C $(BITLAB)

bitlab-debug:
	$(MAKE) -C $(BITLAB) debug

clean:
	find $(BITLAB)/build/src -name '*.o' -delete
	find $(BITLAB)/build/src -name '*~' -delete
	$(RM) $(BITLAB)/build/bin/$(BITLAB)
