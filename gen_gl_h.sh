#!/bin/bash

{
  cat include/GL/gl.h.header 
  if grep -q GL_GLEXT_PROTOTYPES /System/Library/Frameworks/OpenGL.framework/Headers/gl.h ; then
    grep gl.*ProcPtr /System/Library/Frameworks/OpenGL.framework/Headers/gl{,ext}.h | sed 's:^.*\(gl.*Ptr\).*$:\1:' | sort -u | perl -ne 'chomp($_); $s = "PFN".uc($_); $s =~ s/PROCPTR/PROC/; print "#define ".$_." ".$s."\n"'
  fi
  cat include/GL/gl.h.core

  if ! grep -q GL_GLEXT_PROTOTYPES /System/Library/Frameworks/OpenGL.framework/Headers/gl.h ; then
    { 
      echo "#define GL_GLEXT_FUNCTION_POINTERS 1"
      echo "#define GL_GLEXT_LEGACY 1"
      grep gl.*ProcPtr /System/Library/Frameworks/OpenGL.framework/Headers/gl.h | sed 's:^.*\(gl.*Ptr\).*$:\1:' | sort -u | perl -ne 'chomp($_); $s = "PFN".uc($_); $s =~ s/PROCPTR/PROC/; print "#define ".$_." ".$s."\n"'
      echo '#include "/System/Library/Frameworks/OpenGL.framework/Headers/gl.h"'
    } | gcc -E - | grep typedef.*PFN
  fi
  cat include/GL/gl.h.footer
} > include/GL/gl.h
