rm -f out.txt
for((i = 0; i < 5000; i++))
do
	./a.out data.txt 	&
done

pgrep a.out

for((i = 0; i < 5000; i++))
do
	./a.out >>out.txt 	&
done


