col=$2
awk '$'$col' = tolower($'$col')' $1 > temp
mv temp $1

awk '{print $'$col'}' $1 | sort -r | uniq -c > temp
touch 1e_output_"$col"_column.freq
awk '{print $2,$1}' temp > 1e_output_"$col"_column.freq
sort -k2 -n 1e_output_"$col"_column.freq -o temp
mv temp 1e_output_"$col"_column.freq