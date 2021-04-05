#! /run/current-system/sw/bin/perl

# Copyright goes to the author of this post https://stackoverflow.com/a/4807417

# usage: script regexp match_file nomatch_file < input

my $regexp = shift;
open(MATCH, ">".shift);
open(NOMATCH, ">".shift);

while(<STDIN>) {
    if (/$regexp/o) {
        print MATCH $_;
    } else {
        print NOMATCH $_;
    }
}
