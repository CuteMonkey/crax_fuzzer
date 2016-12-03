#!/usr/bin/perl
print `s2e/s2eget/s2eget s2e_input`;
print `s2e/s2eget/s2eget s2e_command`;
$command = `cat s2e_command`;
print `./$command`;
