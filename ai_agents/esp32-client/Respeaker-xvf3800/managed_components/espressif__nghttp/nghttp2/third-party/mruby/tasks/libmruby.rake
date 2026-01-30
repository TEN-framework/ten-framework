MRuby.each_target do
  file libmruby_core_static => libmruby_core_objs.flatten do |t|
    archiver.run t.name, t.prerequisites
  end

  products << libmruby_core_static

  next unless libmruby_enabled?

  copy_headers_task = "expose_header_files:#{self.name}"
  file libmruby_static => libmruby_objs.flatten do |t|
    Rake::Task[copy_headers_task].invoke
    archiver.run t.name, t.prerequisites
  end

  task copy_headers_task do |t|
    # Since header files may be generated dynamically and it is hard to know all of them,
    # the task is executed depending on when libmruby.a is generated.

    gemsbasedir = File.join(build_dir, "include/mruby/gems")
    dirmap = {
      MRUBY_ROOT => build_dir
    }
    gems.each { |g|
      dirmap[g.dir] = File.join(gemsbasedir, g.name)
      dirmap[g.build_dir] = File.join(gemsbasedir, g.name)
    }

    dirs = each_header_files.to_a
    dirs.uniq!
    dirs.replace_prefix_by(dirmap).zip(dirs).each do |dest, src|
      if File.mtime(src).to_i > (File.mtime(dest).to_i rescue 0)
        mkpath File.dirname(dest)
        cp src, dest
      end
    end
  end

  file "#{build_dir}/lib/libmruby.flags.mak" => [__FILE__, libmruby_static] do |t|
    mkdir_p File.dirname t.name
    open(t.name, 'w') do |f|
      f.puts <<~FLAGS_MAKE
        # GNU make is required to use this file.
        MRUBY_PACKAGE_DIR_GNU := $(shell dirname "$(lastword $(MAKEFILE_LIST))")
        MRUBY_PACKAGE_DIR != dirname "$(MRUBY_PACKAGE_DIR_GNU)"
      FLAGS_MAKE

      [
        [cc,   "MRUBY_CC",   "MRUBY_CFLAGS"],
        [cxx,  "MRUBY_CXX",  "MRUBY_CXXFLAGS"],
        [asm,  "MRUBY_AS",   "MRUBY_ASFLAGS"],
        [objc, "MRUBY_OBJC", "MRUBY_OBJCFLAGS"]
      ].each do |cc, cmd, flags|
        incpaths = cc.include_paths.dup
        dirmaps = {
          MRUBY_ROOT => "$(MRUBY_PACKAGE_DIR)",
          build_dir => "$(MRUBY_PACKAGE_DIR)"
        }
        gems.each do |g|
          incpaths.concat g.export_include_paths
          dirmaps[g.dir] = "$(MRUBY_PACKAGE_DIR)/include/mruby/gems/#{g.name}"
          dirmaps[g.build_dir] = "$(MRUBY_PACKAGE_DIR)/include/mruby/gems/#{g.name}"
        end
        modcc = cc.clone
        modcc.include_paths = incpaths.replace_prefix_by(dirmaps).uniq

        f.puts "#{cmd} = #{cc.command}"
        f.puts "#{flags} = #{modcc.all_flags}"
      end

      f.puts "MRUBY_LD = #{linker.command}"

      libgems = gems.reject{|g| g.bin?}
      gem_flags = libgems.map {|g| g.linker.flags }
      gem_library_paths = libgems.map {|g| g.linker.library_paths }
      f.puts "MRUBY_LDFLAGS = #{linker.all_flags(gem_library_paths, gem_flags)} #{linker.option_library_path % "$(MRUBY_PACKAGE_DIR)/lib"}"

      gem_flags_before_libraries = libgems.map {|g| g.linker.flags_before_libraries }
      f.puts "MRUBY_LDFLAGS_BEFORE_LIBS = #{[linker.flags_before_libraries, gem_flags_before_libraries].flatten.join(' ')}"

      gem_libraries = libgems.map {|g| g.linker.libraries }
      libmruby = (toolchains.find { |e| e == "visualcpp" }) ? "libmruby" : "mruby"
      f.puts "MRUBY_LIBS = #{linker.option_library % libmruby} #{linker.library_flags(gem_libraries)}"

      f.puts "MRUBY_LIBMRUBY_PATH = #{libmruby_static.replace_prefix_by(build_dir => "$(MRUBY_PACKAGE_DIR)")}"
    end
  end

  products << libmruby_static
end
