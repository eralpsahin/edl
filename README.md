# EDL generation for Intel-SGX using LLVM passes

This project is an automatization tool for converting general C/C++ projects to Intel SGX projects. 

- It injects necessary headers to both trusted and untrusted domains and modifies function calls in the untrusted domain to comply with the SGX syntax.
- Generates EDL file for the trusted(Enclave) domain.


## Requirements
- [LLVM and Clang 9.0.0](https://releases.llvm.org/download.html)
- Make sure that LLVM is added to $PATH.
- Make sure `make` creates the build folder and compiles the project without any problem before getting started. 


## Getting Started
For quickly getting started just run the `edl` and `sgx` phony targets from the Makefile to generate EDL file and the SGX complied code.
```
make edl
make sgx
```
Above commands will generate the following files that you can use directly within an SGX project.
- `build/Enclave.edl`
- `build/App.cpp`
- `build/Enclave.cpp`

You need a SGX project such as [SampleSGX](https://github.com/eralpsahin/sample-sgx). You can copy paste the above generated files and the header file from the example folder. Follow the regular steps to compile and run the SGX project.

## More information about each step

### 1. Compiling a C/C++ code to LLVM IR
- The path for the sample code project we execute our passes on is the following:
  - `example/App.c` is the untrusted domain.
  - `example/Enclave.c` is the trusted domain.
  - `example/Enclave.h` is the header for function prototypes and user defined types.
- The rules under last section of the Makefile is for compiling and testing the sample project.
- You can test the code before executing the passes with `run` target in Makefile. This will compile and run the sample code.



### 2. Auxiliary information
- We need some auxiliary information to generate the EDL and convert the project to SGX complied one.
- `gen-u` and `gen-t` are phony targets in the `Makefile` that generates the necessary files under the `build/trusted` and `build/untrusted` folders.


### 3. Generating EDL
- `edl` target in the Makefile bootstraps everything by first compiling the sample code, generating auxiliary information and then using those files to execute the EDL generation pass on the sample code.
- This will generate the `build/Enclave.edl` file.

### 4. Converting the C/C++ code to SGX complied code
- `sgx` target in the Makefile again uses the auxiliary information to convert the sample code to SGX complied code.

## Progress Slides

[![Progress Slides](https://user-images.githubusercontent.com/10602289/82127969-45759600-9785-11ea-864d-616e14e2ed7b.png)](https://github.com/eralpsahin/edl/files/4403922/slides.pptx)

## Future work 

- [X] Automatizing C project to SGX
- [X] Public (root) ECALL
- [X] Struct, enum declarations
- [X] Additional include statement features
- [X] [String] attribute
- [ ] [count - size] attributes
