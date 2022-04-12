# Python imports
import sys

# external imports
import minify_html

if __name__ == "__main__":
    # path of this script
    # print("Running minimizer/html ({})".format(sys.argv[0]))

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    print("Minimizing HTML source {}".format(input_file))

    # open input_file, read its content
    with open(input_file) as f_in:
        source = f_in.read()

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
    with open(output_file, "w") as f_out:
        f_out.write(result)

    sys.exit(0)
