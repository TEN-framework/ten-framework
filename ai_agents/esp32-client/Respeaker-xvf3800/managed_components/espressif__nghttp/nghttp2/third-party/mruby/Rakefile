# Build description.
# basic build file for mruby
MRUBY_ROOT = File.dirname(File.expand_path(__FILE__))
MRUBY_BUILD_HOST_IS_CYGWIN = RUBY_PLATFORM.include?('cygwin')
MRUBY_BUILD_HOST_IS_OPENBSD = RUBY_PLATFORM.include?('openbsd')

Rake.verbose(false) if Rake.verbose == Rake::DSL::DEFAULT

$LOAD_PATH << File.join(MRUBY_ROOT, "lib")

# load build systems
require "mruby/core_ext"
require "mruby/build"

# load configuration file
MRUBY_CONFIG = MRuby::Build.mruby_config_path
load MRUBY_CONFIG

# load basic rules
MRuby.each_target do |build|
  build.define_rules
end

# load custom rules
load "#{MRUBY_ROOT}/tasks/core.rake"
load "#{MRUBY_ROOT}/tasks/mrblib.rake"
load "#{MRUBY_ROOT}/tasks/mrbgems.rake"
load "#{MRUBY_ROOT}/tasks/libmruby.rake"
load "#{MRUBY_ROOT}/tasks/bin.rake"
load "#{MRUBY_ROOT}/tasks/presym.rake"
load "#{MRUBY_ROOT}/tasks/test.rake"
load "#{MRUBY_ROOT}/tasks/benchmark.rake"
load "#{MRUBY_ROOT}/tasks/doc.rake"
load "#{MRUBY_ROOT}/tasks/install.rake"

##############################
# generic build targets, rules
task :default => :all

desc "build all targets, install (locally) in-repo"
task :all => :gensym do
  Rake::Task[:build].invoke
  puts
  puts "Build summary:"
  puts
  MRuby.each_target do |build|
    build.print_build_summary
  end
  MRuby::Lockfile.write
end

task :build => MRuby.targets.flat_map{|_, build| build.products}

desc "clean all built and in-repo installed artifacts"
task :clean do
  MRuby.each_target do |build|
    rm_rf build.build_dir
    rm_f build.products
  end
  puts "Cleaned up target build directory"
end

desc "clean everything!"
task :deep_clean => %w[clean doc:clean] do
  MRuby.each_target do |build|
    rm_rf build.gem_clone_dir
  end
  rm_rf "#{MRUBY_ROOT}/bin"
  rm_rf "#{MRUBY_ROOT}/build"
  puts "Cleaned up mrbgems build directory"
end

desc "run all pre-commit hooks against all files"
task :check do
  sh "pre-commit run --all-files"
end

desc "install the pre-commit hooks"
task :checkinstall do
  sh "pre-commit install"
end

desc "check the pre-commit hooks for updates"
task :checkupdate do
  sh "pre-commit autoupdate"
end

desc "run all pre-commit hooks against all files with docker-compose"
task :composecheck do
  sh "docker-compose -p mruby run test pre-commit run --all-files"
end

desc "build and run all mruby tests with docker-compose"
task :composetest do
  sh "docker-compose -p mruby run test"
end
