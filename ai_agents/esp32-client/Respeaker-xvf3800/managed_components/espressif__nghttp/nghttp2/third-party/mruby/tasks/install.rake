desc "install compiled products (on host)"
task :install => "install:full:host"

desc "install compiled executable (on host)"
task :install_bin => "install:bin:host"

desc "install compiled products (all build targets)"
task "install:full"

desc "install compiled executable (all build targets)"
task "install:bin"

MRuby.each_target do |build|
  next if build.internal?

  prefix = File.join(MRuby::INSTALL_DESTDIR, build.install_prefix)

  task "install:full" => "install:full:#{build.name}"

  task "install:full:#{build.name}" => "install:bin:#{build.name}" do
    Dir.glob(File.join(build.build_dir.gsub(/[\[\{\*\?]/, "\\\0"), "{include,lib}/**/*")) do |path|
      install_D path, File.join(prefix, path.relative_path_from(build.build_dir)) if File.file? path
    end
  end

  task "install:bin" => "install:bin:#{build.name}"

  task "install:bin:#{build.name}" => "all" do
    Dir.glob(File.join(build.build_dir.gsub(/[\[\{\*\?]/, "\\\0"), "{bin,host-bin}/**/*")) do |path|
      install_D path, File.join(prefix, path.relative_path_from(build.build_dir)) if File.file? path
    end
  end
end
