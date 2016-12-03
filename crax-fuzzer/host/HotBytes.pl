my %hash;
while (<>)
{
   while(s/\(Read\w* w\d+ (0x\w+) \w+\)/x/)
   {
      $hash{hex($1)} = 1 ;
   }
}
@keys = keys(%hash);
print $#keys+1 . "\n";
exit 0;
for my $i (1...46371)
{
   if($hash{$i})
   {
      print "H";
   }
   else 
   {
      print ".";
   }
   if($i % 64 ==0)
   {
      print "\n";
   }
   elsif ($i % 8==0)
   {
      print " ";
   }
}
