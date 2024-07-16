#!/usr/bin/perl -n

BEGIN {
	do './util/start.pl';

	$| = 1;

	my $hex = qr/[a-fA-F0-9]/;

	sub toLine($) {
		my $h = shift;

		if    (hex($h) >= hex($START_UTENTE))	{ $exe = 'build/utente';  }
		elsif (hex($h) >= hex($START_IO))	{ $exe = 'build/io';      }
		else 				      	{ $exe = "build/sistema"; }

		my $out = `$CE_ADDR2LINE -Cfe $exe $h`;
		if ($?) {
			return "0x$h";
		}
		chomp $out;
		my @lines = split(/\n/, $out);
		if ($lines[1] =~ /^\?\?/) {
			return "0x$h";
		}
		my $s = '';
		$lines[1] =~ s#^.*/##;
		if ($lines[0] ne '??') {
			$s .= $lines[0];
		}
		$s .= ' [' . $lines[1] . ']';
		return $s;
	}

	sub decodeAddr($) {
		my $s = shift;
		$s =~ s#(?<!$hex)0x($hex{1,16})(?!$hex)#&toLine($1)#meg;
		return $s;
	}
}

chomp;
my ($level, $id, $msg) = split /\t/;
if ($level ne "USR") { 
	$msg = &decodeAddr($msg);
}
print "$level\t$id\t$msg\n";
