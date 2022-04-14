# SPDX-FileCopyrightText: 2022 Mischback
# SPDX-License-Identifier: MIT
# SPDX-FileType: SOURCE
"""Minify HTML source code with ``minifiy-html``.

The script is basically just a tiny wrapper around ``minify-html`` and sets the
desired configuration values. These are hard-coded, to create coherent results
for all files during every run.
"""

# Python imports
import sys

# external imports
import minify_html

if __name__ == "__main__":
    # get parameters from ``argv``
    try:
        input_file = sys.argv[1]
        output_file = sys.argv[2]
    except IndexError:
        print("Please specify an INPUT / OUTPUT file!")
        sys.exit(1)

    print("Minimizing HTML source '{}'".format(input_file))

    # open input_file, read its content
    try:
        with open(input_file) as f_in:
            source = f_in.read()
    except FileNotFoundError:
        print("Could not open INPUT '{}' (file not found)!".format(input_file))
        sys.exit(1)
    except PermissionError:
        print(
            "Could not read INPUT '{}' (missing permission)!".format(input_file)
        )
        sys.exit(1)

    # run through minify_html
    # all configuration options are specified explicitly!
    result = minify_html.minify(
        source,
        do_not_minify_doctype=True,
        ensure_spec_compliant_unquoted_attribute_values=True,
        keep_closing_tags=True,
        keep_html_and_head_opening_tags=True,
        keep_spaces_between_attributes=True,
        keep_comments=False,
        minify_css=True,
        minify_js=True,
        remove_bangs=True,
        remove_processing_instructions=True
    )

    # write to output_file
    try:
        with open(output_file, "w") as f_out:
            f_out.write(result)
    except PermissionError:
        print(
            "Could not write to OUTPUT '{}' (missing permission)".format(
                output_file
            )
        )
        sys.exit(1)

    # return "0" = SUCCESS
    sys.exit(0)
