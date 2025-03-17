# Rebuild pass
cd llvm-tutor-main/build/
#cmake ..
make
cd ../..

# Recompile bitcode
clang -S -fno-discard-value-names -emit-llvm -c test1.c -o test1.ll
clang -S -fno-discard-value-names -emit-llvm -c test2.c -o test2.ll
clang -S -fno-discard-value-names -emit-llvm -c test3.c -o test3.ll
clang -S -fno-discard-value-names -emit-llvm -c test4.c -o test4.ll

# Echo new lines
echo ""
echo ""

# Run opt
echo ""
echo "***********************************************************"
echo "*                      Test 1 Output                      *"
echo "***********************************************************"
echo ""
opt -load-pass-plugin ./llvm-tutor-main/build/lib/libHelloWorld.so -passes=hello-world -disable-output test1.ll
echo ""
echo "***********************************************************"
echo "*                      Test 2 Output                      *"
echo "***********************************************************"
opt -load-pass-plugin ./llvm-tutor-main/build/lib/libHelloWorld.so -passes=hello-world -disable-output test2.ll
echo ""
echo "***********************************************************"
echo "*                      Test 3 Output                      *"
echo "***********************************************************"
echo ""
opt -load-pass-plugin ./llvm-tutor-main/build/lib/libHelloWorld.so -passes=hello-world -disable-output test3.ll
echo ""
echo "***********************************************************"
echo "*                      Test 4 Output                      *"
echo "***********************************************************"
echo ""
opt -load-pass-plugin ./llvm-tutor-main/build/lib/libHelloWorld.so -passes=hello-world -disable-output test4.ll


