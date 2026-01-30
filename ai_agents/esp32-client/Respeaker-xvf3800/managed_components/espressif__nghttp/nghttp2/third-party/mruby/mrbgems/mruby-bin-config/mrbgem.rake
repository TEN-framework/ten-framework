iscross = MRuby::Build.current.kind_of?(MRuby::CrossBuild)

MRuby::Gem::Specification.new('mruby-bin-config') do |spec|
  name = 'mruby-config'
  spec.license = 'MIT'
  spec.author  = 'mruby developers'
  spec.summary = "#{name} command"

  if iscross
    mruby_config_dir = "#{build.build_dir}/host-bin"
  else
    mruby_config_dir = "#{build.build_dir}/bin"
  end

  if ENV['OS'] == 'Windows_NT'
    suffix = '.bat'
    refvar = '%\\1%'
  else
    suffix = ''
    refvar = '${\\1}'
  end

  mruby_config = name + suffix
  mruby_config_path = "#{mruby_config_dir}/#{mruby_config}"
  make_cfg = "#{build.build_dir}/lib/libmruby.flags.mak"
  tmplt_path = "#{__dir__}/#{mruby_config}"

  if iscross
    build.products << mruby_config_path
  else
    build.bins << mruby_config
  end

  directory mruby_config_dir

  file mruby_config_path => [__FILE__, mruby_config_dir, make_cfg, tmplt_path] do |t|
    config = Hash[File.readlines(make_cfg).map!(&:chomp).map! {|l|
      l.gsub!(/\$\((\w+)\)/, refvar)
      l.gsub('\\"', '"').split(' = ', 2).map! {|s| s.sub(/^(?=.)/, 'echo ')}
    }]
    tmplt = File.read(tmplt_path)
    tmplt.sub!(%r((?<=\A#!/bin/sh\n\n)), <<~SETDIR)
      MRUBY_PACKAGE_DIR=$(dirname "$(dirname "$(readlink -f "$0")")")

    SETDIR
    File.write(t.name, tmplt.gsub(/(#{Regexp.union(*config.keys)})\b/, config))
    chmod(0755, t.name)
  end
end
