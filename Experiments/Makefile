.PHONY: all clean cleanall

all clean:
	$(MAKE) -C 01\ Decoding\ Times $@
	$(MAKE) -C 02\ Times\ and\ Metrics $@
	$(MAKE) -C 03\ Preprocess\ Metrics $@
	$(MAKE) -C 04\ GCD\ Compatibility $@
	$(MAKE) -C 05\ Prediction $@

%:
	$(MAKE) -C '$(wildcard $@*)'

cleanall: clean
