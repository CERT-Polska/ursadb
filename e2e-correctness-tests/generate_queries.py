import argparse
import os
from yaraparse import parse_yara, combine_rules

current_path = os.path.abspath(os.path.dirname(__file__))
testdir = current_path + "/yararules/"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate result files or file from yara rule file."
    )

    parser.add_argument("file_name", nargs="?", help="File name", default="")

    args = parser.parse_args()

    if args.file_name:
        process_rule(args.file_name)

    else:
        yara_files = [f for f in os.listdir(testdir) if ".txt" not in f]

        for file in yara_files:
            process_rule(file)


def process_rule(in_file):
    with open(testdir + in_file) as f:
        data = f.read()

    result_txt = testdir + in_file + ".txt"
    write_rules_to_file(data, result_txt)


def write_rules_to_file(data, result_txt):
    rules = parse_yara(data)
    with open(result_txt, "w") as fp:
        result = combine_rules(rules).query
        print(result)
        fp.write("select " + result[2:-2] + ";" "\n")


if __name__ == "__main__":
    main()
