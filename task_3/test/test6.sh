rm -f out.txt
for((i = 0; i < 100; i++))
do
	./a.out data.txt	&
	pgrep a.out
	./a.out	>>out.txt	&
done
