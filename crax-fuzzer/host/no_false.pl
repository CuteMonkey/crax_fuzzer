while(<>)
{
   $line = $_;
   if($line =~ /Testing/) {
      my $tmp = $line;
      $line = <>;
      if($line =~ /true/) {
         print $tmp;
         print $line;
      }
   } else {
      print $line;
   }
}
