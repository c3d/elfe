#******************************************************************************
# Makefile<elfe>                                                  Elfe project
#******************************************************************************
#
#  File Description:
#
#     The top-level makefile for ELFE, the
#     Extensible Language for the Internet of Things
#
#
#
#
#
#
#
#******************************************************************************
# (C) 2015-2018 Christophe de Dinechin <christophe@dinechin.org>
#******************************************************************************

MIQ=make-it-quick/

SUBDIRS=src

include $(MIQ)rules.mk

.tests: alltests
alltests: .ALWAYS
	$(PRINT_TESTS) cd tests; ./alltests

$(MIQ)rules.mk:
	$(PRINT_BUILD) git submodule update --init --recursive
