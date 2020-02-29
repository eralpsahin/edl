ODIR := ./build
output_folder := $(shell mkdir -p $(ODIR))
EDIR := ./example
CC := clang-9
CC_FLAGS := -emit-llvm -S -g

libplugin.so:
	@cd $(ODIR) && cmake ..
	@$(MAKE) -C $(ODIR)

# Hard clean cleans whole build directory and compiled files in example directory
clean: eclean
	@rm -rf $(ODIR)

# 
#	* Rules for running the IDL pass on the project
# #########################################################################

# Do EDL generation pass
edl: gen-func
	cd $(ODIR) && opt -load libpdg.so -accinfo-track -d 1 < ../$(EDIR)/test_encrypt.ll >/dev/null

# Generate function list from a single part (trusted or untrusted)
# This is an LLVM pass
gen-func: libplugin.so test_encrypt.ll
	@cd $(ODIR) && opt -load  libpdg.so -llvm-test < ../$(EDIR)/test_encrypt_script.ll >/dev/null
	@echo Done.

# Build clean cleans the files generated from passes in build directory
bclean: eclean
	@rm -rf $(ODIR)/*_func*.txt $(ODIR)/enclave.edl

# *************************************************************************


# 
#	Rules for example project
# #########################################################################

# Run the example project
.PHONY: run
run: test_encrypt.ll
	lli $(EDIR)/test_encrypt.ll

# Link parts to create a combined ll
test_encrypt.ll: $(EDIR)/test_encrypt_util.ll $(EDIR)/test_encrypt_script.ll
	llvm-link $(EDIR)/test_encrypt_*.ll -o $(EDIR)/$@

# Compile trusted and untrusted parts
$(EDIR)/test_encrypt_script.ll: $(EDIR)/test_encrypt_script.c
	$(CC) $(CC_FLAGS) $^ -o $@

$(EDIR)/test_encrypt_util.ll: $(EDIR)/test_encrypt_util.c
	$(CC) $(CC_FLAGS) $^ -o $@

# Example clean cleans the ll files from the example.
eclean:
	@rm -rf $(EDIR)/*.ll

# *************************************************************************