# Print list of word frequencies
{
	$0 = tolower($0)    # remove case distinctions
	gsub(/[^a-z0-9_ \t]/, "", $0)  # remove punctuation
	for (i = 1; i <= NF; i++)
		freq[$i]++
}

END {
	for (word in freq)
		printf "%s\t%d\n", word, freq[word]
}
