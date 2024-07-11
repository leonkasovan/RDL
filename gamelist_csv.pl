#!/usr/bin/perl -w
#
# Generate master text database from FBNEO driver's source code
# Usage:
# git clone https://github.com/finalburnneo/FBNeo.git 
# perl gamelist.pl FBNeo/src/burn/drv/ > FBNEO_GAMES.txt
use strict;

my $Outfile;
my $Listfile;

my @Filelist;
my %Drivers;
my @Driverlist;

# Subroutine to print all files in a directory recursively
sub process_directory_recursively {
    my ($directory) = @_;

    # Check if the provided argument is a directory
    unless (-d $directory) {
        die "'$directory' is not a directory!\n";
    }

    # Open the directory
    opendir(my $dh, $directory) or die "Cannot open directory '$directory': $!\n";
    
    # Read all the entries in the directory
    my @entries = readdir($dh);

    # Close the directory
    closedir($dh);

    foreach my $entry (@entries) {
        # Skip '.' and '..'
        next if ($entry eq '.' || $entry eq '..');

        # Full path of the entry
        my $path = "$directory/$entry";

        if (-d $path) {
            # If the entry is a directory, recurse into it
            process_directory_recursively($path);
        } elsif (-f $path) {
            # If the entry is a file, print its path
            # print "$path\n";
			if ( $entry =~ /d\w?_.+\.cpp/ ) {
				process_source_code($path);
			}
        }
    }
}

sub process_source_code {
	my $filename = shift;
	my $desc;

	open( INFILE, $filename ) or die "\nError: Couldn't read $filename $!";
	while ( my $line = <INFILE> ) {

		# Strip C++ style // comments
		if ( $line =~ /(.*?)\/\// ) {
			$line = $1;
		}

		# Strip C style /* comments */
		if ( $line =~ /(.*)\/\*/ ) {
			my $temp = $1;
			while ( $line and !($line =~ /\*\/(.*)/) ) {
				$line = <INFILE>;
			}
			$line = $temp;
			substr( $line, length( $line ) ) = $1;
		}

		# get included .cpp.h files in the same directory
		if ( $line =~ /^\s*#include "d_\w*.h"/ ) {
			$line =~ /^\s*#include "(.*)"/;
			my $newfile = $1;
			$filename =~ /(.*[\/\\])[^\/\\]/;
			push( @Filelist, "$1$newfile" );
		}

		if ( $line =~ /struct\s+BurnDriver([D|X]?)\s+(\w+)(.*)/ ) {

			# We're at the start of a BurnDriver declaration

			# Create a hash key with the name of the structure,
			# Fill the first element of the array with the driver status
			my $name = $2;
			$Drivers{$name}[0] = "$1";

			# Read the Burndriver structure into a variable
			my $struct = $3;
			do {
				$line = <INFILE>;

				# Strip C++ style // comments
				if ( $line =~ /(.*?)\/\// ) {
					$line = $1;
				}

				substr( $struct, length( $struct ) ) = $line;
			} until $line =~ /;/;

			# Strip anything after the ;
			$struct =~ /(.*;)/s;
			$struct = $1;

			# Strip C style /* comments */
			while ( $struct =~ /(.*)\/\*.*?\*\/(.*)/s ) {
				$struct = $1;
				substr( $struct, length( $struct ) ) = $2;
			}

			# We now have the complete Burndriver structure without comments

			# Extract the strings from the variable
			$struct =~ /\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0)\s*,\s*(".*"|null|0),\s*(L".*"|null|0)\s*,\s*(L".*"|null|0)\s*,\s*(L".*"|null|0)\s*,\s*(L".*"|null)\s*,\s*(\d|[^,]*)/si;

			$Drivers{$name}[1] = $1;						# Name
			$Drivers{$name}[2] = $6;						# Full name
			$Drivers{$name}[3] = $7;						# Remarks
			$Drivers{$name}[4] = $8;						# Company
			$Drivers{$name}[5] = $9;						# Hardware
			$Drivers{$name}[6] = $5;						# Year of release
			$Drivers{$name}[7] = $2;						# Parent

			if ( $14 ne "1" && ($14 =~ /BDF_GAME_WORKING/) == 0 ) {			# Working status
				$Drivers{$name}[8] = "NOT WORKING";
			} else {
				$Drivers{$name}[8] = "";
			}

			# Convert NULL/null/0 to empty string or remove quotes
			foreach $line ( @{$Drivers{$name}} ) {
				if ( $line =~ /^(null|NULL|0)/ ) {
					$line = "";
				} else {
					$line =~ /(^"?)(.*)\1/;
					$line = $2;
				}
			}

			# We only want the 1st name
			$Drivers{$name}[2] =~ /(.*)\\0.*/;
			$Drivers{$name}[2] = $1;

			$desc = "$Drivers{$name}[5]/$Drivers{$name}[4]/$Drivers{$name}[6]";
			if ($Drivers{$name}[7] ne ""){
				$desc = $desc." parent:".$Drivers{$name}[7];
			}
			if ($Drivers{$name}[3] ne ""){
				$desc = $desc." ".$Drivers{$name}[3];
			}
            # print "$Drivers{$name}[1]|$Drivers{$name}[2]|$Drivers{$name}[3]|$Drivers{$name}[4]|$Drivers{$name}[5]|$Drivers{$name}[6]|$Drivers{$name}[7]|$Drivers{$name}[8]|$Drivers{$name}[9]\n";
			print "$Drivers{$name}[1]|$Drivers{$name}[2]|$desc\n";
		}
	}
	close( INFILE );
}

print "1.Name|2.Full Name|3.Description\n";
# Process command line arguments
for ( my $i = 0; $i < scalar @ARGV; $i++ ) {{
	process_directory_recursively($ARGV[$i]);
}}



