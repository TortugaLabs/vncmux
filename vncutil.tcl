#!/usr/bin/env wish

proc vncping {host port} {
  if {[catch {socket $host $port} sock]} {
    # Connection failed, so we quit...
    return 1
  }
  fconfigure $sock -blocking 1 -buffering none -encoding binary
  set serv_version [read $sock 12]
  if {[string bytelength $serv_version] != 12} {
    close $sock
    return 2
  }
  if {![regexp {^RFB \d\d\d\.\d\d\d\n} $serv_version]} {
    close $sock
    return 3
  }
  puts -nonewline $sock "RFB 003.003\n";
  set security_type [read $sock 4]
  if {"\0\0\0\1" != $security_type} {
    close $sock
    return 4
  }
  puts -nonewline $sock "\1";
  read $sock
  close $sock
  return 0
}

