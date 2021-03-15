import argparse

from parser_json import Parser as ParserJson
from parser_svg import Parser as ParserSvg

def main():
    argp = argparse.ArgumentParser()
    argp.add_argument("--log-files", nargs="+", help="these are the collected log files", required=True)
    argp.add_argument("--output", help="the output file", default="/dev/stdout")
    argp.add_argument("--svg", help="the output svg", default=None)
    args = argp.parse_args()

    parser = ParserSvg() if args.svg else ParserJson()

    files = []
    for logfile_name in args.log_files:
        with open(logfile_name) as logfile:
            files.append(list(logfile))

    parser.add_files(files)

    parser.output(args.output)

    if args.svg:
        parser._write_svg(args.svg)
