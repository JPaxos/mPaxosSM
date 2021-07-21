#!/usr/bin/env perl

use 5.32.0;
use strict;
use warnings;
use Scalar::Util qw(looks_like_number);
use experimental qw(switch);

use constant USAGE => "\n $0 {ram|fakepmem|pmem} <requestSizeInBytes>\nReturns window size and batching size, for instance:\n 5 65536\n";

die "Bad argument count" . USAGE unless @ARGV==2;

my $model = $ARGV[0];
die "Bad model name" . USAGE unless $model =~ /^ram|fakepmem|pmem$/;

my $reqSize = $ARGV[1];
die "Request size does not look like a number" . USAGE unless looks_like_number $reqSize;

my $ws="UNDEFINED";
my $bs="UNDEFINED";

given($model){
    when ("ram"){
        given($reqSize){
            when($_ <=    1024){ ($ws,$bs) = (3,  48*1024);}
            when($_ <=  2*1024){ ($ws,$bs) = (5,  48*1024);}
            when($_ <=  3*1024){ ($ws,$bs) = (5,  64*1024);}
            default            { ($ws,$bs) = (7,  80*1024);}
        }
    }
    when (/^fakepmem|pmem$/){
        # throughput is so dominated by execution that BS and WS have little impact
        ($ws,$bs) = (5, 128*1024);
    }
    default {
        die "should never happen";
    }
}

print "$ws $bs\n";
