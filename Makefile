# SPDX-License-Identifier: MIT

# This Makefile is not really a build tool. It provides shortcuts to run certain
# tasks while developing the application and serves as a convenient way to
# launch different tools with sane default settings.
#
# Actually, some of "make"'s capabilities is used to make sure that the
# tox environments, which are used to run most of the commands, are rebuild, if
# environment specifc configuration or requirements have changed.
#
# The Makefile is self-documenting, using code from here:
# https://gist.github.com/klmr/575726c7e05d8780505a#gistcomment-3586983
# The actual implementation is right at the bottom of this file and should be
# left untouched.

# ### INTERNAL SETTINGS / CONSTANTS

# Make's internal stamp directory
# Stamps are used to keep track of certain build steps.
# Should be included in .gitignore
STAMP_DIR := .make-stamps

STAMP_TOX_UTIL := $(STAMP_DIR)/tox-util

UTIL_REQUIREMENTS := .python-requirements/util.txt

# some make settings
.SILENT :
.DELETE_ON_ERROR :
MAKEFLAGS += --no-print-directory
MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules


## Shortcut for "util/tree/project"
## @category Shortcuts
tree : util/tree/project
.PHONY : tree


## Run "cpplint" on all files
## @category Code Quality
util/cpplint : | $(STAMP_TOX_UTIL)
	$(MAKE) util/pre-commit pre-commit_id="cpplint" pre-commit_files="--all-files"
.PHONY : util/cpplint

pre-commit_id ?= ""
pre-commit_files ?= ""
## Run all code quality tools as defined in .pre-commit-config.yaml
## @category Code Quality
util/pre-commit : $(STAMP_TOX_UTIL)
	tox -q -e util -- pre-commit run $(pre-commit_files) $(pre-commit_id)
.PHONY : util/pre-commit

## Install pre-commit hooks to be executed automatically
## @category Code Quality
util/pre-commit/install : $(STAMP_TOX_UTIL)
	tox -q -e util -- pre-commit install
.PHONY : util/pre-commit/install

## Update pre-commit hooks
## @category Code Quality
util/pre-commit/update : $(STAMP_TOX_UTIL)
	tox -q -e util -- pre-commit autoupdate
.PHONY : util/pre-commit/update

## Use "tree" to list project files
## @category Utility
util/tree/project :
	tree -alI ".git|build|.tox" --dirsfirst
.PHONY : util/tree/project


# ### INTERNAL RECIPES

$(STAMP_TOX_UTIL) : $(UTIL_REQUIREMENTS) tox.ini .pre-commit-config.yaml
	$(create_dir)
	tox --recreate -e util
	touch $@

# utility function to create required directories on the fly
create_dir = @mkdir -p $(@D)
