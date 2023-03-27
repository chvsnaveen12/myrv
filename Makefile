code.bin: code.o
	llvm-objcopy -O binary code.o code.bin

code.o: code.s
	clang -Wl,-Ttext=0x0 -nostdlib --target=riscv64 -march=rv64g -mno-relax -o code.o code.s

run: main.c code.bin
	gcc -oemulator main.c
	./emulator code.bin

clean: code.o code.bin
	rm code.o code.bin emulator