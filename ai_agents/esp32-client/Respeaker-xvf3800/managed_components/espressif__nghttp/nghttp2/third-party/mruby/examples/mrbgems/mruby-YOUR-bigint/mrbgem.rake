MRuby::Gem::Specification.new('mruby-bigint') do |spec|
  spec.author = 'YOUR-NAME-HERE'
  spec.license = 'YOUR-LICENSE-HERE'
  spec.summary = 'Yet another multi-precision Integer extension'
  spec.homepage = 'https://gem.example/for/mruby-YOUR-bigint'
  spec.build.defines << 'MRB_USE_BIGINT'
  #spec.build.linker.libraries << 'gmp' # when uses libgmp

  spec.build.libmruby_core_objs << Dir.glob(File.join(__dir__, 'core/**/*.c')).map { |fn|
    objfile(fn.relative_path_from(__dir__).pathmap("#{spec.build_dir}/%X"))
  }
end
