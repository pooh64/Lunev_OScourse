rm -f out.txt
for((i = 0; i < 10; i++))
do
	valgrind --tool=cachegrind ./a.out data.txt 	&
done

for((i = 0; i < 10; i++))
do
	valgrind --tool=cachegrind ./a.out >>out.txt 	&
done
