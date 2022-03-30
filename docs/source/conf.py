# SPDX-FileCopyrightText: 2022 Mischback
# SPDX-License-Identifier: MIT
# SPDX-FileType: SOURCE

# Python imports
import datetime
import os
import sys


def read_version_from_file():
    with open(os.path.join(os.path.abspath("../../"), "version.txt"), "r") as f:
        version = f.read().rstrip()

    return version


# ### Project Information

project = "KrachkisteESP32"
author = "Mischback"
copyright = "{}, {}".format(datetime.datetime.now().year, author)
version = read_version_from_file()
release = version


# ### General Configuration

# https://www.sphinx-doc.org/en/master/usage/configuration.html#confval-exclude_patterns
exclude_patterns = []

# activate extensions
extensions = [
    # automatically insert labels for section titles
    "sphinx.ext.autosectionlabel",
    # make links to other, often referenced, sites easier
    "sphinx.ext.extlinks",
    # provide links to other, sphinx-generated, documentation
    "sphinx.ext.intersphinx",
    # generate documentation for C sources
    "hawkmoth",
    # use the RTD theme
    # configuration is provided in the HTML Output section
    "sphinx_rtd_theme",
]

# "index" is already the default (since Sphinx 2.0), but better be explicit.
master_doc = "index"

# https://www.sphinx-doc.org/en/master/usage/configuration.html#confval-templates_path
templates_path = ["_templates"]

# https://www.sphinx-doc.org/en/master/usage/configuration.html#confval-source_suffix
source_suffix = {
    ".rst": "restructuredtext",
    # ".txt": "restructuredtext",
}

# ### Extension Configuration

# ##### autosectionlabel
autosectionlabel_prefix_document = True
autosectionlabel_maxdepth = 2


# ##### intersphinx

python_version = ".".join(map(str, sys.version_info[0:2]))
intersphinx_mapping = {
    "python": ("https://docs.python.org/" + python_version, None),
    # if the doc is hosted on RTD, the following should work out of the box:
    # "celery": ("https://celery.readthedocs.org/en/latest/", None),
}

# Python's docs don't change every week.
intersphinx_cache_limit = 90  # days


# ##### extlinks
extlinks = {
    # will show commit's SHA1
    "commit": ("https://github.com/Mischback/krachkiste_esp32/commit/%s", ""),
    # will show "issue [number]"
    "issue": ("https://github.com/Mischback/krachkiste_esp32/issues/%s", "issue "),
    # A file or directory. GitHub redirects from blob to tree if needed.
    # will show file/path relative to root-directory of the repository
    "source": (
        "https://github.com/Mischback/krachkiste_esp32/blob/development/%s",
        "",
    ),
    # will show "Wikipedia: [title]"
    "wiki": ("https://en.wikipedia.org/wiki/%s", "Wikipedia: "),
}

# ##### hawkmoth
cautodoc_root = os.path.abspath("../../src")
cautodoc_transformations = {
    None: None,
}

from clang.cindex import Config
Config.set_library_file("/usr/lib/llvm-11/lib/libclang.so")

# ### HTML Output

# set the theme
html_theme = "sphinx_rtd_theme"

html_theme_options = {
    # 'canonical_url': 'http://django-templ4t3.readthedocs.io',  # adjust to real url
    # 'analytics_id': 'UA-XXXXXXX-1',  #  Provided by Google in your dashboard
    # 'logo_only': False,
    # 'display_version': True,
    # 'prev_next_buttons_location': 'bottom',
    "style_external_links": True,  # default: False
    # 'vcs_pageview_mode': '',
    # 'style_nav_header_background': 'white',
    # Toc options
    # 'collapse_navigation': True,
    # 'sticky_navigation': True,
    # 'navigation_depth': 4,  # might be decreased?
    # 'includehidden': True,
    # 'titles_only': False
}

# https://www.sphinx-doc.org/en/master/usage/configuration.html#confval-html_static_path
html_static_path = ["_static"]

# provide a logo (max 200px width)
# html_logo = ""
