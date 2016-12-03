$flag = 0;
while(<>)
{
   $line = $_;
   if(!($line =~ /^\s/)) {
      $flag = 0;
   }
   if($flag==1) {
      next;
   }
   if($line =~ /^KLEE: WARNING:/) {
      $flag = 1;
   } else {
      print $line;
   }
}
