mips-v720s229-linux-gnu-gcc -fPIC -I ../include/jz_aip -c jz_aip.c -o jz_aip.o
mips-v720s229-linux-gnu-gcc -fPIC  -I ../include/jz_aip -c bscaler_api.c -o bscaler_api.o
mips-v720s229-linux-gnu-gcc -fPIC -shared  bscaler_api.c jz_aip.c -I ../include/jz_aip -o ./glibc/libaip.so
mips-v720s229-linux-gnu-ar  -rc ./glibc/libaip.a bscaler_api.o jz_aip.o
rm jz_aip.o bscaler_api.o

mips-v720s229-linux-uclibc-gnu-gcc -fPIC  -I ../include/jz_aip -c jz_aip.c -o jz_aip.o
mips-v720s229-linux-uclibc-gnu-gcc -fPIC  -I ../include/jz_aip -c bscaler_api.c -o bscaler_api.o
mips-v720s229-linux-uclibc-gnu-gcc -fPIC -shared  bscaler_api.c jz_aip.c -I ../include/jz_aip -o ./uclibc/libaip.so
mips-v720s229-linux-uclibc-gnu-ar -rc ./uclibc/libaip.a bscaler_api.o jz_aip.o
rm jz_aip.o bscaler_api.o
