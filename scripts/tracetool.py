#!/usr/bin/env python3

"""
Command-line wrapper for the tracetool machinery.
"""

__author__     = "Lluís Vilanova <vilanova@ac.upc.edu>"
__copyright__  = "Copyright 2012-2014, Lluís Vilanova <vilanova@ac.upc.edu>"
__license__    = "GPL version 2 or (at your option) any later version"

__maintainer__ = "Stefan Hajnoczi"
__email__      = "stefanha@redhat.com"


import sys
import getopt

from tracetool import error_write, out, out_open
import tracetool.backend
import tracetool.format


_SCRIPT = ""

def error_opt(msg = None):
    if msg is not None:
        error_write("Error: " + msg + "\n")

    backend_descr = "\n".join([ "    %-15s %s" % (n, d)
                                for n,d in tracetool.backend.get_list() ])
    format_descr = "\n".join([ "    %-15s %s" % (n, d)
                               for n,d in tracetool.format.get_list() ])
    error_write("""\
Usage: %(script)s --format=<format> --backends=<backends> [<options>] <trace-events> ... <output>

Backends:
%(backends)s

Formats:
%(formats)s

Options:
    --help                   This help message.
    --list-backends          Print list of available backends.
    --check-backends         Check if the given backend is valid.
    --group <name>           Name of the event group.
""" % {
            "script" : _SCRIPT,
            "backends" : backend_descr,
            "formats" : format_descr,
            })

    if msg is None:
        sys.exit(0)
    else:
        sys.exit(1)

def main(args):
    global _SCRIPT
    _SCRIPT = args[0]

    long_opts = ["backends=", "format=", "help", "list-backends",
                 "check-backends", "group="]

    try:
        opts, args = getopt.getopt(args[1:], "", long_opts)
    except getopt.GetoptError as err:
        error_opt(str(err))

    check_backends = False
    arg_backends = []
    arg_format = ""
    arg_group = None
    for opt, arg in opts:
        if opt == "--help":
            error_opt()

        elif opt == "--backends":
            arg_backends = arg.split(",")
        elif opt == "--group":
            arg_group = arg
        elif opt == "--format":
            arg_format = arg

        elif opt == "--list-backends":
            public_backends = tracetool.backend.get_list(only_public = True)
            out(", ".join([ b for b,_ in public_backends ]))
            sys.exit(0)
        elif opt == "--check-backends":
            check_backends = True

        else:
            error_opt("unhandled option: %s" % opt)

    if len(arg_backends) == 0:
        error_opt("no backends specified")

    if check_backends:
        for backend in arg_backends:
            if not tracetool.backend.exists(backend):
                sys.exit(1)
        sys.exit(0)

    if arg_group is None:
        error_opt("group name is required")

    if len(args) < 2:
        error_opt("missing trace-events and output filepaths")
    events = []
    for arg in args[:-1]:
        with open(arg, "r") as fh:
            events.extend(tracetool.read_events(fh, arg))

    out_open(args[-1])

    try:
        tracetool.generate(events, arg_group, arg_format, arg_backends)
    except tracetool.TracetoolError as e:
        error_opt(str(e))

if __name__ == "__main__":
    main(sys.argv)
