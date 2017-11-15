#!/usr/bin/env tclsh

proc session_file {} {global env; return [file join $env(HOME) ".vncsession"]}
proc xinit {}  {global env; return [file join $env(HOME) ".xvncinit"]}
proc localhost {} {return "localhost"}

proc _noop_ {args} {}

proc allocdisp {args} {
  set start [expr {int(rand()*65535)}]
  for {set try 0} {$try < 100} {intr try} {
    set display [expr {($try+$start)%100}]
    foreach range $args {
      set port [expr {$range + $display}]
      if {[catch {socket -server _noop_} sock]} {
	return $display
      }
      close $sock
    }
  }
  return {}
}

proc vncping {host disp} {
  set port [expr {$disp + 5900}]
  if {[catch {socket $host $port} sock]} {
    # Connection failed, so we quit...
    return 10
  }
  fconfigure $sock -blocking 1 -buffering none -encoding binary
  set serv_version [read $sock 12]
  if {[string bytelength $serv_version] != 12} {
    close $sock
    return 20
  }
  if {![regexp {^RFB \d\d\d\.\d\d\d\n} $serv_version]} {
    close $sock
    return 30
  }
  puts -nonewline $sock "RFB 003.003\n";
  set security_type [read $sock 4]
  if {"\0\0\0\1" != $security_type} {
    close $sock
    return 40
  }
  puts -nonewline $sock "\1";
  read $sock
  close $sock
  return 0
}

proc vncmgr {} {
  if {![catch {open [session_file] r} fd]} {
    set session_text [read $fd]
    close $fd
    puts stderr "READ SESSION TEXT $session_text"

    if {[regexp {^(\S+):(\d+)\s*} $session_text -> host disp]} {
      puts stderr "SESSION FOUND: $host $disp"
      if {![vncping $host $disp]} {
	puts -nonewline $session_text
	exit 0
      }
    }
  }

  set disp [allocdisp 6000 5900]
  if {$disp == ""} { exit 1 }

  puts stderr "Starting session on :$disp"
  if {[catch {open "/dev/null" r+} stdio]} {
    puts stderr "/dev/null: $stdio"
    exit 2
  }
  global tcl_platform env
  exec setsid Xvnc \
	-geometry 1024x768 \
	-depth 16 \
	-pixelformat RGB565 \
	-desktop "$tcl_platform(user)@[info hostname]" \
	-Protocol3.3 \
	-SecurityTypes None \
	:$disp <@ $stdio >& [file join $env(HOME) .xvnc.log] &
  after 1500
  set env(DISPLAY) :$disp
  if {[file executable [xinit]]} {
    exec [xinit] <@ $stdio >&@ $stdio &
  } else {
    exec xterm  <@ $stdio >&@ $stdio &
  }
  after 1500
  if {![catch {open [session_file] w} fd]} {
    set session_text "[localhost]:$disp"
    puts $fd $session_text
    close $fd
    puts $session_text
  } else {
    puts stderr "[session_file]: $fd"
    exit 4
  }
}

if {[llength $argv] == 0} {
  puts stderr "Usage:\n\t$argv0 <subcmd> \[opts\]";
  exit 1
}

switch [lindex $argv 0] {
  allocdisp {
    if {[llength $argv] < 2} {
      puts stderr "Usage:\n\t$argv allocdisp <range> \[range ...\]"
      exit 1
    }
    puts [allocdisp {*}[lrange $argv 1 end]]
  }
  vncping {
    if {[llength $argv] != 3} {
      puts stderr "Usage:\n\t$argv vncping <host> <display>"
      exit 1
    }
    exit [vncping [lindex $argv 1] [lindex $argv 2]]
  }
  vncmgr {
    vncmgr
    exit
  }
  default {
    puts stderr "Invalid sub command [lindex $argv 0]"
    exit 2
  }
}
