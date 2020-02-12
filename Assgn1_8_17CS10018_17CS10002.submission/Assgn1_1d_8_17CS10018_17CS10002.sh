mkdir 1.d.files.out
cd 1.d.files

for file in *.txt;
do
	sort -nr $file -o ../1.d.files.out/$file
done

cd ..
sort -mr ./1.d.files.out/*.txt -o 1.d.out.txt