MRuby::Gem::Specification.new('mruby-bin-strip') do |spec|
  spec.license = 'MIT'
  spec.author  = 'mruby developers'
  spec.summary = 'irep dump debug section remover command'
  spec.add_dependency 'mruby-compiler', :core => 'mruby-compiler'
  spec.bins = %w(mruby-strip)
end
