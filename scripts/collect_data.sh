#!/bin/bash

# Matrices compiled from https://sparse.tamu.edu/
# Additional ones can be added by addding (name, matrix market download address) pairs in the followings
declare -A matrices

matrices[psmigr_2]="https://suitesparse-collection-website.herokuapp.com/MM/HB/psmigr_2.tar.gz"                         #| n = 3.1k           | nnz =.54m  | unsymm

download_extract_matrix() {
    if ! wget "$2"; then
        echo "ERROR: can't download: $2" >&2
        exit 1
    fi
    if ! tar -xvf "${1}.tar.gz"; then
        echo "ERROR: can't extract: $1" >&2
        exit 1
    fi
}

sort_file() {
    declare -i last_comment_line
    last_comment_line=1
    while IFS= read -r line; do
        if [[ $line == %* ]]; then
            ((last_comment_line++))
        else break
        fi
    done < "$1"
    new_name="${1%.*}"_row_sorted."${1##*.}"
    { head -n"$last_comment_line" "$1"; tail -n+"$((last_comment_line+1))" "$1" | sort -nk1,1 -nk2,2 ; } > $new_name
    echo "Info: '$1' file is row-sorted and written into '$new_name'"
}

desym_matrix() {
    python3 genralize-matrix.py -d $1
}

echo "Warning: This script should be run inside the ".../HiHiSpMV" directory".

data_dir="${PWD}/data"
mkdir -p $data_dir
echo "Directory created: ${data_dir}"

if ! cd "$data_dir"; then
  echo "ERROR: can't access data directory ($data_dir)" >&2
  exit 1
fi

for key in "${!matrices[@]}"
do
    download_extract_matrix "${key}" ${matrices[$key]}
    # desym_matrix "${data_dir}/${key}/${key}.mtx" # if the matrix is in the symmetric matrix market format
    sort_file "${data_dir}/${key}/${key}.mtx" # "_desym.mtx" suffix if the matrix is symmetric
done

