rm -f out.txt
for((i = 0; i < 100; i++))
do
	./a.out 		&
	pgrep a.out
	./a.out data.txt 	&
done
