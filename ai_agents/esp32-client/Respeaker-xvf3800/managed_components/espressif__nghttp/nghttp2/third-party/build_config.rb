def config(conf)
  toolchain :clang if ENV['MRUBY_CC'].include? "clang"
  toolchain :gcc if ENV['MRUBY_CC'].include? "gcc"

  conf.cc.command = ENV['MRUBY_CC']
  conf.cxx.command = ENV['MRUBY_CXX']

  if ENV['MRUBY_LD']
    conf.linker.command = ENV['MRUBY_LD']
  end
  if ENV['MRUBY_AR']
    conf.archiver.command = ENV['MRUBY_AR']
  end

  # C++ project needs this.  Without this, mruby exception does not
  # properly destroy C++ object allocated on stack.
  conf.enable_cxx_exception

  conf.build_dir = ENV['BUILD_DIR']

  # Here is the mruby gems included in default.gembox minus
  # mruby-bin-debugger which causes the application to crash.
  conf.gembox "stdlib"
  conf.gembox "stdlib-ext"
  conf.gembox "stdlib-io"
  conf.gembox "math"
  conf.gembox "metaprog"

  # Generate mrbc command
  conf.gem :core => "mruby-bin-mrbc"

  # Generate mirb command
  conf.gem :core => "mruby-bin-mirb"

  # Generate mruby command
  conf.gem :core => "mruby-bin-mruby"

  # Generate mruby-strip command
  conf.gem :core => "mruby-bin-strip"

  # Generate mruby-config command
  conf.gem :core => "mruby-bin-config"

  # Added by nghttp2 project
  conf.gem :core => 'mruby-eval'
end

if ENV['BUILD'] == ENV['HOST'] then
  MRuby::Build.new do |conf|
    config(conf)
  end
else
  MRuby::CrossBuild.new(ENV['HOST']) do |conf|
    config(conf)
  end
end
