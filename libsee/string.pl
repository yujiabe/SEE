#!/usr/bin/env perl
# $Id$
#
# The author of this software is David Leonard.
#
# Copyright (c) 2003
#      David Leonard.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by David Leonard and
#      contributors.
# 4. Neither the name of Mr Leonard nor the names of the contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY DAVID LEONARD AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL DAVID LEONARD OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

#
# Usage:   string.pl [h|c] string.defs
#
# Readss a file of string definitions and outputs C code that compiles
# to static UTF-16 strings. Each string definitions must fit one of these
# forms:
#
#	<ident>
#	<ident> = "text"
#
# Blank lines and lines starting with a hash ('#') are ignored.
#
# There are two modes: 'h' and 'c', corresponding to generation of
# the C header file, or the C compilation unit.
#

$mode = $ARGV[0];
$file = $ARGV[1];
open(F, "<$file") || die "$file: $!\n";

print "/* This file generated from ${file} */\n";

if ($mode eq 'h') {
	print 
	      "#define STR(x) (&SEE_stringtab[SEE_STR_##x])\n".
	      "struct SEE_string;\n".
	      "extern struct SEE_string SEE_stringtab[];\n";
} else {
	
}

$index = 0;
$exitcode = 0;
$tabs = "";
%linedef = ();
while (<F>) {
	next if m/^\s*#/ or m/^\s*$/;

	my $name, $text;

	if (m/^\s*(\S+)\s*=\s*"(.*)"\s*;?\s*$/) {
		$name = $1;
		$text = $2;
	} elsif (m/^\s*(\S+)\s*$/) {
		$name = $text = $1;
	} else {
		die "${file}:$.: bad line\n";
	}

	if (defined($linedef{$name})) {
		print STDERR "$file:$.: string '${name}' previously defined ".
				"at line ".  $linedef{$name}. "\n";
		$exitcode = 1;
		next;
	}
	$linedef{$name} = $.;

	if ($mode eq 'h') {
		print "#define SEE_STR_${name} ${index}\n";
	} else {
		$strlen = 0;
		$text =~ s/\\[0-7]{1,3}|\\u(....)|\\.|./
		    $strlen++;
		    ($& eq "'" ? "'\\''"
		    : substr($&,0,2) eq "\\u" ? "0x$1"
		    : "'$&'").",".($strlen % 10 == 0 ? "\n\t  " : " ")/eg;
		$text =~ s/,[\n\t ]*$//;
		print "static SEE_char_t s__${index}[] = {\n\t  ${text} };\n";
		if ($index > 0) { $tabs .= ","; }
		$tabs .= "\n\t{ ${strlen}, s__${index}, NULL, NULL, SEE_STRING_FLAG_INTERNED }";
	}
	$index++;
}

if ($mode eq 'h') {
	print "#define _SEE_STR_MAX ${index}\n";
} else {
	print ";\n\n";
	print "struct SEE_string SEE_stringtab[] = {";
	print $tabs;
	print " };\n";
}

exit($exitcode);
