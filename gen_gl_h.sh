#!/bin/bash

{
  cat include/GL/gl.h.header 
  grep gl.*ProcPtr /System/Library/Frameworks/OpenGL.framework/Headers/gl{,ext}.h | sed 's:^.*\(gl.*Ptr\).*$:\1:' | sort -u | perl -ne 'chomp($_); $s = "PFN".uc($_); $s =~ s/PROCPTR/PROC/; print "#define ".$_." ".$s."\n"'
  cat include/GL/gl.h.footer
} > include/GL/gl.h
