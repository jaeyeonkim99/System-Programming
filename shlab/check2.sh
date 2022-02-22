mkdir test
mkdir rtest

for i in 14 16
do
    make test$i > test/test$i.txt
    make rtest$i > rtest/test$i.txt
    echo "trace $i difference"
    diff test/test$i.txt rtest/test$i.txt
    echo "---------------------------------"
done

rm -r test
rm -r rtest