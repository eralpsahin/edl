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
edl: gen-u gen-t $(EDIR)/test_encrypt.ll
	cd $(ODIR) && opt -disable-output -load libpdg.so -accinfo-track -d 1 -u "u_" -t "t_" < ../$(EDIR)/test_encrypt.ll

# Generate function list from untrusted
gen-u: libplugin.so $(EDIR)/test_encrypt_script.ll
	@cd $(ODIR) && opt -disable-output -load libpdg.so -llvm-test -prefix "u_" < ../$(EDIR)/test_encrypt_script.ll


# Generate function list from trusted
gen-t: libplugin.so $(EDIR)/test_encrypt_util.ll
	@cd $(ODIR) && opt -disable-output -load libpdg.so -llvm-test -prefix "t_" < ../$(EDIR)/test_encrypt_util.ll

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