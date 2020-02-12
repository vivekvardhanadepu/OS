file=$1

# removing parallel edges, self edges, duplicate edges
awk '$1 != $2' $file | sort | uniq > temp
awk '!seen[($1>$2?$1:$2)+0 , ($1<$2?$1:$2)+0]++' temp > 1f_output_graph.edgelist
rm temp

# printing 5 most frequent nodes
sed -e 's/[\s\t]/\n/g' < 1f_output_graph.edgelist | sort | uniq -c | sort -nr | head -5 | awk '{print $2,$1}'
