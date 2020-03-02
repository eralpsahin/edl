ODIR := ./build
UNTRUSTED := untrusted
TRUSTED := trusted
UNTRUSTED_DIR := $(ODIR)/$(UNTRUSTED)
TRUSTED_DIR := $(ODIR)/$(TRUSTED)
build_folder := $(shell mkdir -p $(ODIR))
output_folder := $(shell mkdir -p $(UNTRUSTED_DIR) $(TRUSTED_DIR))
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
edl: gen-u gen-t $(EDIR)/test_encrypt.ll
	cd $(ODIR) && opt -disable-output -load libpdg.so -accinfo-track -d 1 -u "untrusted/" -t "trusted/" < ../$(EDIR)/test_encrypt.ll

# Generate function list from untrusted
gen-u: libplugin.so $(EDIR)/test_encrypt_script.ll
	@cd $(ODIR) && opt -disable-output -load libpdg.so -llvm-test -prefix $(UNTRUSTED)/ < ../$(EDIR)/test_encrypt_script.ll


# Generate function list from trusted
gen-t: libplugin.so $(EDIR)/test_encrypt_util.ll
	@cd $(ODIR) && opt -disable-output -load libpdg.so -llvm-test -prefix $(TRUSTED)/ < ../$(EDIR)/test_encrypt_util.ll

# Build clean cleans the files generated from passes in build directory
bclean: eclean
	@rm -rf $(ODIR)/enclave.edl $(TRUSTED_DIR) $(UNTRUSTED_DIR)

# *************************************************************************


# 
#	Rules for example project
# #########################################################################

# Run the example project
.PHONY: run
run: test_encrypt.ll
	lli $(EDIR)/test_encrypt.ll

.PHONY: compile
compile: $(EDIR)/test_encrypt_util.ll $(EDIR)/test_encrypt_script.ll
	llvm-link $(EDIR)/test_encrypt_*.ll -o $(EDIR)/test_encrypt.ll

# Link parts to create a combined ll
$(EDIR)/test_encrypt.ll: $(EDIR)/test_encrypt_util.ll $(EDIR)/test_encrypt_script.ll
	llvm-link $(EDIR)/test_encrypt_*.ll -o $@

# Compile trusted and untrusted parts
$(EDIR)/test_encrypt_script.ll: $(EDIR)/test_encrypt_script.c
	$(CC) $(CC_FLAGS) $^ -o $@

$(EDIR)/test_encrypt_util.ll: $(EDIR)/test_encrypt_util.c
	$(CC) $(CC_FLAGS) $^ -o $@

# Example clean cleans the ll files from the example.
eclean:
	@rm -rf $(EDIR)/*.ll

# *************************************************************************