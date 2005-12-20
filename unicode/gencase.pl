#! /usr/bin/env perl

%lower = ();
%upper = ();
%special_upper = ();
%special_lower = ();
open(DATA, "<UnicodeData.txt") || die "UnicodeData.txt: $!";
while (<DATA>) {
	next if m/^#/ || m/^\s*$/;
	my @line = split(/\s*;\s*/);
	my $cp = hex($line[0]);
	my $u = hex($line[12] or 0);
	my $l = hex($line[13] or 0);
	$upper{$cp} = $u if $u != $cp and $u;
	$lower{$cp} = $l if $l != $cp and $l;
}
open(CASING, "<SpecialCasing.txt") || die "SpecialCasing.txt: $!";
while (<CASING>) {
	s/^\s*|\s*$|\s*#.*//g;
	next if m/^$/;
	my @line = split(/\s*;\s*/);
	my $cp = hex($line[0]);
	my $l = hex($line[1]) or 0;
	my $u = hex($line[3]) or 0;
	if ($line[4] eq "")  {
		#-- ignore character conversions that are multichar
		$lower{$cp} = $l if $l != $cp and $l and $l !~ m/\s/;
		$upper{$cp} = $u if $u != $cp and $u and $u !~ m/\s/;
	} else {
		#print "$_\n";
		$special_lower{$cp} = [$l,join(',',@line[4 .. $#line])]
			if $l != $cp and $l and $l !~ m/\s/;
		$special_upper{$cp} = [$u,join(',',@line[4 .. $#line])]
			if $u != $cp and $u and $u !~ m/\s/;
	}
}

if (0) {
    print "Lower->Upper:\n";
    $deltamax = $deltamin = undef;
    foreach $cp(sort { $a <=> $b } keys %lower) { 
	printf(" %04x %04x\n", $cp, $lower{$cp}); 
	my $delta = $cp - $lower{$cp};
	$deltamax = $delta if !defined($deltamax) or $delta > $deltamax;
	$deltamin = $delta if !defined($deltamin) or $delta < $deltamin;
    }
    print " delta min=$deltamin max=$deltamax\n";
    
    print "\nUpper->Lower:\n";
    $deltamax = $deltamin = undef;
    foreach $cp(sort { $a <=> $b } keys %upper) { 
	printf("  %04x %04x\n", $cp, $upper{$cp}); 
	my $delta = $cp - $upper{$cp};
	$deltamax = $delta if !defined($deltamax) or $delta > $deltamax;
	$deltamin = $delta if !defined($deltamin) or $delta < $deltamin;
    }
    print " delta min=$deltamin max=$deltamax\n";


    print "Lower->Upper [SPECIAL]:\n";
    foreach $cp(sort { $a <=> $b } keys %special_lower) { 
	my @e = @{$special_lower{$cp}};
	printf("  %04x %04x [%s]\n", $cp, $e[0], join(",",$e[1]));
    }

    print "Upper->Lower [SPECIAL]:\n";
    foreach $cp(sort { $a <=> $b } keys %special_upper) { 
	my @e = @{$special_upper{$cp}};
	printf("  %04x %04x [%s]\n", $cp, $e[0], join(",",$e[1]));
    }
    exit 0;
}

#
# Some code to exhaustive build n-ary search trees and find the
# minimum sized search tree for the map
#
sub build_tables {
	my $codepoints = shift(@_);	# [cp,...]
	my $left = shift(@_);
	my $shift = shift(@_);
	my $bits = shift(@_);		# rest of args should sum to 16
	my $mask = (1<<$bits) - 1;
	my $parts = [];
	my $nparts = 0;

        foreach $cp(@$codepoints) { 
	    my $p = ($cp >> $shift) & $mask;
	    if (!defined($parts->[$p])) { $parts->[$p] = []; $nparts++; } 
	    push(@{$parts->[$p]}, $cp);
	}

	#print "shift=$shift bits=$bits nparts=$nparts\n" if $nparts;

	my $totalsize = 1<<$bits;
	if (@_) {
	    foreach $part(@$parts) {
		my  $subsize =
		&build_tables($part, $left, $left ? $shift + $bits :
						    $shift - $bits, @_);
		$totalsize += $subsize;
	    }
	}
	return $totalsize;
}

sub gen_levels {
	my $cp = shift(@_);
	my $index = shift(@_);
	my @bits = (@_);
#print "gen_levels $index (".join(",",@bits).") bits=$#bits\n";
	my $bi = $bits[$index];
	if ($index == $#bits) {
	    my $sz = &build_tables($cp, 1, 0, @bits);
	    #print join(",", @bits)." -> $sz\n";
	    return $sz;
	} else {
	    my $sz, $minsz = undef;
	    for (my $i = $#bits-$index; $i < $bi; $i++)  {
		$bits[$index] = $bi - $i;
		$bits[$index + 1] = $i;
		$sz = &gen_levels($cp, $index + 1, @bits);
		if (!defined($minsz) or $sz < $minsz) {
			$minsz = $sz;
			$MINSET = [@bits];
		}
	    }
	    #print "minsz=$minsz (".join(",",@bits[0..$index]).")\n";
	    return $minsz;
	}
    }

#$total = &build_tables([keys %lower], 1, 0, 3, 5, 4, 4);
#print "total size: $total\n";

#@k = keys %lower; print "lower keys: ".($#k + 1)."\n";
#for $zeros ([0],[0,0],[0,0,0],[0,0,0,0],[0,0,0,0,0]) {
    #$minsz = gen_levels([keys %lower],0,16,@$zeros);
    #print "lower keys: $minsz (".join(",",@{$MINSET}).")\n";
#}


#-- print case maps for binary searches
print "
/* This file is generated. Do not edit. */

static struct case_map lowercase_map[] = {
";
$first = 0;
foreach $cp(sort { $a <=> $b } keys %lower) { 
    if ($first++) { print ",\n"; }
    printf("      { 0x%04x, 0x%04x }", $cp, $lower{$cp}); 
}
print "\n};

static struct case_map uppercase_map[] = {
";
$first = 0;
foreach $cp(sort { $a <=> $b } keys %upper) { 
    if ($first++) { print ",\n"; }
    printf("      { 0x%04x, 0x%04x }", $cp, $upper{$cp}); 
}
print "\n};\n";

