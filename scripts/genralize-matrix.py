# test-bicgstab.py
import sys
import getopt
import os, psutil

def parse_int_int_int(line, delim):
    split_line = line.split(delim)
    return int(split_line[0]), int(split_line[1]), int(split_line[2])

def parse_int_int_float(line, delim):
    split_line = line.split(delim)
    return int(split_line[0]), int(split_line[1]), float(split_line[2])

def genralize_symmetric_matrix_market_martix(read_file):
    write_file = os.path.splitext(read_file)[0] + "_desym.mtx"
    comment, first_line = '%', True

    with open(read_file, 'r') as read, open(write_file, 'w') as write:
        for line in read:
            if line.startswith(comment):
                write.write(line)
                pass
            elif first_line:
                rows, cols, non_zeros = parse_int_int_int(line, ' ')
                sym_line = '{0} {1} {2}\n'.format(rows, cols, non_zeros*2)
                write.write(sym_line)
                first_line = False
            else:
                row, col, non_zero = parse_int_int_float(line, ' ')
                write.write(line)
                if row != col:
                    sym_line = '{0} {1} {2}\n'.format(col, row, non_zero)
                    write.write(sym_line)


def main(argv):
    try:
        opts, args = getopt.getopt(argv, "d:", ["data"])
    except getopt.GetoptError:
        print('desym-matrix.py -d <data-files-path>')
        sys.exit(2)
        
    if len(opts) == 0:
        print('desym-matrix.py -d <data-files-path>')
        sys.exit()

    for opt, arg in opts:
        if opt == '-h':
            print('desym-matrix.py -d <data-files-path>')
            sys.exit()
        elif opt in ("-d", "--data"):
            data_path = arg

    genralize_symmetric_matrix_market_martix(data_path)
    
    print ("Genralized symmetric matrix : ", data_path)

    print('_'*60)
    print("memory used by the script/process (MiB): ", psutil.Process(os.getpid()).memory_info().rss / 1024 ** 2)

if __name__ == "__main__":
    main(sys.argv[1:])