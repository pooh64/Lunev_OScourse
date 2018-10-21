rm -f out.txt
./a.out /bin/bash
./a.out >>out.txt
diff out.txt /bin/bash