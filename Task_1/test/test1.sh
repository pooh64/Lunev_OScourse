rm -f out.txt
for((i = 0; i < 100; i++))
do
	./a.out >>out.txt 	&
	./a.out data.txt 	&
done