sed = 1b_input.txt | sed  'N;s/\n/\t/' > temp
mv temp 1b_input.txt
sed -i '1i "Serial", "Random String"' 1b_input.txt

#awk  '{printf "%d\t%s\n", NR, $0}' < 1b_input.txt