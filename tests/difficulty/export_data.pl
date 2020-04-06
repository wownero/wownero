#!/usr/bin/perl

# BSD 3-Clause License
#
# Copyright (c) 2018, Karbowanec
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Returns timestamps and difficulty data for wownero (monero RPC)
# Run: perl export_data.pl 50 300

$IP='explorer.wowne.ro:11181';
$begin=$ARGV[0];
if ($#ARGV == 1 ) { $end=$ARGV[1]; }
else {
   $h=`curl -s -X POST http://$IP/json_rpc -d '{"params": {},"jsonrpc":"2.0","id":"test","method":"getblockcount"}' -H 'Content-Type: application/json'`;
   $h=~/"count"\D+(\d+)/sg;
   $end=$1;
   print "$end\n";
}
print "also printed to file\n\n";
open (F,">wownero-data.txt");
for ($i=$begin; $i<$end; $i++) { 
   $k=qq(-d '{"params":{"height":$i},"jsonrpc":"2.0","id":"test","method":"getblockheaderbyheight"}' -H 'Content-Type: application/json');
    $k=`curl -s -X POST http://$IP/json_rpc $k`;  
     $k=~/"difficulty"\D+(\d+).+"timestamp"\D+(\d+)/sg;
    ($d, $t)=($1,$2);
    print "$i\t$t\t$d\n";
    print F "$i\t$t\t$d\n";
  
}
close F;
