import yaramod
import os

current_path = os.path.abspath(os.path.dirname(__file__))
yararules_dir = current_path + "/yararules/"


def main():
    rules = yara.compile(filepath = yararules_dir + "anonymous_strings")
    matches = rules.match("/opt/samples/anonymous_strings")
    print("Matches: " + str(matches))


if __name__ == "__main__":
    main()