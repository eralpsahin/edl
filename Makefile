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

libpdg.so:
	@cd $(ODIR) && cmake ..
	@$(MAKE) -C $(ODIR)

# Hard clean cleans whole build directory and compiled files in example directory
clean: eclean
	@rm -rf $(ODIR)

# 
#	* Rules for running the EDL pass on the project
# #########################################################################

# Do EDL generation pass
edl: gen-u gen-t $(EDIR)/test.ll
	@cd $(ODIR) && opt -disable-output -load ./libpdg.so -accinfo-track -d 1 -u "untrusted/" -t "trusted/" < ../$(EDIR)/test.ll

# Generate function list from untrusted
gen-u: libpdg.so $(EDIR)/App.ll
	@cd $(ODIR) && opt -disable-output -load ./libpdg.so -llvm-test -prefix $(UNTRUSTED)/ < ../$(EDIR)/App.ll


# Generate function list from trusted
gen-t: libpdg.so $(EDIR)/Enclave.ll
	@cd $(ODIR) && opt -disable-output -load ./libpdg.so -llvm-test -prefix $(TRUSTED)/ < ../$(EDIR)/Enclave.ll

# Generate SGX project
sgx: libpdg.so $(EDIR)/App.ll $(EDIR)/Enclave.ll
	@cd $(ODIR) && opt -disable-output -load ./libpdg.so -sgx -defined trusted/defined_func.txt -suffix "_ECALL" < ../$(EDIR)/App.ll
	@cd $(ODIR) && opt -disable-output -load ./libpdg.so -sgx -defined untrusted/defined_func.txt -suffix "_OCALL" < ../$(EDIR)/Enclave.ll

# Build clean cleans the files generated from passes in build directory
bclean: eclean
	@rm -rf $(ODIR)/Enclave.edl $(TRUSTED_DIR) $(UNTRUSTED_DIR) $(ODIR)/*.c* $(ODIR)/*.h

# *************************************************************************


# 
#	Rules for example project
# #########################################################################

# Run the example project
.PHONY: run
run: $(EDIR)/test.ll
	lli $(EDIR)/test.ll

.PHONY: compile
compile: $(EDIR)/App.ll $(EDIR)/Enclave.ll
	llvm-link $(EDIR)/App.ll $(EDIR)/Enclave.ll -S -o $(EDIR)/test.ll

# Link parts to create a combined ll
$(EDIR)/test.ll: $(EDIR)/App.ll $(EDIR)/Enclave.ll
	llvm-link $(EDIR)/App.ll $(EDIR)/Enclave.ll -S -o $@

# Compile trusted and untrusted parts
$(EDIR)/App.ll: $(EDIR)/App.c $(EDIR)/Enclave.h
	$(CC) $(CC_FLAGS) $< -o $@

$(EDIR)/Enclave.ll: $(EDIR)/Enclave.c $(EDIR)/Enclave.h
	$(CC) $(CC_FLAGS) $< -o $@

# Example clean cleans the ll files from the example.
eclean:
	@rm -rf $(EDIR)/*.ll

# *************************************************************************