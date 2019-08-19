set -e
cd lib
echo "compiling in lib"
make
cd ../server\ folder/
echo "compiling in server"
make
cd ../client\ folder/
echo "compiling in client"
make
cd ..