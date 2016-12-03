#!/usr/bin/perl
use strict;
my $kleaver = "/home/hchung/sqlab/hchung/llvm/klee/Release+Asserts/bin/kleaver";
my $array_name = "v0_crax_0";
my $array_size = 1024;
my $offset = 0;
my $input_file = "exploit.vqf";
my $output_file = "out.vqf";
##### End of config
my $type = "";
my @const;
my @expr;
sub run_kleaver {
   (my $expr, my $value) = @_;
   my $temp = $expr;
   my @index;
   while ($temp =~ s/\(Read\w* w(\d+) (0x\w+) \w+\)/x/) {
      for(my $i=$1/8;$i>0;$i=$i-1) {
         push @index, hex($2)+$1/8-$i;
         print hex($2)+4-$i;
         print "\n\n"; 
      }
   }
   @index = sort { $a <=> $b } @index;
   my $kquery = "array ${array_name}[$array_size] : w32 -> w8 = symbolic\n
(query [(Eq $value $expr)] false [] [ $array_name ])";
   print $kquery;
   print "\n";
   my $result = `echo '$kquery' | $kleaver`;
   print $result;
   if($result =~ /INVALID/) {
      print "SUCC\n";
      my $buff = "";
      my $buff_index = 0;
      my @result_array;
      if($result =~ /$array_name\[([\d|,| ]+)\]/) {
         @result_array = split(/, /, $1);
         open(FIN, "<", $input_file) or die "FIN";
         open(FOUT, ">", $output_file) or die "FOUT";
         binmode(FIN);
         binmode(FOUT);
         my $front = shift @index;
         while ( read (FIN, $buff, 1)) {
            if($buff_index==$front) {
               $buff = chr($result_array[$front]);
               print "$front\n";
               $front = shift @index;
            }
            print FOUT $buff;
            $buff_index = $buff_index +1;
         }
         close (FIN);
         close (FOUT);
      }
      return 0;
   }
   print "FAIL\n";
}

sub print_resault {
   if(!length $type) {
      return;
   }
   print "$type\n";
   foreach (@const) {
      #print chr(hex($_));
   }
   print "\n";
   foreach (@const) {
      print "$_ ";
   }
   print "\n";
   foreach (@expr) {
      print "$_\n";
   }

   if($type =~ /_format/) {
      print "Format string bug found!\n";
   } elsif ($type =~ /_size/) {
   }
   print "---\n";
}

my $buff = "";
my $space_flag = 0;
my $kleaver = shift;
if($kleaver) {
   run_kleaver("(Or w32 (Or w32 (Shl w32 (Concat w32 (w8 0x0) (Concat w24 (w8 0x0) (Concat w16 (w8 0x0) (Read w8 0x3f v0_crax_0)))) (w32 0x18)) (And w32 (Shl w32 (Concat w32 (w8 0x0) (Concat w24 (w8 0x0) (Concat w16 (Read w8 0x40 v0_crax_0) (w8 0x0)))) (w32 0x8)) (w32 0xff0000))) (Or w32 (And w32 (LShr w32 (Concat w32 (w8 0x0) (Concat w24 (Read w8 0x41 v0_crax_0) (w16 0x0))) (w32 0x8)) (w32 0xff00)) (LShr w32 (Concat w32 (Read w8 0x42 v0_crax_0) (w24 0x0)) (w32 0x18))))", 0xffef);
   #run_kleaver("(Shl w32 (Extract w32 0 (UDiv w64 (ZExt w64 (Add w32 (w32 0xffffffff) (Add w32 N0:(ReadLSB w32 0x72 v0_file_0) (ReadLSB w32 0x2a v0_file_0)))) (ZExt w64 N0))) (w32 0x2))", 0x0000000);
   #run_kleaver("(Shl w32 (Add w32 (Shl w32 N0:(ZExt w32 (ReadLSB w16 0x8 v0_file_0)) (w32 0x1)) N0) (w32 0x2))", 0xffffffff);
   exit 0;
}
while(<>)
{
   my $line = $_;
   if($line =~ /^SymbExpression (\w+) - (.+$)/) {
      if(length $buff) {
         push @expr, $buff;
         $buff = "";
      }
      $space_flag = 1;
      $buff = $2;
      $type = $1;
      if($buff =~ /Value: (.+$)/) {
         push @const, $1;
         $buff = "";
      } else {
      }
   }
   elsif(!($line =~ /^\s/)) {   # no interest
      if(length $buff) {
         push @expr, $buff;
         $buff = "";
      }
      $space_flag = 0;
      if($line =~ /true/) {   # start
         print_resault();

         $type = "";
         @const = ();
         @expr = ();
      }
   }
   elsif($space_flag==1) {
      if($line =~ /^\s*(.+$)/) {
         $buff = "$buff $1";
      }
   }
}

print_resault();
