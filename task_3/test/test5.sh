rm -f out.txt
./a.out /bin/bash	>sender.txt
./a.out			>receiver.txt
diff out.txt /bin/bash
