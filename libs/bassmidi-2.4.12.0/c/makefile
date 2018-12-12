EXAMPLES = miditest sf2pack synth

.PHONY: all clean $(EXAMPLES)

all: $(EXAMPLES)

clean:
	@$(foreach x,$(EXAMPLES),$(MAKE) -C $(x) clean;)

$(EXAMPLES):
	$(MAKE) -C $@
