#!/usr/bin/env perl

use 5.26.0;
use strict;
use warnings;

use feature 'say';

use DBI;
use DBD::SQLite::Constants qw/:file_open/;
use Data::Dumper;
use Env;

#https://metacpan.org/pod/DBI

if( defined $ENV{"HEADER"}){
    print "id run";
    printf " starttime-up";
    printf " cuFirstQuerySent-starttime";
    printf " cuQueryRcvd-cuFirstQuerySent";
    
    printf " cuSnapSent-cuQueryRcvd";
    printf " cuSnapRcvd-cuSnapSent";
    printf " cuSnapUnpa-cuSnapRcvd";
    printf " cuSnapAppl-cuSnapUnpa";
    printf " cuRespSent-cuSnapSent";
    printf " endtime-cuSnapAppl";
    printf "\n";
    
    print "id run";
    printf " time_to_start_CU";
    printf " time_to_send_Q";
    printf " Q_transmit_time";
    
    printf " generating_snap";
    printf " snap_transmit_time";
    printf " snap_write_files";
    printf " rec_from_snap";
    printf " generating_R";
    printf " after_snap_apply_to_CU_end";
    printf "\n";
    
    exit unless ( $ENV{"HEADER"} eq "1");
    
}


my $dbpath = $ARGV[0] // "jpdb.sqlite3";

my $dbh = DBI->connect("dbi:SQLite:dbname=$dbpath",undef,undef,{sqlite_open_flags => SQLITE_OPEN_READONLY})
    or die "Could not open database: " . $DBI::errstr;
    
my $cuStartsStmt = $dbh->prepare("select id, run, time from catchupStart order by time asc");
# note to self: perl sqlite acts as if single quotation marks were added around bind parameters unless explictly asked not to do it; sqlite +0 is quicker
my $cuEndStmt = $dbh->prepare("select time from catchupEnd where id = ?+0 and run = ?+0 and time > ?+0                  order by time asc  limit 1");
my $cuFirstQuerySendStmt = $dbh->prepare("select time, rcpt from catchQuerySent where id = ?+0 and run = ?+0 and time >= ?+0 and time <= ?+0 order by time asc limit 1");
my $cuQueryRcvdStmt = $dbh->prepare("select time from catchQueryRcvd where sender = ?+0 and id = ?+0 and time >= ?+0 and time <= ?+0 order by time asc");
my $cuSnapSentStmt = $dbh->prepare("select time from catchSnapSent where rcpt = ?+0 and id = ?+0 and time >= ?+0 and time <= ?+0 order by time asc");
my $cuSnapRcvdStmt = $dbh->prepare("select time from catchSnapRcvd where id = ?+0 and run = ?+0 and time >= ?+0 and time <= ?+0 order by time asc limit 1");
my $cuSnapUnpaStmt = $dbh->prepare("select time from catchSnapUnpacked where id = ?+0 and run = ?+0 and time >= ?+0 and time <= ?+0 order by time asc limit 1");
my $cuSnapApplStmt = $dbh->prepare("select time from catchSnapApplied where id = ?+0 and run = ?+0 and time >= ?+0 and time <= ?+0 order by time asc limit 1");
my $cuRespSentStmt = $dbh->prepare("select time from catchRespSent where rcpt = ?+0 and id = ?+0 and time >= ?+0 and time <= ?+0 order by time asc");
my $cuRespRcvdStmt = $dbh->prepare("select time from catchRespRcvd where id = ?+0 and run = ?+0 and time >= ?+0 and time <= ?+0 order by time asc limit 1");

my $upStmt = $dbh->prepare("select time from start where id = ?+0 and run = ?+0 and time <= ?+0 order by time asc limit 1");

$cuStartsStmt->execute();
my $cuStarts = $cuStartsStmt->fetchall_arrayref;

foreach (@$cuStarts){
    my ($id, $run, $starttime) = @$_;

    $upStmt->execute($id, $run, $starttime);
    my ($up) = $upStmt->fetchrow_array;
    
    # check when it edned
    $cuEndStmt->execute($id, $run, $starttime);
    my ($endtime) = $cuEndStmt->fetchrow_array;
    
    # common bind parameters
    my @wheresMe = ($id, $run, $starttime, $endtime);
    
    $cuFirstQuerySendStmt->execute(@wheresMe);
    my ($cuFirstQuerySent, $cuQueryRcpt) = $cuFirstQuerySendStmt->fetchrow_array;
    
    my @wheresPeer = ($id, $cuQueryRcpt, $starttime, $endtime);
    
    $cuQueryRcvdStmt->execute(@wheresPeer);
    my ($cuQueryRcvd) = $cuQueryRcvdStmt->fetchrow_array;
    
    $cuSnapSentStmt->execute(@wheresPeer);
    my ($cuSnapSent) = $cuSnapSentStmt->fetchrow_array;
    $cuSnapRcvdStmt->execute(@wheresMe);
    my ($cuSnapRcvd) = $cuSnapRcvdStmt->fetchrow_array;
    $cuSnapUnpaStmt->execute(@wheresMe);
    my ($cuSnapUnpa) = $cuSnapUnpaStmt->fetchrow_array;
    $cuSnapApplStmt->execute(@wheresMe);
    my ($cuSnapAppl) = $cuSnapApplStmt->fetchrow_array;
    
    $cuRespSentStmt->execute(@wheresPeer);
    my ($cuRespSent) = $cuRespSentStmt->fetchrow_array;
    $cuRespRcvdStmt->execute(@wheresMe);
    my ($cuRespRcvd) = $cuRespRcvdStmt->fetchrow_array;
    
    
    # print "$id | $run | U: $up | S: $starttime | Q→ $cuFirstQuerySent ($cuQueryRcpt) | →Q $cuQueryRcvd | S→ $cuSnapSent | →S $cuSnapRcvd | S! $cuSnapAppl | R→ $cuRespSent | →R $cuRespRcvd | E: $endtime | \n";
    
    print "$id $run";
    printf " %f" ,  $starttime-$up;
    printf " %f" ,  $cuFirstQuerySent-$starttime;
    printf " %f" ,  $cuQueryRcvd-$cuFirstQuerySent;

    if (defined $cuSnapSent){
        printf " %f" ,  $cuSnapSent-$cuQueryRcvd;
        printf " %f" ,  $cuSnapRcvd-$cuSnapSent;
        printf " %f" ,  $cuSnapUnpa-$cuSnapRcvd;
        printf " %f" ,  $cuSnapAppl-$cuSnapUnpa;
        printf " %f" ,  $cuRespSent-$cuSnapSent;
        printf " %f" ,  $endtime-$cuSnapAppl;
    } else {
        printf " - - - -";
        printf " %f" ,  $cuRespSent-$cuFirstQuerySent;
        printf " %f" ,  $endtime-$cuRespSent;
    }
    
    printf "\n";
    
}
