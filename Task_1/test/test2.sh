rm -f out.txt
for((i = 0; i < 10000; i++))
do
	./a.out >>out.txt 	&
	./a.out data.txt 	&
done
