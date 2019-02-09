gcc -c -o ./test/compile/sync.o sync.c
gcc -c -o ./test/compile/transfer.o transfer.c
gcc -o ./test/a.out ./test/compile/sync.o ./test/compile/transfer.o
