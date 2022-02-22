mkdir test
mkdir rtest

for i in $(seq 1 9)
do
    make test0$i > test/test$i.txt
    make rtest0$i > rtest/test$i.txt
done

for i in $(seq 10 16)
do
    make test$i > test/test$i.txt
    make rtest$i > rtest/test$i.txt
done


for i in $(seq 1 16)
do
 echo "trace $i difference"
 diff test/test$i.txt rtest/test$i.txt
 echo "---------------------------------"
done

rm -r test
rm -r rtest