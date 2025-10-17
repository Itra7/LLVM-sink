clang -O0 -Xclang -disable-O0-optnone -emit-llvm -S 1.c -o 1.ll
./bin/opt -passes=mem2reg 1.ll -S -o tmp.ll
./bin/opt -load lib/LLVMOurSinkPass.so -enable-new-pm=0  -our-sink tmp.ll -S -o tmp1.ll
