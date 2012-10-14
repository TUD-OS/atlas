all %:
	@echo '*** Take a moment to make sure…'
	@echo '***  · you are running the ATLAS-patched kernel'
	@echo '***  · the CPU governor is set to performance mode'
	@echo '***  · the X server runs on a real-time priority (I use 5.)'
	sleep 5
	$(MAKE) -C 01\ Preprocess $@
	$(MAKE) -C 02\ CFS\ with\ Load $@
	$(MAKE) -C 03\ ATLAS\ with\ Load $@
